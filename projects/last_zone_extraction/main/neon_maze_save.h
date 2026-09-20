// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdint.h>
#include "esp_err.h"
#include "neon_maze_game.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t best_ticks;
    uint32_t layout_best[NEON_MAZE_LAYOUTS];
    uint8_t layout;
    uint8_t unlocked;
    uint8_t grades[NEON_MAZE_LAYOUTS];
    uint8_t reserved;
} neon_maze_campaign_t;

esp_err_t neon_maze_save_load(neon_maze_campaign_t *campaign);
esp_err_t neon_maze_save_campaign(const neon_maze_campaign_t *campaign);
esp_err_t neon_maze_save_flush(void);
void neon_maze_campaign_from_game(const neon_maze_game_t *game,
                                 neon_maze_campaign_t *campaign);
void neon_maze_campaign_apply(neon_maze_game_t *game,
                              const neon_maze_campaign_t *campaign);

#ifdef __cplusplus
}
#endif
