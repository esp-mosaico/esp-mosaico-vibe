// SPDX-License-Identifier: Apache-2.0

#include <string.h>

#include "esp_err.h"
#include "esp_iris.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "iris_ota_support.h"
#include "nvs_flash.h"

#define ACCEPTANCE_SERVICE_ID 0x6A02U
#define SLOW_DELAY_MS 2000U

static const char *TAG = "iris_acceptance";
/* All three handlers run on the same Iris RPC executor. */
static uint32_t s_slow_completed;
static uint32_t s_oversized_reports;
static uint32_t s_status_calls;

#ifdef IRIS_ACCEPTANCE_WIFI_ENABLED
void iris_acceptance_wifi_start(void);
#endif

static void put_u32_le(uint8_t *out, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i) {
        out[i] = (uint8_t)(value >> (8U * i));
    }
}

static esp_err_t slow_rpc(const esp_iris_rpc_request_t *request,
                          uint8_t *response, size_t response_capacity,
                          size_t *response_size, void *user_ctx)
{
    (void)user_ctx;
    *response_size = 0;
    if (request->payload_size != 0 || response_capacity < 4) {
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGI(TAG, "SLOW_BEGIN");
    vTaskDelay(pdMS_TO_TICKS(SLOW_DELAY_MS));
    ++s_slow_completed;
    put_u32_le(response, s_slow_completed);
    *response_size = 4;
    ESP_LOGI(TAG, "SLOW_END");
    return ESP_OK;
}

static esp_err_t oversized_rpc(const esp_iris_rpc_request_t *request,
                               uint8_t *response, size_t response_capacity,
                               size_t *response_size, void *user_ctx)
{
    (void)response;
    (void)user_ctx;
    *response_size = 0;
    if (request->payload_size != 0) {
        return ESP_ERR_INVALID_ARG;
    }
    ++s_oversized_reports;
    /* Deliberately violate only the reported-size contract. Never read or
     * write outside the buffer: Iris must reject this callback result. */
    *response_size = response_capacity + 1;
    return ESP_OK;
}

static esp_err_t status_rpc(const esp_iris_rpc_request_t *request,
                            uint8_t *response, size_t response_capacity,
                            size_t *response_size, void *user_ctx)
{
    (void)user_ctx;
    *response_size = 0;
    if (request->payload_size != 0 || response_capacity < 12) {
        return ESP_ERR_INVALID_ARG;
    }
    ++s_status_calls;
    put_u32_le(response, s_slow_completed);
    put_u32_le(response + 4, s_oversized_reports);
    put_u32_le(response + 8, s_status_calls);
    *response_size = 12;
    return ESP_OK;
}

void app_main(void)
{
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_iris_boot_probe());
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_iris_rpc_register(ACCEPTANCE_SERVICE_ID, 1, slow_rpc, NULL));
    ESP_ERROR_CHECK(esp_iris_rpc_register(ACCEPTANCE_SERVICE_ID, 2, oversized_rpc, NULL));
    ESP_ERROR_CHECK(esp_iris_rpc_register(ACCEPTANCE_SERVICE_ID, 3, status_rpc, NULL));
    iris_ota_support_start();
#ifdef IRIS_ACCEPTANCE_WIFI_ENABLED
    iris_acceptance_wifi_start();
#endif
    while (true) {
        ESP_LOGI(TAG, "ACCEPTANCE_ALIVE service=0x6a02");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
