from __future__ import annotations

from pathlib import Path
import subprocess
import sys
import unittest


ROOT = Path(__file__).resolve().parents[2]


class GameCliTests(unittest.TestCase):
    def test_game_help_exposes_create_sim_and_build(self) -> None:
        output = subprocess.check_output([
            sys.executable, str(ROOT / "mosaico.py"), "game", "--help",
        ], cwd=ROOT, text=True)
        for command in ("create", "sim", "build"):
            self.assertIn(command, output)


if __name__ == "__main__":
    unittest.main()
