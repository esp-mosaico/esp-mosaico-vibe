import importlib.util
from pathlib import Path
import sys
import unittest
from unittest import mock


REPO_ROOT = Path(__file__).resolve().parents[1]
MODULE_PATH = REPO_ROOT / "tools" / "gsp-sim" / "run.py"
SPEC = importlib.util.spec_from_file_location("gsp_sim_run", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


class GspSimRunTests(unittest.TestCase):
    def run_main(self, *arguments: str) -> tuple[int, list[str]]:
        with (
            mock.patch.object(sys, "argv", [str(MODULE_PATH), *arguments]),
            mock.patch.object(MODULE, "resolve_gsp_root") as resolve_gsp_root,
            mock.patch.object(MODULE, "resolve_gspc", return_value=Path("/tmp/gspc")),
            mock.patch.object(MODULE, "resolve_sim", return_value=Path("/tmp/sim")),
            mock.patch.object(MODULE, "pack_scene"),
            mock.patch.object(MODULE.subprocess, "call", return_value=0) as call,
        ):
            result = MODULE.main()

        resolve_gsp_root.assert_not_called()
        command = call.call_args.args[0]
        return result, command

    def test_frames_and_fps_select_scene_only_without_component(self) -> None:
        result, command = self.run_main(
            "--headless", "--frames", "12", "--fps", "24"
        )

        self.assertEqual(result, 0)
        self.assertIn("--headless", command)
        self.assertEqual(command[command.index("--frames") + 1], "12")
        self.assertEqual(command[command.index("--fps") + 1], "24")

    def test_explicit_fps_selects_scene_only(self) -> None:
        result, command = self.run_main("--interactive", "--fps", "60")

        self.assertEqual(result, 0)
        self.assertEqual(command[command.index("--frames") + 1], "0")
        self.assertEqual(command[command.index("--fps") + 1], "60")

    def test_default_preview_requires_component_for_sim_bridge(self) -> None:
        with (
            mock.patch.object(sys, "argv", [str(MODULE_PATH), "--headless"]),
            mock.patch.object(MODULE, "resolve_gsp_root", return_value=None),
        ):
            with self.assertRaisesRegex(SystemExit, "esp-gsp is not installed"):
                MODULE.main()

    def test_default_preview_still_uses_sim_bridge(self) -> None:
        component = Path("/tmp/esp-gsp")
        with (
            mock.patch.object(sys, "argv", [str(MODULE_PATH), "--headless"]),
            mock.patch.object(MODULE, "resolve_gsp_root", return_value=component),
            mock.patch.object(MODULE, "sim_bridge_script", return_value=MODULE_PATH),
            mock.patch.object(MODULE, "run_sim_bridge", return_value=7) as run_bridge,
        ):
            result = MODULE.main()

        self.assertEqual(result, 7)
        run_bridge.assert_called_once_with(
            REPO_ROOT / "projects" / "gsp_hello" / "pc",
            component,
            headless=True,
        )


if __name__ == "__main__":
    unittest.main()
