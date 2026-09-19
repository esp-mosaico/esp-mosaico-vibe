"""Exercise generated game projects and the effective CMake contract gate."""
import importlib.util
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("game_cli", ROOT / "submodule/raylib-lite-engine/tools/game_cli.py")
GAME = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(GAME)


class ApplicationTemplateTests(unittest.TestCase):
    def test_each_created_game_loads_shared_contract_before_idf(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "projects").mkdir()
            for template, source in GAME.TEMPLATES.items():
                with self.subTest(template=template):
                    shutil.copytree(ROOT / "projects" / source, root / "projects" / source,
                                    ignore=shutil.ignore_patterns("build", "build-*", "managed_components", "assets", ".codex-runs", "pc"))
                    name = "generated_" + source
                    self.assertEqual(GAME.main(["create", name, "--template", template],
                                               repository=root, tool_root=ROOT), 0)
                    cmake = (root / "projects" / name / "CMakeLists.txt").read_text()
                    self.assertLess(cmake.index("mosaico_application.cmake"), cmake.index("$ENV{IDF_PATH}"))
                    self.assertIn("project(" + name, cmake)
                    self.assertIn("system_update.cmake", cmake)

    def test_component_rejects_existing_unknown_role_and_writer(self):
        # Run the real component configure gate with resolved values, without
        # invoking IDF or mutating a generated sdkconfig.
        defaults = ROOT / "components/esp_mosaico_app_recovery/sdkconfig.defaults"
        lines = []
        for line in defaults.read_text().splitlines():
            if line.startswith("CONFIG_"):
                key, value = line.split("=", 1)
                lines.append(f"set({key} {value})")
        with tempfile.TemporaryDirectory() as temporary:
            script = Path(temporary) / "check.cmake"
            for extra, accepted in (("", True), ("set(CONFIG_ESP_IRIS_FIRMWARE_ROLE 0)", False),
                                    ("set(CONFIG_ESP_IRIS_OTA y)", False)):
                script.write_text("function(idf_component_register)\nendfunction()\n" +
                                  "\n".join(lines) + "\n" + extra + "\n" +
                                  f'include("{(defaults.parent / "CMakeLists.txt").as_posix()}")\n')
                result = subprocess.run([shutil.which("cmake"), "-P", str(script)],
                                        text=True, capture_output=True)
                self.assertEqual(result.returncode == 0, accepted, result.stderr)
                if not accepted:
                    self.assertIn("Mosaico application", result.stderr)
