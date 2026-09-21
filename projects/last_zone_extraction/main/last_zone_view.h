// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdint.h>
#include "mosaico_game_2d.h"
#include "last_zone_game.h"

typedef struct {
    uint32_t acquire_us;
    uint32_t raycast_us;
    uint32_t sky_us;
    uint32_t floor_us;
    uint32_t wall_us;
    uint32_t grade_us;
    uint32_t sprites_us;
    uint32_t hud_us;
    uint32_t submit_us;
    uint32_t total_us;
    uint16_t rays_cast;
    uint16_t refined_columns;
} last_zone_view_stats_t;

void last_zone_view_render(const last_zone_game_t *game,MosaicoAtlas enemies,
                           MosaicoAtlas weapon,MosaicoAtlas environment,
                           MosaicoAtlas materials,MosaicoAtlas controls,
                           MosaicoAtlas props);
void last_zone_view_get_stats(last_zone_view_stats_t *stats);
