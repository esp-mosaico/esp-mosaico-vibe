// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "mosaico_game_2d.h"
#include "living_worlds_world.h"

typedef struct {
    MosaicoAtlas aurora,sunrise,ocean,rainforest;
    MosaicoAtlas sunrise_cliff_front,sunrise_cliff_side,sunrise_cliff_rear;
    MosaicoAtlas aurora_ice_front,aurora_ice_side,aurora_ice_rear;
    MosaicoAtlas ocean_left_front,ocean_left_side,ocean_left_rear;
    MosaicoAtlas ocean_right_front,ocean_right_side,ocean_right_rear;
} living_worlds_atlases_t;

void living_worlds_view_render(const living_world_t *world,
                            const living_worlds_atlases_t *atlases);
