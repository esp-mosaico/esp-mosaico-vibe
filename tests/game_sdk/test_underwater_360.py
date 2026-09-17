from __future__ import annotations

import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
PROJECT = ROOT / "projects/underwater_360"


class Underwater360Tests(unittest.TestCase):
    def test_model_compiles_and_clamps_drag(self) -> None:
        compiler = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
        self.assertIsNotNone(compiler)
        source = r'''#include <assert.h>
#include "underwater_world.h"
int main(void){underwater_world_t w;underwater_world_reset(&w);
 underwater_world_pointer(&w,100,100,1);underwater_world_pointer(&w,-1000,1000,1);
 assert(w.yaw>=0&&w.yaw<360);assert(w.pitch==24);
 underwater_world_pointer(&w,-1000,1000,0);for(int i=0;i<100;i++)underwater_world_update(&w);
 assert(w.yaw>=0&&w.yaw<360);return 0;}'''
        with tempfile.TemporaryDirectory() as directory:
            main = Path(directory) / "main.c"
            main.write_text(source, encoding="utf-8")
            executable = Path(directory) / "model"
            subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                            str(main), str(PROJECT / "main/underwater_world.c"),
                            "-I", str(PROJECT / "main"), "-o", str(executable)], check=True)
            subprocess.run([str(executable)], check=True)

    def test_host_drag_changes_yaw_and_pitch(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            replay = Path(directory) / "drag.json"
            replay.write_text(json.dumps({"events": [
                {"frame": 0, "type": "pointer", "track_id": 1, "x": 120, "y": 180, "pressed": True},
                {"frame": 1, "type": "pointer", "track_id": 1, "x": 360, "y": 280, "pressed": True},
                {"frame": 2, "type": "pointer", "track_id": 1, "x": 360, "y": 280, "pressed": False},
            ]}), encoding="utf-8")
            command = [sys.executable, str(ROOT / "submodule/raylib-lite-engine/host/run_game.py"),
                       "--project", str(PROJECT), "--headless", "--frames", "3",
                       "--replay", str(replay)]
            result = json.loads(subprocess.check_output(command, cwd=ROOT))
            # The release frame applies the first bounded inertia step.
            self.assertAlmostEqual(result["yaw"], 280.8, places=1)
            self.assertAlmostEqual(result["pitch"], 20.0, places=1)
            self.assertFalse(result["dragging"])

    def test_bottom_buttons_switch_scene_without_dragging(self) -> None:
        compiler = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
        source = r'''#include <assert.h>
#include "underwater_world.h"
int main(void){underwater_world_t w;underwater_world_reset(&w);
 assert(w.scene==UNDERWATER_SCENE_OCEAN);
 underwater_world_pointer(&w,80,440,1);assert(w.scene==UNDERWATER_SCENE_AURORA&&!w.dragging);
 underwater_world_pointer(&w,80,440,0);
 underwater_world_pointer(&w,300,440,1);assert(w.scene==UNDERWATER_SCENE_SUNRISE&&!w.dragging);
 underwater_world_pointer(&w,300,440,0);
 underwater_world_pointer(&w,410,440,1);assert(w.scene==UNDERWATER_SCENE_RAINFOREST&&!w.dragging);
 underwater_world_pointer(&w,410,440,0);
 assert(w.effects_level==1);underwater_world_pointer(&w,120,32,1);assert(w.effects_level==2&&!w.dragging);
 return 0;}'''
        with tempfile.TemporaryDirectory() as directory:
            main = Path(directory) / "main.c"
            main.write_text(source, encoding="utf-8")
            executable = Path(directory) / "model"
            subprocess.run([compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                            str(main), str(PROJECT / "main/underwater_world.c"),
                            "-I", str(PROJECT / "main"), "-o", str(executable)], check=True)
            subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    unittest.main()
