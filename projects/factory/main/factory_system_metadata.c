#include "factory_system_metadata.h"

#include <stddef.h>
#include <string.h>

#include "nvs.h"
#include "nvs_flash.h"

#define FACTORY_SYSTEM_UPDATE_NAMESPACE "update"
#define FACTORY_SYSTEM_UPDATE_RESULT_KEY "last_result"

esp_err_t factory_system_metadata_init(void)
{
    return nvs_flash_init_partition(FACTORY_SYSTEM_METADATA_PARTITION);
}

esp_err_t factory_system_metadata_load_last_result(
    factory_sysmeta_record_t *record)
{
    if (record == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open_from_partition(
        FACTORY_SYSTEM_METADATA_PARTITION, FACTORY_SYSTEM_UPDATE_NAMESPACE,
        NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;
    }

    size_t size = sizeof(*record);
    err = nvs_get_blob(handle, FACTORY_SYSTEM_UPDATE_RESULT_KEY, record, &size);
    nvs_close(handle);
    if (err != ESP_OK) {
        return err;
    }
    if (size != sizeof(*record) ||
        record->magic != FACTORY_SYSTEM_METADATA_MAGIC ||
        record->version != FACTORY_SYSTEM_METADATA_VERSION) {
        return ESP_ERR_INVALID_VERSION;
    }
    return ESP_OK;
}

esp_err_t factory_system_metadata_store_last_result(
    const uint8_t operation_id[ESP_IRIS_SYSTEM_OPERATION_ID_BYTES],
    esp_err_t result)
{
    if (operation_id == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    factory_sysmeta_record_t record = {
        .magic = FACTORY_SYSTEM_METADATA_MAGIC,
        .version = FACTORY_SYSTEM_METADATA_VERSION,
        .result = result,
    };
    memcpy(record.operation_id, operation_id, sizeof(record.operation_id));

    nvs_handle_t handle;
    esp_err_t err = nvs_open_from_partition(
        FACTORY_SYSTEM_METADATA_PARTITION, FACTORY_SYSTEM_UPDATE_NAMESPACE,
        NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_blob(handle, FACTORY_SYSTEM_UPDATE_RESULT_KEY, &record,
                       sizeof(record));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}
