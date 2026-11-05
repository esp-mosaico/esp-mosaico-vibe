// SPDX-License-Identifier: Apache-2.0
/* Board Manager panel factories. Full initialization remains the driver's
 * lifetime configuration; adoption skips only the first reset/init/on calls. */
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "mosaico_hardware.h"
#include "esp_lcd_co5300.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch_cst9220.h"
#include "soc/lp_system_reg.h"
#include "soc/soc.h"

#define HANDOFF_MAGIC UINT32_C(0x4D4C4344)
#define QSPI_COMMAND(cmd) ((0x02U << 24) | ((uint32_t)(cmd) << 8))

static const co5300_lcd_init_cmd_t s_vendor_init[] = {
    {0x11, NULL, 0, 600},
    {0xFE, (uint8_t[]){0x20}, 1, 0},
    {0x19, (uint8_t[]){0x10}, 1, 0},
    {0x1C, (uint8_t[]){0xA0}, 1, 0},
    {0xFE, (uint8_t[]){0x00}, 1, 0},
    {0xC4, (uint8_t[]){0x80}, 1, 0},
    {0x3A, (uint8_t[]){0x55}, 1, 0},
    {0x35, (uint8_t[]){0x00}, 1, 0},
    {0x53, (uint8_t[]){0x20}, 1, 0},
    {0x51, (uint8_t[]){0xFF}, 1, 0},
    {0x63, (uint8_t[]){0xFF}, 1, 0},
    {0x2A, (uint8_t[]){0x00, 0x00, 0x01, 0xDF}, 4, 0},
    {0x2B, (uint8_t[]){0x00, 0x00, 0x01, 0xDF}, 4, 0},
    {0x29, NULL, 0, 600},
};
static const co5300_vendor_config_t s_vendor = {
    .init_cmds = s_vendor_init,
    .init_cmds_size = sizeof(s_vendor_init) / sizeof(s_vendor_init[0]),
    .flags.use_qspi_interface = true,
};

/* Exactly one panel belongs to the selected native Board. The driver keeps
 * its original callbacks for later reset, deep-standby wake and deletion. */
static esp_lcd_panel_handle_t s_panel;
static esp_lcd_panel_io_handle_t s_io;
static esp_err_t (*s_reset)(esp_lcd_panel_t *);
static esp_err_t (*s_init)(esp_lcd_panel_t *);
static esp_err_t (*s_on)(esp_lcd_panel_t *, bool);
static esp_err_t (*s_del)(esp_lcd_panel_t *);
static bool s_skip_reset, s_adopt_init, s_skip_on;

static esp_err_t panel_reset(esp_lcd_panel_t *panel)
{
    if (s_skip_reset) {
        s_skip_reset = false;
        return ESP_OK;
    }
    s_adopt_init = false;
    s_skip_on = false;
    return s_reset(panel);
}

static esp_err_t panel_init(esp_lcd_panel_t *panel)
{
    if (!s_adopt_init) return s_init(panel);
    s_adopt_init = false;
    /* Keep brightness and GRAM. BM applies mirror/swap after this call, which
     * also updates the driver's cached MADCTL value. COLMOD is already RGB565. */
    for (size_t i = 0; i < sizeof(s_vendor_init) / sizeof(s_vendor_init[0]); ++i) {
        const co5300_lcd_init_cmd_t *cmd = &s_vendor_init[i];
        if (cmd->cmd == 0x11 || cmd->cmd == 0x29 || cmd->cmd == 0x51) continue;
        esp_err_t err = esp_lcd_panel_io_tx_param(s_io, QSPI_COMMAND(cmd->cmd),
                                                  cmd->data, cmd->data_bytes);
        if (err != ESP_OK) return err;
    }
    return ESP_OK;
}

static esp_err_t panel_on(esp_lcd_panel_t *panel, bool on)
{
    if (on && s_skip_on) {
        s_skip_on = false;
        return ESP_OK;
    }
    s_skip_on = false;
    return s_on(panel, on);
}

static esp_err_t panel_del(esp_lcd_panel_t *panel)
{
    esp_err_t err = s_del(panel);
    if (err == ESP_OK) {
        s_panel = NULL;
        s_io = NULL;
        s_skip_reset = s_adopt_init = s_skip_on = false;
    }
    return err;
}

esp_err_t lcd_panel_factory_entry_t(esp_lcd_panel_io_handle_t io,
    const esp_lcd_panel_dev_config_t *config, esp_lcd_panel_handle_t *out)
{
    if (!io || !config || !out) return ESP_ERR_INVALID_ARG;
    *out = NULL;
    if (s_panel) return ESP_ERR_INVALID_STATE;
    const bool ready = REG_READ(LP_SYSTEM_REG_LP_STORE15_REG) == HANDOFF_MAGIC;
    REG_WRITE(LP_SYSTEM_REG_LP_STORE15_REG, 0);
    esp_lcd_panel_dev_config_t active = *config;
    active.reset_gpio_num = mosaico_hardware_is_v1_0() ? 42 : 44;
    active.vendor_config = (void *)&s_vendor;
    esp_err_t err = esp_lcd_new_panel_co5300(io, &active, out);
    if (err != ESP_OK) return err;
    s_panel = *out;
    s_io = io;
    s_reset = s_panel->reset;
    s_init = s_panel->init;
    s_on = s_panel->disp_on_off;
    s_del = s_panel->del;
    s_skip_reset = s_adopt_init = s_skip_on = ready;
    s_panel->reset = panel_reset;
    s_panel->init = panel_init;
    s_panel->disp_on_off = panel_on;
    s_panel->del = panel_del;
    return ESP_OK;
}

esp_err_t lcd_panel_set_brightness_entry_t(esp_lcd_panel_handle_t panel, uint8_t percent)
{
    return esp_lcd_panel_co5300_set_brightness(panel, percent);
}

esp_err_t lcd_touch_factory_entry_t(esp_lcd_panel_io_handle_t io,
    const esp_lcd_touch_config_t *config, esp_lcd_touch_handle_t *out)
{
    return esp_lcd_touch_new_i2c_cst9220(io, config, out);
}
