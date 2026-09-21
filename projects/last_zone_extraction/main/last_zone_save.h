// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdint.h>
#include "esp_err.h"
#include "last_zone_game.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t best_ticks;
    uint32_t layout_best[LAST_ZONE_LAYOUTS];
    uint8_t layout;
    uint8_t unlocked;
    uint8_t grades[LAST_ZONE_LAYOUTS];
    uint8_t reserved;
} last_zone_campaign_t;

esp_err_t last_zone_save_load(last_zone_campaign_t *campaign);
esp_err_t last_zone_save_campaign(const last_zone_campaign_t *campaign);
esp_err_t last_zone_save_flush(void);
void last_zone_campaign_from_game(const last_zone_game_t *game,
                                 last_zone_campaign_t *campaign);
void last_zone_campaign_apply(last_zone_game_t *game,
                              const last_zone_campaign_t *campaign);

#ifdef __cplusplus
}
#endif
