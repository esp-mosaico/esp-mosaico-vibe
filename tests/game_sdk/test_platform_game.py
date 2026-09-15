from pathlib import Path
import subprocess
import tempfile
import unittest


REPOSITORY = Path(__file__).resolve().parents[2]


class PlatformGameTests(unittest.TestCase):
    def test_model_compiles_and_moves_and_jumps(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / "platform_game_test"
            subprocess.run([
                "cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                f"-I{REPOSITORY / 'projects/sky_hop/main'}",
                str(REPOSITORY / "tests/game_sdk/test_platform_game.c"),
                str(REPOSITORY / "projects/sky_hop/main/platform_game.c"),
                "-o", str(executable),
            ], check=True)
            result = subprocess.run([str(executable)], check=True,
                                    text=True, capture_output=True)
            self.assertEqual(result.stdout.strip(), "platform game model: ok")


if __name__ == "__main__":
    unittest.main()
