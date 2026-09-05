// SPDX-License-Identifier: Apache-2.0

#include "ui_bundle.h"

#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#include "esp_check.h"
#include "esp_gsp_deployable.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "psa/crypto.h"

#define UI_BUNDLE_PARTITION_LABEL "ui_apps"
#define UI_BUNDLE_HEADER_VERSION 1
#define UI_BUNDLE_HEADER_SIZE 64

typedef struct {
    uint8_t magic[8];
    uint16_t version;
    uint16_t header_size;
    uint32_t bundle_size;
    uint8_t bundle_sha256[32];
    uint8_t reserved[16];
} ui_bundle_header_t;

_Static_assert(sizeof(ui_bundle_header_t) == UI_BUNDLE_HEADER_SIZE,
               "ui_apps header must be 64 bytes");

static const uint8_t s_magic[8] = {'M', 'O', 'S', 'G', 'S', 'P', 0, 1};
static const char *TAG = "ui_bundle";
static esp_partition_mmap_handle_t s_mmap_handle;
static esp_gsp_deployable_bundle_t *s_deployable;

static esp_err_t verify_bundle_sha256(const uint8_t *bundle, size_t size,
                                      const uint8_t expected[32])
{
    uint8_t actual[32];
    size_t actual_size = 0;
    const psa_status_t init_status = psa_crypto_init();
    const psa_status_t hash_status = init_status == PSA_SUCCESS
        ? psa_hash_compute(PSA_ALG_SHA_256, bundle, size, actual,
                           sizeof(actual), &actual_size)
        : init_status;
    if (hash_status != PSA_SUCCESS || actual_size != sizeof(actual)) {
        ESP_LOGE(TAG, "Could not calculate ui_apps SHA-256: %d",
                 (int)hash_status);
        return ESP_FAIL;
    }
    return memcmp(actual, expected, sizeof(actual)) == 0
        ? ESP_OK
        : ESP_ERR_INVALID_CRC;
}

esp_err_t ui_bundle_open(esp_gsp_config_t *out_config)
{
    if (out_config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_deployable != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    const esp_partition_t *partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY,
        UI_BUNDLE_PARTITION_LABEL);
    if (partition == NULL) {
        ESP_LOGE(TAG, "Partition '%s' not found", UI_BUNDLE_PARTITION_LABEL);
        return ESP_ERR_NOT_FOUND;
    }

    ui_bundle_header_t header;
    ESP_RETURN_ON_ERROR(esp_partition_read(partition, 0, &header,
                                            sizeof(header)),
                        TAG, "read ui_apps header");
    if (memcmp(header.magic, s_magic, sizeof(s_magic)) != 0 ||
        header.version != UI_BUNDLE_HEADER_VERSION ||
        header.header_size != UI_BUNDLE_HEADER_SIZE ||
        header.bundle_size == 0 ||
        partition->size < UI_BUNDLE_HEADER_SIZE ||
        header.bundle_size > partition->size - UI_BUNDLE_HEADER_SIZE) {
        ESP_LOGE(TAG, "Invalid ui_apps header");
        return ESP_ERR_INVALID_SIZE;
    }

    const void *mapped = NULL;
    ESP_RETURN_ON_ERROR(
        esp_partition_mmap(partition, 0,
                           UI_BUNDLE_HEADER_SIZE + header.bundle_size,
                           ESP_PARTITION_MMAP_DATA, &mapped, &s_mmap_handle),
        TAG, "map ui_apps partition");
    const uint8_t *bundle = (const uint8_t *)mapped + UI_BUNDLE_HEADER_SIZE;
    if (((uintptr_t)bundle & 63U) != 0) {
        ESP_LOGE(TAG, "Mapped GSPB is not 64-byte aligned");
        esp_partition_munmap(s_mmap_handle);
        s_mmap_handle = 0;
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = verify_bundle_sha256(bundle, header.bundle_size,
                                         header.bundle_sha256);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ui_apps SHA-256 mismatch");
        goto fail;
    }
    err = esp_gsp_deployable_bundle_open(bundle, header.bundle_size, true,
                                         &s_deployable);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not open deployable GSPB: 0x%x", err);
        goto fail;
    }

    esp_gsp_deployable_info_t info = {
        .struct_size = sizeof(esp_gsp_deployable_info_t),
    };
    err = esp_gsp_deployable_bundle_get_info(s_deployable, &info);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not read deployable GSPB metadata: 0x%x", err);
        goto fail;
    }
    err = esp_gsp_deployable_bundle_make_config(s_deployable, out_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not create GSP configuration: 0x%x", err);
        goto fail;
    }

    ESP_LOGI(TAG,
             "Mapped ui_apps GSPB: %u bytes, scenes=%u, content_id=0x%08" PRIx32,
             (unsigned)header.bundle_size, (unsigned)info.scene_count,
             info.content_id);
    return ESP_OK;

fail:
    if (s_deployable != NULL) {
        esp_gsp_deployable_bundle_close(s_deployable);
        s_deployable = NULL;
    }
    esp_partition_munmap(s_mmap_handle);
    s_mmap_handle = 0;
    return err;
}
