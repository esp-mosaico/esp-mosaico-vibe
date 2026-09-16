# SPDX-License-Identifier: Apache-2.0

import pathlib
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

    def test_bootloader_does_not_publish_cross_stage_handoff(self) -> None:
        boot = (RECOVERY / "bootloader_components/main/mosaico_boot_splash.c").read_text(
            encoding="utf-8"
        )
        self.assertNotIn("HANDOFF", boot)
        self.assertNotIn("LP_STORE", boot)
        self.assertNotIn("REG_WRITE", boot)

    def test_bsp_keeps_its_original_full_initialization(self) -> None:
        source = BSP_DISPLAY.read_text(encoding="utf-8")
        self.assertNotIn("bootloader_panel_ready", source)
        self.assertNotIn("s_handoff_init", source)
        self.assertIn("ESP_GOTO_ON_ERROR(esp_lcd_panel_reset(s_panel)", source)
        self.assertIn("ESP_GOTO_ON_ERROR(esp_lcd_panel_disp_on_off(s_panel, true)", source)

    def test_display_failure_is_nonfatal(self) -> None:
        source = (RECOVERY / "bootloader_components/main/mosaico_boot_splash.c").read_text(
            encoding="utf-8"
        )
        entry = source.split("bool mosaico_boot_splash_show(void)", 1)[1]
        self.assertRegex(
            entry,
            r"if \(!panel_init\(\) \|\| !draw_splash\(\)\) \{"
            r"[^}]*return false;\s*\}[^}]*return true;",
        )
        boot_entry = (RECOVERY / "bootloader_components/main/bootloader_start.c").read_text(
            encoding="utf-8"
        )
        self.assertIn("(void)mosaico_boot_splash_show();", boot_entry)

    def test_bootloader_uses_size_constrained_logging(self) -> None:
        defaults = (RECOVERY / "sdkconfig.defaults").read_text(encoding="utf-8")
        self.assertIn("CONFIG_BOOTLOADER_LOG_LEVEL_ERROR=y", defaults)

    def test_partition_table_offset_stays_at_retained_contract(self) -> None:
        defaults = (RECOVERY / "sdkconfig.defaults").read_text(encoding="utf-8")
        partitions = (RECOVERY / "partitions.csv").read_text(encoding="utf-8")
        self.assertIn('CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"', defaults)
        self.assertIn("otadata,   data, ota,     0x9000", partitions)


if __name__ == "__main__":
    unittest.main()
