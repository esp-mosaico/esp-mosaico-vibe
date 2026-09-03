// SPDX-License-Identifier: Apache-2.0

#include "recovery_ota_support.h"

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
#include "factory_system_metadata.h"
#include "nvs.h"

#define OTA_SERVICE_ID      0x1200U
#define OTA_STATE_METHOD_ID 1U

static const char *TAG = "recovery_ota";

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

esp_err_t esp_iris_platform_prepare_ota(uint32_t running_address,
                                       uint32_t target_address)
{
    if (running_address == 0 || target_address == 0 ||
        running_address == target_address) {
        return ESP_ERR_INVALID_ARG;
    }

    const esp_partition_t *running = esp_ota_get_running_partition();
    if (running == NULL || running->address != running_address ||
        running->subtype != ESP_PARTITION_SUBTYPE_APP_FACTORY) {
        return ESP_ERR_INVALID_STATE;
    }

    return recovery_write(recovery_read_u32("last_good"), target_address,
                          false);
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

    /* The writer is executing from retained Recovery, so a single-slot
     * product can safely replace ota_0 without preserving another app slot. */
    const esp_partition_t *running = esp_ota_get_running_partition();
    return running != NULL &&
                   running->subtype == ESP_PARTITION_SUBTYPE_APP_FACTORY
               ? ESP_OK
               : ESP_ERR_NOT_FOUND;
}

esp_err_t esp_iris_platform_mark_planned_restart(void)
{
    return recovery_write(recovery_read_u32("last_good"),
                          recovery_read_u32("target"), true);
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
        "{\"project\":\"%s\",\"version\":\"%s\",\"mode\":\"recovery\","
        "\"ota_execution\":\"recovery-writer\",\"ota_writer\":true,"
        "\"running\":\"%s\",\"next\":\"%s\",\"image_state\":%d,"
        "\"last_good\":%" PRIu32 ",\"target\":%" PRIu32
        ",\"planned\":%u}",
        app->project_name, app->version, running->label, next->label,
        state_err == ESP_OK ? (int)image_state : -1,
        recovery_read_u32("last_good"), recovery_read_u32("target"),
        recovery_read_u8("planned"));

    if (written < 0 || (size_t)written >= response_capacity) {
        return ESP_ERR_INVALID_SIZE;
    }
    *response_size = (size_t)written;
    return ESP_OK;
}

void recovery_ota_support_start(void)
{
    ESP_ERROR_CHECK(esp_iris_rpc_register(OTA_SERVICE_ID, OTA_STATE_METHOD_ID,
                                          state_rpc, NULL));
    ESP_ERROR_CHECK(esp_iris_start());

    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_app_desc_t *app = esp_app_get_description();
    esp_rom_printf("IRIS_READY version=%s mode=recovery writer=1 partition=%s\n",
                   app->version,
                   running != NULL ? running->label : "unknown");
    ESP_LOGI(TAG,
             "boot state: running=%s@0x%08" PRIx32
             " configured=%s@0x%08" PRIx32,
             running != NULL ? running->label : "unknown",
             running != NULL ? running->address : 0,
             configured != NULL ? configured->label : "unknown",
             configured != NULL ? configured->address : 0);
}
