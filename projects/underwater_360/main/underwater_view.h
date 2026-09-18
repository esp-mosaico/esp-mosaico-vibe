// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "mosaico_game_2d.h"
#include "underwater_world.h"
void underwater_view_render(const underwater_world_t *world,MosaicoAtlas panorama,
                            MosaicoAtlas fish,MosaicoAtlas creatures,
                            MosaicoAtlas aurora,MosaicoAtlas sunrise,
                            MosaicoAtlas sunrise_cliff_front,
                            MosaicoAtlas sunrise_cliff_side,
                            MosaicoAtlas sunrise_cliff_rear,
                            MosaicoAtlas reindeer,MosaicoAtlas rainforest);
