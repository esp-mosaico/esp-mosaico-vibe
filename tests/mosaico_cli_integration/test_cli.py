from __future__ import annotations

import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest


REPOSITORY = Path(__file__).resolve().parents[2]
UTILS_ROOT = REPOSITORY / "submodule" / "esp-mosaico-utils"
TOOL_ROOT = UTILS_ROOT / "esp-mosaico-recovery"
sys.path.insert(0, str(TOOL_ROOT / "tools"))

from mosaico_cli.project import resolve_project
from mosaico_cli.workspace import load_workspace


class ToolSubmoduleIntegrationTests(unittest.TestCase):
    def test_workspace_configuration_resolves_main_repository_resources(self) -> None:
        workspace = load_workspace(TOOL_ROOT, explicit=str(REPOSITORY))

        self.assertEqual(workspace.root, REPOSITORY)
        self.assertEqual(
            workspace.esp_iris_path,
            UTILS_ROOT / "ESP-Iris",
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

    def test_launcher_initializes_real_reference_from_nested_workspace_directory(self) -> None:
        # Local component references must share a drive with the generated app;
        # Windows CI puts the checkout on D: and the default temp directory on C:.
        with tempfile.TemporaryDirectory(prefix="mosaico init ", dir=REPOSITORY) as temporary:
            root = Path(temporary).resolve()
            config = json.loads((REPOSITORY / ".mosaico.json").read_text(encoding="utf-8"))
            config["workspace"]["init_template"] = str(REPOSITORY / "projects/hello_world")
            config["workspace"]["projects_dir"] = "apps/nested"
            for key, value in config["dependencies"].items():
                config["dependencies"][key] = str(REPOSITORY / value)
            config_path = root / ".mosaico.json"
            config_path.write_text(json.dumps(config), encoding="utf-8")
            for filename in ("components/esp_mosaico_app_recovery/CMakeLists.txt",
                             "cmake/system_update.cmake", "tools/prepare_system_update.py"):
                target = root / filename
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(REPOSITORY / filename, target)
            nested = root / "apps"
            nested.mkdir()
            result = subprocess.run(
                [sys.executable, str(REPOSITORY / "mosaico.py"), "init", "my_app", "--json"],
                cwd=nested, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            payload = json.loads(result.stdout)
            project = root / "apps/nested/my_app"
            self.assertEqual(Path(payload["project"]), project)
            workspace = load_workspace(TOOL_ROOT, explicit=str(root))
            self.assertEqual(resolve_project(workspace, None, project / "main"), project)
            self.assertEqual(json.loads(config_path.read_text()), config)
            reference = REPOSITORY / "projects/hello_world"
            for filename in ("partitions.csv", "sdkconfig.defaults", "main/CMakeLists.txt"):
                self.assertEqual((project / filename).read_bytes(), (reference / filename).read_bytes())
            application = (project / "sdkconfig.application.defaults").read_text()
            original = (reference / "sdkconfig.application.defaults").read_text()
            without_product = lambda text: re.sub(r'^CONFIG_ESP_IRIS_USB_PRODUCT=.*$', '', text, flags=re.MULTILINE)
            self.assertEqual(without_product(application), without_product(original))
            source = (project / "main/main.c").read_text()
            self.assertIn("iris_ota_support_start();", source)
            self.assertIn("esp_iris_boot_probe()", source)
            self.assertIn('"Hello World!"', source)
            self.assertIn('TAG = "my_app"', source)
            self.assertFalse((project / "build").exists())
            self.assertFalse(workspace.run_dir.exists())


if __name__ == "__main__":
    unittest.main()
