"""Check the product contract across independently built firmware projects."""
import ast
import csv
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "submodule/esp-mosaico-tools"
LAYOUT_ID = "mosaico-retained-recovery-2m-v1"
NORMAL_FIRMWARE_PROJECTS = (
    ROOT / "projects/hello_world",
    ROOT / "tests/firmware/iris_acceptance",
    ROOT / "projects/gsp_hello",
)
USER_EXAMPLE_PROJECTS = (
    ROOT / "projects/hello_world",
    ROOT / "projects/gsp_hello",
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
    def test_user_examples_persist_core_dump_and_pre_crash_logs(self):
        for project in USER_EXAMPLE_PROJECTS:
            with self.subTest(project=project.name):
                config = defaults(project / "sdkconfig.defaults")
                self.assertEqual(config["CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH"], "y")
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
                if project.name != "gsp_hello":
                    self.assertEqual(actual, recovery)

    def test_firmware_identity_matches_host_expectation(self):
        tree = ast.parse((TOOLS / "tools/mosaico_cli/gateway.py").read_text(encoding="utf-8"))
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
                config = defaults(path)
                self.assertEqual(int(config["CONFIG_ESP_IRIS_FIRMWARE_ROLE"]), role)
                for key in ("product_contract", "board_id", "layout_id", "recovery_abi"):
                    self.assertEqual(ast.literal_eval(config["CONFIG_ESP_IRIS_" + key.upper()]),
                                     expectation[key])
                if role == 1:
                    self.assertEqual(config["CONFIG_ESP_IRIS_OTA_DEFAULT_VIA_RECOVERY"], "y")
                    self.assertNotEqual(config.get("CONFIG_ESP_IRIS_OTA"), "y")


if __name__ == "__main__":
    unittest.main()
