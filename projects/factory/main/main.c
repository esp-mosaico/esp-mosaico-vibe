// SPDX-License-Identifier: Apache-2.0

#include "esp_err.h"
#include "esp_log.h"
#include "factory_network.h"
#include "factory_system_inventory.h"
#include "factory_system_metadata.h"
#include "factory_system_update.h"
#include "factory_ui.h"
#include "iris_ota_support.h"
#include "iris_screen_mirror.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

static const char *TAG = "factory";

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(factory_system_metadata_init());

#if CONFIG_GET_STARTED_RECOVERY
    /* Make the retained USB OTA writer reachable before display and network
     * initialization. Inventory and system-update providers must be
     * registered before esp_iris_start(); the screen backend may be attached
     * after the transport is running. */
    ESP_ERROR_CHECK(factory_system_inventory_register());
    ESP_ERROR_CHECK(factory_system_update_register());
    iris_ota_support_start();

    ESP_ERROR_CHECK(factory_ui_start());
    ESP_ERROR_CHECK(iris_screen_mirror_register());
#else
    /* Normal firmware becomes reachable only after its product services and
     * UI are initialized, so OTA health validation still represents a fully
     * started application. */
    ESP_ERROR_CHECK(factory_ui_start());
    ESP_ERROR_CHECK(iris_screen_mirror_register());
    ESP_ERROR_CHECK(factory_system_inventory_register());
    ESP_ERROR_CHECK(factory_system_update_register());
    iris_ota_support_start();
#endif

#if CONFIG_GET_STARTED_RECOVERY
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
#endif

    ESP_LOGI(TAG, "ESP-Mosaico %s firmware is ready",
#if CONFIG_GET_STARTED_RECOVERY
             "factory recovery"
#else
             "application"
#endif
    );
}
