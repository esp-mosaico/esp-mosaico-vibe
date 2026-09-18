// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "mosaico_game_2d.h"
#include "underwater_world.h"

typedef struct {
    MosaicoAtlas panorama,fish,creatures,aurora,sunrise;
    MosaicoAtlas sunrise_cliff_front,sunrise_cliff_side,sunrise_cliff_rear;
    MosaicoAtlas aurora_ice_front,aurora_ice_side,aurora_ice_rear;
    MosaicoAtlas ocean;
    MosaicoAtlas ocean_left_front,ocean_left_side,ocean_left_rear;
    MosaicoAtlas ocean_right_front,ocean_right_side,ocean_right_rear;
    MosaicoAtlas reindeer,rainforest;
} underwater_view_atlases_t;

void underwater_view_render(const underwater_world_t *world,
                            const underwater_view_atlases_t *atlases);
