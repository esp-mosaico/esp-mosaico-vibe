import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
FIXTURE = ROOT / "tests" / "firmware" / "iris_crash"


class IrisCrashFixtureTests(unittest.TestCase):
    def test_fixture_preserves_recovery_and_enables_flash_coredumps(self) -> None:
        partitions = (FIXTURE / "partitions.csv").read_text(encoding="utf-8")
        defaults = (FIXTURE / "sdkconfig.defaults").read_text(encoding="utf-8")
        application = (
            FIXTURE / "sdkconfig.application.defaults"
        ).read_text(encoding="utf-8")

        self.assertIn("factory,   app,  factory, 0x20000,  0x1c0000", partitions)
        self.assertIn("coredump,  data, coredump,0x1e0000, 0x20000", partitions)
        self.assertIn("CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH=y", defaults)
        self.assertIn("CONFIG_ESP_TASK_WDT_PANIC=y", defaults)
        self.assertIn("CONFIG_ESP_IRIS_OTA_DEFAULT_VIA_RECOVERY=y", application)
        self.assertIn("# CONFIG_ESP_IRIS_OTA is not set", application)

    def test_fixture_covers_requested_crash_matrix(self) -> None:
        source = (FIXTURE / "main" / "main.c").read_text(encoding="utf-8")
        for behavior in (
            "ACTION_ASSERT",
            "ACTION_ILLEGAL_ACCESS",
            "ACTION_TASK_WDT",
            "ACTION_RESTART",
            "STARTUP_CRASH_COUNT",
            "esp_iris_crash_loop_reset",
        ):
            self.assertIn(behavior, source)


if __name__ == "__main__":
    unittest.main()
