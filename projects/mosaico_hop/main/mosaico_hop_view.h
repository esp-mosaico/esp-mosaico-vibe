// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "raylib_lite_2d.h"
#include "raylib_lite_fx.h"
#include "platform_game.h"

#define SKY_HOP_PARTICLE_COUNT 24

typedef struct {
    raylib_lite_tween_t tween;
    float y;
    bool shown;
} mosaico_hop_overlay_t;

typedef struct {
    const platform_game_t *game;
    raylib_lite_atlas_t atlas;
    uint16_t best_score;
    float overlay_y;
    const raylib_lite_particle_t *particles;
    size_t particle_count;
} mosaico_hop_view_t;

void mosaico_hop_overlay_sync(mosaico_hop_overlay_t *overlay, platform_phase_t phase);
void mosaico_hop_view_spawn_particles(raylib_lite_particle_pool_t *pool, float x, float y,
                                  Color color, unsigned count);
bool mosaico_hop_view_render(const mosaico_hop_view_t *view);
