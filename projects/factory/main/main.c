// SPDX-License-Identifier: Apache-2.0

#include "esp_err.h"
#include "esp_log.h"
#include "factory_network.h"
#include "factory_system_inventory.h"
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
    ESP_ERROR_CHECK(factory_ui_start());
    ESP_ERROR_CHECK(iris_screen_mirror_register());
    ESP_ERROR_CHECK(factory_system_inventory_register());
    ESP_ERROR_CHECK(factory_system_update_register());

    /* Register product RPCs and start ESP-Iris before Wi-Fi. USB recovery must
     * remain available even when credentials are absent or radio startup fails.
     */
    iris_ota_support_start();

#if CONFIG_GET_STARTED_RECOVERY
    const esp_err_t network_err = factory_network_start();
    if (network_err != ESP_OK) {
        ESP_LOGE(TAG, "Factory network unavailable; USB recovery remains active: %s",
                 esp_err_to_name(network_err));
    }
#endif

    ESP_LOGI(TAG, "ESP-Mosaico %s firmware is ready",
#if CONFIG_GET_STARTED_RECOVERY
             "factory recovery"
#else
             "application"
#endif
    );
}
