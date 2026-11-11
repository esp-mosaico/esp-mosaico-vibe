// SPDX-License-Identifier: Apache-2.0

#include "web_seed.h"

#include <string.h>
#include <sys/stat.h>

#include "esp_log.h"
#include "minihub_config.h"
#include "minihub_paths.h"

static const char *TAG = "app_minihub";

extern const uint8_t web_seed_bin_start[] asm("_binary_web_seed_bin_start");
extern const uint8_t web_seed_bin_end[] asm("_binary_web_seed_bin_end");

#define WEB_SEED_MAGIC "MHWS"

static bool read_u16(const uint8_t **cursor, const uint8_t *end, uint16_t *out)
{
    if (*cursor + sizeof(*out) > end) {
        return false;
    }
    memcpy(out, *cursor, sizeof(*out));
    *cursor += sizeof(*out);
    return true;
}

static bool read_u32(const uint8_t **cursor, const uint8_t *end, uint32_t *out)
{
    if (*cursor + sizeof(*out) > end) {
        return false;
    }
    memcpy(out, *cursor, sizeof(*out));
    *cursor += sizeof(*out);
    return true;
}

static bool web_index_present(void)
{
    struct stat st;
    return stat(MINIHUB_WEB_INDEX, &st) == 0 && S_ISREG(st.st_mode) &&
           st.st_size > 0;
}

esp_err_t app_minihub_seed_web(void)
{
    if (web_index_present()) {
        return ESP_OK;
    }

    const uint8_t *cursor = web_seed_bin_start;
    const uint8_t *const end = web_seed_bin_end;
    if (cursor + 8 > end || memcmp(cursor, WEB_SEED_MAGIC, 4) != 0) {
        ESP_LOGE(TAG, "web seed blob missing");
        return ESP_ERR_INVALID_STATE;
    }
    cursor += 4;

    uint32_t count = 0;
    if (!read_u32(&cursor, end, &count)) {
        return ESP_ERR_INVALID_SIZE;
    }

    ESP_LOGW(TAG, "/data/web missing; seeding %u factory file(s)", (unsigned)count);
    uint32_t written = 0;
    for (uint32_t i = 0; i < count; ++i) {
        uint16_t name_len = 0;
        uint32_t data_len = 0;
        if (!read_u16(&cursor, end, &name_len) || cursor + name_len > end) {
            return ESP_ERR_INVALID_SIZE;
        }
        const uint8_t *name_bytes = cursor;
        cursor += name_len;
        if (!read_u32(&cursor, end, &data_len) || cursor + data_len > end) {
            return ESP_ERR_INVALID_SIZE;
        }

        char rel[96];
        if (name_len == 0 || name_len >= sizeof(rel) - 5) {
            cursor += data_len;
            continue;
        }
        memcpy(rel, "web/", 4);
        memcpy(rel + 4, name_bytes, name_len);
        rel[4 + name_len] = '\0';

        const esp_err_t err = minihub_fs_write_file(rel, cursor, data_len);
        cursor += data_len;
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "seed %s failed: %s", rel, esp_err_to_name(err));
            return err;
        }
        ++written;
    }

    ESP_LOGI(TAG, "seeded /data/web (%u files)", (unsigned)written);
    return web_index_present() ? ESP_OK : ESP_FAIL;
}
