// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <stdio.h>
#include <math.h>
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
    for (int i = 0; i < NEON_MAZE_ENEMIES; ++i) {
        if (!game->enemies[i].active) continue;
        int cx = (int)game->enemies[i].x, cy = (int)game->enemies[i].y;
        assert(seen[cy][cx]);
        assert(!(cx == 21 && cy == 21));
        assert(!(cx == 22 && cy == 21));
        assert(!(cx == 22 && cy == 22));
    }
    for (int i = 0; i < NEON_MAZE_PROPS; ++i) {
        if (!game->props[i].active) continue;
        int cx = (int)game->props[i].x, cy = (int)game->props[i].y;
        assert(!(cx == 21 && cy == 21));
        assert(!(cx == 22 && cy == 21));
    }
}

int main(void)
{
    neon_maze_game_t game = {0};
    for (int mission = 0; mission < NEON_MAZE_LAYOUTS; ++mission) {
        game.layout = (uint8_t)mission;
        neon_maze_reset(&game);
        assert(game.layout == mission);
        assert(neon_maze_enemies_alive(&game) == neon_maze_enemy_total(&game));
        assert(game.armor == (mission == 0 ? 2 : ((mission == 1 || mission == 3) ? 1 : 0)));
        for (int i = 0; i < NEON_MAZE_ENEMIES; ++i) {
            if (!game.enemies[i].active) continue;
            assert(game.enemies[i].ai_state == NEON_ENEMY_PATROL);
            float dx = game.enemies[i].x - game.x;
            float dy = game.enemies[i].y - game.y;
            assert(dx * dx + dy * dy > 64.0f);
            float cx = game.enemies[i].x - game.pickups[1].x;
            float cy = game.enemies[i].y - game.pickups[1].y;
            assert(cx * cx + cy * cy > 2.56f);
        }
        assert_mission_reachable(&game);
        assert(neon_maze_cell(&game, 21, 21) == 5);
        assert(neon_maze_cell(&game, 22, 21) == 5);
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
    neon_maze_move_radar(&game, 400, 400);
    assert(game.radar_y + NEON_MAZE_RADAR_SIZE <=
           NEON_MAZE_MOVE_Y - NEON_MAZE_MOVE_R - 8);
    assert(game.radar_x >= 4 && game.radar_x + NEON_MAZE_RADAR_SIZE <= 476);

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
    assert(game.last_blast && game.barrel_used);

    neon_maze_reset(&game);
    neon_maze_confirm(&game);
    uint16_t shots = game.shots_fired;
    neon_maze_set_fire_held(&game, true);
    neon_maze_update(&game);
    assert(game.last_fire == NEON_FIRE_NONE);
    assert(game.shots_fired == shots);
    neon_maze_set_fire_held(&game, false);
    neon_maze_update(&game);
    assert(game.last_fire == NEON_FIRE_SHOT);
    assert(game.shots_fired == shots + 1);

    neon_maze_reset(&game);
    neon_maze_confirm(&game);
    for (int i = 0; i < 10; ++i) {
        neon_maze_set_fire_held(&game, true);
        neon_maze_update(&game);
    }
    assert(game.holding_breath);

    neon_maze_reset(&game);
    neon_maze_confirm(&game);
    for (int i = 0; i < NEON_MAZE_ENEMIES; ++i) {
        game.enemies[i].active = false;
        game.enemies[i].hp = 0;
    }
    game.x = 21.5f;
    game.y = 21.5f;
    neon_maze_update(&game);
    assert(game.phase == NEON_MAZE_PHASE_WON);
    assert(neon_maze_on_extract(&game));

    neon_maze_reset(&game);
    neon_maze_confirm(&game);
    game.enemies[0].ai_state = NEON_ENEMY_PATROL;
    game.enemies[0].aim_timer = 0;
    game.enemies[0].x = 1.5f;
    game.enemies[0].y = 7.5f;
    game.enemies[0].active = true;
    game.x = 2.5f;
    game.y = 3.5f;
    neon_maze_set_motion(&game, 0, 0, 0);
    neon_maze_set_sprint(&game, false);
    for (int i = 0; i < 4; ++i) neon_maze_update(&game);
    assert(game.enemies[0].ai_state == NEON_ENEMY_PATROL);
    neon_maze_set_motion(&game, 1.0f, 0, 0);
    neon_maze_set_sprint(&game, true);
    for (int i = 0; i < 8; ++i) neon_maze_update(&game);
    assert(game.enemies[0].ai_state != NEON_ENEMY_PATROL);
    assert(game.spotted);

    game.layout = 4;
    game.phase = NEON_MAZE_PHASE_WON;
    neon_maze_confirm(&game);
    assert(game.layout == 0 && game.phase == NEON_MAZE_PHASE_PLAYING);

    puts("neon maze model: ok");
    return 0;
}
