"""Cross-platform host paths and ESP-IDF process preparation."""

from __future__ import annotations

from dataclasses import dataclass
import os
from pathlib import Path
import subprocess
import sys
from typing import Mapping


class HostEnvironmentError(RuntimeError):
    """A required host-side runtime could not be prepared."""


def host_platform(
    *, os_name: str | None = None, sys_platform: str | None = None
) -> str:
    selected_os = os.name if os_name is None else os_name
    selected_platform = sys.platform if sys_platform is None else sys_platform
    if selected_os == "nt":
        return "windows"
    if selected_platform == "darwin":
        return "macos"
    if selected_os == "posix":
        return "linux"
    return selected_platform


def state_root(
    application: str,
    *,
    environment: Mapping[str, str] | None = None,
    home: Path | None = None,
    os_name: str | None = None,
    sys_platform: str | None = None,
) -> Path:
    values = os.environ if environment is None else environment
    user_home = Path.home() if home is None else home
    platform = host_platform(os_name=os_name, sys_platform=sys_platform)
    if platform == "windows":
        root = Path(values.get("LOCALAPPDATA", user_home / "AppData" / "Local"))
    elif platform == "macos":
        root = user_home / "Library" / "Application Support"
    else:
        root = Path(values.get("XDG_STATE_HOME", user_home / ".local" / "state"))
    return root.expanduser() / application


def virtual_environment_python(
    environment_root: Path, *, os_name: str | None = None
) -> Path:
    selected_os = os.name if os_name is None else os_name
    if selected_os == "nt":
        return environment_root / "Scripts" / "python.exe"
    return environment_root / "bin" / "python"


def valid_idf_path(path: Path) -> bool:
    return (
        (path / "tools" / "idf.py").is_file()
        and (path / "tools" / "idf_tools.py").is_file()
    )


@dataclass(frozen=True)
class IdfEnvironment:
    root: Path
    python: Path
    idf_py: Path
    values: dict[str, str]


def _parse_idf_exports(output: str, base: Mapping[str, str]) -> dict[str, str]:
    exports: dict[str, str] = {}
    for raw_line in output.splitlines():
        key, separator, value = raw_line.partition("=")
        if not separator or not key or any(character.isspace() for character in key):
            continue
        exports[key] = value

    if "PATH" in exports:
        inherited = base.get("PATH", "")
        exports["PATH"] = (
            exports["PATH"]
            .replace("%PATH%", inherited)
            .replace("${PATH}", inherited)
            .replace("$PATH", inherited)
        )
    return exports


def prepare_idf_environment(
    idf_path: Path,
    *,
    base_environment: Mapping[str, str] | None = None,
    bootstrap_python: Path | None = None,
    timeout: float = 60,
) -> IdfEnvironment:
    root = idf_path.expanduser().resolve()
    if not valid_idf_path(root):
        raise HostEnvironmentError(f"Invalid ESP-IDF path: {root}")

    values = dict(os.environ if base_environment is None else base_environment)
    values["IDF_PATH"] = str(root)
    python = Path(sys.executable) if bootstrap_python is None else bootstrap_python
    command = [
        str(python),
        str(root / "tools" / "idf_tools.py"),
        "--quiet",
        "--non-interactive",
        "--idf-path",
        str(root),
        "export",
        "--format",
        "key-value",
    ]
    try:
        result = subprocess.run(
            command,
            env=values,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=timeout,
            check=False,
        )
    except (OSError, subprocess.TimeoutExpired) as error:
        raise HostEnvironmentError(
            f"Could not export the ESP-IDF environment: {error}"
        ) from error
    if result.returncode:
        diagnostic = (result.stdout or "").strip()
        raise HostEnvironmentError(
            "Could not export the ESP-IDF environment."
            + (f" {diagnostic}" if diagnostic else "")
        )

    values.update(_parse_idf_exports(result.stdout or "", values))
    python_root = values.get("IDF_PYTHON_ENV_PATH")
    if not python_root:
        raise HostEnvironmentError("ESP-IDF did not report IDF_PYTHON_ENV_PATH.")
    idf_python = virtual_environment_python(Path(python_root))
    if not idf_python.is_file():
        raise HostEnvironmentError(
            f"The ESP-IDF Python interpreter does not exist: {idf_python}"
        )
    return IdfEnvironment(root, idf_python, root / "tools" / "idf.py", values)
