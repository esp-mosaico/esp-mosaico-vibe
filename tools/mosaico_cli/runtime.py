"""Run processes, store logs, and resolve the ESP-IDF environment."""

from __future__ import annotations

from dataclasses import dataclass
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import subprocess
from typing import Sequence

from .errors import BuildError, DeviceError, EnvironmentError


@dataclass
class RunContext:
    repository: Path
    action: str
    verbose: bool = False

    def __post_init__(self) -> None:
        stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
        base = self.repository / ".codex-runs" / "mosaico"
        candidate = base / f"{stamp}-{self.action}"
        suffix = 1
        while candidate.exists():
            candidate = base / f"{stamp}-{self.action}-{suffix}"
            suffix += 1
        candidate.mkdir(parents=True, exist_ok=False)
        self.directory = candidate
        self.log_path = candidate / "raw.log"

    def note(self, message: str) -> None:
        with self.log_path.open("a", encoding="utf-8") as stream:
            stream.write(message.rstrip() + "\n")

    def run(
        self,
        argv: Sequence[str | os.PathLike[str]],
        *,
        timeout: float | None = None,
        cwd: Path | None = None,
        check: bool = False,
        env: dict[str, str] | None = None,
    ) -> subprocess.CompletedProcess[str]:
        command = [os.fspath(item) for item in argv]
        self.note("$ " + " ".join(command))
        try:
            result = subprocess.run(
                command,
                cwd=cwd,
                env=env,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                timeout=timeout,
                check=False,
            )
        except (OSError, subprocess.TimeoutExpired) as error:
            self.note(f"process error: {error}")
            raise
        self.note(result.stdout or "")
        self.note(f"exit_code={result.returncode}")
        if check and result.returncode:
            raise subprocess.CalledProcessError(
                result.returncode, command, output=result.stdout
            )
        return result


def resolve_idf_path(repository: Path, project: Path | None = None) -> Path:
    candidates: list[Path] = []
    environment = os.environ.get("IDF_PATH")
    if environment:
        candidates.append(Path(environment))
    for owner in (project, repository / "projects" / "factory"):
        if owner is None:
            continue
        description = owner / "build" / "project_description.json"
        try:
            value = json.loads(description.read_text(encoding="utf-8"))
            if value.get("idf_path"):
                candidates.append(Path(value["idf_path"]))
        except (OSError, ValueError, TypeError):
            pass
    for candidate in candidates:
        resolved = candidate.expanduser().resolve()
        if (resolved / "export.sh").is_file() and (resolved / "tools" / "idf.py").is_file():
            return resolved
    raise EnvironmentError(
        "未找到兼容的 ESP-IDF 环境；请先配置 IDF_PATH 或完成一次环境验证"
    )


def build_application(context: RunContext, project: Path) -> None:
    runner = (
        context.repository
        / "skills"
        / "idf-low-noise-build"
        / "scripts"
        / "idf_low_noise_build.py"
    )
    idf_path = resolve_idf_path(context.repository, project)
    result = context.run(
        [
            "python3",
            runner,
            "--project",
            project,
            "--idf-path",
            idf_path,
            "--log-dir",
            context.directory / "build",
            "build",
        ],
        timeout=3600,
        cwd=context.repository,
    )
    if result.returncode:
        raise BuildError(
            "应用构建失败",
            details={"log": str(context.log_path), "build_log_dir": str(context.directory / "build")},
        )


def run_idf_target(
    context: RunContext,
    *,
    idf_path: Path,
    project: Path,
    build_dir: Path,
    target: str,
    definitions: dict[str, str] | None = None,
    port: str | None = None,
    timeout: float,
) -> None:
    command: list[str | Path] = [
        "bash",
        "-c",
        'source "$1/export.sh" >/dev/null && shift && exec idf.py "$@"',
        "mosaico-idf",
        idf_path,
        "-C",
        project,
        "-B",
        build_dir,
    ]
    for key, value in (definitions or {}).items():
        command.extend(["-D", f"{key}={value}"])
    command.append(target)
    process_environment = os.environ.copy()
    if port:
        # Custom ESP-IDF flash targets consume ESPPORT in run_serial_tool.cmake;
        # idf.py's -p option is only propagated to its built-in flash actions.
        process_environment["ESPPORT"] = port
    try:
        result = context.run(
            command,
            timeout=timeout,
            cwd=context.repository,
            env=process_environment,
        )
    except subprocess.TimeoutExpired as error:
        raise BuildError(
            f"ESP-IDF 操作超时（{timeout:g} 秒）",
            details={"log": str(context.log_path)},
        ) from error
    if result.returncode:
        output = (result.stdout or "").lower()
        if "port is busy" in output or "could not exclusively lock port" in output:
            raise DeviceError(
                "设备配置通道被其他进程占用；请关闭占用该设备的程序后重试",
                details={"log": str(context.log_path)},
            )
        if "could not connect to an espressif device" in output:
            raise DeviceError(
                "无法连接处于恢复配置状态的 ESP-Mosaico 设备",
                details={"log": str(context.log_path)},
            )
        raise BuildError(
            f"ESP-IDF target {target} 执行失败",
            details={"log": str(context.log_path)},
        )
