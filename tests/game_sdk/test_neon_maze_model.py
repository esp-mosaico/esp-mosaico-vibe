from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]


class NeonMazeModelTests(unittest.TestCase):
    def test_preview_keeps_device_corner_mask(self) -> None:
        preview = (ROOT / "projects/last_zone_extraction/sim_preview.html").read_text(
            encoding="utf-8"
        )
        self.assertIn("border-radius:60px", preview)
        self.assertIn("clip-path:inset(0 round 60px)", preview)
        self.assertIn("context.roundRect(0,0,480,480,60)", preview)

    def test_armor_and_explosive_barrel(self) -> None:
        compiler = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
        self.assertIsNotNone(compiler, "a C compiler is required")
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / (
                "neon-maze-test.exe" if os.name == "nt" else "neon-maze-test"
            )
            subprocess.run([
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                f"-I{ROOT / 'projects/last_zone_extraction/main'}",
                str(ROOT / "tests/game_sdk/test_neon_maze_model.c"),
                str(ROOT / "projects/last_zone_extraction/main/neon_maze_game.c"),
                "-lm", "-o", str(executable),
            ], check=True)
            result = subprocess.run([str(executable)], check=True,
                                    text=True, capture_output=True)
            self.assertEqual(result.stdout.strip(), "neon maze model: ok")


if __name__ == "__main__":
    unittest.main()
