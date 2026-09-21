// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "mosaico_game_2d.h"
#include "living_worlds_aurora.h"
void living_aurora_draw(const living_aurora_t *aurora,float yaw,float pitch,
                            uint8_t effects_level,MosaicoAtlas space,
                            MosaicoAtlas ice_front,MosaicoAtlas ice_side,
                            MosaicoAtlas ice_rear);
