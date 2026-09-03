#!/usr/bin/env python3
"""Cache standalone GSPC / GSP simulator releases for the pinned ESP-GSP submodule."""

from __future__ import annotations

import argparse
import os
import platform
import shutil
import sys
import tarfile
import tempfile
import urllib.request
import zipfile
from pathlib import Path

TOOLS_DIR = Path(__file__).resolve().parent
REPO_ROOT = TOOLS_DIR.parents[1]
GSP_ROOT = REPO_ROOT / "submodule" / "esp-gsp"
LICENSE_NAME = "THIRD_PARTY_LICENSES.txt"


def gspc_version() -> str:
    marker = GSP_ROOT / ".gspc_version"
    if marker.is_file():
        text = marker.read_text(encoding="utf-8").strip()
        if text:
            return text
    return "0.2.8"


def gsp_component_version() -> str:
    manifest = GSP_ROOT / "idf_component.yml"
    if manifest.is_file():
        for line in manifest.read_text(encoding="utf-8").splitlines():
            if line.startswith("version:"):
                return line.split(":", 1)[1].strip().strip("'\"")
    return "1.1.0"


def host_os_arch() -> tuple[str, str]:
    system = platform.system()
    machine = platform.machine().lower()
    if machine in {"amd64", "x86_64"}:
        arch = "x86_64"
    elif machine in {"aarch64", "arm64"}:
        arch = "aarch64"
    else:
        raise RuntimeError(f"unsupported host architecture: {machine}")
    if system == "Linux":
        return "linux", arch
    if system == "Darwin":
        return "macos", "universal2"
    if system == "Windows":
        return "windows", arch
    raise RuntimeError(f"unsupported GSP host: {system}/{machine}")


def archive_name(product: str, version: str) -> str:
    os_name, arch = host_os_arch()
    suffix = "zip" if os_name == "windows" else "tar.gz"
    return f"{product}-{version}-{os_name}-{arch}.{suffix}"


def default_cache_dir(kind: str, version: str) -> Path:
    configured = os.environ.get("GSPC_CACHE_DIR" if kind == "gspc" else "GSP_SIM_CACHE_DIR")
    if configured:
        path = Path(configured).expanduser()
        path.mkdir(parents=True, exist_ok=True)
        return path
    xdg = os.environ.get("XDG_CACHE_HOME")
    candidates = []
    if xdg:
        candidates.append(Path(xdg) / "esp-mosaico" / kind / version)
    candidates.append(Path.home() / ".cache" / "esp-mosaico" / kind / version)
    candidates.append(TOOLS_DIR / ".cache" / kind / version)
    for path in candidates:
        try:
            path.mkdir(parents=True, exist_ok=True)
            return path
        except OSError:
            continue
    raise RuntimeError(f"cannot create a {kind} cache directory")


def download(url: str, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.is_file():
        return
    fd, temporary_name = tempfile.mkstemp(
        prefix=f".{destination.name}.",
        suffix=".download",
        dir=destination.parent,
    )
    temporary = Path(temporary_name)
    try:
        request = urllib.request.Request(
            url, headers={"User-Agent": "esp-mosaico-vibe-gsp/1"}
        )
        with os.fdopen(fd, "wb") as output:
            with urllib.request.urlopen(request, timeout=120) as response:
                while True:
                    chunk = response.read(1024 * 1024)
                    if not chunk:
                        break
                    output.write(chunk)
            output.flush()
            os.fsync(output.fileno())
        os.replace(temporary, destination)
    except Exception:
        temporary.unlink(missing_ok=True)
        raise


def extract_binary(archive: Path, binary: str, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=f"{binary}-extract-") as directory:
        staging = Path(directory)
        if archive.suffix == ".zip":
            with zipfile.ZipFile(archive) as bundle:
                bundle.extractall(staging)
        else:
            with tarfile.open(archive, "r:gz") as bundle:
                bundle.extractall(staging)
        matches = [path for path in staging.rglob(binary) if path.is_file()]
        if not matches:
            raise RuntimeError(f"{binary} missing from {archive.name}")
        shutil.copy2(matches[0], destination)
    if os.name != "nt":
        destination.chmod(0o755)


def resolve_release(
    *,
    product: str,
    version: str,
    env_var: str,
    cache_dir: Path | None = None,
) -> Path:
    configured = os.environ.get(env_var)
    if configured:
        executable = Path(configured).expanduser()
        if not executable.is_absolute():
            raise RuntimeError(f"{env_var} must be an absolute path")
        if not executable.is_file():
            raise RuntimeError(f"{env_var} is not a file: {executable}")
        return executable

    cache = cache_dir or default_cache_dir(product, version)
    binary = cache / product
    if binary.is_file():
        return binary
    name = archive_name(product, version)
    base = f"https://dl.espressif.com/AE/gsp/{product}/v{version}/"
    archive = cache / name
    download(base + name, archive)
    download(base + LICENSE_NAME, cache / LICENSE_NAME)
    extract_binary(archive, product, binary)
    return binary


def resolve_gspc(cache_dir: Path | None = None) -> Path:
    return resolve_release(
        product="gspc",
        version=gspc_version(),
        env_var="GSPC_EXECUTABLE",
        cache_dir=cache_dir,
    )


def resolve_sim(cache_dir: Path | None = None) -> Path:
    return resolve_release(
        product="sim",
        version=gsp_component_version(),
        env_var="GSP_SIM_EXECUTABLE",
        cache_dir=cache_dir,
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--sim", action="store_true", help="print the simulator path")
    args = parser.parse_args()
    try:
        path = resolve_sim(args.output_dir) if args.sim else resolve_gspc(args.output_dir)
    except Exception as error:
        print(f"{'sim' if args.sim else 'gspc'}: {error}", file=sys.stderr)
        return 1
    print(path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
