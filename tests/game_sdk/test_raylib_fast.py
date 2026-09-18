from __future__ import annotations

from pathlib import Path
import os
import shutil
import struct
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]


class RaylibFastHostTests(unittest.TestCase):
    def test_extended_api_draws_and_matches_runtime_contract(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            temp = Path(directory)
            executable = temp / ("raylib-fast-test.exe" if os.name == "nt" else "raylib-fast-test")
            compiler = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
            self.assertIsNotNone(compiler, "a C compiler is required")
            pixels = struct.pack("<12H", *range(1, 13))
            frame = struct.pack("<IHHHHhh", 0x12345678, 0, 0, 4, 3, 2, 1)
            (temp / "scale.atlas").write_bytes(
                struct.pack("<4sHHHHII", b"MSA1", 4, 3, 1, 0,
                            len(pixels), 0) + frame + pixels
            )
            alpha = bytes([255, 0, 255, 0, 0, 255, 0, 255, 255, 255, 0, 0])
            (temp / "mask.atlas").write_bytes(
                struct.pack("<4sHHHHII", b"MSA1", 4, 3, 0, 1,
                            len(pixels), len(alpha)) + pixels + alpha
            )
            smooth_alpha = bytes([255, 128, 0, 64, 32, 255, 192, 0,
                                  255, 96, 48, 224])
            (temp / "smooth.atlas").write_bytes(
                struct.pack("<4sHHHHII", b"MSA1", 4, 3, 0, 0,
                            len(pixels), len(smooth_alpha)) + pixels + smooth_alpha
            )
            command = [
                compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                str(ROOT / "tests/game_sdk/test_raylib_fast_host.c"),
                str(ROOT / "submodule/raylib-lite-engine/host/host_raylib_port.c"),
                str(ROOT / "submodule/raylib-lite-engine/host/host_asset_runtime.c"),
                str(ROOT / "submodule/raylib-lite-engine/components/mosaico_game_2d/mosaico_game_2d.c"),
                str(ROOT / "submodule/raylib-lite-engine/components/mosaico_game_2d/mosaico_rgb565.c"),
                str(ROOT / "submodule/raylib-lite-engine/components/mosaico_raylib_fast/mosaico_raylib_fast.c"),
                "-I", str(ROOT / "submodule/raylib-lite-engine/host/include"),
                "-I", str(ROOT / "submodule/raylib-lite-engine/host"),
                "-I", str(ROOT / "submodule/raylib-lite-engine/components/mosaico_game_assets/include"),
                "-I", str(ROOT / "submodule/raylib-lite-engine/components/mosaico_game_2d/include"),
                "-I", str(ROOT / "submodule/raylib-lite-engine/components/mosaico_raylib_fast/include"),
                "-lm", "-o", str(executable),
            ]
            subprocess.run(command, check=True, cwd=ROOT)
            subprocess.run([str(executable), str(temp)], check=True, cwd=ROOT)


if __name__ == "__main__":
    unittest.main()
