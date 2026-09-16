// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "mosaico_game_2d.h"
#include "neon_maze_game.h"
void neon_maze_view_render(const neon_maze_game_t *game,MosaicoAtlas enemies,
                           MosaicoAtlas weapon,MosaicoAtlas environment,
                           MosaicoAtlas materials,MosaicoAtlas controls);
