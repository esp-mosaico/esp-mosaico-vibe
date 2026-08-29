/*
 * SPDX-FileCopyrightText: 2015-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdbool.h>
#include <inttypes.h>
#include <sys/reent.h>

#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_rom_caps.h"
#include "esp_rom_gpio.h"
#include "esp_rom_sys.h"
#include "bootloader_init.h"
#include "bootloader_utility.h"
#include "bootloader_common.h"
#include "hal/gpio_ll.h"
#include "soc/gpio_struct.h"
#include "soc/soc_caps.h"

ESP_LOG_ATTR_TAG(TAG, "boot");

static int select_partition_number(bootloader_state_t *bs);
static int selected_boot_partition(const bootloader_state_t *bs);

static bool factory_recovery_requested(void)
{
    const uint32_t pin = CONFIG_FACTORY_RECOVERY_BOOT_GPIO;

    if (((1ULL << pin) & SOC_GPIO_VALID_GPIO_MASK) == 0) {
        ESP_LOGE(TAG, "Factory recovery GPIO %" PRIu32 " is not a valid input", pin);
        return false;
    }

    esp_rom_gpio_pad_select_gpio(pin);
    gpio_ll_input_enable(&GPIO, pin);
    esp_rom_gpio_pad_pullup_only(pin);

    if (gpio_ll_get_level(&GPIO, pin) != 0) {
        return false;
    }

    const uint32_t started_ms = esp_log_early_timestamp();
    while ((esp_log_early_timestamp() - started_ms) <
           CONFIG_FACTORY_RECOVERY_BOOT_DEBOUNCE_MS) {
        if (gpio_ll_get_level(&GPIO, pin) != 0) {
            ESP_LOGI(TAG, "Ignoring short low pulse on factory recovery GPIO %" PRIu32, pin);
            return false;
        }
    }

    return true;
}

/*
 * We arrive here after the ROM bootloader has loaded this second-stage
 * bootloader from flash. This remains aligned with ESP-IDF's default
 * bootloader entry flow; only partition selection is extended below.
 */
void __attribute__((noreturn)) call_start_cpu0(void)
{
    if (bootloader_init() != ESP_OK) {
        bootloader_reset();
    }

#ifdef CONFIG_BOOTLOADER_SKIP_VALIDATE_IN_DEEP_SLEEP
    bootloader_utility_load_boot_image_from_deep_sleep();
#endif

    bootloader_state_t bs = {0};
    int boot_index = select_partition_number(&bs);
    if (boot_index == INVALID_INDEX) {
        bootloader_reset();
    }

#if CONFIG_SECURE_ENABLE_TEE
    bootloader_utility_load_tee_image(&bs);
#endif

    bootloader_utility_load_boot_image(&bs, boot_index);
}

static int select_partition_number(bootloader_state_t *bs)
{
    if (!bootloader_utility_load_partition_table(bs)) {
        ESP_LOGE(TAG, "load partition table error!");
        return INVALID_INDEX;
    }

    if (factory_recovery_requested()) {
        if (bs->factory.offset != 0 && bs->factory.size != 0) {
            ESP_LOGW(TAG,
                     "GPIO%d held low; booting factory recovery without changing OTA data",
                     CONFIG_FACTORY_RECOVERY_BOOT_GPIO);
            return FACTORY_INDEX;
        }
        ESP_LOGE(TAG, "Factory recovery requested, but no factory partition exists");
    }

    return selected_boot_partition(bs);
}

/* Keep ESP-IDF's standard OTA, rollback, factory-reset, and test-app rules. */
static int selected_boot_partition(const bootloader_state_t *bs)
{
    int boot_index = bootloader_utility_get_selected_boot_partition(bs);
    if (boot_index == INVALID_INDEX) {
        return boot_index;
    }

    if (esp_rom_get_reset_reason(0) != RESET_REASON_CORE_DEEP_SLEEP) {
#ifdef CONFIG_BOOTLOADER_FACTORY_RESET
        bool reset_level = false;
#if CONFIG_BOOTLOADER_FACTORY_RESET_PIN_HIGH
        reset_level = true;
#endif
        if (bootloader_common_check_long_hold_gpio_level(
                CONFIG_BOOTLOADER_NUM_PIN_FACTORY_RESET,
                CONFIG_BOOTLOADER_HOLD_TIME_GPIO,
                reset_level) == GPIO_LONG_HOLD) {
            ESP_LOGI(TAG, "Detect a condition of the factory reset");
            bool ota_data_erase = false;
#ifdef CONFIG_BOOTLOADER_OTA_DATA_ERASE
            ota_data_erase = true;
#endif
            const char *list_erase = CONFIG_BOOTLOADER_DATA_FACTORY_RESET;
            ESP_LOGI(TAG, "Data partitions to erase: %s", list_erase);
            if (!bootloader_common_erase_part_type_data(list_erase, ota_data_erase)) {
                ESP_LOGE(TAG, "Not all partitions were erased");
            }
#ifdef CONFIG_BOOTLOADER_RESERVE_RTC_MEM
            bootloader_common_set_rtc_retain_mem_factory_reset_state();
#endif
            return bootloader_utility_get_selected_boot_partition(bs);
        }
#endif

#ifdef CONFIG_BOOTLOADER_APP_TEST
        bool app_test_level = false;
#if CONFIG_BOOTLOADER_APP_TEST_PIN_HIGH
        app_test_level = true;
#endif
        if (bootloader_common_check_long_hold_gpio_level(
                CONFIG_BOOTLOADER_NUM_PIN_APP_TEST,
                CONFIG_BOOTLOADER_HOLD_TIME_GPIO,
                app_test_level) == GPIO_LONG_HOLD) {
            ESP_LOGI(TAG, "Detect a boot condition of the test firmware");
            if (bs->test.offset != 0) {
                return TEST_APP_INDEX;
            }
            ESP_LOGE(TAG, "Test firmware is not found in partition table");
            return INVALID_INDEX;
        }
#endif
    }

    return boot_index;
}

#if CONFIG_LIBC_NEWLIB
struct _reent *__getreent(void)
{
    return _GLOBAL_REENT;
}
#endif
