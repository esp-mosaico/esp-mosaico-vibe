# SPDX-License-Identifier: Apache-2.0

import pathlib
import shutil
import subprocess
import tempfile
import unittest


ROOT = pathlib.Path(__file__).parents[1]
RECOVERY = (
    ROOT
    / "submodule/esp-mosaico-utils/esp-mosaico-recovery/firmware/recovery"
)
BSP_DISPLAY = (
    ROOT
    / "submodule/esp-mosaico-bsp/components/esp-mosaico-bsp/onboard/display.c"
)


class BootSplashContractTest(unittest.TestCase):
    def test_recovery_bootloader_paints_after_hardware_init(self) -> None:
        source = (RECOVERY / "bootloader_components/main/bootloader_start.c").read_text(
            encoding="utf-8"
        )
        self.assertLess(source.index("bootloader_init()"), source.index("mosaico_boot_splash_show()"))
        self.assertLess(
            source.index("mosaico_boot_splash_show()"),
            source.index("select_partition_number(&bs)"),
        )

    def test_bsp_adopts_handoff_with_cold_initialization_fallback(self) -> None:
        source = BSP_DISPLAY.read_text(encoding="utf-8")
        self.assertIn("mosaico_boot_handoff_consume()", source)
        self.assertIn("s_handoff_init", source)
        self.assertIn("if (!boot_panel_ready) {", source)
        self.assertIn("ESP_GOTO_ON_ERROR(esp_lcd_panel_reset(s_panel)", source)
        self.assertIn("ESP_GOTO_ON_ERROR(esp_lcd_panel_disp_on_off(s_panel, true)", source)

    def test_splash_result_controls_handoff_and_stops_feedback(self) -> None:
        source = (RECOVERY / "bootloader_components/main/mosaico_boot_splash.c").read_text(
            encoding="utf-8"
        )
        # Exercise the actual entry point with host fakes for hardware access.
        # It is the final function in the source; do not duplicate its logic.
        entry = source[source.index("bool mosaico_boot_splash_show(void)"):]
        compiler = shutil.which("cc")
        self.assertIsNotNone(compiler, "a C compiler is required for the splash contract test")
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            (root / "splash_entry.inc").write_text(entry, encoding="utf-8")
            executable = root / "splash-contract"
            build = subprocess.run(
                [str(compiler), "-std=c11", "-Wall", "-Wextra", "-Werror",
                 "-I", str(root), str(ROOT / "tests/boot_splash_host/splash_contract.c"),
                 "-o", str(executable)],
                capture_output=True, text=True, timeout=60, check=False,
            )
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            run = subprocess.run(
                [str(executable)], capture_output=True, text=True, timeout=30, check=False,
            )
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_display_failure_is_nonfatal(self) -> None:
        boot_entry = (RECOVERY / "bootloader_components/main/bootloader_start.c").read_text(
            encoding="utf-8"
        )
        self.assertIn("(void)mosaico_boot_splash_show();", boot_entry)

    def test_bootloader_keeps_info_logging_enabled(self) -> None:
        defaults = (RECOVERY / "sdkconfig.defaults").read_text(encoding="utf-8")
        self.assertIn("CONFIG_BOOTLOADER_LOG_LEVEL_INFO=y", defaults)
        self.assertNotIn("CONFIG_BOOTLOADER_LOG_LEVEL_NONE=y", defaults)

    def test_partition_table_offset_stays_at_retained_contract(self) -> None:
        defaults = (RECOVERY / "sdkconfig.defaults").read_text(encoding="utf-8")
        partitions = (RECOVERY / "partitions.csv").read_text(encoding="utf-8")
        self.assertIn('CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"', defaults)
        self.assertIn("otadata,   data, ota,     0x9000", partitions)


if __name__ == "__main__":
    unittest.main()
