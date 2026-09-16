# SPDX-License-Identifier: Apache-2.0

import pathlib
import re
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
HANDOFF_MAGIC = "0x4D4C4344"


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

    def test_handoff_magic_matches_bootloader_and_bsp(self) -> None:
        boot = (RECOVERY / "bootloader_components/main/mosaico_boot_splash.c").read_text(
            encoding="utf-8"
        )
        bsp = BSP_DISPLAY.read_text(encoding="utf-8")
        for source in (boot, bsp):
            match = re.search(r"MOSAICO_BOOT_LCD_HANDOFF_MAGIC\s+UINT32_C\((0x[0-9A-F]+)\)", source)
            self.assertIsNotNone(match)
            self.assertEqual(match.group(1), HANDOFF_MAGIC)

    def test_bsp_preserves_panel_only_for_valid_handoff(self) -> None:
        source = BSP_DISPLAY.read_text(encoding="utf-8")
        self.assertIn("consume_bootloader_handoff()", source)
        self.assertIn("bootloader_panel_ready ? s_handoff_init : s_vendor_init", source)
        self.assertRegex(
            source,
            r"if \(!bootloader_panel_ready\) \{\s*ESP_GOTO_ON_ERROR\(esp_lcd_panel_reset",
        )
        self.assertRegex(
            source,
            r"if \(!bootloader_panel_ready\) \{\s*ESP_GOTO_ON_ERROR\(esp_lcd_panel_disp_on_off",
        )

    def test_handoff_is_published_only_after_successful_draw(self) -> None:
        source = (RECOVERY / "bootloader_components/main/mosaico_boot_splash.c").read_text(
            encoding="utf-8"
        )
        entry = source.split("bool mosaico_boot_splash_show(void)", 1)[1]
        self.assertLess(entry.index("handoff_clear();"), entry.index("hardware_version_supported()"))
        self.assertRegex(
            entry,
            r"if \(!panel_init\(\) \|\| !draw_splash\(\)\) \{\s*handoff_clear\(\);"
            r"[^}]*return false;\s*\}\s*handoff_publish\(\);",
        )

    def test_handoff_does_not_repeat_sleep_out_brightness_or_display_on(self) -> None:
        source = BSP_DISPLAY.read_text(encoding="utf-8")
        commands = source.split("s_handoff_init[] = {", 1)[1].split("};", 1)[0]
        for command in ("0x11", "0x51", "0x29"):
            self.assertNotIn(command, commands)

    def test_partition_table_offset_stays_at_retained_contract(self) -> None:
        defaults = (RECOVERY / "sdkconfig.defaults").read_text(encoding="utf-8")
        partitions = (RECOVERY / "partitions.csv").read_text(encoding="utf-8")
        self.assertIn('CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"', defaults)
        self.assertIn("otadata,   data, ota,     0x9000", partitions)


if __name__ == "__main__":
    unittest.main()
