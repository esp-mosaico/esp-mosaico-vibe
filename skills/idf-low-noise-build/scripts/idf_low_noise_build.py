#!/usr/bin/env python3
"""Run ESP-IDF builds without emitting verbose build output to agent stdout."""

from __future__ import annotations

import argparse
import bisect
import datetime as dt
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time
from typing import Any, Sequence


REPOSITORY_ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPOSITORY_ROOT / "tools"))

from mosaico_cli.host import (  # noqa: E402
    HostEnvironmentError,
    prepare_idf_environment,
    valid_idf_path,
)


ANSI_RE = re.compile(r"\x1b(?:\[[0-?]*[ -/]*[@-~]|\][^\x07]*(?:\x07|\x1b\\))")
PROJECT_RE = re.compile(r"(?:^|\n)\s*(?:include\s*\([^)]*project\.cmake|project\s*\()", re.I)
TARGET_RE = re.compile(r'^CONFIG_IDF_TARGET="([^"]+)"$', re.M)
IDF_CONSTRAINT_RE = re.compile(r'^\s{2}idf:\s*["\']?([^"\'\s#]+)', re.M)
WARNING_RE = re.compile(r"\bwarning:", re.I)
DEFAULT_CONTEXT_BEFORE = 8
DEFAULT_CONTEXT_AFTER = 20
DEFAULT_MAX_LINES = 120
SCAN_EXCLUDES = {
    ".git",
    ".codex",
    ".venv",
    "build",
    "dist",
    "managed_components",
    "node_modules",
}

ERROR_PATTERNS: tuple[tuple[str, re.Pattern[str]], ...] = (
    (
        "compiler",
        re.compile(
            r"^(?:[^:\n]+:)+\d+(?::\d+)?:\s+(?:fatal\s+)?error:\s+.+$",
            re.I | re.M,
        ),
    ),
    (
        "linker",
        re.compile(
            r"(?:undefined reference to|will not fit in region|region [`'\"]?.+[`'\"]? "
            r"overflowed|collect2:\s*error|ld(?:\.exe)?:\s*error)",
            re.I,
        ),
    ),
    (
        "partition",
        re.compile(
            r"(?:does not fit|too large for|exceeds .*partition|partition .* overflow|"
            r"app partition is too small)",
            re.I,
        ),
    ),
    (
        "cmake",
        re.compile(r"^(?:CMake Error|-- Configuring incomplete, errors occurred)", re.I | re.M),
    ),
    (
        "python",
        re.compile(r"^Traceback \(most recent call last\):", re.M),
    ),
    (
        "generic",
        re.compile(r"^(?!ninja: build stopped)(?!FAILED:).*\b(?:fatal|error):\s+.+$", re.I | re.M),
    ),
)


class RunnerError(RuntimeError):
    """Represent an expected configuration or invocation failure."""


def strip_ansi(text: str) -> str:
    return ANSI_RE.sub("", text).replace("\r\n", "\n").replace("\r", "\n")


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace")


def write_json(path: Path, value: dict[str, Any]) -> None:
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    temporary.replace(path)


def is_idf_project(path: Path) -> bool:
    cmake = path / "CMakeLists.txt"
    if not cmake.is_file():
        return False
    try:
        return bool(PROJECT_RE.search(read_text(cmake)))
    except OSError:
        return False


def scan_projects(base: Path, max_depth: int = 4) -> list[Path]:
    results: list[Path] = []
    base = base.resolve()
    for root, directories, files in os.walk(base):
        root_path = Path(root)
        depth = len(root_path.relative_to(base).parts)
        directories[:] = [name for name in directories if name not in SCAN_EXCLUDES]
        if depth >= max_depth:
            directories[:] = []
        if "CMakeLists.txt" in files and is_idf_project(root_path):
            results.append(root_path)
            directories[:] = []
    return sorted(set(results))


def resolve_project(candidate: Path) -> Path:
    candidate = candidate.expanduser().resolve()
    if candidate.is_file():
        candidate = candidate.parent
    for path in (candidate, *candidate.parents):
        if is_idf_project(path):
            return path
    if candidate.is_dir():
        projects = scan_projects(candidate)
        if len(projects) == 1:
            return projects[0]
        if projects:
            shown = "\n".join(f"  - {path}" for path in projects[:10])
            suffix = "\n  - ..." if len(projects) > 10 else ""
            raise RunnerError(
                "Multiple ESP-IDF projects found; pass --project explicitly:\n" + shown + suffix
            )
    raise RunnerError(f"No ESP-IDF project found from: {candidate}")


def idf_from_active_command() -> Path | None:
    command = shutil.which("idf.py")
    if not command:
        return None
    resolved = Path(command).resolve()
    return resolved.parent.parent if resolved.parent.name == "tools" else None


def idf_from_build_metadata(project: Path) -> Path | None:
    description = project / "build" / "project_description.json"
    if not description.is_file():
        return None
    try:
        value = json.loads(read_text(description)).get("idf_path")
    except (OSError, json.JSONDecodeError):
        return None
    return Path(value).expanduser() if isinstance(value, str) and value else None


def resolve_idf_path(project: Path, explicit: Path | None) -> Path:
    if explicit is not None:
        path = explicit.expanduser().resolve()
        if not valid_idf_path(path):
            raise RunnerError(f"Invalid ESP-IDF path: {path}")
        return path

    env_path = os.environ.get("IDF_PATH")
    candidates = [
        Path(env_path).expanduser() if env_path else None,
        idf_from_active_command(),
        idf_from_build_metadata(project),
    ]
    for candidate in candidates:
        if candidate is not None:
            candidate = candidate.resolve()
            if valid_idf_path(candidate):
                return candidate
    raise RunnerError(
        "ESP-IDF was not found. Export IDF_PATH or pass --idf-path /path/to/esp-idf."
    )


def configured_target(project: Path) -> str | None:
    for path in (
        project / "sdkconfig",
        project / "sdkconfig.defaults",
        project / "build" / "sdkconfig",
    ):
        if path.is_file():
            match = TARGET_RE.search(read_text(path))
            if match:
                return match.group(1)
    description = project / "build" / "project_description.json"
    if description.is_file():
        try:
            value = json.loads(read_text(description)).get("target")
        except (OSError, json.JSONDecodeError):
            value = None
        if isinstance(value, str) and value:
            return value
    return None


def declared_idf_constraint(project: Path) -> str | None:
    for path in (project / "main" / "idf_component.yml", project / "idf_component.yml"):
        if path.is_file():
            match = IDF_CONSTRAINT_RE.search(read_text(path))
            if match:
                return match.group(1)
    return None


def default_log_root(project: Path) -> Path:
    return project / ".codex-runs" / "idf-low-noise-build"


def create_run_dir(log_root: Path, action: str) -> tuple[str, Path]:
    stamp = dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    run_id = f"{stamp}-{action}-{os.getpid()}"
    run_dir = log_root / run_id
    run_dir.mkdir(parents=True, exist_ok=False)
    return run_id, run_dir


def idf_command(
    idf_path: Path, arguments: Sequence[str]
) -> tuple[list[str], dict[str, str], Path]:
    try:
        prepared = prepare_idf_environment(idf_path)
    except HostEnvironmentError as error:
        raise RunnerError(str(error)) from error
    return (
        [str(prepared.python), str(prepared.idf_py), *arguments],
        prepared.values,
        prepared.python,
    )


def save_clean_log(raw_path: Path, clean_path: Path) -> str:
    clean = strip_ansi(read_text(raw_path))
    clean_path.write_text(clean, encoding="utf-8")
    return clean


def find_diagnostic(text: str) -> dict[str, Any] | None:
    lines = text.splitlines()
    offsets: list[int] = []
    offset = 0
    for line in lines:
        offsets.append(offset)
        offset += len(line) + 1

    candidates: list[tuple[int, int, str, re.Match[str]]] = []
    for priority, (category, pattern) in enumerate(ERROR_PATTERNS):
        match = pattern.search(text)
        if match:
            line_number = bisect.bisect_right(offsets, match.start()) if offsets else 1
            candidates.append((line_number, priority, category, match))
    if not candidates:
        return None

    line_number, _, category, match = min(candidates, key=lambda item: (item[0], item[1]))
    start = max(0, line_number - 1 - DEFAULT_CONTEXT_BEFORE)
    end = min(len(lines), line_number + DEFAULT_CONTEXT_AFTER)
    return {
        "category": category,
        "line": line_number,
        "message": match.group(0).strip(),
        "excerpt_start": start + 1,
        "excerpt_end": end,
        "excerpt": "\n".join(lines[start:end]),
    }


def warning_count(text: str) -> int:
    return sum(1 for line in text.splitlines() if WARNING_RE.search(line))


def collect_artifacts(project: Path) -> list[dict[str, Any]]:
    build = project / "build"
    if not build.is_dir():
        return []
    candidates: list[Path] = []
    description = build / "project_description.json"
    if description.is_file():
        try:
            data = json.loads(read_text(description))
            for key in ("app_bin", "app_elf"):
                value = data.get(key)
                if isinstance(value, str):
                    candidates.append(Path(value))
        except (OSError, json.JSONDecodeError):
            pass
    candidates.extend(build.glob("*.bin"))
    candidates.extend(build.glob("*.elf"))
    artifacts: list[dict[str, Any]] = []
    for path in sorted(set(candidates)):
        if path.is_file():
            artifacts.append({"path": str(path.resolve()), "size_bytes": path.stat().st_size})
    return artifacts[:12]


def print_diagnostic(diagnostic: dict[str, Any] | None) -> None:
    if not diagnostic:
        print("root_cause: not identified; use inspect --run latest --grep <pattern>")
        return
    print(f"category: {diagnostic['category']}")
    print(f"log_line: {diagnostic['line']}")
    print(f"error: {diagnostic['message']}")
    print("context:")
    for line in diagnostic["excerpt"].splitlines()[:DEFAULT_MAX_LINES]:
        print(f"  {line}")


def print_summary(result: dict[str, Any]) -> None:
    print(f"IDF LOW-NOISE {result['action'].upper()}: {result['status'].upper()}")
    print(f"run_id: {result['run_id']}")
    print(f"duration_seconds: {result['duration_seconds']:.1f}")
    print(f"exit_code: {result['exit_code']}")
    if result.get("target"):
        print(f"target: {result['target']}")
    print(f"warnings: {result['warnings']}")
    artifacts = result.get("artifacts", [])
    if artifacts:
        first = artifacts[0]
        print(f"artifact: {first['path']} ({first['size_bytes']} bytes)")
    if result["status"] != "ok":
        print_diagnostic(result.get("diagnostic"))
    print(f"log: {result['raw_log']}")
    print(f"result: {result['result_file']}")


def run_build_step(
    *, action: str, project: Path, idf_path: Path, arguments: Sequence[str], log_root: Path
) -> int:
    run_id, run_dir = create_run_dir(log_root, action)
    raw_path = run_dir / "raw.log"
    clean_path = run_dir / "clean.log"
    result_path = run_dir / "result.json"
    command_path = run_dir / "command.json"
    write_json(
        command_path,
        {
            "action": action,
            "project": str(project),
            "idf_path": str(idf_path),
            "idf_arguments": list(arguments),
        },
    )

    started = time.monotonic()
    try:
        command, environment, _ = idf_command(idf_path, arguments)
        with raw_path.open("wb") as output:
            completed = subprocess.run(
                command,
                stdout=output,
                stderr=subprocess.STDOUT,
                check=False,
                cwd=project,
                env=environment,
            )
        exit_code = completed.returncode
    except OSError as exc:
        raw_path.write_text(f"Runner failed to start command: {exc}\n", encoding="utf-8")
        exit_code = 127

    clean = save_clean_log(raw_path, clean_path)
    result: dict[str, Any] = {
        "schema_version": 1,
        "run_id": run_id,
        "action": action,
        "status": "ok" if exit_code == 0 else "failed",
        "exit_code": exit_code,
        "duration_seconds": round(time.monotonic() - started, 3),
        "project": str(project),
        "idf_path": str(idf_path),
        "target": configured_target(project),
        "warnings": warning_count(clean),
        "diagnostic": find_diagnostic(clean) if exit_code else None,
        "artifacts": collect_artifacts(project),
        "raw_log": str(raw_path.resolve()),
        "clean_log": str(clean_path.resolve()),
        "result_file": str(result_path.resolve()),
    }
    write_json(result_path, result)
    (log_root / "latest.txt").write_text(run_id + "\n", encoding="utf-8")
    print_summary(result)
    return exit_code if 0 <= exit_code <= 125 else 1


def command_output(
    command: Sequence[str], cwd: Path, *, environment: dict[str, str] | None = None
) -> tuple[int, str]:
    try:
        completed = subprocess.run(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
            cwd=cwd,
            text=True,
            errors="replace",
            env=environment,
        )
    except OSError as exc:
        return 127, str(exc)
    return completed.returncode, strip_ansi(completed.stdout)


def parse_version(text: str) -> tuple[int, int, int] | None:
    match = re.search(r"\bv?(\d+)\.(\d+)(?:\.(\d+))?", text)
    if not match:
        return None
    return tuple(int(value or 0) for value in match.groups())


def check_constraint(version_text: str, constraint: str | None) -> bool | None:
    if not constraint:
        return None
    version = parse_version(version_text)
    if not version:
        return None
    clauses = [part.strip() for part in constraint.split(",") if part.strip()]
    for clause in clauses:
        match = re.fullmatch(r"(>=|<=|==|>|<)?\s*v?(\d+)\.(\d+)(?:\.(\d+))?", clause)
        if not match:
            return None
        operator = match.group(1) or "=="
        required = tuple(int(value or 0) for value in match.groups()[1:])
        comparisons = {
            ">=": version >= required,
            "<=": version <= required,
            "==": version == required,
            ">": version > required,
            "<": version < required,
        }
        if not comparisons[operator]:
            return False
    return True


def doctor(project: Path, idf_path: Path | None) -> int:
    target = configured_target(project)
    constraint = declared_idf_constraint(project)
    print("IDF LOW-NOISE BUILD DOCTOR")
    print(f"project: {project}")
    print(f"idf_path: {idf_path if idf_path else 'unresolved'}")
    print(f"declared_idf_constraint: {constraint or 'unresolved'}")
    print(f"configured_target: {target or 'unresolved'}")
    if idf_path is None:
        return 2

    command, environment, idf_python = idf_command(idf_path, ["--version"])

    version_code, version_output = command_output(
        command, project, environment=environment
    )
    version_lines = [line.strip() for line in version_output.splitlines() if line.strip()]
    idf_version = next((line for line in reversed(version_lines) if "ESP-IDF" in line), None)
    print(f"idf_version: {idf_version or 'unresolved'}")
    constraint_ok = check_constraint(idf_version or "", constraint)
    constraint_status = {True: "yes", False: "no", None: "unverified"}[constraint_ok]
    print(f"idf_constraint_satisfied: {constraint_status}")

    targets_command = [
        str(idf_python),
        str(idf_path / "tools" / "idf.py"),
        "--preview",
        "--list-targets",
    ]
    targets_code, targets_output = command_output(
        targets_command, project, environment=environment
    )
    supported_targets = {
        line.strip()
        for line in targets_output.splitlines()
        if re.fullmatch(r"[a-z][a-z0-9]*", line.strip())
    }
    target_ok = target in supported_targets if target else None
    target_status = {True: "yes", False: "no", None: "unverified"}[target_ok]
    print(f"target_supported: {target_status}")

    revision_code, revision_output = command_output(
        ["git", "-C", str(idf_path), "rev-parse", "--short=12", "HEAD"], project
    )
    revision = revision_output.strip().splitlines()[-1] if revision_code == 0 else "unresolved"
    print(f"idf_revision: {revision}")

    python_code, python_output = command_output(
        [str(idf_python), "--version"], project, environment=environment
    )
    python_lines = [line.strip() for line in python_output.splitlines() if line.strip()]
    python_value = (
        f"{idf_python} | {python_lines[-1]}" if python_lines else str(idf_python)
    )
    print(f"idf_python: {python_value}")
    constraint_check_passed = constraint_ok is True if constraint else True
    checks_pass = (
        version_code == 0
        and python_code == 0
        and targets_code == 0
        and constraint_check_passed
        and target_ok is True
    )
    return 0 if checks_pass else 2


def resolve_run_dir(log_root: Path, run_value: str) -> Path:
    if run_value == "latest":
        latest = log_root / "latest.txt"
        if not latest.is_file():
            raise RunnerError(f"No latest run found under: {log_root}")
        run_value = read_text(latest).strip()
    candidate = Path(run_value).expanduser()
    if candidate.is_dir():
        return candidate.resolve()
    run_dir = (log_root / run_value).resolve()
    if not run_dir.is_dir():
        raise RunnerError(f"Run not found: {run_value}")
    return run_dir


def bounded_lines(lines: Sequence[str], maximum: int) -> list[str]:
    if len(lines) <= maximum:
        return list(lines)
    return [*lines[:maximum], f"... truncated {len(lines) - maximum} lines ..."]


def inspect_log(
    run_dir: Path, *, grep_pattern: str | None, context: int, tail: int | None, full: bool
) -> int:
    clean_path = run_dir / "clean.log"
    if not clean_path.is_file():
        raise RunnerError(f"Missing clean log: {clean_path}")
    text = read_text(clean_path)
    lines = text.splitlines()
    print(f"run: {run_dir.name}")
    print(f"log: {clean_path.resolve()}")

    if full:
        print(text, end="" if text.endswith("\n") else "\n")
        return 0
    if grep_pattern:
        try:
            pattern = re.compile(grep_pattern, re.I)
        except re.error as exc:
            raise RunnerError(f"Invalid --grep pattern: {exc}") from exc
        selected: set[int] = set()
        for index, line in enumerate(lines):
            if pattern.search(line):
                selected.update(range(max(0, index - context), min(len(lines), index + context + 1)))
        if not selected:
            print("matches: 0")
            return 1
        output: list[str] = []
        previous = -2
        for index in sorted(selected):
            if index != previous + 1:
                output.append("--")
            output.append(f"{index + 1}: {lines[index]}")
            previous = index
        for line in bounded_lines(output, DEFAULT_MAX_LINES):
            print(line)
        return 0
    if tail is not None:
        for line in lines[-min(tail, DEFAULT_MAX_LINES) :]:
            print(line)
        return 0

    diagnostic = find_diagnostic(text)
    print_diagnostic(diagnostic)
    return 0 if diagnostic else 1


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Run low-noise ESP-IDF builds and inspect their stored logs."
    )
    parser.add_argument(
        "--project",
        type=Path,
        default=Path.cwd(),
        help="ESP-IDF project directory or a directory containing one",
    )
    parser.add_argument("--idf-path", type=Path, help="ESP-IDF installation root")
    parser.add_argument("--log-dir", type=Path, help="Override the per-run log directory")
    subparsers = parser.add_subparsers(dest="operation", required=True)

    subparsers.add_parser("doctor", help="Resolve and report the build environment")
    build = subparsers.add_parser("build", help="Run a low-noise idf.py build")
    build.add_argument(
        "--fullclean",
        action="store_true",
        help="Run idf.py fullclean first; requires explicit user approval",
    )

    inspect = subparsers.add_parser("inspect", help="Inspect a bounded part of a stored log")
    inspect.add_argument("--run", default="latest", help="Run id, run directory, or 'latest'")
    inspect.add_argument("--grep", help="Case-insensitive regular expression")
    inspect.add_argument("--context", type=int, default=4, help="Lines around grep matches")
    inspect.add_argument("--tail", type=int, help="Show at most this many final lines")
    inspect.add_argument(
        "--full",
        action="store_true",
        help="Print the complete clean log only when bounded inspection is insufficient",
    )

    analyze = subparsers.add_parser("analyze", help="Analyze an existing log without running IDF")
    analyze.add_argument("--log", type=Path, required=True, help="Existing log file")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        if args.operation == "analyze":
            diagnostic = find_diagnostic(strip_ansi(read_text(args.log.expanduser())))
            print_diagnostic(diagnostic)
            return 0 if diagnostic else 1

        project = resolve_project(args.project)
        log_root = args.log_dir.expanduser().resolve() if args.log_dir else default_log_root(project)
        if args.operation == "inspect":
            return inspect_log(
                resolve_run_dir(log_root, args.run),
                grep_pattern=args.grep,
                context=max(0, args.context),
                tail=max(0, args.tail) if args.tail is not None else None,
                full=args.full,
            )

        try:
            idf_path = resolve_idf_path(project, args.idf_path)
        except RunnerError:
            if args.operation == "doctor" and args.idf_path is None:
                idf_path = None
            else:
                raise

        if args.operation == "doctor":
            return doctor(project, idf_path)
        assert idf_path is not None

        if args.operation == "build":
            if args.fullclean:
                clean_code = run_build_step(
                    action="fullclean",
                    project=project,
                    idf_path=idf_path,
                    arguments=["fullclean"],
                    log_root=log_root,
                )
                if clean_code:
                    return clean_code
            return run_build_step(
                action="build",
                project=project,
                idf_path=idf_path,
                arguments=["build"],
                log_root=log_root,
            )
    except (RunnerError, OSError, json.JSONDecodeError) as exc:
        print(f"IDF LOW-NOISE BUILD: CONFIGURATION ERROR\nerror: {exc}", file=sys.stderr)
        return 2
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
