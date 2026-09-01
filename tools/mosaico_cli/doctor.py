"""Read-only host diagnostics for the public mosaico command."""

from __future__ import annotations

import json
from pathlib import Path
import re
import subprocess
import sys
import uuid
from typing import Any

from .gateway import locate_iris_tools
from .host import (
    HostEnvironmentError,
    host_platform,
    prepare_idf_environment,
    state_root,
)
from .runtime import resolve_idf_path


def _check(
    checks: list[dict[str, str]], name: str, status: str, message: str
) -> None:
    checks.append({"name": name, "status": status, "message": message})


def _command(
    command: list[str], *, environment: dict[str, str] | None = None
) -> tuple[int, str]:
    try:
        result = subprocess.run(
            command,
            env=environment,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=30,
            check=False,
        )
    except (OSError, subprocess.TimeoutExpired) as error:
        return 127, str(error)
    return result.returncode, (result.stdout or "").strip()


def _declared_constraint(repository: Path) -> str:
    manifest = repository / "projects" / "factory" / "main" / "idf_component.yml"
    try:
        text = manifest.read_text(encoding="utf-8")
    except OSError:
        return ">=6.1"
    match = re.search(r"(?m)^\s*idf:\s*[\"']?([^\"'\s]+)", text)
    return match.group(1) if match else ">=6.1"


def _version_satisfies(version_text: str, constraint: str) -> bool:
    version_match = re.search(r"\bv?(\d+)\.(\d+)(?:\.(\d+))?", version_text)
    constraint_match = re.fullmatch(r">=\s*v?(\d+)\.(\d+)(?:\.(\d+))?", constraint)
    if not version_match or not constraint_match:
        return False
    version = tuple(int(value or 0) for value in version_match.groups())
    required = tuple(int(value or 0) for value in constraint_match.groups())
    return version >= required


def diagnose_host(repository: Path) -> dict[str, Any]:
    checks: list[dict[str, str]] = []
    platform = host_platform()
    python_version = ".".join(str(item) for item in sys.version_info[:3])
    python_ready = sys.version_info >= (3, 11)
    _check(
        checks,
        "python",
        "pass" if python_ready else "fail",
        f"Python {python_version}; 3.11 or newer is required",
    )
    platform_ready = platform in {"linux", "macos", "windows"}
    _check(
        checks,
        "platform",
        "pass" if platform_ready else "fail",
        f"Host platform: {platform}",
    )

    state = state_root("esp-mosaico")
    try:
        state.mkdir(parents=True, exist_ok=True)
        probe = state / f".doctor-{uuid.uuid4().hex}.tmp"
        probe.write_text("mosaico doctor\n", encoding="utf-8")
        probe.unlink()
        _check(checks, "state", "pass", f"State directory is writable: {state}")
    except OSError as error:
        _check(checks, "state", "fail", f"State directory is not writable: {error}")

    inventory = repository / "Environment"
    if not inventory.is_file() or inventory.stat().st_size == 0:
        _check(checks, "inventory", "warn", "Environment inventory is absent or empty")
    else:
        _check(checks, "inventory", "pass", "Environment inventory is present")

    idf: dict[str, Any] = {"constraint": _declared_constraint(repository)}
    try:
        idf_path = resolve_idf_path(repository, repository / "projects" / "factory")
        prepared = prepare_idf_environment(idf_path)
        idf.update(
            {
                "path": str(idf_path),
                "python": str(prepared.python),
                "target": "esp32s31",
            }
        )
        version_code, version_output = _command(
            [str(prepared.python), str(prepared.idf_py), "--version"],
            environment=prepared.values,
        )
        version_ready = version_code == 0 and _version_satisfies(
            version_output, str(idf["constraint"])
        )
        idf["version"] = version_output.splitlines()[-1] if version_output else "unresolved"
        _check(
            checks,
            "idf-version",
            "pass" if version_ready else "fail",
            f"{idf['version']} (required {idf['constraint']})",
        )
        target_code, target_output = _command(
            [
                str(prepared.python),
                str(prepared.idf_py),
                "--preview",
                "--list-targets",
            ],
            environment=prepared.values,
        )
        target_ready = target_code == 0 and "esp32s31" in {
            line.strip() for line in target_output.splitlines()
        }
        idf["target_supported"] = target_ready
        _check(
            checks,
            "idf-target",
            "pass" if target_ready else "fail",
            "ESP32-S31 target is available"
            if target_ready
            else "ESP32-S31 target is unavailable",
        )
        revision_code, revision_output = _command(
            ["git", "-C", str(idf_path), "rev-parse", "--short=12", "HEAD"]
        )
        idf["revision"] = revision_output if revision_code == 0 else "unresolved"
        _check(
            checks,
            "idf-revision",
            "pass" if revision_code == 0 and bool(revision_output) else "fail",
            f"ESP-IDF revision: {idf['revision']}",
        )
    except (HostEnvironmentError, OSError, RuntimeError) as error:
        idf["error"] = str(error)
        _check(checks, "idf-environment", "fail", str(error))

    iris: dict[str, Any] = {"devices": []}
    try:
        iris_python, iris_script = locate_iris_tools(repository)
        iris.update({"python": str(iris_python), "script": str(iris_script)})
        iris_code, iris_output = _command(
            [str(iris_python), str(iris_script), "doctor", "--json"]
        )
        report = json.loads(iris_output) if iris_output else {}
        iris["devices"] = report.get("devices", []) if isinstance(report, dict) else []
        iris_ready = iris_code == 0 and bool(report.get("python_supported"))
        _check(
            checks,
            "esp-iris",
            "pass" if iris_ready else "fail",
            "ESP-Iris host dependencies are ready"
            if iris_ready
            else "ESP-Iris host dependencies are unavailable",
        )
        _check(
            checks,
            "usb",
            "pass" if iris["devices"] else "warn",
            f"Detected {len(iris['devices'])} compatible USB device(s)"
            if iris["devices"]
            else "No compatible USB device is currently connected",
        )
    except (OSError, RuntimeError, ValueError, json.JSONDecodeError) as error:
        iris["error"] = str(error)
        _check(checks, "esp-iris", "fail", str(error))

    failed = any(item["status"] == "fail" for item in checks)
    return {
        "command": "doctor",
        "status": "not_ready" if failed else "ready",
        "host": {
            "platform": platform,
            "python": python_version,
            "python_supported": python_ready,
            "state_dir": str(state),
        },
        "idf": idf,
        "iris": iris,
        "checks": checks,
        "exit_code": 3 if failed else 0,
    }


def print_diagnosis(result: dict[str, Any]) -> None:
    print("MOSAICO HOST DOCTOR")
    for item in result["checks"]:
        print(f"{item['status'].upper():4}  {item['name']}: {item['message']}")
    print(f"status: {result['status']}")
