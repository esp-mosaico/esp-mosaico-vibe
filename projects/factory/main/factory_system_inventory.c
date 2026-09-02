#include "factory_system_inventory.h"

#include <string.h>

#include "sdkconfig.h"

#if CONFIG_ESP_IRIS_SYSTEM_INVENTORY

#include "esp_check.h"
#include "esp_flash.h"
#include "esp_iris_system_inventory.h"
#include "esp_log.h"
#include "factory_system_metadata.h"
#include "psa/crypto.h"

#define FACTORY_LAYOUT_VERSION 4U
#define FACTORY_HASH_CHUNK_BYTES 1024U

static const char *TAG = "factory_inventory";

static esp_err_t hash_flash_region(uint32_t address, size_t size,
                                   uint8_t output[ESP_IRIS_SYSTEM_SHA256_BYTES])
{
    uint8_t buffer[FACTORY_HASH_CHUNK_BYTES];
    psa_hash_operation_t operation = PSA_HASH_OPERATION_INIT;
    if (psa_crypto_init() != PSA_SUCCESS ||
        psa_hash_setup(&operation, PSA_ALG_SHA_256) != PSA_SUCCESS) {
        return ESP_FAIL;
    }

    esp_err_t err = ESP_OK;
    for (size_t offset = 0; offset < size;) {
        size_t chunk = size - offset;
        if (chunk > sizeof(buffer)) {
            chunk = sizeof(buffer);
        }
        err = esp_flash_read(NULL, buffer, address + offset, chunk);
        if (err != ESP_OK ||
            psa_hash_update(&operation, buffer, chunk) != PSA_SUCCESS) {
            err = err == ESP_OK ? ESP_FAIL : err;
            break;
        }
        offset += chunk;
    }
    size_t output_size = 0;
    if (err == ESP_OK &&
        (psa_hash_finish(&operation, output,
                         ESP_IRIS_SYSTEM_SHA256_BYTES, &output_size) !=
             PSA_SUCCESS ||
         output_size != ESP_IRIS_SYSTEM_SHA256_BYTES)) {
        err = ESP_FAIL;
    }
    if (err != ESP_OK) {
        (void)psa_hash_abort(&operation);
    }
    return err;
}

static void load_last_result(esp_iris_system_inventory_t *inventory)
{
    factory_sysmeta_record_t record;
    if (factory_system_metadata_load_last_result(&record) != ESP_OK) {
        return;
    }
    memcpy(inventory->last_operation_id, record.operation_id,
           sizeof(inventory->last_operation_id));
    inventory->last_result = record.result;
    inventory->flags |= ESP_IRIS_SYSTEM_INVENTORY_LAST_OPERATION;
}

static esp_err_t get_inventory(esp_iris_system_inventory_t *inventory,
                               void *user_ctx)
{
    (void)user_ctx;
    ESP_RETURN_ON_FALSE(inventory, ESP_ERR_INVALID_ARG, TAG,
                        "inventory is null");
    memset(inventory, 0, sizeof(*inventory));
    inventory->layout_version = FACTORY_LAYOUT_VERSION;

    const uint32_t bootloader_offset = CONFIG_BOOTLOADER_OFFSET_IN_FLASH;
    const uint32_t partition_offset = CONFIG_PARTITION_TABLE_OFFSET;
    ESP_RETURN_ON_FALSE(partition_offset > bootloader_offset,
                        ESP_ERR_INVALID_STATE, TAG,
                        "invalid protected Flash ranges");
    ESP_RETURN_ON_ERROR(hash_flash_region(
                            bootloader_offset,
                            partition_offset - bootloader_offset,
                            inventory->bootloader_sha256),
                        TAG, "hash bootloader range");
    inventory->flags |= ESP_IRIS_SYSTEM_INVENTORY_BOOTLOADER_SHA256;
    ESP_RETURN_ON_ERROR(hash_flash_region(
                            partition_offset, 0x1000,
                            inventory->partition_table_sha256),
                        TAG, "hash partition table");
    inventory->flags |= ESP_IRIS_SYSTEM_INVENTORY_PARTITION_TABLE_SHA256;
    load_last_result(inventory);
    return ESP_OK;
}

esp_err_t factory_system_inventory_register(void)
{
    const esp_iris_system_inventory_provider_t provider = {
        .get_inventory = get_inventory,
        .user_ctx = NULL,
    };
    ESP_RETURN_ON_ERROR(esp_iris_system_inventory_register(&provider), TAG,
                        "register system inventory");
    return ESP_OK;
}

#else

esp_err_t factory_system_inventory_register(void)
{
    return ESP_OK;
}

#endif
