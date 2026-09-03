#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_iris_system_update.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FACTORY_SYSTEM_UPDATE_FILENAME_BYTES 129U
#define FACTORY_SYSTEM_UPDATE_URL_BYTES 512U
#define FACTORY_SYSTEM_UPDATE_PATH_BYTES 256U

typedef enum {
    FACTORY_SYSTEM_UPDATE_OWNER_NONE = 0,
    FACTORY_SYSTEM_UPDATE_OWNER_ESP_IRIS,
    FACTORY_SYSTEM_UPDATE_OWNER_HTTP,
    FACTORY_SYSTEM_UPDATE_OWNER_NAND,
} factory_system_update_owner_t;

typedef struct {
    factory_system_update_owner_t owner;
    esp_iris_system_update_status_t update;
} factory_system_update_status_t;

/* Register the recovery-only, product-owned Flash-policy backend.
 * When the backend is disabled, this remains a successful no-op and the
 * read-only System Inventory service is still available. */
esp_err_t factory_system_update_register(void);

/* Start an asynchronous update from an exploded HTTP(S) bundle. manifest_url
 * names manifest.json; every component is fetched from the same directory
 * using its bounded root-level `file` member. Only one local or ESP-Iris
 * system-update transaction may own the Flash writer at a time. */
esp_err_t factory_system_update_start_http(const char *manifest_url);
esp_err_t factory_system_update_http_register(void);
esp_err_t factory_system_update_start_nand(const char *manifest_path);
esp_err_t factory_system_update_nand_register(void);

esp_err_t factory_system_update_get_status(
    factory_system_update_status_t *status);

/* Source-neutral transaction API used by the HTTP and NAND adapters. It
 * deliberately remains product-private: target addresses are authorized by
 * the manifest parser in the implementation, never by the transport. */
esp_err_t factory_system_update_source_prepare(
    factory_system_update_owner_t owner,
    const uint8_t *manifest, size_t manifest_size,
    const uint8_t operation_id[ESP_IRIS_SYSTEM_OPERATION_ID_BYTES]);
size_t factory_system_update_source_component_count(
    factory_system_update_owner_t owner);
esp_err_t factory_system_update_source_component(
    factory_system_update_owner_t owner, size_t index,
    esp_iris_system_update_component_t *component,
    char filename[FACTORY_SYSTEM_UPDATE_FILENAME_BYTES]);
esp_err_t factory_system_update_source_begin_component(
    factory_system_update_owner_t owner,
    const esp_iris_system_update_component_t *component);
esp_err_t factory_system_update_source_write_component(
    factory_system_update_owner_t owner,
    const esp_iris_system_update_component_t *component, uint32_t offset,
    const uint8_t *data, size_t size);
esp_err_t factory_system_update_source_end_component(
    factory_system_update_owner_t owner,
    const esp_iris_system_update_component_t *component,
    const uint8_t actual_sha256[ESP_IRIS_SYSTEM_SHA256_BYTES]);
esp_err_t factory_system_update_source_commit(
    factory_system_update_owner_t owner,
    const uint8_t operation_id[ESP_IRIS_SYSTEM_OPERATION_ID_BYTES]);
void factory_system_update_source_abort(
    factory_system_update_owner_t owner,
    const uint8_t operation_id[ESP_IRIS_SYSTEM_OPERATION_ID_BYTES],
    esp_err_t reason);

#ifdef __cplusplus
}
#endif
