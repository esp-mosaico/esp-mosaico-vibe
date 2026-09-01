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

from .errors import DeviceError, EnvironmentError, OperationError, OutcomeUnknownError, SelectionError
from .runtime import RunContext


LOCAL_URL = "http://127.0.0.1:8443"


def _state_home() -> Path:
    configured = os.environ.get("XDG_STATE_HOME")
    if configured:
        return Path(configured).expanduser() / "esp-mosaico" / "gateway"
    return Path.home() / ".local" / "state" / "esp-mosaico" / "gateway"


def locate_iris_tools(repository: Path) -> tuple[Path, Path]:
    manifest = repository / "projects" / "factory" / "main" / "idf_component.yml"
    try:
        text = manifest.read_text(encoding="utf-8")
    except OSError as error:
        raise EnvironmentError(f"无法读取 ESP-Iris 组件声明：{manifest}") from error
    match = re.search(r"(?ms)^\s*esp_iris:\s*\n\s*override_path:\s*([^\n#]+)", text)
    candidates: list[Path] = []
    if match:
        candidates.append((manifest.parent / match.group(1).strip()).resolve())
    candidates.extend(
        [
            repository / "projects" / "factory" / "managed_components" / "espressif__esp_iris",
            repository / "components" / "esp_iris",
        ]
    )
    for component in candidates:
        script = component / "tools" / "esp_iris.py"
        if not script.is_file():
            continue
        component_repo = component.parent.parent
        python = component_repo / ".venv" / "bin" / "python"
        return (python if python.is_file() else Path(os.sys.executable), script)
    raise EnvironmentError("未找到仓库声明的 ESP-Iris CLI")


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
        raise OperationError("ESP-Iris 返回了无效结果") from error


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


def _pid_alive(pid: int) -> bool:
    try:
        os.kill(pid, 0)
    except (OSError, ValueError):
        return False
    try:
        stat = Path(f"/proc/{pid}/stat").read_text(encoding="ascii")
    except FileNotFoundError:
        return False
    except OSError:
        # kill(pid, 0) succeeded, so conservatively treat an unreadable process
        # as live rather than risk terminating or replacing an unrelated one.
        return True
    # A zombie has already closed all file descriptors, including the USB
    # device. PID 1 may reap it slightly after the Gateway has stopped.
    state = stat.rpartition(") ")[2][:1]
    return state not in {"Z", "X"}


def _owned_local_gateway(pid: int, script: Path, state: Path) -> bool:
    try:
        command_line = Path(f"/proc/{pid}/cmdline").read_bytes()
    except OSError:
        return False
    return (
        str(script).encode() in command_line
        and str(state / "state").encode() in command_line
    )


def _terminate_managed_gateway(pid: int) -> bool:
    """Terminate the complete, dedicated process group created by this CLI."""

    try:
        os.killpg(pid, signal.SIGTERM)
    except ProcessLookupError:
        return True
    except OSError:
        return False
    deadline = time.monotonic() + 3
    while _pid_alive(pid) and time.monotonic() < deadline:
        time.sleep(0.1)
    if not _pid_alive(pid):
        return True

    # start_new_session=True makes this process group exclusive to the local
    # Gateway. Escalation prevents a worker retaining the USB descriptor.
    try:
        os.killpg(pid, signal.SIGKILL)
    except ProcessLookupError:
        return True
    except OSError:
        return False
    deadline = time.monotonic() + 2
    while _pid_alive(pid) and time.monotonic() < deadline:
        time.sleep(0.1)
    return not _pid_alive(pid)


def pause_managed_local_gateway(context: RunContext) -> bool:
    """Stop only the local Gateway instance created by mosaico.py.

    Recovery flashing needs exclusive ownership of the same USB device.  An
    unrelated or remotely managed Gateway is deliberately never terminated.
    """

    _, script = locate_iris_tools(context.repository)
    state = _state_home()
    pid_path = state / "gateway.pid"
    try:
        pid = int(pid_path.read_text(encoding="ascii").strip())
    except (OSError, ValueError):
        return False
    if not _pid_alive(pid):
        pid_path.unlink(missing_ok=True)
        return False
    if not _owned_local_gateway(pid, script, state):
        return False

    if not _terminate_managed_gateway(pid):
        raise DeviceError("本地 ESP-Iris Gateway 未能释放设备配置通道")
    pid_path.unlink(missing_ok=True)
    context.note(f"paused managed local Gateway pid={pid} for recovery")
    return True


def ensure_gateway(context: RunContext, profile: str | None) -> GatewaySession:
    python, script = locate_iris_tools(context.repository)
    if profile:
        connection = ("--profile", profile)
        if not _probe(context, python, script, connection):
            raise DeviceError(f"Gateway profile 不可达：{profile}")
        return GatewaySession(python, script, connection, profile, False)

    if _probe(context, python, script, ()):
        return GatewaySession(python, script, (), None, False)
    local = ("--url", LOCAL_URL)
    if _probe(context, python, script, local):
        return GatewaySession(python, script, local, None, False)

    state = _state_home()
    state.mkdir(parents=True, exist_ok=True)
    pid_path = state / "gateway.pid"
    try:
        old_pid = int(pid_path.read_text(encoding="ascii").strip())
        if _pid_alive(old_pid):
            if _owned_local_gateway(old_pid, script, state):
                if not _terminate_managed_gateway(old_pid):
                    raise DeviceError("无法清理失效的本地 ESP-Iris Gateway")
        pid_path.unlink(missing_ok=True)
    except (OSError, ValueError):
        pid_path.unlink(missing_ok=True)

    log_path = state / "gateway.log"
    try:
        log_stream = log_path.open("a", encoding="utf-8")
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
                "mosaico",
                "--state-dir",
                str(state / "state"),
                "--no-tls",
            ],
            stdin=subprocess.DEVNULL,
            stdout=log_stream,
            stderr=subprocess.STDOUT,
            start_new_session=True,
            close_fds=True,
        )
        log_stream.close()
        pid_path.write_text(f"{process.pid}\n", encoding="ascii")
    except OSError as error:
        raise DeviceError(
            "无法启动本地 ESP-Iris Gateway",
            details={"gateway_log": str(log_path)},
        ) from error

    deadline = time.monotonic() + 20
    while time.monotonic() < deadline:
        if process.poll() is not None:
            break
        if _probe(context, python, script, local):
            return GatewaySession(python, script, local, None, True)
        time.sleep(0.25)
    if process.poll() is None:
        try:
            os.killpg(process.pid, signal.SIGTERM)
        except OSError:
            pass
    raise DeviceError(
        "本地 ESP-Iris Gateway 启动失败",
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
        raise DeviceError("ESP-Iris Gateway 请求超时") from error
    if result.returncode:
        raise DeviceError(
            "ESP-Iris Gateway 请求失败", details={"log": str(context.log_path)}
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
            raise DeviceError("ESP-Iris 设备列表格式无效")
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
        raise DeviceError(f"指定设备当前不可用：{requested}")
    if len(devices) == 1:
        return devices[0]
    if not devices:
        raise DeviceError("未发现可用的 ESP-Mosaico 设备")
    raise SelectionError(
        "发现多个设备，请使用 --device-id 指定",
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
    validation_mode = validation.replace("-", "_")
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
                "--validation-mode",
                validation_mode,
            ),
            timeout=timeout,
        )
    except subprocess.TimeoutExpired as error:
        raise OutcomeUnknownError(
            "安装等待超时，设备结果不确定；不会自动重放写操作",
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
                "安装提交结果不确定；不会自动重放写操作",
                details={"result": value, "log": str(context.log_path)},
            )
        raise OperationError(
            "安装提交失败",
            details={"result": value, "log": str(context.log_path)},
        )
    if not operation_id or status in {"outcome_unknown", "unknown"} or status is None:
        raise OutcomeUnknownError(
            "安装结果不确定；不会自动重放写操作",
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
                "安装等待超时，设备结果不确定；不会自动重放写操作",
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
                "安装状态响应无效；不会自动重放写操作",
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
            f"安装失败：{status}",
            details={"result": operation, "log": str(context.log_path)},
        )
    if isinstance(value, dict):
        value["operation"] = operation
        return value
    return {"operation": operation}
