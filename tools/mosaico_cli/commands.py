"""Implement the public ESP-Mosaico product commands."""

from __future__ import annotations

import json
import codecs
import os
from pathlib import Path
import selectors
import subprocess
import sys
import time
from typing import Any

from .errors import BuildError, DeviceError, OperationError, RecoveryRequiredError, SelectionError
from .gateway import (
    connected_devices,
    ensure_gateway,
    gateway_json,
    pause_managed_local_gateway,
    run_ota,
    select_device,
)
from .project import discover_artifacts, resolve_project
from .recovery import (
    load_bundle,
    preserve_evidence,
    provisioning_candidate,
    record_recovery_verification,
    recovery_is_verified,
    wait_recovery_ready,
)
from .registry import DeviceModel, load_registry, select_model
from .runtime import RunContext, build_application, resolve_idf_path, run_idf_target


def list_models(repository: Path, details: bool) -> dict[str, Any]:
    rows: list[dict[str, Any]] = []
    for model in load_registry():
        row: dict[str, Any] = {
            "id": model.id,
            "name": model.name,
            "target": model.target,
            "status": model.status,
            "default": model.default,
        }
        if details:
            manifest_path = repository / model.recovery_dir / "manifest.json"
            try:
                manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
                recovery_version = manifest.get("version", "unknown")
            except (OSError, json.JSONDecodeError):
                recovery_version = "unavailable"
            bsp = repository / model.bsp_path
            try:
                revision = subprocess.run(
                    ["git", "-C", str(bsp), "rev-parse", "--short", "HEAD"],
                    text=True,
                    capture_output=True,
                    timeout=3,
                    check=True,
                ).stdout.strip()
            except (OSError, subprocess.SubprocessError):
                revision = "unavailable"
            row.update(
                {
                    "reference_project": model.reference_project,
                    "bsp_revision": revision,
                    "recovery_version": recovery_version,
                }
            )
        rows.append(row)
    return {"models": rows}


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
            f"工程 target 为 {artifacts.target!r}，设备要求 {model.target!r}"
        )

    context.status("gateway: connecting")
    session = ensure_gateway(context, arguments.gateway_profile)
    device = select_device(connected_devices(context, session), arguments.device_id)
    device_id = str(device.get("device_id"))
    context.status(
        f"device: {device_id} mode={device.get('firmware_mode', 'unknown')} "
        f"boot_id={device.get('boot_id', 'unknown')}"
    )
    status_value = gateway_json(context, session, "status", device_id)
    status = status_value.get("device", status_value) if isinstance(status_value, dict) else {}
    if not isinstance(status, dict) or not recovery_is_verified(
        device_id, status, str(recovery_manifest.get("version") or "")
    ):
        raise RecoveryRequiredError(
            "设备尚未完成 Recovery 初始化或验证，请先运行 python mosaico.py recover"
        )
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
        print("警告：将从当前源码构建未经评审的 Recovery 候选包。", file=sys.stderr)
        context.status("bundle: current-source Recovery candidate selected")

    prior_device_id: str | None = arguments.device_id
    prior_boot_id: str | None = None
    prior_session = None
    context.status("gateway: checking the currently connected device")
    try:
        prior_session = ensure_gateway(context, arguments.gateway_profile)
        prior_device = select_device(
            connected_devices(context, prior_session), arguments.device_id
        )
        prior_device_id = str(prior_device.get("device_id"))
        prior_boot_id = str(prior_device.get("boot_id") or "") or None
        context.status(
            f"device: {prior_device_id} mode="
            f"{prior_device.get('firmware_mode', 'unknown')} "
            f"boot_id={prior_boot_id or 'unknown'}"
        )
    except (DeviceError, SelectionError):
        context.status(
            "gateway: managed device unavailable; checking the recovery interface"
        )
        if arguments.device_id:
            context.note("warning: requested device was not reachable through Gateway")

    context.status("device: detecting a unique ROM/recovery configuration interface")
    port = provisioning_candidate(context, model)
    context.status(f"device: recovery interface ready at {port}")
    idf_path = resolve_idf_path(repository, factory)
    context.status(f"idf: environment ready at {idf_path}")
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
            "device_unique": True,
        },
        "log": str(context.log_path),
    }
    if arguments.dry_run:
        context.status("validation: recovery preflight checks passed; no write performed")
        return plan

    if prior_session and prior_device_id:
        context.status("evidence: preserving crash index and any valid core dump")
        preserve_evidence(context, prior_session, prior_device_id)
        context.status("evidence: preservation attempt complete")

    # ESP-Iris and the ESP-IDF recovery target use the same USB device.  Stop
    # only the local process managed by this CLI after evidence collection and
    # immediately before handing exclusive ownership to ESP-IDF.
    context.status("gateway: releasing the USB interface for ESP-IDF")
    pause_managed_local_gateway(context)

    context.status("flash: building and writing the complete Recovery bundle")
    run_idf_target(
        context,
        idf_path=idf_path,
        project=factory,
        build_dir=factory / "build-mosaico-recover",
        target="mosaico-recover-flash",
        definitions={
            "BUILD_PROFILE": "application",
            "MOSAICO_RECOVERY_SOURCE": arguments.source,
        },
        port=port,
        timeout=arguments.timeout,
    )
    context.status("flash: ESP-IDF write completed successfully")
    context.status("gateway: reconnecting after device reset")
    session = ensure_gateway(context, arguments.gateway_profile)
    context.status("validation: waiting for the new Recovery service")
    status = wait_recovery_ready(
        context,
        session,
        expected_device_id=prior_device_id,
        previous_boot_id=prior_boot_id,
        expected_version=str(manifest.get("version") if manifest else ""),
        timeout=arguments.timeout,
    )
    verified_device_id = str(status.get("device_id") or prior_device_id or "")
    if not verified_device_id:
        raise OperationError("Recovery 已就绪，但无法确认 Device ID")
    recovery_version = str(manifest.get("version") if manifest else status.get("app_version") or "current-source")
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
        "gateway_started": session.started_local,
    }


def monitor(repository: Path, arguments: Any, context: RunContext, json_output: bool) -> int:
    session = ensure_gateway(context, arguments.gateway_profile)
    device = select_device(connected_devices(context, session), arguments.device_id)
    device_id = str(device.get("device_id"))
    argv = session.ctl_argv(
        "logs", "--device", device_id, *( () if arguments.snapshot else ("--follow",) ),
        json_output=json_output,
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
        bufsize=0,
        env=monitor_environment,
    )
    deadline = time.monotonic() + arguments.timeout if arguments.timeout else None
    try:
        assert process.stdout is not None
        selector = selectors.DefaultSelector()
        selector.register(process.stdout, selectors.EVENT_READ)
        decoder = codecs.getincrementaldecoder("utf-8")(errors="replace")
        buffered = ""
        while True:
            if deadline is not None and time.monotonic() >= deadline:
                process.terminate()
                break
            wait = 0.2
            if deadline is not None:
                wait = max(0, min(wait, deadline - time.monotonic()))
            events = selector.select(wait)
            if not events:
                if process.poll() is not None:
                    break
                continue
            chunk = os.read(process.stdout.fileno(), 4096)
            if not chunk:
                break
            buffered += decoder.decode(chunk)
            lines = buffered.splitlines(keepends=True)
            buffered = "" if not lines or lines[-1].endswith(("\n", "\r")) else lines.pop()
            for line in lines:
                context.note(line)
                searchable = line
                if json_output and arguments.grep:
                    try:
                        item = json.loads(line)
                        searchable = str(item.get("text", "")) if isinstance(item, dict) else ""
                    except json.JSONDecodeError:
                        searchable = ""
                if not arguments.grep or arguments.grep in searchable:
                    print(line, end="", flush=True)
        buffered += decoder.decode(b"", final=True)
        if buffered:
            context.note(buffered)
            if not arguments.grep or arguments.grep in buffered:
                print(buffered, end="", flush=True)
        return_code = process.wait(timeout=5)
        if return_code not in {0, -15}:
            raise DeviceError(
                "日志连接异常结束", details={"log": str(context.log_path)}
            )
    except KeyboardInterrupt:
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
        return 0
    return 0
