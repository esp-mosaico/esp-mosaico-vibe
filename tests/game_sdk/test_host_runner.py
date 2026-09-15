from __future__ import annotations

import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
PROJECT = ROOT / "projects/tower_defense"
SKY_PROJECT = ROOT / "projects/sky_hop"
SHOOTER_PROJECT = ROOT / "projects/raylib_shooter"


class TowerHostRunnerTests(unittest.TestCase):
    @unittest.skipUnless(
        (PROJECT / "managed_components/georgik__raylib/raylib/src/raylib.h").is_file(),
        "run game build first to resolve the pinned Raylib component",
    )
    def test_state_and_rgb565_screenshot_are_deterministic(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            replay = Path(directory) / "replay.json"
            replay.write_text(json.dumps({"events": [
                {"frame": 0, "type": "tap", "x": 240, "y": 220},
                {"frame": 1, "type": "tap", "x": 58, "y": 154},
                {"frame": 2, "type": "tap", "x": 200, "y": 435},
                {"frame": 3, "type": "tap", "x": 195, "y": 130},
                {"frame": 4, "type": "tap", "x": 350, "y": 435},
                {"frame": 5, "type": "tap", "x": 414, "y": 218},
            ]}), encoding="utf-8")
            command = [sys.executable, str(ROOT / "submodule/raylib-lite-engine/host/run_game.py"),
                       "--project", str(PROJECT), "--headless", "--frames", "300",
                       "--replay", str(replay)]
            first = json.loads(subprocess.check_output(command, cwd=ROOT))
            first_png = Path(first["frame"])
            if not first_png.is_absolute(): first_png = ROOT / first_png
            first_hash = hashlib.sha256(Image.open(first_png).tobytes()).hexdigest()
            second = json.loads(subprocess.check_output(command, cwd=ROOT))
            second_png = Path(second["frame"])
            if not second_png.is_absolute(): second_png = ROOT / second_png
            self.assertEqual(first["state_hash"], second["state_hash"])
            self.assertEqual(first_hash, hashlib.sha256(Image.open(second_png).tobytes()).hexdigest())
            self.assertEqual(first["game_id"], "tower_defense")
            self.assertEqual(first["state_hash"], "e7ece7ba")
            self.assertEqual(first_hash,
                             "6880cc956e3f16b7d41a4674a2f8e7a9215d5ab51ed096f9f84a20d00114a79a")

    def test_replay_supports_pause_single_step_and_state_output(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            replay = root / "replay.json"
            state = root / "state.json"
            replay.write_text(json.dumps({"events": [
                {"frame": 0, "type": "tap", "x": 240, "y": 220},
                {"frame": 5, "type": "pause"},
                {"frame": 10, "type": "step"},
                {"frame": 20, "type": "resume"},
            ]}), encoding="utf-8")
            command = [sys.executable, str(ROOT / "submodule/raylib-lite-engine/host/run_game.py"),
                       "--project", str(PROJECT), "--headless", "--frames", "30",
                       "--replay", str(replay), "--state-output", str(state)]
            result = json.loads(subprocess.check_output(command, cwd=ROOT))
            persisted = json.loads(state.read_text(encoding="utf-8"))
            self.assertEqual(result["state_hash"], persisted["state_hash"])
            self.assertEqual(result["frames"], 30)
            self.assertTrue(Path(result["frame"]).is_file())


class SkyHopHostRunnerTests(unittest.TestCase):
    def test_host_runtime_has_no_project_specific_renderer(self) -> None:
        source = (ROOT / "submodule/raylib-lite-engine/host/run_game.py").read_text(encoding="utf-8")
        for project_name in ("tower_defense", "sky_hop", "raylib_shooter"):
            self.assertNotIn(project_name, source)
        self.assertFalse((ROOT / "submodule/raylib-lite-engine/host/tower_host_renderer.c").exists())

    def test_headless_preview_uses_shared_game_model(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            state = Path(directory) / "state.json"
            command = [sys.executable, str(ROOT / "submodule/raylib-lite-engine/host/run_game.py"),
                       "--project", str(SKY_PROJECT), "--headless", "--frames", "120",
                       "--state-output", str(state)]
            first = json.loads(subprocess.check_output(command, cwd=ROOT))
            second = json.loads(subprocess.check_output(command, cwd=ROOT))
            self.assertEqual(first["state_hash"], second["state_hash"])
            self.assertEqual(first["phase"], "playing")
            self.assertEqual(first["abi"], 1)
            self.assertEqual(first["game_id"], "sky_hop")
            self.assertEqual(first["level"], 1)
            self.assertEqual(first["levels"], 4)
            persisted = json.loads(state.read_text(encoding="utf-8"))
            for key in ("host_render_ms", "host_render_mean_ms", "host_encode_ms"):
                first.pop(key)
                persisted.pop(key)
            self.assertEqual(first, persisted)
            self.assertGreater(first["raster"]["binary_alpha_calls"], 0)
            self.assertTrue(Path(first["frame"]).is_file())

    def test_shooter_uses_shared_source_renderer_module(self) -> None:
        command = [sys.executable, str(ROOT / "submodule/raylib-lite-engine/host/run_game.py"),
                   "--project", str(SHOOTER_PROJECT), "--headless",
                   "--frames", "10"]
        result = json.loads(subprocess.check_output(command, cwd=ROOT))
        self.assertEqual(result["frames"], 10)
        self.assertEqual(result["abi"], 1)
        self.assertEqual(result["game_id"], "raylib_shooter")
        self.assertEqual(result["phase"], "playing")
        self.assertRegex(result["state_hash"], r"^[0-9a-f]{8}$")

    def test_shooter_keyboard_action_moves_player(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            replay = Path(directory) / "replay.json"
            replay.write_text(json.dumps({"events": [
                {"frame": 0, "type": "action", "code": "restart", "pressed": True},
                {"frame": 1, "type": "action", "code": "right", "pressed": True},
                {"frame": 11, "type": "action", "code": "right", "pressed": False},
            ]}), encoding="utf-8")
            command = [sys.executable, str(ROOT / "submodule/raylib-lite-engine/host/run_game.py"),
                       "--project", str(SHOOTER_PROJECT), "--headless",
                       "--frames", "12", "--replay", str(replay)]
            result = json.loads(subprocess.check_output(command, cwd=ROOT))
            self.assertEqual(result["phase"], "playing")
            self.assertGreater(result["player_x"], 222)


if __name__ == "__main__":
    unittest.main()
