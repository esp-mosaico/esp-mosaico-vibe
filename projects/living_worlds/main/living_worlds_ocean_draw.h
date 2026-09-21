// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "mosaico_game_2d.h"
#include "living_worlds_ocean.h"
void living_ocean_draw(const living_ocean_t *ocean,float yaw,float pitch,
                           uint8_t effects_level,MosaicoAtlas water,
                           MosaicoAtlas left_front,MosaicoAtlas left_side,
                           MosaicoAtlas left_rear,MosaicoAtlas right_front,
                           MosaicoAtlas right_side,MosaicoAtlas right_rear);
