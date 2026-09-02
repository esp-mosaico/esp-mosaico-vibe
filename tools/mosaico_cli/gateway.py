"""Manage ESP-Iris Gateway discovery and lifecycle for mosaico.py."""

from __future__ import annotations

from dataclasses import dataclass
import json
import os
from pathlib import Path
import re
import signal
import subprocess
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


def _state_home() -> Path:
    return state_root("esp-mosaico") / "gateway"


def locate_iris_tools(repository: Path) -> tuple[Path, Path]:
    manifest = repository / "projects" / "factory" / "main" / "idf_component.yml"
    try:
        text = manifest.read_text(encoding="utf-8")
    except OSError as error:
        raise EnvironmentError(
            f"Could not read the ESP-Iris component manifest: {manifest}"
        ) from error
    match = re.search(r"(?ms)^\s*esp_iris:\s*\n\s*override_path:\s*([^\n#]+)", text)
    candidates: list[Path] = []
    if match:
        candidates.append((manifest.parent / match.group(1).strip()).resolve())
    projects = repository / "projects"
    project_directories = (
        sorted(path for path in projects.iterdir() if path.is_dir())
        if projects.is_dir()
        else []
    )
    candidates.extend(
        component
        for project in project_directories
        for component in (
            project / "managed_components" / "esp_iris",
            project / "managed_components" / "espressif__esp_iris",
        )
    )
    candidates.extend(
        [
            repository / "components" / "esp_iris",
        ]
    )
    for component in candidates:
        script = component / "tools" / "esp_iris.py"
        if not script.is_file():
            continue
        component_repo = component.parent.parent
        python = virtual_environment_python(component_repo / ".venv")
        return (python if python.is_file() else Path(os.sys.executable), script)
    raise EnvironmentError("The ESP-Iris CLI declared by this repository was not found.")


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


def _health_instance(url: str = LOCAL_URL) -> str | None:
    try:
        with urlopen(f"{url}/v1/health", timeout=2) as response:
            value = json.loads(response.read().decode("utf-8"))
    except (OSError, URLError, ValueError, json.JSONDecodeError):
        return None
    instance = value.get("instance_id") if isinstance(value, dict) else None
    return str(instance) if instance else None


def _ownership_path(state: Path) -> Path:
    return state / "gateway-owner.json"


def _load_owner(state: Path) -> dict[str, Any] | None:
    try:
        value = json.loads(_ownership_path(state).read_text(encoding="utf-8"))
        if (
            value.get("schema_version") == 1
            and isinstance(value.get("pid"), int)
            and isinstance(value.get("instance_id"), str)
            and isinstance(value.get("script"), str)
        ):
            return value
    except (OSError, TypeError, ValueError, json.JSONDecodeError):
        pass
    return None


def _write_owner(state: Path, *, pid: int, instance_id: str, script: Path) -> None:
    path = _ownership_path(state)
    temporary = path.with_suffix(".tmp")
    temporary.write_text(
        json.dumps(
            {
                "schema_version": 1,
                "pid": pid,
                "instance_id": instance_id,
                "script": str(script.resolve()),
                "state_dir": str((state / "state").resolve()),
            },
            sort_keys=True,
        )
        + "\n",
        encoding="utf-8",
    )
    temporary.replace(path)


def _owner_is_current(owner: dict[str, Any], script: Path) -> bool:
    try:
        recorded_script = Path(owner["script"]).resolve()
    except (KeyError, OSError, TypeError, ValueError):
        return False
    return (
        recorded_script == script.resolve()
        and _health_instance() == owner["instance_id"]
    )


def _owned_local_gateway(pid: int, script: Path, state: Path) -> bool:
    """Validate a legacy Linux PID record during one-way state migration."""

    if not Path("/proc").is_dir():
        return False
    try:
        command_line = Path(f"/proc/{pid}/cmdline").read_bytes()
    except OSError:
        return False
    return (
        str(script).encode() in command_line
        and str(state / "state").encode() in command_line
    )


def _terminate_managed_gateway(pid: int, instance_id: str) -> bool:
    """Terminate the complete, dedicated process group created by this CLI."""

    try:
        if os.name == "nt":
            break_event = getattr(signal, "CTRL_BREAK_EVENT", None)
            if break_event is None:
                raise OSError("CTRL_BREAK_EVENT is unavailable")
            os.kill(pid, break_event)
        else:
            os.killpg(pid, signal.SIGTERM)
    except (OSError, ProcessLookupError):
        pass
    deadline = time.monotonic() + 3
    while _health_instance() == instance_id and time.monotonic() < deadline:
        time.sleep(0.1)
    if _health_instance() != instance_id:
        return True

    if os.name == "nt":
        try:
            result = subprocess.run(
                ["taskkill", "/PID", str(pid), "/T", "/F"],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                timeout=5,
                check=False,
            )
            if result.returncode and _health_instance() == instance_id:
                return False
        except (OSError, subprocess.TimeoutExpired):
            return False
    else:
        try:
            os.killpg(pid, signal.SIGKILL)
        except ProcessLookupError:
            return True
        except OSError:
            return False
    deadline = time.monotonic() + 2
    while _health_instance() == instance_id and time.monotonic() < deadline:
        time.sleep(0.1)
    return _health_instance() != instance_id


def pause_managed_local_gateway(context: RunContext) -> bool:
    """Stop only the local Gateway instance created by mosaico.py.

    Recovery flashing needs exclusive ownership of the same USB device.  An
    unrelated or remotely managed Gateway is deliberately never terminated.
    """

    _, script = locate_iris_tools(context.repository)
    state = _state_home()
    owner = _load_owner(state)
    if owner is not None and _owner_is_current(owner, script):
        pid = int(owner["pid"])
        instance_id = str(owner["instance_id"])
    else:
        _ownership_path(state).unlink(missing_ok=True)
        pid_path = state / "gateway.pid"
        try:
            pid = int(pid_path.read_text(encoding="ascii").strip())
        except (OSError, ValueError):
            return False
        if not _owned_local_gateway(pid, script, state):
            pid_path.unlink(missing_ok=True)
            return False
        instance_id = _health_instance() or "mosaico"

    if not _terminate_managed_gateway(pid, instance_id):
        raise DeviceError(
            "The local ESP-Iris Gateway could not release the device configuration channel."
        )
    _ownership_path(state).unlink(missing_ok=True)
    (state / "gateway.pid").unlink(missing_ok=True)
    context.note(f"paused managed local Gateway pid={pid} for recovery")
    return True


def ensure_gateway(context: RunContext, profile: str | None) -> GatewaySession:
    python, script = locate_iris_tools(context.repository)
    if profile:
        connection = ("--profile", profile)
        if not _probe(context, python, script, connection):
            raise DeviceError(f"Gateway profile is unreachable: {profile}")
        return GatewaySession(python, script, connection, profile, False)

    if _probe(context, python, script, ()):
        return GatewaySession(python, script, (), None, False)
    local = ("--url", LOCAL_URL)
    if _probe(context, python, script, local):
        return GatewaySession(python, script, local, None, False)

    state = _state_home()
    state.mkdir(parents=True, exist_ok=True)
    pid_path = state / "gateway.pid"
    owner = _load_owner(state)
    if owner is not None and _owner_is_current(owner, script):
        if not _terminate_managed_gateway(
            int(owner["pid"]), str(owner["instance_id"])
        ):
            raise DeviceError("Could not stop the stale local ESP-Iris Gateway.")
    _ownership_path(state).unlink(missing_ok=True)
    pid_path.unlink(missing_ok=True)

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
            **popen_options,
        )
        log_stream.close()
        _write_owner(state, pid=process.pid, instance_id=instance_id, script=script)
    except OSError as error:
        raise DeviceError(
            "Could not start the local ESP-Iris Gateway.",
            details={"gateway_log": str(log_path)},
        ) from error

    deadline = time.monotonic() + 20
    while time.monotonic() < deadline:
        if process.poll() is not None:
            break
        if _probe(context, python, script, local) and _health_instance() == instance_id:
            return GatewaySession(python, script, local, None, True)
        time.sleep(0.25)
    if process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            process.kill()
    _ownership_path(state).unlink(missing_ok=True)
    raise DeviceError(
        "The local ESP-Iris Gateway failed to start.",
        details={"gateway_log": str(log_path)},
    )


def gateway_json(
    context: RunContext,
    session: GatewaySession,
    *arguments: str,
    timeout: float = 15,
) -> Any:
    try:
        result = context.run(session.ctl_argv(*arguments), timeout=timeout)
    except subprocess.TimeoutExpired as error:
        raise DeviceError("The ESP-Iris Gateway request timed out.") from error
    if result.returncode:
        raise DeviceError(
            "The ESP-Iris Gateway request failed.",
            details={"log": str(context.log_path)},
        )
    return _decode_json(result.stdout)


def connected_devices(
    context: RunContext,
    session: GatewaySession,
    *,
    wait_seconds: float = 5,
) -> list[dict[str, Any]]:
    deadline = time.monotonic() + wait_seconds
    while True:
        value = gateway_json(context, session, "devices")
        if not isinstance(value, dict):
            raise DeviceError("ESP-Iris returned an invalid device list.")
        devices = [
            item for item in value.get("devices", [])
            if isinstance(item, dict) and item.get("connected") is not False
        ]
        if devices or time.monotonic() >= deadline:
            return devices
        time.sleep(0.25)


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
