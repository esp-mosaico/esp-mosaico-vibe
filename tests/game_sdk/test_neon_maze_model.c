// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <stdio.h>
#include "neon_maze_game.h"

static void assert_mission_reachable(neon_maze_game_t *game)
{
    unsigned char seen[NEON_MAZE_HEIGHT][NEON_MAZE_WIDTH] = {{0}};
    int qx[NEON_MAZE_WIDTH * NEON_MAZE_HEIGHT];
    int qy[NEON_MAZE_WIDTH * NEON_MAZE_HEIGHT];
    int head = 0, tail = 0;
    qx[tail] = 2; qy[tail++] = 3; seen[3][2] = 1;
    while (head < tail) {
        int x = qx[head], y = qy[head++];
        static const int d[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
        for (int i = 0; i < 4; ++i) {
            int nx = x + d[i][0], ny = y + d[i][1];
            if (nx < 0 || ny < 0 || nx >= NEON_MAZE_WIDTH || ny >= NEON_MAZE_HEIGHT ||
                seen[ny][nx]) continue;
            uint8_t cell = neon_maze_cell(game, nx, ny);
            if (cell != 0 && cell != 4 && cell != 5) continue;
            seen[ny][nx] = 1; qx[tail] = nx; qy[tail++] = ny;
        }
    }
    assert(seen[(int)NEON_MAZE_EXTRACT_Y][(int)NEON_MAZE_EXTRACT_X]);
    for (int i = 0; i < NEON_MAZE_ENEMIES; ++i)
        if (game->enemies[i].active)
            assert(seen[(int)game->enemies[i].y][(int)game->enemies[i].x]);
}

int main(void)
{
    neon_maze_game_t game = {0};
    for (int mission = 0; mission < NEON_MAZE_LAYOUTS; ++mission) {
        game.layout = (uint8_t)mission;
        neon_maze_reset(&game);
        assert(game.layout == mission);
        assert(neon_maze_enemies_alive(&game) == neon_maze_enemy_total(&game));
        assert_mission_reachable(&game);
    }
    game.layout = 1;
    game.phase = NEON_MAZE_PHASE_DEAD;
    neon_maze_confirm(&game);
    assert(game.layout == 1 && game.phase == NEON_MAZE_PHASE_PLAYING);
    game.phase = NEON_MAZE_PHASE_WON;
    neon_maze_confirm(&game);
    assert(game.layout == 2 && game.phase == NEON_MAZE_PHASE_PLAYING);
    neon_maze_reset(&game);
    neon_maze_confirm(&game);

    game.armor = 1;
    for (int i = 1; i < NEON_MAZE_ENEMIES; ++i) game.enemies[i].active = false;
    game.enemies[0].x = game.x;
    game.enemies[0].y = game.y;
    game.enemies[0].active = true;
    game.enemy_shot_lock = 0;
    game.hurt_cooldown = 0;
    neon_maze_update(&game);
    assert(game.armor == 0);
    assert(game.hp == NEON_MAZE_MAX_HP);

    neon_maze_reset(&game);
    neon_maze_confirm(&game);
    for (int i = 1; i < NEON_MAZE_ENEMIES; ++i) game.enemies[i].active = false;
    game.x = 4.5f;
    game.y = 3.5f;
    game.angle = 0.0f;
    game.props[0].x = 5.5f;
    game.props[0].y = 3.5f;
    game.props[0].kind = 0;
    game.props[0].active = true;
    game.enemies[0].x = 6.5f;
    game.enemies[0].y = 3.5f;
    game.enemies[0].active = true;
    assert(neon_maze_fire(&game) == NEON_FIRE_KILL);
    assert(!game.props[0].active && game.props[0].blast_timer == 18);
    assert(!game.enemies[0].active && game.kills == 1);
    assert(game.shots_fired == 1 && game.shots_hit == 1);

    puts("neon maze model: ok");
    return 0;
}
