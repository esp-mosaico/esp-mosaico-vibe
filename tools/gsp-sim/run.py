#!/usr/bin/env python3
"""Run a GSP app preview. Defaults to sim_bridge when the project has pc/."""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
import tempfile
from pathlib import Path

TOOLS_DIR = Path(__file__).resolve().parent
REPO_ROOT = TOOLS_DIR.parents[1]
DEFAULT_SCENE = REPO_ROOT / "projects" / "gsp_hello" / "ui" / "main.json"

sys.path.insert(0, str(TOOLS_DIR))
from fetch_gspc import resolve_gsp_root, resolve_gspc, resolve_sim  # noqa: E402


def find_pc_project(scene: Path) -> Path | None:
    for parent in (scene.parent, *scene.parents):
        pc = parent / "pc"
        if (pc / "CMakeLists.txt").is_file():
            return pc
    return None


def sim_bridge_script(gsp_root: Path) -> Path:
    return gsp_root / "tools" / "sim_bridge" / "run.py"


def run_sim_bridge(pc: Path, gsp_root: Path, *, headless: bool) -> int:
    command = [
        sys.executable,
        str(sim_bridge_script(gsp_root)),
        "--project",
        str(pc),
        "--component-dir",
        str(gsp_root),
        "--gspc",
        str(resolve_gspc(gsp_root=gsp_root)),
        "--host",
        str(resolve_sim(gsp_root=gsp_root)),
        "--build-dir",
        str(REPO_ROOT / "build" / "sim_bridge" / pc.parent.name / pc.name),
    ]
    if headless:
        command.append("--headless")
    return subprocess.call(command)


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
    parser.add_argument(
        "--scene-only",
        action="store_true",
        help="Skip sim_bridge and preview the packed scene without a native backend",
    )
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

    extra = list(args.sim_args)
    if extra and extra[0] == "--":
        extra = extra[1:]

    pc = find_pc_project(scene)
    gsp_root = resolve_gsp_root(pc.parent if pc is not None else None)
    use_bridge = (
        not args.scene_only
        and pc is not None
        and scene.suffix != ".gspb"
        and args.dump_ppm is None
        and not extra
        and gsp_root is not None
        and sim_bridge_script(gsp_root).is_file()
    )
    if use_bridge:
        return run_sim_bridge(pc, gsp_root, headless=args.headless)
    if (
        not args.scene_only
        and pc is not None
        and scene.suffix != ".gspb"
        and args.dump_ppm is None
        and not extra
        and gsp_root is None
    ):
        raise SystemExit(
            "espressif/esp-gsp is not installed. From the application "
            "directory run `idf.py reconfigure` to pull "
            "espressif/esp-gsp==1.2.0 from the ESP Component Registry, "
            "or set ESP_GSP_COMPONENT_DIR."
        )

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
