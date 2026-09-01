"""Run processes, store logs, and resolve the ESP-IDF environment."""

from __future__ import annotations

from dataclasses import dataclass
from datetime import datetime, timezone
import json
import os
from pathlib import Path
from queue import Empty, Queue
import re
import subprocess
from threading import Thread
import time
from typing import Callable, Sequence

from .errors import BuildError, DeviceError, EnvironmentError


@dataclass
class RunContext:
    repository: Path
    action: str
    verbose: bool = False
    json_output: bool = False

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
        self.started_monotonic = time.monotonic()

    def note(self, message: str) -> None:
        with self.log_path.open("a", encoding="utf-8") as stream:
            stream.write(message.rstrip() + "\n")

    def status(self, message: str) -> None:
        elapsed = time.monotonic() - self.started_monotonic
        line = f"[+{elapsed:6.1f}s] {message}"
        self.note(line)
        if not self.json_output:
            print(line, flush=True)

    def run(
        self,
        argv: Sequence[str | os.PathLike[str]],
        *,
        timeout: float | None = None,
        cwd: Path | None = None,
        check: bool = False,
        env: dict[str, str] | None = None,
        output_status: Callable[[str], str | None] | None = None,
    ) -> subprocess.CompletedProcess[str]:
        command = [os.fspath(item) for item in argv]
        self.note("$ " + " ".join(command))
        try:
            if output_status is None:
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
                self.note(result.stdout or "")
            else:
                result = self._run_streaming(
                    command,
                    timeout=timeout,
                    cwd=cwd,
                    env=env,
                    output_status=output_status,
                )
        except (OSError, subprocess.TimeoutExpired) as error:
            self.note(f"process error: {error}")
            raise
        self.note(f"exit_code={result.returncode}")
        if check and result.returncode:
            raise subprocess.CalledProcessError(
                result.returncode, command, output=result.stdout
            )
        return result

    def _run_streaming(
        self,
        command: list[str],
        *,
        timeout: float | None,
        cwd: Path | None,
        env: dict[str, str] | None,
        output_status: Callable[[str], str | None],
    ) -> subprocess.CompletedProcess[str]:
        process = subprocess.Popen(
            command,
            cwd=cwd,
            env=env,
            text=True,
            bufsize=1,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )
        assert process.stdout is not None
        sentinel = object()
        events: Queue[str | object] = Queue()

        def read_output() -> None:
            for line in process.stdout:
                events.put(line)
            events.put(sentinel)

        reader = Thread(target=read_output, daemon=True)
        reader.start()
        deadline = None if timeout is None else time.monotonic() + timeout
        output: list[str] = []
        while True:
            remaining = None if deadline is None else deadline - time.monotonic()
            if remaining is not None and remaining <= 0:
                process.kill()
                process.wait()
                reader.join(timeout=1)
                process.stdout.close()
                raise subprocess.TimeoutExpired(
                    command, timeout, output="".join(output)
                )
            try:
                event = events.get(
                    timeout=0.25 if remaining is None else min(0.25, remaining)
                )
            except Empty:
                continue
            if event is sentinel:
                break
            line = str(event)
            output.append(line)
            self.note(line)
            message = output_status(line)
            if message:
                self.status(message)
        reader.join(timeout=1)
        process.stdout.close()
        return subprocess.CompletedProcess(
            command, process.wait(), "".join(output), None
        )


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
        "No compatible ESP-IDF environment was found. Set IDF_PATH or complete "
        "environment verification first."
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
    context.status(f"build: compiling {project}")
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
        diagnostic = (result.stdout or "").strip()
        raise BuildError(
            "Application build failed.",
            details={
                "diagnostic": diagnostic,
                "log": str(context.log_path),
                "build_log_dir": str(context.directory / "build"),
            },
        )
    for line in (result.stdout or "").splitlines():
        if line.strip():
            context.status(f"build: {line.strip()}")


def _idf_progress_parser() -> Callable[[str], str | None]:
    ansi = re.compile(r"\x1b\[[0-?]*[ -/]*[@-~]")
    ninja_progress = re.compile(r"^\[\s*(\d+)\s*/\s*(\d+)\s*\]")
    flash_progress = re.compile(
        r"Writing at (0x[0-9a-fA-F]+).*?\(\s*(\d+)\s*%\s*\)"
    )
    last_ninja_total = 0
    last_ninja_bucket = -1
    last_flash_percent = -1

    def parse(raw_line: str) -> str | None:
        nonlocal last_ninja_total, last_ninja_bucket, last_flash_percent
        line = ansi.sub("", raw_line).strip()
        if not line:
            return None

        ninja = ninja_progress.match(line)
        if ninja:
            completed, total = (int(value) for value in ninja.groups())
            percent = min(100, completed * 100 // max(total, 1))
            bucket = percent // 10
            if total != last_ninja_total:
                last_ninja_total = total
                last_ninja_bucket = -1
            if bucket != last_ninja_bucket or completed == total:
                last_ninja_bucket = bucket
                return f"idf: building {percent}% ({completed}/{total})"
            return None

        writing = flash_progress.search(line)
        if writing:
            address, percent_text = writing.groups()
            percent = int(percent_text)
            if percent < last_flash_percent:
                last_flash_percent = -1
            if percent != last_flash_percent:
                last_flash_percent = percent
                return f"flash: writing {percent}% at {address}"
            return None

        lower = line.lower()
        if line.startswith("Executing action:"):
            return f"idf: {line}"
        if "validating the reviewed recovery bundle" in lower:
            return "bundle: validating reviewed Recovery images and manifest"
        if "building an unreviewed recovery candidate bundle" in lower:
            return "bundle: building current-source Recovery candidate"
        if line.startswith("recovery bundle ready:"):
            return "bundle: Recovery images and manifest verified"
        if line.startswith("Serial port "):
            return f"flash: {line}"
        if line.startswith("Connecting"):
            return "flash: connecting to ROM download service"
        if line.startswith("Chip is "):
            return f"flash: {line}"
        if "will be erased" in lower or line.startswith("Erasing flash"):
            return f"flash: {line}"
        if line.startswith("Wrote "):
            return f"flash: {line}"
        if "hash of data verified" in lower:
            return "flash: image hash verified"
        if line.startswith("Leaving"):
            return "flash: transfer complete"
        if "hard resetting" in lower:
            return "flash: resetting device into Recovery"
        if "project build complete" in lower:
            return "idf: build complete"
        return None

    return parse


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
            output_status=_idf_progress_parser(),
        )
    except subprocess.TimeoutExpired as error:
        raise BuildError(
            f"ESP-IDF operation timed out after {timeout:g} seconds.",
            details={"log": str(context.log_path)},
        ) from error
    if result.returncode:
        output = (result.stdout or "").lower()
        if "port is busy" in output or "could not exclusively lock port" in output:
            raise DeviceError(
                "The device configuration channel is busy. Close the process using "
                "the device and try again.",
                details={"log": str(context.log_path)},
            )
        if "could not connect to an espressif device" in output:
            raise DeviceError(
                "Could not connect to the ESP-Mosaico device in recovery "
                "configuration mode.",
                details={"log": str(context.log_path)},
            )
        raise BuildError(
            f"ESP-IDF target {target} failed.",
            details={"log": str(context.log_path)},
        )
