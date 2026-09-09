"""Compile the GSP screen backend and regress its frame-buffer lifecycle."""
from __future__ import annotations

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HOST = Path(__file__).parent / "gsp_mirror_host"
MIRROR = ROOT / "projects" / "gsp_hello" / "main" / "iris_screen_mirror.c"
MAIN = MIRROR.parent


class GspMirrorLifecycleTest(unittest.TestCase):
    def test_frames_exist_only_during_stream(self) -> None:
        compiler = shutil.which("cc")
        self.assertIsNotNone(compiler, "a C compiler is required for the mirror lifecycle test")
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "mirror-lifecycle"
            command = [
                str(compiler),
                "-std=c11",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-I",
                str(HOST),
                "-I",
                str(MAIN),
                str(HOST / "mirror_lifecycle_test.c"),
                str(MIRROR),
                "-o",
                str(output),
            ]
            build = subprocess.run(
                command, capture_output=True, text=True, timeout=60, check=False
            )
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            run = subprocess.run(
                [str(output)], capture_output=True, text=True, timeout=30, check=False
            )
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == "__main__":
    unittest.main()
