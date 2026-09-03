// SPDX-License-Identifier: Apache-2.0
// Recovery-first integration derived from the ESP-Iris OTA example.

#include "iris_ota_support.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

#include "esp_app_desc.h"
#include "esp_check.h"
#include "esp_iris.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_rom_sys.h"
#include "esp_system.h"
#include "factory_system_metadata.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"

#define OTA_SERVICE_ID          0x1200U
#define OTA_STATE_METHOD_ID     1U
#define OTA_ACCEPT_METHOD_ID    2U
#define RECOVERY_SERVICE_ID     0x7FFFU
#define ENTER_RECOVERY_METHOD   2U
static const char *TAG = "iris_ota";

static bool is_ota_partition(const esp_partition_t *partition)
{
    return partition != NULL &&
           partition->subtype >= ESP_PARTITION_SUBTYPE_APP_OTA_0 &&
           partition->subtype <= ESP_PARTITION_SUBTYPE_APP_OTA_MAX;
}

static esp_err_t recovery_write(uint32_t last_good, uint32_t target,
                                bool planned)
{
    nvs_handle_t handle;
    ESP_RETURN_ON_ERROR(
        nvs_open_from_partition(FACTORY_SYSTEM_METADATA_PARTITION,
                                FACTORY_SYSTEM_METADATA_OTA_NAMESPACE,
                                NVS_READWRITE, &handle),
        TAG, "open recovery metadata");

    esp_err_t err = nvs_set_u32(handle, "last_good", last_good);
    if (err == ESP_OK) {
        err = nvs_set_u32(handle, "target", target);
    }
    if (err == ESP_OK) {
        err = nvs_set_u8(handle, "planned", planned ? 1 : 0);
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

static uint32_t recovery_read_u32(const char *key)
{
    nvs_handle_t handle;
    uint32_t value = 0;
    if (nvs_open_from_partition(FACTORY_SYSTEM_METADATA_PARTITION,
                                FACTORY_SYSTEM_METADATA_OTA_NAMESPACE,
                                NVS_READONLY, &handle) == ESP_OK) {
        (void)nvs_get_u32(handle, key, &value);
        nvs_close(handle);
    }
    return value;
}

static uint8_t recovery_read_u8(const char *key)
{
    nvs_handle_t handle;
    uint8_t value = 0;
    if (nvs_open_from_partition(FACTORY_SYSTEM_METADATA_PARTITION,
                                FACTORY_SYSTEM_METADATA_OTA_NAMESPACE,
                                NVS_READONLY, &handle) == ESP_OK) {
        (void)nvs_get_u8(handle, key, &value);
        nvs_close(handle);
    }
    return value;
}

#if CONFIG_ESP_IRIS_OTA
esp_err_t esp_iris_platform_prepare_ota(uint32_t running_address,
                                       uint32_t target_address)
{
    if (running_address == 0 || target_address == 0 ||
        running_address == target_address) {
        return ESP_ERR_INVALID_ARG;
    }

    const esp_partition_t *running = esp_ota_get_running_partition();
    if (running == NULL || running->address != running_address) {
        return ESP_ERR_INVALID_STATE;
    }

    const uint32_t last_good =
        running->subtype == ESP_PARTITION_SUBTYPE_APP_FACTORY
            ? recovery_read_u32("last_good")
            : running_address;
    return recovery_write(last_good, target_address, false);
}

esp_err_t esp_iris_platform_select_ota_target(uint32_t default_address,
                                              uint32_t *target_address)
{
    if (default_address == 0 || target_address == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *target_address = default_address;
    const uint32_t last_good = recovery_read_u32("last_good");
    if (default_address != last_good) {
        return ESP_OK;
    }

    esp_partition_iterator_t iterator = esp_partition_find(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, NULL);
    while (iterator != NULL) {
        const esp_partition_t *partition = esp_partition_get(iterator);
        if (is_ota_partition(partition) && partition->address != last_good) {
            *target_address = partition->address;
            esp_partition_iterator_release(iterator);
            return ESP_OK;
        }
        iterator = esp_partition_next(iterator);
    }
    esp_partition_iterator_release(iterator);

    /* A single-slot product has no second normal image to preserve. It is
     * still safe to overwrite ota_0 here because the writer is executing from
     * the retained factory recovery partition. A failed or rejected image
     * therefore falls back to factory recovery, not to an older normal image.
     */
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (running != NULL &&
        running->subtype == ESP_PARTITION_SUBTYPE_APP_FACTORY) {
        return ESP_OK;
    }
    return ESP_ERR_NOT_FOUND;
}
#endif

esp_err_t esp_iris_platform_mark_planned_restart(void)
{
    return recovery_write(recovery_read_u32("last_good"),
                          recovery_read_u32("target"), true);
}

esp_err_t esp_iris_platform_mark_healthy(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (running == NULL) {
        return ESP_ERR_NOT_FOUND;
    }
    if (!is_ota_partition(running)) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_RETURN_ON_ERROR(esp_ota_mark_app_valid_cancel_rollback(), TAG,
                        "accept pending image");
    return recovery_write(running->address, 0, false);
}

static esp_err_t state_rpc(const esp_iris_rpc_request_t *request,
                           uint8_t *response, size_t response_capacity,
                           size_t *response_size, void *user_ctx)
{
    (void)user_ctx;
    if (request->payload_size != 0) {
        return ESP_ERR_INVALID_SIZE;
    }

    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *next = esp_ota_get_next_update_partition(NULL);
    const esp_app_desc_t *app = esp_app_get_description();
    if (running == NULL || next == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    esp_ota_img_states_t image_state;
    const esp_err_t state_err = esp_ota_get_state_partition(running,
                                                             &image_state);
    const int written = snprintf(
        (char *)response, response_capacity,
        "{\"project\":\"%s\",\"version\":\"%s\",\"mode\":\"%s\","
        "\"ota_execution\":\"%s\",\"ota_writer\":%s,"
        "\"running\":\"%s\",\"next\":\"%s\",\"image_state\":%d,"
        "\"last_good\":%" PRIu32 ",\"target\":%" PRIu32
        ",\"planned\":%u}",
        app->project_name, app->version,
#if CONFIG_GET_STARTED_RECOVERY
        "recovery", "recovery-writer",
#else
        "normal", "recovery",
#endif
#if CONFIG_ESP_IRIS_OTA
        "true",
#else
        "false",
#endif
        running->label, next->label,
        state_err == ESP_OK ? (int)image_state : -1,
        recovery_read_u32("last_good"), recovery_read_u32("target"),
        recovery_read_u8("planned"));

    if (written < 0 || (size_t)written >= response_capacity) {
        return ESP_ERR_INVALID_SIZE;
    }
    *response_size = (size_t)written;
    return ESP_OK;
}

static esp_err_t accept_rpc(const esp_iris_rpc_request_t *request,
                            uint8_t *response, size_t response_capacity,
                            size_t *response_size, void *user_ctx)
{
    (void)response;
    (void)response_capacity;
    (void)user_ctx;
    if (request->payload_size != 0) {
        return ESP_ERR_INVALID_SIZE;
    }
    *response_size = 0;
    return esp_iris_mark_healthy();
}

#if CONFIG_ESP_IRIS_OTA_DEFAULT_VIA_RECOVERY && !CONFIG_GET_STARTED_RECOVERY
static void enter_recovery_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
}

static esp_err_t enter_recovery_rpc(const esp_iris_rpc_request_t *request,
                                    uint8_t *response,
                                    size_t response_capacity,
                                    size_t *response_size, void *user_ctx)
{
    (void)response;
    (void)response_capacity;
    (void)user_ctx;
    if (request->payload_size != 0) {
        return ESP_ERR_INVALID_SIZE;
    }

    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *factory = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
    if (running == NULL || factory == NULL ||
        running->address == factory->address) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_RETURN_ON_ERROR(esp_iris_mark_planned_restart(), TAG,
                        "record recovery restart");
    ESP_RETURN_ON_ERROR(esp_ota_set_boot_partition(factory), TAG,
                        "select factory recovery");
    if (xTaskCreate(enter_recovery_task, "enter_recovery", 2048, NULL, 5,
                    NULL) != pdPASS) {
        (void)esp_ota_set_boot_partition(running);
        return ESP_ERR_NO_MEM;
    }

    *response_size = 0;
    return ESP_OK;
}
#endif

#if CONFIG_GET_STARTED_AUTO_ACCEPT
static void acceptance_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(CONFIG_GET_STARTED_ACCEPT_DELAY_MS));

    const esp_partition_t *running = esp_ota_get_running_partition();
    if (is_ota_partition(running)) {
        const esp_err_t err = esp_iris_mark_healthy();
        ESP_ERROR_CHECK_WITHOUT_ABORT(err);
        if (err == ESP_OK) {
            esp_rom_printf("IRIS_OTA_HEALTHY partition=%s\n", running->label);
        }
    }
    vTaskDelete(NULL);
}
#endif

void iris_ota_support_start(void)
{
    ESP_ERROR_CHECK(esp_iris_rpc_register(OTA_SERVICE_ID, OTA_STATE_METHOD_ID,
                                          state_rpc, NULL));
    ESP_ERROR_CHECK(esp_iris_rpc_register(OTA_SERVICE_ID, OTA_ACCEPT_METHOD_ID,
                                          accept_rpc, NULL));

#if CONFIG_ESP_IRIS_OTA_DEFAULT_VIA_RECOVERY && !CONFIG_GET_STARTED_RECOVERY
    ESP_ERROR_CHECK(esp_iris_rpc_register(RECOVERY_SERVICE_ID,
                                          ENTER_RECOVERY_METHOD,
                                          enter_recovery_rpc, NULL));
#endif

    ESP_ERROR_CHECK(esp_iris_start());

    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_app_desc_t *app = esp_app_get_description();
    esp_rom_printf("IRIS_READY version=%s mode=%s writer=%d partition=%s\n",
                   app->version,
#if CONFIG_GET_STARTED_RECOVERY
                   "recovery",
#else
                   "normal",
#endif
#if CONFIG_ESP_IRIS_OTA
                   1,
#else
                   0,
#endif
                   running != NULL ? running->label : "unknown");
    ESP_LOGI(TAG,
             "boot state: running=%s@0x%08" PRIx32
             " configured=%s@0x%08" PRIx32,
             running != NULL ? running->label : "unknown",
             running != NULL ? running->address : 0,
             configured != NULL ? configured->label : "unknown",
             configured != NULL ? configured->address : 0);

#if CONFIG_GET_STARTED_AUTO_ACCEPT
    ESP_ERROR_CHECK(xTaskCreate(acceptance_task, "ota_accept", 2048, NULL, 4,
                                NULL) == pdPASS
                        ? ESP_OK
                        : ESP_ERR_NO_MEM);
#endif
}
