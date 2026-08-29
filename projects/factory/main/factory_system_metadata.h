#pragma once

#include <stdint.h>

#include "esp_iris_system_inventory.h"

#define FACTORY_SYSTEM_METADATA_MAGIC 0x49535953U
#define FACTORY_SYSTEM_METADATA_VERSION 1U

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint8_t operation_id[ESP_IRIS_SYSTEM_OPERATION_ID_BYTES];
    int32_t result;
    uint8_t reserved[36];
} factory_sysmeta_record_t;
