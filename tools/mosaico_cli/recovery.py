"""Validate Recovery bundles and implement internal provisioning helpers."""

from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import subprocess
import time
from typing import Any

from .errors import DeviceError, EnvironmentError, OperationError, SelectionError
from .gateway import GatewaySession, connected_devices, gateway_json, locate_iris_tools
from .host import state_root
from .registry import DeviceModel
from .runtime import RunContext


REQUIRED_IMAGES = ("bootloader", "partition_table", "ota_data", "recovery")
_VERIFICATION_READ_ATTEMPTS = 3
_VERIFICATION_READ_RETRY_SECONDS = 0.05


def _verification_key(device_id: str) -> str:
    return hashlib.sha256(device_id.encode("utf-8")).hexdigest()


def _verification_path(device_id: str) -> Path:
    """Return the repository-local state path shared with interactive shells."""
    repository = Path(__file__).resolve().parents[2]
    return repository / ".mosaico-state" / "devices" / f"{_verification_key(device_id)}.json"


def _host_verification_path(device_id: str) -> Path:
    key = hashlib.sha256(device_id.encode("utf-8")).hexdigest()
    return state_root("esp-mosaico") / "devices" / f"{key}.json"


def _legacy_verification_path(device_id: str) -> Path:
    key = hashlib.sha256(device_id.encode("utf-8")).hexdigest()
    legacy_root = Path(
        os.environ.get("XDG_STATE_HOME", Path.home() / ".local" / "state")
    )
    return legacy_root / "esp-mosaico" / "devices" / f"{key}.json"


def _read_verification_record(
    path: Path, diagnostics: list[dict[str, Any]] | None = None
) -> dict[str, Any] | None:
    """Read a host verification record despite brief Windows sharing races."""
    for attempt in range(_VERIFICATION_READ_ATTEMPTS):
        try:
            value = json.loads(path.read_text(encoding="utf-8"))
        except FileNotFoundError:
            if diagnostics is not None:
                diagnostics.append({"path": str(path), "state": "not_found"})
            return None
        except (OSError, json.JSONDecodeError) as error:
            if attempt + 1 == _VERIFICATION_READ_ATTEMPTS:
                if diagnostics is not None:
                    diagnostics.append(
                        {
                            "path": str(path),
                            "state": "read_failed",
                            "error": f"{type(error).__name__}: {error}",
                        }
                    )
                return None
            time.sleep(_VERIFICATION_READ_RETRY_SECONDS)
            continue
        if isinstance(value, dict):
            if diagnostics is not None:
                diagnostics.append(
                    {
                        "path": str(path),
                        "state": "read",
                        "device_id": value.get("device_id"),
                        "recovery_version": value.get("recovery_version"),
                    }
                )
            return value
        if diagnostics is not None:
            diagnostics.append({"path": str(path), "state": "invalid_value"})
        return None
    return None


def recovery_verification_details(
    device_id: str, status: dict[str, Any], expected_version: str
) -> tuple[bool, dict[str, Any]]:
    if status.get("firmware_mode") == "recovery":
        capabilities = status.get("capability_names", [])
        verified = (
            isinstance(capabilities, list)
            and "ota" in capabilities
            and status.get("app_version") == expected_version
        )
        return verified, {
            "source": "live_recovery",
            "firmware_mode": "recovery",
            "expected_version": expected_version,
            "actual_version": status.get("app_version"),
            "ota_ready": isinstance(capabilities, list) and "ota" in capabilities,
        }
    records: list[dict[str, Any]] = []
    for path in dict.fromkeys(
        (
            _verification_path(device_id),
            _host_verification_path(device_id),
            _legacy_verification_path(device_id),
        )
    ):
        value = _read_verification_record(path, records)
        if value is None:
            continue
        if (
            value.get("device_id") == device_id
            and value.get("recovery_version") == expected_version
        ):
            if path != _verification_path(device_id):
                try:
                    record_recovery_verification(
                        device_id,
                        expected_version,
                        value.get("recovery_boot_id"),
                    )
                except OSError:
                    # The existing host record remains valid for this process;
                    # installation diagnostics will expose a later shared-path
                    # write failure if the other environment still cannot read it.
                    pass
            return True, {
                "source": "host_record",
                "firmware_mode": status.get("firmware_mode"),
                "expected_device_id": device_id,
                "expected_version": expected_version,
                "records": records,
            }
    return False, {
        "source": "host_record",
        "firmware_mode": status.get("firmware_mode"),
        "expected_device_id": device_id,
        "expected_version": expected_version,
        "records": records,
    }


def recovery_is_verified(
    device_id: str, status: dict[str, Any], expected_version: str
) -> bool:
    verified, _ = recovery_verification_details(device_id, status, expected_version)
    return verified


def record_recovery_verification(
    device_id: str, recovery_version: str, boot_id: str | int | None
) -> None:
    path = _verification_path(device_id)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(".tmp")
    temporary.write_text(
        json.dumps(
            {
                "device_id": device_id,
                "recovery_version": recovery_version,
                "recovery_boot_id": boot_id,
            },
            sort_keys=True,
        )
        + "\n",
        encoding="utf-8",
    )
    temporary.replace(path)


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def load_bundle(directory: Path, expected_target: str) -> dict[str, Any]:
    manifest_path = directory / "manifest.json"
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except FileNotFoundError as error:
        raise EnvironmentError(
            f"The reviewed Recovery bundle does not exist: {manifest_path}"
        ) from error
    except (OSError, json.JSONDecodeError) as error:
        raise EnvironmentError(f"Invalid Recovery manifest: {manifest_path}") from error
    if manifest.get("schema_version") != 2:
        raise EnvironmentError(
            "The Recovery bundle schema is obsolete; publish a complete reviewed bundle again."
        )
    if manifest.get("target") != expected_target or manifest.get("profile") != "recovery":
        raise EnvironmentError("The Recovery bundle is incompatible with the target model.")
    images = manifest.get("images")
    if not isinstance(images, dict):
        raise EnvironmentError("The Recovery manifest is missing 'images'.")
    seen_offsets: set[int] = set()
    for name in REQUIRED_IMAGES:
        item = images.get(name)
        if not isinstance(item, dict):
            raise EnvironmentError(f"The Recovery manifest is missing '{name}'.")
        try:
            path = directory / item["file"]
            offset = int(str(item["offset"]), 0)
            size = int(item["size"])
            expected_hash = str(item["sha256"])
        except (KeyError, TypeError, ValueError) as error:
            raise EnvironmentError(
                f"The '{name}' entry in the Recovery manifest is invalid."
            ) from error
        if path.parent.resolve() != directory.resolve() or not path.is_file():
            raise EnvironmentError(f"A Recovery bundle file does not exist: {path}")
        if offset in seen_offsets:
            raise EnvironmentError(
                f"The Recovery bundle contains a duplicate offset: 0x{offset:x}"
            )
        seen_offsets.add(offset)
        if path.stat().st_size != size or _sha256(path) != expected_hash:
            raise EnvironmentError(f"Recovery bundle verification failed: {path.name}")
    return manifest


def _stable_port_path(device: str) -> str:
    if os.name != "posix":
        return device
    target = os.path.realpath(device)
    for directory in (Path("/dev/serial/by-path"), Path("/dev/serial/by-id")):
        if not directory.is_dir():
            continue
        for candidate in sorted(directory.iterdir()):
            if os.path.realpath(candidate) == target:
                return str(candidate)
    return device


def _registered_recovery_ports(model: DeviceModel) -> list[str]:
    try:
        from serial.tools import list_ports
    except ImportError:
        return []
    expected = [
        (
            int(item["vid"], 0),
            int(item["pid"], 0),
            item.get("product", ""),
        )
        for item in model.recovery_usb_ids
    ]
    matches: list[str] = []
    for port in list_ports.comports():
        # Windows' generic USB serial driver commonly omits the USB product
        # string even though the VID/PID and serial number are available.
        # Require the registered VID/PID and validate the product only when
        # the host actually reports one.
        registered = any(
            port.vid == vid
            and port.pid == pid
            and (not port.product or not product or port.product == product)
            for vid, pid, product in expected
        )
        if registered:
            matches.append(_stable_port_path(port.device))
    return sorted(set(matches))


def provisioning_candidate(context: RunContext, model: DeviceModel) -> str:
    python, script = locate_iris_tools(context.repository)
    try:
        result = context.run([python, script, "doctor", "--json"], timeout=15)
    except subprocess.TimeoutExpired as error:
        raise DeviceError("Device configuration channel detection timed out.") from error
    if result.returncode:
        raise DeviceError("Could not detect a device configuration channel.")
    try:
        report = json.loads(result.stdout)
    except json.JSONDecodeError as error:
        raise DeviceError(
            "Device configuration channel detection returned an invalid result."
        ) from error
    candidates = [
        item for item in report.get("devices", [])
        if isinstance(item, dict)
        and item.get("path")
        and item.get("transport") in {"usb_serial_jtag", "serial_jtag", "rom"}
    ]
    candidates.extend(
        {"path": path, "transport": "registered_recovery"}
        for path in _registered_recovery_ports(model)
        if not any(item.get("path") == path for item in candidates)
    )
    if not candidates:
        raise DeviceError(
            "No ESP-Mosaico device in recovery configuration mode was detected. "
            "Power off the device, hold the Boot button to the left of the USB-C port, "
            "power it on, release Boot after it enters recovery mode, and try again."
        )
    if len(candidates) != 1:
        raise SelectionError(
            "Multiple low-level device candidates were detected; leave only the target "
            "device connected."
        )
    return str(candidates[0]["path"])


def preserve_evidence(
    context: RunContext,
    session: GatewaySession,
    device_id: str,
) -> None:
    try:
        crashes = gateway_json(context, session, "crash", device_id, timeout=10)
        (context.directory / "crash-index.json").write_text(
            json.dumps(crashes, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
        )
        items = (
            crashes.get("reports", crashes.get("crashes", []))
            if isinstance(crashes, dict)
            else []
        )
        items = items if isinstance(items, list) else []
        has_core = any(
            item.get("core_dump_present") and item.get("core_dump_valid")
            for item in items
            if isinstance(item, dict)
        )
        if has_core:
            result = context.run(
                session.ctl_argv(
                    "coredump", device_id, str(context.directory / "core-dump.bin")
                ),
                timeout=30,
            )
            if result.returncode:
                context.note("warning: coredump preservation failed")
    except (OSError, DeviceError, subprocess.TimeoutExpired):
        context.note("warning: Gateway evidence was not available before recovery")


def wait_recovery_ready(
    context: RunContext,
    session: GatewaySession,
    *,
    expected_device_id: str | None,
    previous_boot_id: str | None,
    expected_version: str,
    timeout: float,
) -> dict[str, Any]:
    deadline = time.monotonic() + timeout
    last_status: dict[str, Any] | None = None
    last_reported: tuple[Any, ...] | None = None
    last_wait_notice = time.monotonic()
    while time.monotonic() < deadline:
        try:
            devices = connected_devices(context, session)
            if expected_device_id:
                matches = [item for item in devices if item.get("device_id") == expected_device_id]
            else:
                matches = devices if len(devices) == 1 else []
            if len(matches) == 1:
                device_id = str(matches[0].get("device_id"))
                value = gateway_json(context, session, "status", device_id, timeout=8)
                status = value.get("device", value) if isinstance(value, dict) else {}
                if isinstance(status, dict):
                    last_status = status
                    mode = status.get("firmware_mode") or status.get("mode")
                    boot_id = status.get("boot_id")
                    app = status.get("app", {}) if isinstance(status.get("app"), dict) else {}
                    version = status.get("app_version") or status.get("version") or app.get("version")
                    capabilities = status.get("capability_names", [])
                    boot_changed = not previous_boot_id or boot_id != previous_boot_id
                    version_matches = not expected_version or version == expected_version
                    writer_ready = isinstance(capabilities, list) and "ota" in capabilities
                    observation = (device_id, mode, boot_id, version, writer_ready)
                    if observation != last_reported:
                        context.status(
                            f"validation: device={device_id} mode={mode or 'unknown'} "
                            f"version={version or 'unknown'} boot_id={boot_id or 'unknown'} "
                            f"ota={'ready' if writer_ready else 'unavailable'}"
                        )
                        last_reported = observation
                        last_wait_notice = time.monotonic()
                    if mode == "recovery" and boot_changed and version_matches and writer_ready:
                        return status
        except DeviceError:
            pass
        if time.monotonic() - last_wait_notice >= 5:
            context.status("validation: still waiting for Recovery to reconnect")
            last_wait_notice = time.monotonic()
        time.sleep(0.5)
    raise OperationError(
        "Recovery was written, but the Recovery service could not be verified as ready.",
        details={"last_status": last_status, "log": str(context.log_path)},
    )
