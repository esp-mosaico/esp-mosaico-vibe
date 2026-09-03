// SPDX-License-Identifier: Apache-2.0

#include "esp_err.h"
#include "esp_log.h"
#include "factory_network.h"
#include "factory_system_inventory.h"
#include "factory_system_metadata.h"
#include "factory_system_update.h"
#include "factory_ui.h"
#include "recovery_ota_support.h"
#include "iris_screen_mirror.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

static const char *TAG = "factory";

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(factory_system_metadata_init());

    /* Make the retained USB OTA writer reachable before display and network
     * initialization. Inventory and system-update providers must be
     * registered before esp_iris_start(); the screen backend may be attached
     * after the transport is running. */
    ESP_ERROR_CHECK(factory_system_inventory_register());
    ESP_ERROR_CHECK(factory_system_update_register());
    recovery_ota_support_start();

    ESP_ERROR_CHECK(factory_ui_start());
    ESP_ERROR_CHECK(iris_screen_mirror_register());

#if CONFIG_IRIS_FACTORY_NAND_SYSTEM_UPDATE && \
    CONFIG_IRIS_FACTORY_NAND_SYSTEM_UPDATE_AUTO_START
    if (CONFIG_IRIS_FACTORY_NAND_SYSTEM_UPDATE_MANIFEST_PATH[0] != '\0') {
        const esp_err_t update_err = factory_system_update_start_nand(
            CONFIG_IRIS_FACTORY_NAND_SYSTEM_UPDATE_MANIFEST_PATH);
        if (update_err != ESP_OK) {
            ESP_LOGE(TAG, "Could not start configured NAND system update: %s",
                     esp_err_to_name(update_err));
        }
    }
#endif
    const esp_err_t network_err = factory_network_start();
    if (network_err != ESP_OK) {
        ESP_LOGE(TAG, "Factory network unavailable; USB recovery remains active: %s",
                 esp_err_to_name(network_err));
    }
#if CONFIG_IRIS_FACTORY_HTTP_SYSTEM_UPDATE && \
    CONFIG_IRIS_FACTORY_HTTP_SYSTEM_UPDATE_AUTO_START
    if (network_err == ESP_OK &&
        CONFIG_IRIS_FACTORY_HTTP_SYSTEM_UPDATE_MANIFEST_URL[0] != '\0') {
        const esp_err_t update_err = factory_system_update_start_http(
            CONFIG_IRIS_FACTORY_HTTP_SYSTEM_UPDATE_MANIFEST_URL);
        if (update_err != ESP_OK) {
            ESP_LOGE(TAG, "Could not start configured HTTP system update: %s",
                     esp_err_to_name(update_err));
        }
    }
#endif

    ESP_LOGI(TAG, "ESP-Mosaico factory recovery firmware is ready");
}
