"""Check the effective CMake contract gate without generating game templates."""
import subprocess
import tempfile
import unittest
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[1]


class ApplicationContractTests(unittest.TestCase):
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
