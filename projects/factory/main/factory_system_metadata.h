#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "esp_iris_system_inventory.h"

#define FACTORY_SYSTEM_METADATA_PARTITION "sysmeta"
#define FACTORY_SYSTEM_METADATA_OTA_NAMESPACE "iris_ota_demo"

#define FACTORY_SYSTEM_METADATA_MAGIC 0x49535953U
#define FACTORY_SYSTEM_METADATA_VERSION 2U

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint8_t operation_id[ESP_IRIS_SYSTEM_OPERATION_ID_BYTES];
    int32_t result;
    uint8_t reserved[36];
} factory_sysmeta_record_t;

esp_err_t factory_system_metadata_init(void);
esp_err_t factory_system_metadata_load_last_result(
    factory_sysmeta_record_t *record);
esp_err_t factory_system_metadata_store_last_result(
    const uint8_t operation_id[ESP_IRIS_SYSTEM_OPERATION_ID_BYTES],
    esp_err_t result);
