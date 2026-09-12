// SPDX-License-Identifier: Apache-2.0

#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_attr.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_iris.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_rom_sys.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "iris_ota_support.h"
#include "nvs.h"
#include "nvs_flash.h"

#define CRASH_SERVICE_ID 0x6A03U
#define STARTUP_CRASH_COUNT 3U
#define ACTION_DELAY_MS 400U
#define TEST_NAMESPACE "iris_crash"
#define STARTUP_REMAINING_KEY "remaining"
#define RUN_TOKEN_KEY "run_token"

typedef enum {
    ACTION_ASSERT = 2,
    ACTION_ILLEGAL_ACCESS = 3,
    ACTION_TASK_WDT = 4,
    ACTION_RESTART = 5,
} crash_action_t;

typedef struct {
    uint32_t magic;
    uint32_t action;
    uint64_t run_token;
} crash_test_context_t;

COREDUMP_DRAM_ATTR __attribute__((used)) volatile crash_test_context_t
    g_iris_crash_test_context;

static const char *TAG = "iris_crash";

static uint64_t request_token(const esp_iris_rpc_request_t *request,
                              esp_err_t *error)
{
    uint64_t token = 0;
    if (request->payload_size != sizeof(token)) {
        *error = ESP_ERR_INVALID_SIZE;
        return 0;
    }
    memcpy(&token, request->payload, sizeof(token));
    *error = ESP_OK;
    return token;
}

static esp_err_t write_startup_plan(uint8_t remaining, uint64_t token)
{
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(TEST_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        err = nvs_set_u8(handle, STARTUP_REMAINING_KEY, remaining);
    }
    if (err == ESP_OK) {
        err = nvs_set_u64(handle, RUN_TOKEN_KEY, token);
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    if (handle != 0) {
        nvs_close(handle);
    }
    return err;
}

static void record_context(crash_action_t action, uint64_t token)
{
    g_iris_crash_test_context.magic = UINT32_C(0x54524349);
    g_iris_crash_test_context.action = (uint32_t)action;
    g_iris_crash_test_context.run_token = token;
}

static void execute_action(void *argument)
{
    const crash_action_t action = (crash_action_t)(uintptr_t)argument;
    vTaskDelay(pdMS_TO_TICKS(ACTION_DELAY_MS));
    ESP_LOGI(TAG, "EXECUTE action=%u token=%" PRIu64,
             (unsigned)action, g_iris_crash_test_context.run_token);
    switch (action) {
    case ACTION_ASSERT:
        assert(!"ESP-Iris injected assertion");
        break;
    case ACTION_ILLEGAL_ACCESS:
        *(volatile uint32_t *)0 = UINT32_C(0x49524953);
        break;
    case ACTION_TASK_WDT: {
        const esp_task_wdt_config_t config = {
            .timeout_ms = 1000,
            .idle_core_mask = 0,
            .trigger_panic = true,
        };
        esp_err_t err = esp_task_wdt_reconfigure(&config);
        if (err == ESP_ERR_INVALID_STATE) {
            err = esp_task_wdt_init(&config);
        }
        ESP_ERROR_CHECK(err);
        ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
        while (true) {
            esp_rom_delay_us(1000);
        }
        break;
    }
    case ACTION_RESTART:
        esp_restart();
        break;
    default:
        abort();
    }
    abort();
}

static esp_err_t schedule_action(crash_action_t action, uint64_t token)
{
    record_context(action, token);
    BaseType_t created = xTaskCreate(
        execute_action, "iris-crash-action", 4096,
        (void *)(uintptr_t)action, 8, NULL);
    return created == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

static esp_err_t status_rpc(const esp_iris_rpc_request_t *request,
                            uint8_t *response, size_t response_capacity,
                            size_t *response_size, void *user_ctx)
{
    (void)user_ctx;
    *response_size = 0;
    if (request->payload_size != 0) {
        return ESP_ERR_INVALID_SIZE;
    }
    esp_iris_status_t status;
    ESP_RETURN_ON_ERROR(esp_iris_get_status(&status), TAG, "read Iris status");
    const int written = snprintf(
        (char *)response, response_capacity,
        "{\"boot_id\":\"%" PRIu64 "\",\"crash_count\":%" PRIu32
        ",\"failed_boot_id\":\"%" PRIu64 "\"}",
        status.boot_id, status.crash_count, status.crash_failed_boot_id);
    if (written < 0 || (size_t)written >= response_capacity) {
        return ESP_ERR_INVALID_SIZE;
    }
    *response_size = (size_t)written;
    return ESP_OK;
}

static esp_err_t action_rpc(const esp_iris_rpc_request_t *request,
                            uint8_t *response, size_t response_capacity,
                            size_t *response_size, void *user_ctx)
{
    (void)response;
    (void)response_capacity;
    *response_size = 0;
    esp_err_t err = ESP_OK;
    const uint64_t token = request_token(request, &err);
    if (err != ESP_OK) {
        return err;
    }
    const crash_action_t action = (crash_action_t)(uintptr_t)user_ctx;
    if (action == ACTION_RESTART) {
        ESP_RETURN_ON_ERROR(
            esp_iris_mark_planned_restart(), TAG, "mark planned restart");
    }
    return schedule_action(action, token);
}

static esp_err_t arm_startup_loop_rpc(
    const esp_iris_rpc_request_t *request, uint8_t *response,
    size_t response_capacity, size_t *response_size, void *user_ctx)
{
    (void)response;
    (void)response_capacity;
    (void)user_ctx;
    *response_size = 0;
    esp_err_t err = ESP_OK;
    const uint64_t token = request_token(request, &err);
    if (err != ESP_OK) {
        return err;
    }
    ESP_RETURN_ON_ERROR(
        write_startup_plan(STARTUP_CRASH_COUNT, token), TAG,
        "persist startup crash plan");
    ESP_RETURN_ON_ERROR(
        esp_iris_mark_planned_restart(), TAG, "mark startup-loop restart");
    return schedule_action(ACTION_RESTART, token);
}

static esp_err_t reset_crash_loop_rpc(
    const esp_iris_rpc_request_t *request, uint8_t *response,
    size_t response_capacity, size_t *response_size, void *user_ctx)
{
    (void)response;
    (void)response_capacity;
    (void)user_ctx;
    *response_size = 0;
    if (request->payload_size != 0) {
        return ESP_ERR_INVALID_SIZE;
    }
    return esp_iris_crash_loop_reset();
}

static void startup_crash_if_armed(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state = ESP_OTA_IMG_UNDEFINED;
    if (running != NULL &&
        esp_ota_get_state_partition(running, &ota_state) == ESP_OK &&
        ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
        /* A retained test plan can survive an interrupted acceptance run.
         * Never let stale injection reject a newly installed fixture. */
        ESP_ERROR_CHECK(write_startup_plan(0, 0));
        return;
    }
    nvs_handle_t handle = 0;
    if (nvs_open(TEST_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
        return;
    }
    uint8_t remaining = 0;
    uint64_t token = 0;
    (void)nvs_get_u8(handle, STARTUP_REMAINING_KEY, &remaining);
    (void)nvs_get_u64(handle, RUN_TOKEN_KEY, &token);
    if (remaining == 0) {
        nvs_close(handle);
        return;
    }
    --remaining;
    ESP_ERROR_CHECK(nvs_set_u8(handle, STARTUP_REMAINING_KEY, remaining));
    ESP_ERROR_CHECK(nvs_commit(handle));
    nvs_close(handle);
    record_context(ACTION_ASSERT, token);
    ESP_LOGE(TAG, "STARTUP_CRASH remaining=%u token=%" PRIu64,
             remaining, token);
    vTaskDelay(pdMS_TO_TICKS(150));
    assert(!"ESP-Iris injected startup crash");
}

void app_main(void)
{
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_iris_boot_probe());
    ESP_ERROR_CHECK(nvs_flash_init());
    startup_crash_if_armed();

    ESP_ERROR_CHECK(esp_iris_rpc_register(
        CRASH_SERVICE_ID, 1, status_rpc, NULL));
    ESP_ERROR_CHECK(esp_iris_rpc_register(
        CRASH_SERVICE_ID, 2, action_rpc,
        (void *)(uintptr_t)ACTION_ASSERT));
    ESP_ERROR_CHECK(esp_iris_rpc_register(
        CRASH_SERVICE_ID, 3, action_rpc,
        (void *)(uintptr_t)ACTION_ILLEGAL_ACCESS));
    ESP_ERROR_CHECK(esp_iris_rpc_register(
        CRASH_SERVICE_ID, 4, action_rpc,
        (void *)(uintptr_t)ACTION_TASK_WDT));
    ESP_ERROR_CHECK(esp_iris_rpc_register(
        CRASH_SERVICE_ID, 5, action_rpc,
        (void *)(uintptr_t)ACTION_RESTART));
    ESP_ERROR_CHECK(esp_iris_rpc_register(
        CRASH_SERVICE_ID, 6, arm_startup_loop_rpc, NULL));
    ESP_ERROR_CHECK(esp_iris_rpc_register(
        CRASH_SERVICE_ID, 7, reset_crash_loop_rpc, NULL));
    iris_ota_support_start();

    while (true) {
        ESP_LOGI(TAG, "CRASH_TEST_ALIVE service=0x6a03");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
