// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "mosaico_game_2d.h"
#include "underwater_aurora.h"
void underwater_aurora_draw(const underwater_aurora_t *aurora,float yaw,float pitch,
                            uint8_t effects_level,MosaicoAtlas space,
                            MosaicoAtlas ice_front,MosaicoAtlas ice_side,
                            MosaicoAtlas ice_rear);
