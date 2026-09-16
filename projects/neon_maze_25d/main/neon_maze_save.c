// SPDX-License-Identifier: Apache-2.0
#include "neon_maze_save.h"
#include "esp_timer.h"
#include "mosaico_game_save.h"

#define NEON_MAZE_SAVE_VERSION 1U

static mosaico_save_t s_save;
static bool s_ready;

static uint64_t now_ms(void)
{
    return (uint64_t)esp_timer_get_time() / 1000U;
}

static esp_err_t ensure_ready(void)
{
    if (s_ready) return ESP_OK;
    mosaico_save_config_t config = {
        .nvs_namespace = "neon_maze",
        .key = "best",
        .version = NEON_MAZE_SAVE_VERSION,
        .payload_size = sizeof(uint32_t),
        .debounce_ms = 750,
        .migrate = NULL,
    };
    esp_err_t err = mosaico_save_init(&s_save, &config);
    s_ready = err == ESP_OK;
    return err;
}

esp_err_t neon_maze_save_load(uint32_t *best_ticks)
{
    if (!best_ticks) return ESP_ERR_INVALID_ARG;
    *best_ticks = 0;
    esp_err_t err = ensure_ready();
    if (err != ESP_OK) return err;
    err = mosaico_save_load(&s_save, best_ticks, NULL);
    if (err == ESP_ERR_INVALID_CRC || err == ESP_ERR_INVALID_VERSION) {
        *best_ticks = 0;
        return ESP_OK;
    }
    return err;
}

esp_err_t neon_maze_save_best(uint32_t best_ticks)
{
    esp_err_t err = ensure_ready();
    return err == ESP_OK ? mosaico_save_request(&s_save, &best_ticks, now_ms()) : err;
}

esp_err_t neon_maze_save_flush(void)
{
    return s_ready ? mosaico_save_flush(&s_save, now_ms(), false) : ESP_OK;
}
