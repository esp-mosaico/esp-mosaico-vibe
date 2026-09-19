"""Check the product contract across independently built firmware projects."""
import ast
import csv
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "submodule/esp-mosaico-utils/esp-mosaico-recovery"
LAYOUT_ID = "mosaico-retained-recovery-2m-v1"
NORMAL_FIRMWARE_PROJECTS = tuple(sorted((ROOT / "projects").glob("*/sdkconfig.application.defaults"))) + (
    ROOT / "tests/firmware/iris_acceptance/sdkconfig.application.defaults",
    ROOT / "tests/firmware/iris_crash/sdkconfig.application.defaults",
)
NORMAL_FIRMWARE_PROJECTS = tuple(path.parent for path in NORMAL_FIRMWARE_PROJECTS)
USER_EXAMPLE_PROJECTS = (
    ROOT / "projects/hello_world",
)


def partitions(path):
    with path.open(encoding="utf-8") as source:
        return {
            row[0].strip(): (
                row[1].strip(), row[2].strip(), int(row[3], 0),
                int(row[4], 0), row[5].strip() if len(row) > 5 else "",
            )
            for row in csv.reader(line for line in source if not line.startswith("#"))
            if row
        }


def defaults(path):
    return dict(line.split("=", 1) for line in path.read_text().splitlines()
                if line.startswith("CONFIG_") and "=" in line)


class RetainedRecoveryContractTests(unittest.TestCase):
    def test_normal_app_reads_update_result_from_recovery_sysmeta(self):
        source = (ROOT / "components/esp_mosaico_app_recovery/iris_ota_support.c").read_text(
            encoding="utf-8"
        )
        self.assertIn('#define SYSTEM_METADATA_PARTITION MOSAICO_SYSMETA_PARTITION', source)
        self.assertIn(
            "nvs_open_from_partition(SYSTEM_METADATA_PARTITION,\n"
            "                                SYSTEM_UPDATE_NAMESPACE, NVS_READONLY",
            source,
        )
        self.assertIn("nvs_flash_init_partition(SYSTEM_METADATA_PARTITION)", source)

    def test_user_example_system_updates_preserve_recovery_bootloader(self):
        system_update_rule = (ROOT / "cmake/system_update.cmake").read_text(
            encoding="utf-8"
        )
        self.assertNotIn("--bootloader", system_update_rule)
        self.assertNotIn("app bootloader", system_update_rule)
        for project in USER_EXAMPLE_PROJECTS:
            with self.subTest(project=project.name):
                cmake = (project / "CMakeLists.txt").read_text(encoding="utf-8")
                self.assertIn("include(../../cmake/system_update.cmake)", cmake)

    def test_user_examples_persist_core_dump_and_pre_crash_logs(self):
        for project in USER_EXAMPLE_PROJECTS:
            with self.subTest(project=project.name):
                config = defaults(project / "sdkconfig.defaults")
                self.assertEqual(config["CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH"], "y")
                # Both storage choices are Core Dump sections. Low-internal-
                # RAM normal apps use PSRAM; Recovery retains internal logs.
                if config.get("CONFIG_ESP_IRIS_LOG_RING_STORAGE_PSRAM") == "y":
                    self.assertEqual(config["CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY"], "y")
                else:
                    self.assertEqual(config["CONFIG_ESP_IRIS_LOG_RING_STORAGE_INTERNAL"], "y")

    def test_fixed_prefix_and_acceptance_layout_match_recovery(self):
        recovery = partitions(TOOLS / "firmware/recovery/partitions.csv")
        self.assertEqual(recovery["factory"], ("app", "factory", 0x20000, 0x1C0000, ""))
        self.assertEqual(recovery["coredump"], ("data", "coredump", 0x1E0000, 0x20000, ""))
        for project in NORMAL_FIRMWARE_PROJECTS:
            with self.subTest(project=project.name):
                actual = partitions(project / "partitions.csv")
                for label in ("otadata", "phy_init", "sysmeta", "factory", "coredump"):
                    self.assertEqual(actual[label], recovery[label])
                for label, entry in actual.items():
                    if label not in ("otadata", "phy_init", "sysmeta", "factory", "coredump"):
                        self.assertGreaterEqual(entry[2], 0x200000)
                if project.name in ("iris_acceptance", "iris_crash"):
                    self.assertEqual(actual, recovery)

    def test_firmware_identity_matches_host_expectation(self):
        tree = ast.parse((TOOLS.parent / "mosaico-tools/tools/mosaico_cli/product_contract.py").read_text(encoding="utf-8"))
        expectations = [ast.literal_eval(node) for node in ast.walk(tree)
                        if isinstance(node, ast.Dict)
                        and any(isinstance(key, ast.Constant) and key.value == "layout_id"
                                for key in node.keys)]
        self.assertEqual(len(expectations), 1)
        expectation = expectations[0]
        self.assertEqual(expectation["layout_id"], LAYOUT_ID)
        configs = [(project / "sdkconfig.application.defaults", 1)
                   for project in NORMAL_FIRMWARE_PROJECTS]
        configs.append((TOOLS / "firmware/recovery/sdkconfig.recovery.defaults", 2))
        for path, role in configs:
            with self.subTest(config=str(path)):
                config = defaults(ROOT / "components/esp_mosaico_app_recovery/sdkconfig.defaults") if role == 1 else {}
                config.update(defaults(path))
                if role == 1:
                    self.assertIn("mosaico_application.cmake", (path.parent / "CMakeLists.txt").read_text())
                self.assertEqual(int(config["CONFIG_ESP_IRIS_FIRMWARE_ROLE"]), role)
                for key in ("product_contract", "board_id", "layout_id", "recovery_abi"):
                    self.assertEqual(ast.literal_eval(config["CONFIG_ESP_IRIS_" + key.upper()]),
                                     expectation[key])
                if role == 1:
                    self.assertEqual(config["CONFIG_ESP_IRIS_OTA_DEFAULT_VIA_RECOVERY"], "y")
                    self.assertNotEqual(config.get("CONFIG_ESP_IRIS_OTA"), "y")


if __name__ == "__main__":
    unittest.main()
