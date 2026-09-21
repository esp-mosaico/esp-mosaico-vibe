// SPDX-License-Identifier: Apache-2.0
#include "last_zone_save.h"
#include <string.h>
#include "esp_timer.h"
#include "mosaico_game_save.h"

#define LAST_ZONE_SAVE_VERSION 2U

static mosaico_save_t s_save;
static bool s_ready;

static uint64_t now_ms(void)
{
    return (uint64_t)esp_timer_get_time() / 1000U;
}

static esp_err_t migrate_v1(uint16_t old_version, const void *old_data, size_t old_size,
                            void *new_data, size_t new_size)
{
    last_zone_campaign_t *campaign = new_data;
    memset(campaign, 0, new_size);
    if (old_version == 1U && old_size >= sizeof(uint32_t) && old_data)
        campaign->best_ticks = *(const uint32_t *)old_data;
    return ESP_OK;
}

static esp_err_t ensure_ready(void)
{
    if (s_ready) return ESP_OK;
    mosaico_save_config_t config = {
        .nvs_namespace = "neon_maze",
        .key = "best",
        .version = LAST_ZONE_SAVE_VERSION,
        .payload_size = sizeof(last_zone_campaign_t),
        .debounce_ms = 750,
        .migrate = migrate_v1,
    };
    esp_err_t err = mosaico_save_init(&s_save, &config);
    s_ready = err == ESP_OK;
    return err;
}

void last_zone_campaign_from_game(const last_zone_game_t *game,
                                 last_zone_campaign_t *campaign)
{
    if (!campaign) return;
    memset(campaign, 0, sizeof(*campaign));
    if (!game) return;
    campaign->best_ticks = game->best_ticks;
    memcpy(campaign->layout_best, game->layout_best, sizeof(campaign->layout_best));
    campaign->layout = game->layout;
    campaign->unlocked = game->unlocked;
    campaign->grades[game->layout < LAST_ZONE_LAYOUTS ? game->layout : 0] =
        (uint8_t)last_zone_grade(game);
}

void last_zone_campaign_apply(last_zone_game_t *game,
                              const last_zone_campaign_t *campaign)
{
    if (!game || !campaign) return;
    game->best_ticks = campaign->best_ticks;
    memcpy(game->layout_best, campaign->layout_best, sizeof(game->layout_best));
    game->layout = campaign->layout < LAST_ZONE_LAYOUTS ? campaign->layout : 0;
    game->unlocked = campaign->unlocked;
}

esp_err_t last_zone_save_load(last_zone_campaign_t *campaign)
{
    if (!campaign) return ESP_ERR_INVALID_ARG;
    memset(campaign, 0, sizeof(*campaign));
    esp_err_t err = ensure_ready();
    if (err != ESP_OK) return err;
    err = mosaico_save_load(&s_save, campaign, NULL);
    if (err == ESP_ERR_INVALID_CRC || err == ESP_ERR_INVALID_VERSION) {
        memset(campaign, 0, sizeof(*campaign));
        return ESP_OK;
    }
    return err;
}

esp_err_t last_zone_save_campaign(const last_zone_campaign_t *campaign)
{
    if (!campaign) return ESP_ERR_INVALID_ARG;
    esp_err_t err = ensure_ready();
    return err == ESP_OK ? mosaico_save_request(&s_save, campaign, now_ms()) : err;
}

esp_err_t last_zone_save_flush(void)
{
    return s_ready ? mosaico_save_flush(&s_save, now_ms(), false) : ESP_OK;
}
