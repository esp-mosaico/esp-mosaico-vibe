from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]


class NeonMazeModelTests(unittest.TestCase):
    def test_armor_and_explosive_barrel(self) -> None:
        compiler = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
        self.assertIsNotNone(compiler, "a C compiler is required")
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / (
                "neon-maze-test.exe" if os.name == "nt" else "neon-maze-test"
            )
            subprocess.run([
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                f"-I{ROOT / 'projects/neon_maze_25d/main'}",
                str(ROOT / "tests/game_sdk/test_neon_maze_model.c"),
                str(ROOT / "projects/neon_maze_25d/main/neon_maze_game.c"),
                "-lm", "-o", str(executable),
            ], check=True)
            result = subprocess.run([str(executable)], check=True,
                                    text=True, capture_output=True)
            self.assertEqual(result.stdout.strip(), "neon maze model: ok")


if __name__ == "__main__":
    unittest.main()
