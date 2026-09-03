"""Implement the public ESP-Mosaico product commands."""

from __future__ import annotations

import json
import os
from pathlib import Path
from queue import Empty, Queue
import re
import subprocess
import sys
from threading import Thread
import time
from typing import Any, TextIO

from .errors import BuildError, DeviceError, OperationError, RecoveryRequiredError, SelectionError
from .gateway import (
    acquire_endpoint_maintenance_lease,
    acquire_maintenance_lease,
    connected_devices,
    ensure_gateway,
    finish_maintenance_lease,
    gateway_devices,
    gateway_json,
    renew_maintenance_lease,
    run_ota,
    select_device,
)
from .project import discover_artifacts, resolve_project
from .recovery import (
    load_bundle,
    provisioning_candidate,
    record_recovery_verification,
    recovery_verification_details,
)
from .registry import select_model
from .runtime import RunContext, build_application, resolve_idf_path, run_idf_target


_IDF_MONITOR_LOG_PATTERN = re.compile(r"^(I|W|E) \([\d:\. -]+\)")
_IDF_MONITOR_COLORS = {
    "I": "\033[0;32m",
    "W": "\033[0;33m",
    "E": "\033[1;31m",
}
_ANSI_NORMAL = "\033[0m"


def _device_status(value: Any) -> dict[str, Any]:
    """Normalize Gateway status responses without bypassing host verification."""
    if not isinstance(value, dict):
        return {}
    status = value.get("device", value)
    return status if isinstance(status, dict) else {}


def _recovery_verification_status(
    device: dict[str, Any], status_value: Any
) -> dict[str, Any]:
    """Reconcile the two Gateway snapshots used during install preflight."""
    status = _device_status(status_value)
    listed_mode = device.get("firmware_mode")
    if listed_mode == "normal":
        # The selected-device snapshot identifies the current application
        # boot. A second status request may briefly return an incomplete or
        # stale Recovery-shaped payload while Gateway reconciles reconnects.
        status = {**status, "firmware_mode": "normal"}
    elif listed_mode == "recovery" and not status.get("firmware_mode"):
        status = {**status, "firmware_mode": "recovery"}
    return status


class _MonitorTextRenderer:
    """Reassemble ESP-Iris log records and render complete device log lines."""

    def __init__(
        self,
        stream: TextIO,
        *,
        grep: str | None,
        color_enabled: bool,
    ) -> None:
        self._stream = stream
        self._grep = grep
        self._color_enabled = color_enabled
        self._buffer = ""

    def feed(self, text: str) -> None:
        self._buffer += text
        while True:
            newline = self._buffer.find("\n")
            if newline < 0:
                return
            line = self._buffer[: newline + 1]
            self._buffer = self._buffer[newline + 1 :]
            self._emit(line)

    def finish(self) -> None:
        if self._buffer:
            self._emit(self._buffer)
            self._buffer = ""

    def _emit(self, line: str) -> None:
        if self._grep and self._grep not in line:
            return
        if line.endswith("\r\n"):
            body, ending = line[:-2], "\r\n"
        elif line.endswith("\n"):
            body, ending = line[:-1], "\n"
        else:
            body, ending = line, ""

        match = _IDF_MONITOR_LOG_PATTERN.match(body) if self._color_enabled else None
        if match:
            color = _IDF_MONITOR_COLORS[match.group(1)]
            rendered = f"{color}{body}{_ANSI_NORMAL}{ending}"
        else:
            rendered = line
        self._stream.write(rendered)
        self._stream.flush()


def _monitor_color_enabled(arguments: Any, json_output: bool) -> bool:
    if json_output or getattr(arguments, "disable_auto_color", False):
        return False
    if getattr(arguments, "force_color", False):
        return True
    if not sys.stdout.isatty():
        return False
    if os.name != "nt":
        return True
    try:
        import ctypes

        ctypes_module: Any = ctypes
        kernel = ctypes_module.windll.kernel32
        handle = kernel.GetStdHandle(-11)
        mode = ctypes.c_uint32()
        if not kernel.GetConsoleMode(handle, ctypes.byref(mode)):
            return False
        return bool(kernel.SetConsoleMode(handle, mode.value | 0x0004))
    except (AttributeError, OSError, ValueError):
        return False


def list_devices(context: RunContext, gateway_profile: str | None) -> dict[str, Any]:
    """List live devices visible through the selected Gateway."""

    session = ensure_gateway(context, gateway_profile)
    devices = sorted(
        gateway_devices(context, session),
        key=lambda item: str(item.get("device_id") or ""),
    )
    for device in devices:
        device["online"] = device.get("connected") is not False
        device["connection"] = device.get("transport_name") or device.get("transport")
    return {
        "command": "list",
        "status": "succeeded",
        "gateway_started": session.started_local,
        "gateway_profile": session.profile,
        "devices": devices,
    }


def start_http_system_update(
    arguments: Any, context: RunContext
) -> dict[str, Any]:
    """Ask a live Recovery instance to pull and apply an HTTP(S) bundle."""

    context.status("gateway: connecting")
    session = ensure_gateway(context, arguments.gateway_profile)
    device = select_device(connected_devices(context, session), arguments.device_id)
    device_id = str(device.get("device_id"))
    status = _device_status(
        gateway_json(context, session, "status", device_id)
    )
    firmware_mode = status.get("firmware_mode") or device.get("firmware_mode")
    context.status(
        f"device: {device_id} mode={firmware_mode or 'unknown'} "
        f"boot_id={status.get('boot_id') or device.get('boot_id') or 'unknown'}"
    )
    if firmware_mode != "recovery":
        raise RecoveryRequiredError(
            "HTTP system update requires a live Recovery service. "
            "Run 'python mosaico.py recover' first."
        )

    context.status("system update: requesting Recovery HTTP(S) pull")
    response = gateway_json(
        context,
        session,
        "rpc-raw",
        device_id,
        "0x1201",
        "1",
        "--payload",
        arguments.manifest_url,
        "--deadline-ms",
        "5000",
        timeout=10,
        sensitive_output=True,
    )
    context.status("system update: accepted by Recovery; download is running")
    return {
        "command": "system-update",
        "status": "accepted",
        "device_id": device_id,
        "firmware_mode": firmware_mode,
        "gateway_started": session.started_local,
        "response": response,
        "log": str(context.log_path),
    }


def install(repository: Path, arguments: Any, context: RunContext) -> dict[str, Any]:
    project = resolve_project(repository, arguments.project, Path.cwd())
    context.status(f"project: {project}")
    reused = bool(arguments.skip_build)
    if not reused:
        build_application(context, project)
    else:
        context.status("build: reusing existing artifacts (--skip-build)")
    artifacts = discover_artifacts(project)
    context.status(
        f"artifact: {artifacts.project_name} {artifacts.project_version} "
        f"({artifacts.image.stat().st_size} bytes, {artifacts.target})"
    )
    model = select_model(None)
    recovery_manifest = load_bundle(repository / model.recovery_dir, model.target)
    if artifacts.target != model.target:
        raise BuildError(
            f"The project target is {artifacts.target!r}, but the device requires "
            f"{model.target!r}."
        )

    context.status("gateway: connecting")
    session = ensure_gateway(context, arguments.gateway_profile)
    device = select_device(connected_devices(context, session), arguments.device_id)
    device_id = str(device.get("device_id"))
    context.status(
        f"device: {device_id} mode={device.get('firmware_mode', 'unknown')} "
        f"boot_id={device.get('boot_id', 'unknown')}"
    )
    status = _recovery_verification_status(
        device, gateway_json(context, session, "status", device_id)
    )
    recovery_version = str(recovery_manifest.get("version") or "")
    recovery_verified, verification = recovery_verification_details(
        device_id, status, recovery_version
    )
    context.note(
        "recovery verification: "
        + json.dumps(verification, ensure_ascii=False, sort_keys=True)
    )
    if not recovery_verified:
        raise RecoveryRequiredError(
            "The device has not completed Recovery initialization or verification. "
            "Run 'python mosaico.py recover' first. "
            f"Verification details: {json.dumps(verification, ensure_ascii=False)}"
        )
    if status.get("firmware_mode") == "recovery":
        try:
            record_recovery_verification(
                device_id, recovery_version, status.get("boot_id")
            )
        except OSError as error:
            # Live Recovery is authoritative for this install. Do not discard
            # that proof merely because Windows briefly blocks the state file.
            context.note(f"warning: could not refresh Recovery verification: {error}")
    context.status("recovery: verified retained Recovery service")

    context.status(f"ota: starting recovery-first installation ({arguments.validation})")
    operation = run_ota(
        context,
        session,
        device_id=device_id,
        image=artifacts.image,
        elf=artifacts.elf,
        map_file=artifacts.map_file,
        validation=arguments.validation,
        timeout=arguments.timeout,
    )
    context.status("validation: installed application is connected and healthy")
    return {
        "command": "install",
        "status": "succeeded",
        "project": str(project),
        "device_id": device_id,
        "firmware": {
            "name": artifacts.project_name,
            "version": artifacts.project_version,
            "target": artifacts.target,
        },
        "validation": arguments.validation,
        "reused_build": reused,
        "gateway_started": session.started_local,
        "operation": operation,
        "log": str(context.log_path),
    }


def recover(repository: Path, arguments: Any, context: RunContext) -> dict[str, Any]:
    if getattr(arguments, "gateway_profile", None):
        raise DeviceError(
            "Remote Recovery is not supported; run recovery on the Gateway host."
        )
    model = select_model(arguments.model)
    context.status(
        f"recovery: model={model.id} target={model.target} source={arguments.source}"
    )
    factory = repository / model.reference_project
    bundle_dir = repository / model.recovery_dir
    manifest: dict[str, Any] | None = None
    if arguments.source == "reviewed":
        manifest = load_bundle(bundle_dir, model.target)
        recovery_image = manifest.get("images", {}).get("recovery", {})
        context.status(
            f"bundle: verified {manifest.get('version', 'unknown')} "
            f"({recovery_image.get('size', 'unknown')} bytes)"
        )
    elif not arguments.dry_run:
        print(
            "Warning: building an unreviewed Recovery candidate bundle from the current source.",
            file=sys.stderr,
        )
        context.status("bundle: current-source Recovery candidate selected")

    prior_device_id: str | None = arguments.device_id
    prior_boot_id: str | None = None
    prior_session = None
    context.status("gateway: checking the currently connected device")
    try:
        prior_session = ensure_gateway(context, None)
        devices = connected_devices(context, prior_session)
    except DeviceError:
        if not arguments.dry_run:
            raise
        devices = []
        context.note("warning: local Gateway was unavailable during dry-run")
    prior_device = None
    if arguments.device_id:
        matches = [item for item in devices if item.get("device_id") == arguments.device_id]
        if len(matches) == 1:
            prior_device = matches[0]
        else:
            context.note("warning: requested device was not reachable through Gateway")
    elif len(devices) == 1:
        prior_device = devices[0]
    elif len(devices) > 1:
        raise SelectionError(
            "Multiple managed devices were found; specify the recovery target with --device-id."
        )
    if prior_device is not None:
        prior_device_id = str(prior_device.get("device_id"))
        prior_boot_id = str(prior_device.get("boot_id") or "") or None
        context.status(
            f"device: {prior_device_id} mode="
            f"{prior_device.get('firmware_mode', 'unknown')} "
            f"boot_id={prior_boot_id or 'unknown'}"
        )
    else:
        context.status(
            "gateway: managed device unavailable; checking the recovery interface"
        )

    unowned_port: str | None = None
    if prior_device is None:
        context.status("device: detecting a unique unowned ROM configuration interface")
        unowned_port = provisioning_candidate(context, model)
        context.status(f"device: recovery interface ready at {unowned_port}")

    idf_path = resolve_idf_path(repository, factory)
    context.status(f"idf: environment ready at {idf_path}")
    build_dir = factory / "build-mosaico-recover"
    plan = {
        "command": "recover",
        "status": "dry_run" if arguments.dry_run else "planned",
        "model": model.id,
        "source": arguments.source,
        "device_id": prior_device_id,
        "target": model.target,
        "recovery_version": manifest.get("version") if manifest else "current-source",
        "checks": {
            "idf": str(idf_path),
            "bundle_verified": manifest is not None,
            "device_unique": prior_device is not None or unowned_port is not None,
            "gateway_local": True,
        },
        "log": str(context.log_path),
    }
    if arguments.dry_run:
        context.status("validation: recovery preflight checks passed; no write performed")
        return plan

    if prior_session is None:
        raise DeviceError("The local Gateway is required to verify Recovery.")

    context.status("bundle: preparing all Recovery artifacts before device maintenance")
    run_idf_target(
        context,
        idf_path=idf_path,
        project=factory,
        build_dir=build_dir,
        target="mosaico-recover-prepare",
        definitions={
            "BUILD_PROFILE": "application",
            "MOSAICO_RECOVERY_SOURCE": arguments.source,
        },
        timeout=arguments.timeout,
    )
    prepared_dir = build_dir / (
        "recovery" if arguments.source == "reviewed" else "recovery-current"
    )
    prepared_manifest = load_bundle(prepared_dir, model.target)
    expected_version = str(prepared_manifest.get("version") or "")

    lease: dict[str, Any] | None = None
    lease_finished = False
    if prior_device is not None and prior_device_id:
        context.status(f"gateway: acquiring maintenance lease for {prior_device_id}")
        lease = acquire_maintenance_lease(
            context,
            prior_session,
            device_id=prior_device_id,
            expected_version=expected_version,
            timeout=arguments.timeout,
        )
        endpoint = lease.get("endpoint", {})
        if not isinstance(endpoint, dict):
            raise DeviceError("The maintenance lease did not include a physical endpoint.")
        port = str(endpoint.get("path") or "")
        if not port:
            raw_endpoint = str(endpoint.get("endpoint") or "")
            port = raw_endpoint.removeprefix("usb:") if raw_endpoint.startswith("usb:") else ""
        if not port:
            raise DeviceError("The maintenance lease did not include a writable local port.")
        context.status(f"device: leased recovery interface ready at {port}")
    else:
        assert unowned_port is not None
        context.status(f"gateway: acquiring maintenance lease for endpoint {unowned_port}")
        lease = acquire_endpoint_maintenance_lease(
            context,
            prior_session,
            endpoint=unowned_port,
            expected_version=expected_version,
            timeout=arguments.timeout,
        )
        endpoint = lease.get("endpoint", {})
        if not isinstance(endpoint, dict):
            raise DeviceError("The maintenance lease did not include a physical endpoint.")
        port = str(endpoint.get("path") or "")
        if not port:
            raw_endpoint = str(endpoint.get("endpoint") or "")
            port = (
                raw_endpoint.removeprefix("usb:")
                if raw_endpoint.startswith("usb:")
                else ""
            )
        if not port:
            raise DeviceError("The maintenance lease did not include a writable local port.")
        context.status(f"device: leased recovery interface ready at {port}")

    try:
        if lease is not None:
            renew_maintenance_lease(
                context,
                prior_session,
                lease,
                ttl_seconds=arguments.timeout + 60,
            )
        context.status("flash: writing the prepared complete Recovery bundle")
        run_idf_target(
            context,
            idf_path=idf_path,
            project=factory,
            build_dir=build_dir,
            target="mosaico-recover-flash",
            definitions={
                "BUILD_PROFILE": "application",
                "MOSAICO_RECOVERY_SOURCE": arguments.source,
            },
            port=port,
            timeout=arguments.timeout,
        )
        context.status("flash: ESP-IDF write completed successfully")
        context.status("gateway: reattaching and verifying the leased device")
        completed = finish_maintenance_lease(
            context,
            prior_session,
            lease,
            abort=False,
            timeout=arguments.timeout,
        )
        lease_finished = True
        evidence = completed.get("evidence", {})
        status = evidence.get("verification", {}) if isinstance(evidence, dict) else {}
    except BaseException:
        if lease is not None and not lease_finished:
            context.status("gateway: aborting maintenance lease and reattaching the device")
            try:
                finish_maintenance_lease(
                    context,
                    prior_session,
                    lease,
                    abort=True,
                    timeout=min(arguments.timeout, 30),
                )
            except (DeviceError, OperationError):
                context.note(
                    "warning: maintenance lease remains quarantined; inspect the local Gateway"
                )
        raise

    verified_device_id = str(status.get("device_id") or prior_device_id or "")
    if not verified_device_id:
        raise OperationError(
            "Recovery is ready, but the Device ID could not be confirmed."
        )
    recovery_version = expected_version or str(status.get("app_version") or "current-source")
    record_recovery_verification(
        verified_device_id, recovery_version, status.get("boot_id")
    )
    context.status(
        f"validation: Recovery {recovery_version} ready on {verified_device_id} "
        f"boot_id={status.get('boot_id', 'unknown')}"
    )
    return {
        **plan,
        "status": "succeeded",
        "device_id": verified_device_id,
        "boot_id": status.get("boot_id"),
        "gateway_started": prior_session.started_local,
        "maintenance_lease_id": lease.get("lease_id") if lease else None,
    }


def monitor(repository: Path, arguments: Any, context: RunContext, json_output: bool) -> int:
    session = ensure_gateway(context, arguments.gateway_profile)
    device = select_device(connected_devices(context, session), arguments.device_id)
    device_id = str(device.get("device_id"))
    argv = session.ctl_argv(
        "logs", "--device", device_id, *( () if arguments.snapshot else ("--follow",) ),
        # Always consume structured events. The ESP-Iris text formatter uses
        # print() even though event text already carries its line ending.
        json_output=True,
    )
    context.note("$ " + " ".join(argv))
    monitor_environment = os.environ.copy()
    # The ESP-Iris CLI writes into a pipe here, so Python would otherwise use
    # block buffering. Force each printed log line through to this process,
    # whose user-facing print calls already use flush=True below.
    monitor_environment["PYTHONUNBUFFERED"] = "1"
    process = subprocess.Popen(
        argv,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        bufsize=1,
        env=monitor_environment,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    deadline = time.monotonic() + arguments.timeout if arguments.timeout else None
    try:
        stdout = process.stdout
        assert stdout is not None
        sentinel = object()
        records: Queue[str | object] = Queue()

        def read_records() -> None:
            try:
                for record in stdout:
                    records.put(record)
            finally:
                records.put(sentinel)

        reader = Thread(target=read_records, daemon=True)
        reader.start()
        invalid_child_output = False
        stopped_by_us = False
        renderer = _MonitorTextRenderer(
            sys.stdout,
            grep=arguments.grep,
            color_enabled=_monitor_color_enabled(arguments, json_output),
        )

        def handle_record(record: str) -> None:
            nonlocal invalid_child_output
            context.note(record)
            try:
                item = json.loads(record)
            except json.JSONDecodeError:
                invalid_child_output = True
                if not json_output:
                    renderer.feed(record)
                return

            text = str(item.get("text", "")) if isinstance(item, dict) else ""
            if json_output:
                if not arguments.grep or arguments.grep in text:
                    sys.stdout.write(record)
                    sys.stdout.flush()
                return
            if isinstance(item, dict) and isinstance(item.get("text"), str):
                renderer.feed(item["text"])
            else:
                renderer.feed(json.dumps(item, ensure_ascii=False) + "\n")

        while True:
            if deadline is not None and time.monotonic() >= deadline:
                process.terminate()
                stopped_by_us = True
                break
            wait = 0.2
            if deadline is not None:
                wait = max(0, min(wait, deadline - time.monotonic()))
            try:
                record = records.get(timeout=wait)
            except Empty:
                if process.poll() is not None:
                    break
                continue
            if record is sentinel:
                break
            handle_record(str(record))
        renderer.finish()
        try:
            return_code = process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            return_code = process.wait(timeout=5)
        reader.join(timeout=1)
        stdout.close()
        if not stopped_by_us and return_code != 0:
            raise DeviceError(
                "The log connection ended unexpectedly.",
                details={"log": str(context.log_path)},
            )
        if invalid_child_output:
            raise DeviceError(
                "ESP-Iris returned an invalid log event.",
                details={"log": str(context.log_path)},
            )
    except KeyboardInterrupt:
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=5)
        if process.stdout is not None:
            process.stdout.close()
        return 0
    return 0
