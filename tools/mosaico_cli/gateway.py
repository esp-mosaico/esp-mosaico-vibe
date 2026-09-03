"""Manage ESP-Iris Gateway discovery and lifecycle for mosaico.py."""

from __future__ import annotations

from dataclasses import dataclass
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time
from typing import Any
from urllib.error import URLError
from urllib.request import urlopen
import uuid

from .errors import (
    DeviceError,
    EnvironmentError,
    OperationError,
    OutcomeUnknownError,
    SelectionError,
)
from .host import state_root, virtual_environment_python
from .runtime import RunContext


LOCAL_URL = "http://127.0.0.1:8443"
REQUIRED_GATEWAY_API_MAJOR = 1
MAINTENANCE_CAPABILITY = "device-maintenance-lease/v1"
ENDPOINT_MAINTENANCE_CAPABILITY = "physical-endpoint-maintenance-lease/v1"


def _state_home() -> Path:
    return state_root("esp-mosaico") / "gateway"


def _python_major_minor(python: Path) -> tuple[int, int] | None:
    try:
        result = subprocess.run(
            [
                str(python),
                "-c",
                "import sys; print(f'{sys.version_info.major}.{sys.version_info.minor}')",
            ],
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            timeout=10,
            check=False,
        )
    except (OSError, subprocess.TimeoutExpired):
        return None
    match = re.fullmatch(r"(\d+)\.(\d+)", result.stdout.strip())
    if result.returncode or match is None:
        return None
    return int(match.group(1)), int(match.group(2))


def iris_environment_root(source: Path) -> Path:
    """Select an isolated Gateway environment for the active Python runtime."""

    desired = sys.version_info.major, sys.version_info.minor
    legacy = source / ".venv"
    legacy_python = virtual_environment_python(legacy)
    if legacy_python.is_file() and _python_major_minor(legacy_python) == desired:
        return legacy
    return source / f".venv-py{desired[0]}.{desired[1]}"


def locate_iris_tools(repository: Path) -> tuple[Path, Path]:
    source = repository / "submodule" / "esp-iris"
    script = source / "components" / "esp_iris" / "tools" / "esp_iris.py"
    python = virtual_environment_python(iris_environment_root(source))
    if not script.is_file():
        raise EnvironmentError(
            "The pinned ESP-Iris submodule is unavailable. Run "
            "'git submodule update --init submodule/esp-iris'."
        )
    if not python.is_file():
        raise EnvironmentError(
            f"The pinned ESP-Iris host environment is unavailable: {python}"
        )
    return python, script


def ensure_iris_tools(context: RunContext) -> tuple[Path, Path]:
    repository = context.repository
    source = repository / "submodule" / "esp-iris"
    tools = source / "components" / "esp_iris" / "tools"
    script = tools / "esp_iris.py"
    lock = tools / "requirements.lock"
    if not script.is_file() or not lock.is_file():
        raise EnvironmentError(
            "The pinned ESP-Iris submodule or its host dependency lock is unavailable."
        )
    environment_root = iris_environment_root(source)
    python = virtual_environment_python(environment_root)
    marker = environment_root / ".mosaico-requirements"
    fingerprint = (
        f"python={sys.version_info.major}.{sys.version_info.minor}\n"
        f"requirements={hashlib.sha256(lock.read_bytes()).hexdigest()}\n"
    )
    current = marker.read_text(encoding="utf-8") if marker.is_file() else ""
    desired = sys.version_info.major, sys.version_info.minor
    if python.is_file() and _python_major_minor(python) != desired:
        result = context.run(
            [sys.executable, "-m", "venv", "--clear", environment_root],
            timeout=120,
        )
        if result.returncode:
            raise EnvironmentError(
                "Could not refresh the pinned ESP-Iris host environment."
            )
        current = ""
    if not python.is_file():
        if sys.version_info < (3, 8):
            raise EnvironmentError("ESP-Iris host runtime requires Python 3.8 or newer.")
        result = context.run(
            [sys.executable, "-m", "venv", environment_root], timeout=120
        )
        if result.returncode:
            raise EnvironmentError("Could not create the pinned ESP-Iris host environment.")
    if current != fingerprint:
        result = context.run(
            [
                python,
                "-m",
                "pip",
                "install",
                "--disable-pip-version-check",
                "--requirement",
                lock,
            ],
            timeout=600,
        )
        if result.returncode:
            raise EnvironmentError("Could not install the pinned ESP-Iris host dependencies.")
        marker.write_text(fingerprint, encoding="utf-8")
    return python, script


@dataclass(frozen=True)
class GatewaySession:
    python: Path
    script: Path
    connection_args: tuple[str, ...]
    profile: str | None
    started_local: bool

    def ctl_argv(self, *arguments: str, json_output: bool = True) -> list[str]:
        result = [str(self.python), str(self.script), "ctl", *self.connection_args]
        if json_output:
            result.append("--json")
        result.extend(arguments)
        return result


def _decode_json(output: str) -> Any:
    try:
        return json.loads(output)
    except json.JSONDecodeError as error:
        raise OperationError("ESP-Iris returned an invalid result.") from error


def _probe(
    context: RunContext,
    python: Path,
    script: Path,
    connection: tuple[str, ...],
) -> bool:
    try:
        result = context.run(
            [python, script, "ctl", *connection, "--json", "devices"], timeout=4
        )
    except (OSError, subprocess.TimeoutExpired):
        return False
    if result.returncode:
        return False
    try:
        value = json.loads(result.stdout)
    except json.JSONDecodeError:
        return False
    return isinstance(value, dict) and isinstance(value.get("devices"), list)


def _gateway_health(
    context: RunContext,
    python: Path,
    script: Path,
    connection: tuple[str, ...],
) -> dict[str, Any] | None:
    try:
        result = context.run(
            [python, script, "ctl", *connection, "--json", "health"], timeout=4
        )
    except (OSError, subprocess.TimeoutExpired):
        return None
    if result.returncode:
        return None
    try:
        value = json.loads(result.stdout)
    except json.JSONDecodeError:
        return None
    return value if isinstance(value, dict) else None


def _require_compatible_gateway(
    health: dict[str, Any] | None,
    *,
    expected_revision: str | None = None,
) -> dict[str, Any]:
    api = health.get("gateway_api") if isinstance(health, dict) else None
    capabilities = health.get("capabilities") if isinstance(health, dict) else None
    if not isinstance(api, dict) or api.get("major") != REQUIRED_GATEWAY_API_MAJOR:
        raise EnvironmentError(
            "The reachable ESP-Iris Gateway does not implement the required API version."
        )
    if not isinstance(capabilities, list):
        raise EnvironmentError("The reachable ESP-Iris Gateway did not report capabilities.")
    assert isinstance(health, dict)
    if (
        expected_revision is not None
        and health.get("esp_iris_revision") != expected_revision
    ):
        raise EnvironmentError(
            "The local ESP-Iris Gateway does not match the pinned submodule revision; "
            "it was not stopped."
        )
    return health


def _pinned_source_revision(repository: Path) -> str:
    try:
        result = subprocess.run(
            ["git", "-C", str(repository / "submodule" / "esp-iris"), "rev-parse", "HEAD"],
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            timeout=5,
            check=False,
        )
    except (OSError, subprocess.TimeoutExpired) as error:
        raise EnvironmentError("Could not resolve the pinned ESP-Iris revision.") from error
    revision = result.stdout.strip()
    if result.returncode or not revision:
        raise EnvironmentError("Could not resolve the pinned ESP-Iris revision.")
    return revision


def _health_instance(url: str = LOCAL_URL) -> str | None:
    try:
        with urlopen(f"{url}/v1/health", timeout=2) as response:
            value = json.loads(response.read().decode("utf-8"))
    except (OSError, URLError, ValueError, json.JSONDecodeError):
        return None
    instance = value.get("instance_id") if isinstance(value, dict) else None
    return str(instance) if instance else None


def ensure_gateway(context: RunContext, profile: str | None) -> GatewaySession:
    python, script = ensure_iris_tools(context)
    if profile:
        connection = ("--profile", profile)
        if not _probe(context, python, script, connection):
            raise DeviceError(f"Gateway profile is unreachable: {profile}")
        _require_compatible_gateway(_gateway_health(context, python, script, connection))
        return GatewaySession(python, script, connection, profile, False)

    expected_revision = _pinned_source_revision(context.repository)
    local = ("--url", LOCAL_URL)
    if _probe(context, python, script, local):
        _require_compatible_gateway(
            _gateway_health(context, python, script, local),
            expected_revision=expected_revision,
        )
        return GatewaySession(python, script, local, None, False)

    # A reachable process is shared host infrastructure.  Never replace it
    # merely because this CLI cannot probe it: doing so would interrupt every
    # device attached to that Gateway.
    if _health_instance() is not None:
        raise DeviceError(
            "A local ESP-Iris Gateway is running but could not be used; it was not stopped."
        )

    state = _state_home()
    state.mkdir(parents=True, exist_ok=True)

    log_path = state / "gateway.log"
    instance_id = f"mosaico-{uuid.uuid4().hex}"
    try:
        log_stream = log_path.open("a", encoding="utf-8")
        popen_options: dict[str, Any] = {"close_fds": True}
        if os.name == "nt":
            popen_options["creationflags"] = getattr(
                subprocess, "CREATE_NEW_PROCESS_GROUP", 0
            )
        else:
            popen_options["start_new_session"] = True
        gateway_environment = os.environ.copy()
        gateway_environment["ESP_IRIS_SOURCE_REVISION"] = expected_revision
        process = subprocess.Popen(
            [
                str(python),
                str(script),
                "web",
                "--listen",
                "127.0.0.1",
                "--port",
                "8443",
                "--instance-id",
                instance_id,
                "--state-dir",
                str(state / "state"),
                "--no-tls",
            ],
            stdin=subprocess.DEVNULL,
            stdout=log_stream,
            stderr=subprocess.STDOUT,
            env=gateway_environment,
            **popen_options,
        )
        log_stream.close()
    except OSError as error:
        raise DeviceError(
            "Could not start the local ESP-Iris Gateway.",
            details={"gateway_log": str(log_path)},
        ) from error

    deadline = time.monotonic() + 20
    while time.monotonic() < deadline:
        health = _gateway_health(context, python, script, local)
        if _probe(context, python, script, local) and health is not None:
            _require_compatible_gateway(
                health, expected_revision=expected_revision
            )
            started_local = health.get("instance_id") == instance_id
            if not started_local and process.poll() is None:
                # Another workspace won the startup race.  Stop only the
                # process created above and share the compatible winner.
                process.terminate()
            return GatewaySession(python, script, local, None, started_local)
        if process.poll() is not None:
            break
        time.sleep(0.25)
    if process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            process.kill()
    raise DeviceError(
        "The local ESP-Iris Gateway failed to start.",
        details={"gateway_log": str(log_path)},
    )


def gateway_json(
    context: RunContext,
    session: GatewaySession,
    *arguments: str,
    timeout: float = 15,
    environment: dict[str, str] | None = None,
    sensitive_output: bool = False,
) -> Any:
    try:
        result = context.run(
            session.ctl_argv(*arguments),
            timeout=timeout,
            env=environment,
            sensitive_output=sensitive_output,
        )
    except subprocess.TimeoutExpired as error:
        raise DeviceError("The ESP-Iris Gateway request timed out.") from error
    if result.returncode:
        raise DeviceError(
            "The ESP-Iris Gateway request failed.",
            details={"log": str(context.log_path)},
        )
    return _decode_json(result.stdout)


def acquire_maintenance_lease(
    context: RunContext,
    session: GatewaySession,
    *,
    device_id: str,
    expected_version: str,
    timeout: float,
) -> dict[str, Any]:
    if session.profile is not None or session.connection_args != ("--url", LOCAL_URL):
        raise DeviceError("Remote Recovery is not supported; use the local Gateway.")
    health = gateway_json(context, session, "health")
    capabilities = health.get("capabilities", []) if isinstance(health, dict) else []
    if MAINTENANCE_CAPABILITY not in capabilities:
        raise EnvironmentError(
            "The local ESP-Iris Gateway does not support device maintenance leases."
        )
    value = gateway_json(
        context,
        session,
        "maintenance-acquire",
        device_id,
        "--expected-version",
        expected_version,
        "--wait-timeout",
        str(min(timeout, 60)),
        "--ttl-seconds",
        str(timeout + 60),
        timeout=min(timeout, 75),
        sensitive_output=True,
    )
    lease = value.get("lease") if isinstance(value, dict) else None
    if not isinstance(lease, dict) or not lease.get("lease_id") or not lease.get("token"):
        raise DeviceError("ESP-Iris returned an invalid maintenance lease.")
    endpoint = lease.get("endpoint")
    if not isinstance(endpoint, dict) or not (
        endpoint.get("path") or str(endpoint.get("endpoint") or "").startswith("usb:")
    ):
        try:
            finish_maintenance_lease(
                context, session, lease, abort=True, timeout=min(timeout, 30)
            )
        except DeviceError:
            pass
        raise DeviceError("The maintenance lease did not include a writable local port.")
    return lease


def acquire_endpoint_maintenance_lease(
    context: RunContext,
    session: GatewaySession,
    *,
    endpoint: str,
    expected_version: str,
    timeout: float,
) -> dict[str, Any]:
    if session.profile is not None or session.connection_args != ("--url", LOCAL_URL):
        raise DeviceError("Remote Recovery is not supported; use the local Gateway.")
    health = gateway_json(context, session, "health")
    capabilities = health.get("capabilities", []) if isinstance(health, dict) else []
    if ENDPOINT_MAINTENANCE_CAPABILITY not in capabilities:
        raise EnvironmentError(
            "The local ESP-Iris Gateway does not support physical endpoint maintenance leases."
        )
    value = gateway_json(
        context,
        session,
        "maintenance-acquire-endpoint",
        endpoint,
        "--expected-version",
        expected_version,
        "--wait-timeout",
        str(min(timeout, 60)),
        "--ttl-seconds",
        str(timeout + 60),
        timeout=min(timeout, 75),
        sensitive_output=True,
    )
    lease = value.get("lease") if isinstance(value, dict) else None
    if not isinstance(lease, dict) or not lease.get("lease_id") or not lease.get("token"):
        raise DeviceError("ESP-Iris returned an invalid maintenance lease.")
    leased_endpoint = lease.get("endpoint")
    if not isinstance(leased_endpoint, dict) or not (
        leased_endpoint.get("path")
        or str(leased_endpoint.get("endpoint") or "").startswith("usb:")
    ):
        try:
            finish_maintenance_lease(
                context, session, lease, abort=True, timeout=min(timeout, 30)
            )
        except DeviceError:
            pass
        raise DeviceError("The maintenance lease did not include a writable local port.")
    return lease


def finish_maintenance_lease(
    context: RunContext,
    session: GatewaySession,
    lease: dict[str, Any],
    *,
    abort: bool,
    timeout: float,
) -> dict[str, Any]:
    token = str(lease.get("token") or "")
    lease_id = str(lease.get("lease_id") or "")
    if not token or not lease_id:
        raise DeviceError("The maintenance lease credentials are unavailable.")
    environment = os.environ.copy()
    environment["ESP_IRIS_MAINTENANCE_TOKEN"] = token
    arguments = [
        "maintenance-abort" if abort else "maintenance-complete",
        lease_id,
    ]
    if not abort:
        arguments.extend(["--timeout", str(timeout)])
    value = gateway_json(
        context,
        session,
        *arguments,
        timeout=timeout + 15,
        environment=environment,
    )
    result = value.get("lease") if isinstance(value, dict) else None
    if not isinstance(result, dict):
        raise DeviceError("ESP-Iris returned an invalid maintenance completion result.")
    return result


def renew_maintenance_lease(
    context: RunContext,
    session: GatewaySession,
    lease: dict[str, Any],
    *,
    ttl_seconds: float,
) -> dict[str, Any]:
    token = str(lease.get("token") or "")
    lease_id = str(lease.get("lease_id") or "")
    if not token or not lease_id:
        raise DeviceError("The maintenance lease credentials are unavailable.")
    environment = os.environ.copy()
    environment["ESP_IRIS_MAINTENANCE_TOKEN"] = token
    value = gateway_json(
        context,
        session,
        "maintenance-renew",
        lease_id,
        "--ttl-seconds",
        str(ttl_seconds),
        environment=environment,
    )
    result = value.get("lease") if isinstance(value, dict) else None
    if not isinstance(result, dict):
        raise DeviceError("ESP-Iris returned an invalid maintenance renewal result.")
    return result


def gateway_devices(
    context: RunContext,
    session: GatewaySession,
    *,
    wait_seconds: float = 5,
) -> list[dict[str, Any]]:
    """Return connected and cached devices, waiting briefly for live discovery."""

    deadline = time.monotonic() + wait_seconds
    while True:
        value = gateway_json(context, session, "devices")
        if not isinstance(value, dict):
            raise DeviceError("ESP-Iris returned an invalid device list.")
        devices = [
            item for item in value.get("devices", []) if isinstance(item, dict)
        ]
        if (
            any(item.get("connected") is not False for item in devices)
            or time.monotonic() >= deadline
        ):
            return devices
        time.sleep(0.25)


def connected_devices(
    context: RunContext,
    session: GatewaySession,
    *,
    wait_seconds: float = 5,
) -> list[dict[str, Any]]:
    return [
        item
        for item in gateway_devices(context, session, wait_seconds=wait_seconds)
        if item.get("connected") is not False
    ]


def select_device(devices: list[dict[str, Any]], requested: str | None) -> dict[str, Any]:
    if requested:
        matches = [item for item in devices if item.get("device_id") == requested]
        if len(matches) == 1:
            return matches[0]
        raise DeviceError(f"The requested device is currently unavailable: {requested}")
    if len(devices) == 1:
        return devices[0]
    if not devices:
        raise DeviceError("No available ESP-Mosaico device was found.")
    raise SelectionError(
        "Multiple devices were found; specify one with --device-id.",
        details={"candidates": [item.get("device_id") for item in devices]},
    )


def run_ota(
    context: RunContext,
    session: GatewaySession,
    *,
    device_id: str,
    image: Path,
    elf: Path,
    map_file: Path,
    validation: str,
    timeout: float,
) -> dict[str, Any]:
    started = time.monotonic()
    try:
        result = context.run(
            session.ctl_argv(
                "ota",
                device_id,
                str(image),
                "--elf",
                str(elf),
                "--map",
                str(map_file),
                "--execution-mode",
                "recovery",
            ),
            timeout=timeout,
        )
    except subprocess.TimeoutExpired as error:
        raise OutcomeUnknownError(
            "Installation timed out and the device outcome is unknown; the write "
            "operation will not be replayed automatically.",
            details={"log": str(context.log_path)},
        ) from error
    try:
        value = _decode_json(result.stdout)
    except OperationError:
        value = {}
    operation = value.get("operation", value) if isinstance(value, dict) else {}
    if not isinstance(operation, dict):
        operation = {}
    operation_id = str(operation.get("operation_id") or "")
    status = operation.get("status")
    if result.returncode:
        if status in {"outcome_unknown", "unknown"} or status is None:
            raise OutcomeUnknownError(
                "The installation submission outcome is unknown; the write operation "
                "will not be replayed automatically.",
                details={"result": value, "log": str(context.log_path)},
            )
        raise OperationError(
            "Installation submission failed.",
            details={"result": value, "log": str(context.log_path)},
        )
    if not operation_id or status in {"outcome_unknown", "unknown"} or status is None:
        raise OutcomeUnknownError(
            "The installation outcome is unknown; the write operation will not be "
            "replayed automatically.",
            details={"result": value, "log": str(context.log_path)},
        )

    terminal = {
        "succeeded",
        "success",
        "completed",
        "failed",
        "cancelled",
        "interrupted",
        "outcome_unknown",
        "unknown",
    }
    last_stage = ""
    last_bucket = -1
    while status not in terminal:
        if time.monotonic() - started >= timeout:
            raise OutcomeUnknownError(
                "Installation timed out and the device outcome is unknown; the write "
                "operation will not be replayed automatically.",
                details={"operation_id": operation_id, "log": str(context.log_path)},
            )
        progress = operation.get("progress")
        progress = progress if isinstance(progress, dict) else {}
        stage = str(progress.get("stage") or status)
        permille = max(0, min(int(progress.get("progress_permille") or 0), 1000))
        bucket = permille // 50
        if stage != last_stage or bucket != last_bucket:
            detail = f"ota: {stage} {permille / 10:.1f}%"
            received = int(progress.get("bytes_received") or 0)
            total = int(progress.get("bytes_total") or 0)
            if total > 0:
                detail += f" ({received}/{total} bytes)"
            context.status(detail)
            last_stage = stage
            last_bucket = bucket
        time.sleep(0.25)
        current = gateway_json(context, session, "ota-status", operation_id)
        operation = current.get("operation", current) if isinstance(current, dict) else {}
        if not isinstance(operation, dict):
            operation = {}
        status = operation.get("status")
        if status is None:
            raise OutcomeUnknownError(
                "The installation status response is invalid; the write operation will "
                "not be replayed automatically.",
                details={"operation_id": operation_id, "result": current,
                         "log": str(context.log_path)},
            )

    progress = operation.get("progress")
    progress = progress if isinstance(progress, dict) else {}
    context.status(
        f"ota: {progress.get('stage', status)} "
        f"{int(progress.get('progress_permille') or 0) / 10:.1f}%"
    )
    if status not in {"succeeded", "success", "completed"}:
        raise OperationError(
            f"Installation failed: {status}",
            details={"result": operation, "log": str(context.log_path)},
        )
    if isinstance(value, dict):
        value["operation"] = operation
        return value
    return {"operation": operation}
