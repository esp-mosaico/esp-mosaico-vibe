// SPDX-License-Identifier: Apache-2.0
/* Revision selection and hardware ownership for the native game profile. */
#include "mosaico_hardware.h"
#include <stdbool.h>
#include <string.h>
#include "esp_board_manager_includes.h"
#include "esp_check.h"
#include "esp_efuse.h"
#include "esp_efuse_table.h"
#include "esp_log.h"
#include "gen_board_device_custom.h"
#include "soc/gpio_reg.h"
#include "soc/soc.h"

#define LCD_POWER_GPIO GPIO_NUM_60
#define CODEC_POWER_GPIO GPIO_NUM_56
#define POWER_SWITCH_GPIO GPIO_NUM_57
#define MOTOR_MAX_DUTY 1023U

static const char *TAG = "mosaico_hardware";
static bool s_v1_0;
static bool s_power_owned;

static esp_err_t configure_output(gpio_num_t pin, int level, gpio_mode_t mode)
{
    const gpio_config_t config = {
        .pin_bit_mask = BIT64(pin), .mode = mode,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_set_level(pin, level), TAG, "preset power output failed");
    ESP_RETURN_ON_ERROR(gpio_config(&config), TAG, "configure power output failed");
    return gpio_hold_dis(pin);
}

extern esp_board_periph_entry_t g_esp_board_periph_handles[];
extern esp_board_device_handle_t g_esp_board_device_handles[];
static int board_power_deinit(void *handle);

bool mosaico_hardware_is_v1_0(void)
{
    return s_v1_0;
}

static esp_err_t revision_i2c_init(void *config, int size, void **out)
{
    if (!config || size != sizeof(i2c_master_bus_config_t)) return ESP_ERR_INVALID_ARG;
    i2c_master_bus_config_t active = *(const i2c_master_bus_config_t *)config;
    active.sda_io_num = s_v1_0 ? GPIO_NUM_0 : GPIO_NUM_56;
    active.scl_io_num = s_v1_0 ? GPIO_NUM_1 : GPIO_NUM_3;
    return periph_i2c_init(&active, sizeof(active), out);
}

static esp_err_t revision_spi_init(void *config, int size, void **out)
{
    if (!config || size != sizeof(periph_spi_config_t)) return ESP_ERR_INVALID_ARG;
    periph_spi_config_t active = *(const periph_spi_config_t *)config;
    active.spi_bus_config.sclk_io_num = s_v1_0 ? GPIO_NUM_44 : GPIO_NUM_42;
    esp_err_t err = periph_spi_init(&active, sizeof(active), out);
    if (err != ESP_OK) return err;
    const gpio_num_t pins[] = {active.spi_bus_config.sclk_io_num,
        GPIO_NUM_50, GPIO_NUM_36, GPIO_NUM_51, GPIO_NUM_35, GPIO_NUM_9};
    for (size_t i = 0; i < sizeof(pins) / sizeof(pins[0]); ++i) {
        err = gpio_set_drive_capability(pins[i], GPIO_DRIVE_CAP_3);
        if (err != ESP_OK) {
            (void)periph_spi_deinit(*out);
            *out = NULL;
            return err;
        }
    }
    return ESP_OK;
}

esp_err_t mosaico_hardware_prepare(void)
{
    uint16_t version = 0;
    ESP_RETURN_ON_ERROR(esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA, &version,
                                                sizeof(version) * 8U),
                        TAG, "read hardware revision failed");
    switch (version) {
    case 0x0100: s_v1_0 = true; break;
    case 0x0101:
    case 0x0102: s_v1_0 = false; break;
    default:
        ESP_LOGE(TAG, "unsupported hardware revision 0x%04x", version);
        return ESP_ERR_NOT_SUPPORTED;
    }
    /* Generated configurations are const. Adapt copies in peripheral factories
     * rather than writing revision-dependent pins into read-only descriptors. */
    for (esp_board_periph_entry_t *entry = g_esp_board_periph_handles; entry; entry = entry->next) {
        if (!strcmp(entry->type, "i2c")) entry->init = revision_i2c_init;
        if (!strcmp(entry->type, "spi")) entry->init = revision_spi_init;
    }
    /* The generic custom-device deinit logs and discards errors in BM 0.7.x.
     * Use our actual deinit callbacks so ownership is retained on failure. */
    for (esp_board_device_handle_t *entry = g_esp_board_device_handles; entry; entry = entry->next) {
        if (!strcmp(entry->name, "board_power")) entry->deinit = board_power_deinit;
        if (!strcmp(entry->name, "imu_sensor")) entry->deinit = mosaico_hardware_imu_deinit;
    }
    ESP_RETURN_ON_ERROR(gpio_set_level(LCD_POWER_GPIO, 0), TAG, "preset LCD rail failed");
    ESP_RETURN_ON_ERROR(gpio_hold_dis(LCD_POWER_GPIO), TAG, "release LCD rail hold failed");
    ESP_LOGI(TAG, "Board Manager hardware revision v%u.%u", version >> 8, version & 255);
    return ESP_OK;
}

static int board_power_init(void *config, int size, void **out)
{
    (void)config;
    (void)size;
    if (!out) return ESP_ERR_INVALID_ARG;
    /* The codec rail on GPIO56 exists only on v1.0; newer boards use that pin
     * for I2C. The shutdown line must always remain released. */
    esp_err_t err = configure_output(POWER_SWITCH_GPIO, 1, GPIO_MODE_OUTPUT_OD);
    if (err != ESP_OK) return err;
    s_power_owned = true;
    if (s_v1_0) {
        err = configure_output(CODEC_POWER_GPIO, 1, GPIO_MODE_OUTPUT);
        if (err != ESP_OK) {
            (void)mosaico_hardware_release();
            return err;
        }
    }
    *out = &s_power_owned;
    return ESP_OK;
}

esp_err_t mosaico_hardware_release(void)
{
    if (!s_power_owned) return ESP_OK;
    if (s_v1_0) {
        ESP_RETURN_ON_ERROR(gpio_set_level(CODEC_POWER_GPIO, 0), TAG, "disable codec rail failed");
        ESP_RETURN_ON_ERROR(gpio_reset_pin(CODEC_POWER_GPIO), TAG, "release codec rail failed");
    }
    /* Never assert the shutdown pin as part of a game's teardown. */
    ESP_RETURN_ON_ERROR(gpio_set_level(POWER_SWITCH_GPIO, 1), TAG, "release shutdown failed");
    s_power_owned = false;
    return ESP_OK;
}

static int board_power_deinit(void *handle)
{
    if (handle != &s_power_owned) return ESP_ERR_INVALID_ARG;
    return mosaico_hardware_release();
}

CUSTOM_DEVICE_IMPLEMENT(board_power, board_power_init, board_power_deinit);

esp_err_t mosaico_hardware_haptic_set(uint8_t strength)
{
    if (strength > 100) return ESP_ERR_INVALID_ARG;
    periph_ledc_handle_t *motor = NULL;
    ESP_RETURN_ON_ERROR(esp_board_manager_get_device_handle("vibration_motor", (void **)&motor),
                        TAG, "get motor handle failed");
    if (!motor) return ESP_ERR_INVALID_STATE;
    return ledc_set_duty_and_update(motor->speed_mode, motor->channel,
                                   strength * MOTOR_MAX_DUTY / 100U, 0);
}
