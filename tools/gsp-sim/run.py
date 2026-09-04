#!/usr/bin/env python3
"""Pack a GSP scene and run the standalone ESP-GSP simulator."""

from __future__ import annotations

import argparse
import os
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
        return subprocess.call(command)


if __name__ == "__main__":
    raise SystemExit(main())
