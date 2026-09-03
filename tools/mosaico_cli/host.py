"""Cross-platform host paths and ESP-IDF process preparation."""

from __future__ import annotations

from dataclasses import dataclass
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
from typing import Mapping, Sequence


class HostEnvironmentError(RuntimeError):
    """A required host-side runtime could not be prepared."""


MINIMUM_IDF_PYTHON = (3, 10)


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


def _probe_python(
    command: Sequence[str], environment: Mapping[str, str]
) -> tuple[Path, tuple[int, int]] | None:
    try:
        result = subprocess.run(
            [
                *command,
                "-c",
                "import json,sys; print(json.dumps([sys.executable, sys.version_info.major, sys.version_info.minor]))",
            ],
            env=dict(environment),
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            timeout=10,
            check=False,
        )
        value = json.loads(result.stdout)
    except (OSError, subprocess.TimeoutExpired, ValueError, TypeError):
        return None
    if (
        result.returncode
        or not isinstance(value, list)
        or len(value) != 3
        or not isinstance(value[0], str)
        or not all(isinstance(item, int) for item in value[1:])
    ):
        return None
    return Path(value[0]), (value[1], value[2])


def _idf_major_minor(idf_path: Path) -> str | None:
    header = (
        idf_path
        / "components"
        / "esp_common"
        / "include"
        / "esp_idf_version.h"
    )
    try:
        text = header.read_text(encoding="utf-8")
    except OSError:
        return None
    major = re.search(r"^#define\s+ESP_IDF_VERSION_MAJOR\s+(\d+)", text, re.MULTILINE)
    minor = re.search(r"^#define\s+ESP_IDF_VERSION_MINOR\s+(\d+)", text, re.MULTILINE)
    if major is None or minor is None:
        return None
    return f"{major.group(1)}.{minor.group(1)}"


def resolve_idf_bootstrap_python(
    idf_path: Path,
    environment: Mapping[str, str],
    explicit: Path | None = None,
) -> Path:
    """Find a Python accepted by ESP-IDF without coupling it to mosaico.py."""

    configured = environment.get("MOSAICO_IDF_PYTHON")
    if explicit is None and configured:
        explicit = Path(configured).expanduser()
    commands: list[list[str]] = []
    if explicit is not None:
        commands.append([str(explicit)])
    else:
        active_idf_python = environment.get("IDF_PYTHON_ENV_PATH")
        if active_idf_python:
            commands.append(
                [str(virtual_environment_python(Path(active_idf_python)))]
            )

        tools_root = Path(
            environment.get("IDF_TOOLS_PATH", Path.home() / ".espressif")
        ).expanduser()
        idf_version = _idf_major_minor(idf_path)
        if idf_version:
            python_env_root = tools_root / "python_env"
            for candidate in sorted(
                python_env_root.glob(f"idf{idf_version}_py*_env"), reverse=True
            ):
                commands.append([str(virtual_environment_python(candidate))])

        commands.append([sys.executable])
        search_path = environment.get("PATH")
        for name in (
            "python3",
            "python3.14",
            "python3.13",
            "python3.12",
            "python3.11",
            "python3.10",
            "python",
        ):
            executable = shutil.which(name, path=search_path)
            if executable:
                commands.append([executable])
        launcher = shutil.which("py", path=search_path)
        if launcher:
            commands.append([launcher, "-3"])

    seen: set[tuple[str, ...]] = set()
    for command in commands:
        identity = tuple(command)
        if identity in seen:
            continue
        seen.add(identity)
        probed = _probe_python(command, environment)
        if probed is not None and probed[1] >= MINIMUM_IDF_PYTHON:
            return probed[0]

    if explicit is not None:
        raise HostEnvironmentError(
            f"The configured ESP-IDF bootstrap Python must be 3.10 or newer: {explicit}"
        )
    raise HostEnvironmentError(
        "mosaico.py supports Python 3.8 or newer, but ESP-IDF requires a separate "
        "Python 3.10 or newer interpreter and none was found."
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
    python = resolve_idf_bootstrap_python(
        root, values, explicit=bootstrap_python
    )
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
    idf_python_probe = _probe_python([str(idf_python)], values)
    if idf_python_probe is None or idf_python_probe[1] < MINIMUM_IDF_PYTHON:
        raise HostEnvironmentError(
            "mosaico.py supports Python 3.8 or newer, but the selected ESP-IDF "
            f"environment requires Python 3.10 or newer: {idf_python}"
        )
    return IdfEnvironment(root, idf_python, root / "tools" / "idf.py", values)
