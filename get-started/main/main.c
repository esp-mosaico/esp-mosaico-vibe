// SPDX-License-Identifier: Apache-2.0

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "iris_ota_support.h"
#include "nvs_flash.h"

static const char *TAG = "get_started";

static void hello_world_task(void *arg)
{
    (void)arg;
    TickType_t next_wake = xTaskGetTickCount();

    while (true) {
        ESP_LOGI(TAG, "HELLO WORLD");
        vTaskDelayUntil(&next_wake, pdMS_TO_TICKS(5000));
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    iris_ota_support_start();

    ESP_ERROR_CHECK(xTaskCreate(hello_world_task, "hello_world", 2048, NULL, 4,
                                NULL) == pdPASS
                        ? ESP_OK
                        : ESP_ERR_NO_MEM);
}

