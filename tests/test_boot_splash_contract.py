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
BSP_BOARD = (
    ROOT
    / "submodule/esp-mosaico-bsp/components/esp-mosaico-bsp/onboard/esp_mosaico.c"
)
BSP_HEADER = (
    ROOT
    / "submodule/esp-mosaico-bsp/components/esp-mosaico-bsp/include/bsp/esp_mosaico.h"
)
BSP_HANDOFF = (
    ROOT
    / "submodule/esp-mosaico-bsp/components/mosaico_boot_splash/include/mosaico_boot_handoff.h"
)
RECOVERY_HANDOFF = RECOVERY / "bootloader_components/main/mosaico_boot_handoff.h"


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

    def test_bootloader_publishes_handoff_only_after_complete_splash(self) -> None:
        boot = (RECOVERY / "bootloader_components/main/mosaico_boot_splash.c").read_text(
            encoding="utf-8"
        )
        entry = boot.split("bool mosaico_boot_splash_show(void)", 1)[1]
        self.assertLess(entry.index("mosaico_boot_handoff_clear()"), entry.index("panel_init()"))
        self.assertLess(entry.index("draw_splash()"), entry.index("mosaico_boot_handoff_publish()"))
        self.assertLess(entry.index("mosaico_boot_handoff_publish()"), entry.index("return true"))

    def test_motor_feedback_precedes_display_and_always_stops(self) -> None:
        boot = (RECOVERY / "bootloader_components/main/mosaico_boot_splash.c").read_text(
            encoding="utf-8"
        )
        entry = boot.split("bool mosaico_boot_splash_show(void)", 1)[1]
        panel = boot.split("static bool panel_init(void)", 1)[1].split(
            "static bool lcd_write_solid", 1
        )[0]
        self.assertLess(entry.index("hardware_version_supported()"), entry.index("boot_feedback_start()"))
        self.assertLess(entry.index("boot_feedback_start()"), entry.index("panel_init()"))
        self.assertIn("BOOT_MOTOR_PULSE_US 60000U", boot)
        self.assertIn("boot_feedback_stop();", panel)
        self.assertLess(entry.index("boot_feedback_stop()"), entry.index("if (!splash_visible)"))

    def test_bootloader_motor_electrical_contract_matches_bsp(self) -> None:
        boot = (RECOVERY / "bootloader_components/main/mosaico_boot_splash.c").read_text(
            encoding="utf-8"
        )
        bsp = BSP_HEADER.read_text(encoding="utf-8")
        for boot_definition, bsp_definition in (
            ("BOOT_MOTOR_GPIO 8", "BSP_MOTOR_GPIO            GPIO_NUM_8"),
            ("BOOT_MOTOR_ON_LEVEL 1", "BSP_MOTOR_ON_LEVEL        1"),
            ("BOOT_MOTOR_OFF_LEVEL 0", "BSP_MOTOR_OFF_LEVEL       0"),
        ):
            self.assertIn(boot_definition, boot)
            self.assertIn(bsp_definition, bsp)

    def test_bsp_adopts_handoff_and_keeps_cold_boot_fallback(self) -> None:
        source = BSP_DISPLAY.read_text(encoding="utf-8")
        self.assertIn("mosaico_boot_handoff_consume()", source)
        self.assertIn("s_handoff_init", source)
        self.assertIn("if (!boot_panel_ready) {", source)
        self.assertIn("esp_lcd_panel_reset(s_panel)", source)
        self.assertIn("esp_lcd_panel_disp_on_off(s_panel, true)", source)
        handoff = source.split("static const co5300_lcd_init_cmd_t s_handoff_init[]", 1)[1]
        handoff = handoff.split("};", 1)[0]
        self.assertNotIn("{0x11", handoff)
        self.assertNotIn("{0x29", handoff)

    def test_handoff_abi_matches_across_repositories(self) -> None:
        bsp = BSP_HANDOFF.read_text(encoding="utf-8")
        recovery = RECOVERY_HANDOFF.read_text(encoding="utf-8")
        for definition in (
            "LP_SYSTEM_REG_LP_STORE15_REG",
            'UINT32_C(0x4D4C4344) /* "MLCD" */',
        ):
            self.assertIn(definition, bsp)
            self.assertIn(definition, recovery)

    def test_efuse_revision_matrix_and_lcd_pin_swap_match(self) -> None:
        boot = (RECOVERY / "bootloader_components/main/mosaico_boot_splash.c").read_text(
            encoding="utf-8"
        )
        board = BSP_BOARD.read_text(encoding="utf-8")
        display = BSP_DISPLAY.read_text(encoding="utf-8")
        for version in ("(1, 0)", "(1, 1)", "(1, 2)"):
            self.assertIn("MOSAICO_HW_VERSION" + version, boot)
            self.assertIn("BSP_HW_VERSION" + version, board)
        self.assertIn("esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA", boot)
        self.assertIn("esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA", board)
        self.assertIn("LCD_RESET_GPIO_V1_0 42", boot)
        self.assertIn("LCD_CLK_GPIO_V1_0 44", boot)
        self.assertIn("LCD_RESET_GPIO_V1_2 44", boot)
        self.assertIn("LCD_CLK_GPIO_V1_2 42", boot)
        self.assertIn("BSP_LCD_RST_V1_0", display)
        self.assertIn("BSP_LCD_RST_V1_2", display)
        self.assertIn("BSP_LCD_SCL_V1_0", display)
        self.assertIn("BSP_LCD_SCL_V1_2", display)

    def test_display_failure_is_nonfatal(self) -> None:
        source = (RECOVERY / "bootloader_components/main/mosaico_boot_splash.c").read_text(
            encoding="utf-8"
        )
        entry = source.split("bool mosaico_boot_splash_show(void)", 1)[1]
        self.assertIn("const bool splash_visible = panel_init() && draw_splash();", entry)
        self.assertRegex(entry, r"if \(!splash_visible\) \{[^}]*return false;\s*\}[^}]*return true;")
        boot_entry = (RECOVERY / "bootloader_components/main/bootloader_start.c").read_text(
            encoding="utf-8"
        )
        self.assertIn("(void)mosaico_boot_splash_show();", boot_entry)

    def test_bootloader_uses_size_constrained_logging(self) -> None:
        defaults = (RECOVERY / "sdkconfig.defaults").read_text(encoding="utf-8")
        self.assertIn("CONFIG_BOOTLOADER_LOG_LEVEL_NONE=y", defaults)

    def test_partition_table_offset_stays_at_retained_contract(self) -> None:
        defaults = (RECOVERY / "sdkconfig.defaults").read_text(encoding="utf-8")
        partitions = (RECOVERY / "partitions.csv").read_text(encoding="utf-8")
        self.assertIn('CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"', defaults)
        self.assertIn("otadata,   data, ota,     0x9000", partitions)


if __name__ == "__main__":
    unittest.main()
