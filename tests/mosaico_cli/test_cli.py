from __future__ import annotations

import json
from pathlib import Path
import subprocess
import sys
import unittest


REPOSITORY = Path(__file__).resolve().parents[2]
TOOL_ROOT = REPOSITORY / "submodule" / "esp-mosaico-tools"
sys.path.insert(0, str(TOOL_ROOT / "tools"))

from mosaico_cli.project import resolve_project
from mosaico_cli.workspace import load_workspace


class ToolSubmoduleIntegrationTests(unittest.TestCase):
    def test_workspace_configuration_resolves_main_repository_resources(self) -> None:
        workspace = load_workspace(TOOL_ROOT, explicit=str(REPOSITORY))

        self.assertEqual(workspace.root, REPOSITORY)
        self.assertEqual(
            workspace.esp_iris_path,
            TOOL_ROOT / "submodule" / "esp-iris",
        )
        self.assertEqual(
            workspace.build_runner,
            TOOL_ROOT
            / "skills"
            / "idf-low-noise-build"
            / "scripts"
            / "idf_low_noise_build.py",
        )
        self.assertEqual(
            workspace.recovery_project, TOOL_ROOT / "firmware" / "recovery"
        )
        self.assertEqual(
            workspace.recovery_dir,
            TOOL_ROOT / "firmware" / "recovery" / "prebuilt" / "recovery",
        )

    def test_default_application_is_selected_from_workspace_not_tool_checkout(self) -> None:
        workspace = load_workspace(TOOL_ROOT, explicit=str(REPOSITORY))
        selected = resolve_project(workspace, None, REPOSITORY)
        self.assertEqual(selected, REPOSITORY / "projects" / "hello_world")

    def test_root_launcher_uses_pinned_tool_checkout(self) -> None:
        result = subprocess.run(
            [sys.executable, str(REPOSITORY / "mosaico.py"), "--version"],
            cwd=REPOSITORY / "projects" / "hello_world",
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
        )
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertRegex(result.stdout.strip(), r"^mosaico\.py \d+\.\d+\.\d+$")

    def test_workspace_file_contains_a_supported_schema(self) -> None:
        value = json.loads((REPOSITORY / ".mosaico.json").read_text(encoding="utf-8"))
        self.assertEqual(value["schema_version"], 1)
        self.assertTrue(value["devices"])


if __name__ == "__main__":
    unittest.main()
