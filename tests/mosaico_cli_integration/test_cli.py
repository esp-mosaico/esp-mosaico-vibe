from __future__ import annotations

from contextlib import contextmanager
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
from mosaico_cli.scaffold import initialize_project
from mosaico_cli.workspace import load_workspace


class ToolSubmoduleIntegrationTests(unittest.TestCase):
    def test_nested_help_does_not_dispatch_game_option_values(self) -> None:
        result = subprocess.run(
            [sys.executable, str(REPOSITORY / "mosaico.py"),
             "iris", "rpc", "1", "2", "--payload", "game", "--help"],
            cwd=REPOSITORY, text=True, capture_output=True, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("mosaico.py iris rpc", result.stdout)
        self.assertNotIn("Game development:", result.stdout)

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

    @contextmanager
    def reference_workspace(self):
        # Local component references must share a drive with the generated app;
        # Windows CI puts the checkout on D: and the default temp directory on C:.
        with tempfile.TemporaryDirectory(prefix="mosaico init ", dir=REPOSITORY) as temporary:
            root = Path(temporary).resolve()
            config = json.loads((REPOSITORY / ".mosaico.json").read_text(encoding="utf-8"))
            config["workspace"]["init_template"] = str(REPOSITORY / "projects/hello_world/mosaico-template.json")
            config["workspace"]["projects_dir"] = "apps/nested"
            for key, value in config["dependencies"].items():
                config["dependencies"][key] = str(REPOSITORY / value)
            config_path = root / ".mosaico.json"
            config_path.write_text(json.dumps(config), encoding="utf-8")
            for filename in ("components/esp_mosaico_app_recovery/CMakeLists.txt",
                             "components/esp_mosaico_app_recovery/sdkconfig.defaults",
                             "cmake/mosaico_application.cmake",
                             "cmake/system_update.cmake", "tools/prepare_system_update.py",
                             "tools/gsp-sim/fetch_gspc.py", "tools/pack_gsp_partition.py"):
                target = root / filename
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(REPOSITORY / filename, target)
            yield root, config

    def test_launcher_initializes_real_reference_from_nested_workspace_directory(self) -> None:
        with self.reference_workspace() as (root, config):
            config_path = root / ".mosaico.json"
            nested = root / "apps"
            nested.mkdir()
            result = subprocess.run(
                [sys.executable, str(REPOSITORY / "mosaico.py"), "project", "init", "my_app", "--json"],
                cwd=nested, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            payload = json.loads(result.stdout)
            project = root / "apps/nested/my_app"
            self.assertEqual(Path(payload["project"]), project)
            workspace = load_workspace(TOOL_ROOT, explicit=str(root))
            self.assertEqual(resolve_project(workspace, None, project / "main"), project)
            self.assertEqual(json.loads(config_path.read_text(encoding="utf-8")), config)
            reference = REPOSITORY / "projects/hello_world"
            for filename in ("partitions.csv", "sdkconfig.defaults", "main/hello_ui.c",
                             "pc/CMakeLists.txt", "ui/main.json", "ui/fonts/DejaVuSans-Bold.ttf"):
                self.assertEqual((project / filename).read_bytes(), (reference / filename).read_bytes())
            application = (project / "sdkconfig.application.defaults").read_text(encoding="utf-8")
            original = (reference / "sdkconfig.application.defaults").read_text(encoding="utf-8")
            without_product = lambda text: re.sub(r'^CONFIG_ESP_IRIS_USB_PRODUCT=.*$', '', text, flags=re.MULTILINE)
            self.assertEqual(without_product(application), without_product(original))
            source = (project / "main/main.c").read_text(encoding="utf-8")
            self.assertIn("iris_ota_support_start();", source)
            self.assertIn("esp_iris_boot_probe()", source)
            self.assertIn("hello_ui_init(ui, &s_hello)", source)
            self.assertIn("Say hello", (project / "ui/main.json").read_text(encoding="utf-8"))
            for filename, resource in (("CMakeLists.txt", "tools/gsp-sim/fetch_gspc.py"),
                                       ("main/CMakeLists.txt", "tools/pack_gsp_partition.py")):
                rendered = (project / filename).read_text(encoding="utf-8")
                references = re.findall(r'\$\{CMAKE_CURRENT_LIST_DIR\}/([^"\n]+)', rendered)
                self.assertIn((root / resource).resolve(),
                              [(project / filename).parent.joinpath(ref).resolve() for ref in references])
            self.assertIn('TAG = "my_app"', source)
            self.assertFalse((project / "build").exists())
            self.assertFalse(workspace.run_dir.exists())


    def test_reference_can_evolve_using_only_workspace_owned_files(self) -> None:
        with self.reference_workspace() as (root, config):
            original = REPOSITORY / "projects/hello_world"
            template = root / "templates/changed_reference"
            descriptor = json.loads((original / "mosaico-template.json").read_text(encoding="utf-8"))
            for entry in descriptor["files"]:
                target = template / entry["source"]
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(original / entry["source"], target)

            # Rename the entry source and its TAG, and maintain its generation
            # rule in the same workspace revision, with no tool changes.
            source = template / "source/entry.c"
            source.parent.mkdir()
            (template / "main/main.c").rename(source)
            source.write_text(source.read_text(encoding="utf-8").replace("TAG", "APP_TAG"), encoding="utf-8")
            cmake = template / "main/CMakeLists.txt"
            cmake.write_text(cmake.read_text(encoding="utf-8").replace('"main.c"', '"../source/entry.c"'), encoding="utf-8")
            for entry in descriptor["files"]:
                if entry["source"] == "main/main.c":
                    entry["source"] = "source/entry.c"
                    entry["replacements"][0]["pattern"] = entry["replacements"][0]["pattern"].replace("TAG", "APP_TAG")
                    entry["replacements"][0]["replacement"] = entry["replacements"][0]["replacement"].replace("TAG", "APP_TAG")
                if entry["source"] == "README.md":
                    entry["replacements"][0]["pattern"] = "^# Reference Application$"
            readme = template / "README.md"
            readme.write_text(readme.read_text(encoding="utf-8").replace("# ESP-Mosaico Hello World", "# Reference Application"), encoding="utf-8")
            (template / "assets").mkdir()
            (template / "assets/extra.dat").write_bytes(b"\x00\xffextra")
            descriptor["files"].append({"source": "assets/extra.dat"})
            description_path = template / "mosaico-template.json"
            description_path.write_text(json.dumps(descriptor), encoding="utf-8")
            config["workspace"]["init_template"] = str(description_path)
            (root / ".mosaico.json").write_text(json.dumps(config), encoding="utf-8")

            workspace = load_workspace(TOOL_ROOT, explicit=str(root))
            result = initialize_project(workspace, "evolved_app")
            project = Path(result["project"])
            self.assertEqual(len(result["files"]), len(descriptor["files"]))
            self.assertFalse((project / "main/main.c").exists())
            self.assertIn('APP_TAG = "evolved_app"', (project / "source/entry.c").read_text(encoding="utf-8"))
            self.assertIn("iris_ota_support_start();", (project / "source/entry.c").read_text(encoding="utf-8"))
            self.assertEqual((project / "assets/extra.dat").read_bytes(), b"\x00\xffextra")
            self.assertTrue((project / "README.md").read_text(encoding="utf-8").startswith("# ESP-Mosaico evolved_app"))
            self.assertEqual((project / "partitions.csv").read_bytes(), (original / "partitions.csv").read_bytes())


if __name__ == "__main__":
    unittest.main()
