#!/usr/bin/env python3
"""Pack a GSP scene and run the standalone ESP-GSP simulator."""

from __future__ import annotations

import argparse
import os
import socket
import subprocess
import sys
import tempfile
from pathlib import Path

TOOLS_DIR = Path(__file__).resolve().parent
REPO_ROOT = TOOLS_DIR.parents[1]
GSP_ROOT = REPO_ROOT / "submodule" / "esp-gsp"
DEFAULT_SCENE = REPO_ROOT / "projects" / "gsp_hello" / "ui" / "main.json"

sys.path.insert(0, str(TOOLS_DIR))
from fetch_gspc import resolve_gspc, resolve_sim  # noqa: E402


def discover_backend(scene: Path) -> Path | None:
    if scene.suffix == ".gspb":
        return None
    candidate = scene.resolve().parent.parent / "sim_backend.py"
    return candidate if candidate.is_file() else None


def pick_backend_port() -> int:
    for port in range(8684, 8694):
        probe = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            probe.bind(("127.0.0.1", port))
            return port
        except OSError:
            continue
        finally:
            probe.close()
    raise SystemExit("no free simulator backend port in 8684-8693")


def pack_scene(scene: Path, output: Path, gspc: Path) -> None:
    command = [
        str(gspc),
        "pack",
        str(scene),
        "--pixel-format",
        "rgb565",
        "--deployable",
        "-o",
        str(output),
    ]
    subprocess.run(command, check=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "scene",
        nargs="?",
        type=Path,
        default=DEFAULT_SCENE,
        help="scene JSON or precompiled .gspb (default: projects/gsp_hello/ui/main.json)",
    )
    parser.add_argument("--headless", action="store_true")
    parser.add_argument("--interactive", action="store_true")
    parser.add_argument("--frames", type=int)
    parser.add_argument("--fps", type=int, default=60)
    parser.add_argument("--dump-ppm", type=Path)
    parser.add_argument(
        "--backend",
        type=Path,
        help="application backend script (default: <project>/sim_backend.py)",
    )
    parser.add_argument(
        "--no-backend",
        action="store_true",
        help="do not attach an application backend",
    )
    parser.add_argument(
        "sim_args",
        nargs=argparse.REMAINDER,
        help="extra simulator flags; pass after -- (for example -- --drag 80 360 400 360)",
    )
    args = parser.parse_args()
    scene = args.scene.expanduser().resolve()
    if not scene.is_file():
        raise SystemExit(f"scene not found: {scene}")
    if args.headless and args.interactive:
        raise SystemExit("use either --headless or --interactive")
    if not GSP_ROOT.is_dir():
        raise SystemExit(
            f"ESP-GSP submodule is missing at {GSP_ROOT}; "
            "run git submodule update --init submodule/esp-gsp"
        )

    extra = list(args.sim_args)
    if extra and extra[0] == "--":
        extra = extra[1:]

    sim_args: list[str] = []
    if args.headless:
        sim_args.append("--headless")
        if args.frames is None:
            sim_args.extend(["--frames", "30"])
    elif args.frames is None:
        sim_args.extend(["--frames", "0"])
    if args.frames is not None:
        sim_args.extend(["--frames", str(args.frames)])
    if args.fps:
        sim_args.extend(["--fps", str(args.fps)])
    if args.dump_ppm is not None:
        dump = args.dump_ppm.expanduser().resolve()
        dump.parent.mkdir(parents=True, exist_ok=True)
        sim_args.extend(["--dump", str(dump), "--dump-format", "ppm"])
    sim_args.extend(extra)

    backend_script = None
    if not args.no_backend and "--backend-listen" not in extra:
        if args.backend is not None:
            backend_script = args.backend.expanduser().resolve()
            if not backend_script.is_file():
                raise SystemExit(f"backend not found: {backend_script}")
        else:
            backend_script = discover_backend(scene)
    backend_port = None
    if backend_script is not None:
        backend_port = pick_backend_port()
        sim_args.extend(
            [
                "--backend-listen",
                f"tcp://127.0.0.1:{backend_port}",
                "--backend-required",
                "--backend-idle-timeout",
                "15",
            ]
        )

    gspc = resolve_gspc()
    simulator = resolve_sim()
    os.environ["GSPC_EXECUTABLE"] = str(gspc)
    os.environ["GSP_SIM_EXECUTABLE"] = str(simulator)

    with tempfile.TemporaryDirectory(prefix="mosaico-gsp-sim-") as directory:
        if scene.suffix == ".gspb":
            bundle = scene
        else:
            bundle = Path(directory) / "preview.gspb"
            pack_scene(scene, bundle, gspc)
        command = [str(simulator), "--bundle", str(bundle), *sim_args]
        if backend_script is None:
            return subprocess.call(command)
        simulator_proc = subprocess.Popen(command)
        backend_proc = subprocess.Popen(
            [
                sys.executable,
                str(backend_script),
                "--host",
                "127.0.0.1",
                "--port",
                str(backend_port),
            ]
        )
        try:
            return simulator_proc.wait()
        except KeyboardInterrupt:
            simulator_proc.terminate()
            return 130
        finally:
            backend_proc.terminate()
            try:
                backend_proc.wait(timeout=2)
            except subprocess.TimeoutExpired:
                backend_proc.kill()
            if simulator_proc.poll() is None:
                simulator_proc.terminate()


if __name__ == "__main__":
    raise SystemExit(main())
