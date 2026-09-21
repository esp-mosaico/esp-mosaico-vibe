// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include "last_zone_game.h"

static void assert_mission_reachable(last_zone_game_t *game)
{
    unsigned char seen[LAST_ZONE_HEIGHT][LAST_ZONE_WIDTH] = {{0}};
    int qx[LAST_ZONE_WIDTH * LAST_ZONE_HEIGHT];
    int qy[LAST_ZONE_WIDTH * LAST_ZONE_HEIGHT];
    int head = 0, tail = 0;
    int spawn_x = (int)last_zone_spawn_x(game);
    int spawn_y = (int)last_zone_spawn_y(game);
    qx[tail] = spawn_x; qy[tail++] = spawn_y; seen[spawn_y][spawn_x] = 1;
    while (head < tail) {
        int x = qx[head], y = qy[head++];
        static const int d[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
        for (int i = 0; i < 4; ++i) {
            int nx = x + d[i][0], ny = y + d[i][1];
            if (nx < 0 || ny < 0 || nx >= LAST_ZONE_WIDTH || ny >= LAST_ZONE_HEIGHT ||
                seen[ny][nx]) continue;
            uint8_t cell = last_zone_cell(game, nx, ny);
            if (cell != 0 && cell != 4 && cell != 5) continue;
            seen[ny][nx] = 1; qx[tail] = nx; qy[tail++] = ny;
        }
    }
    if (!seen[(int)last_zone_extract_y(game)][(int)last_zone_extract_x(game)])
        fprintf(stderr, "mission %u extract is unreachable\n", (unsigned)game->layout);
    assert(seen[(int)last_zone_extract_y(game)][(int)last_zone_extract_x(game)]);
    for (int i = 0; i < LAST_ZONE_ENEMIES; ++i) {
        if (!game->enemies[i].active) continue;
        int cx = (int)game->enemies[i].x, cy = (int)game->enemies[i].y;
        assert(seen[cy][cx]);
        assert(last_zone_cell(game, cx, cy) != 5);
    }
    for (int i = 0; i < LAST_ZONE_PROPS; ++i) {
        if (!game->props[i].active) continue;
        int cx = (int)game->props[i].x, cy = (int)game->props[i].y;
        assert(last_zone_cell(game, cx, cy) != 5);
    }
}

int main(void)
{
    static const uint8_t expected_enemies[LAST_ZONE_LAYOUTS] = {5, 7, 8, 8, 9};
    static const uint8_t expected_elites[LAST_ZONE_LAYOUTS] = {0, 0, 2, 0, 3};
    static const uint8_t expected_ammo[LAST_ZONE_LAYOUTS] = {20, 18, 17, 17, 16};
    static const uint8_t expected_armor[LAST_ZONE_LAYOUTS] = {2, 2, 1, 1, 0};
    static const uint8_t expected_props[LAST_ZONE_LAYOUTS] = {4, 6, 6, 3, 7};
    static const uint8_t expected_barrels[LAST_ZONE_LAYOUTS] = {1, 3, 2, 1, 2};
    last_zone_game_t game = {0};
    for (int mission = 0; mission < LAST_ZONE_LAYOUTS; ++mission) {
        game.layout = (uint8_t)mission;
        last_zone_reset(&game);
        assert(game.layout == mission);
        assert(last_zone_enemies_alive(&game) == last_zone_enemy_total(&game));
        assert(last_zone_enemy_total(&game) == expected_enemies[mission]);
        assert(game.ammo == expected_ammo[mission]);
        assert(game.armor == expected_armor[mission]);
        assert(game.x == last_zone_spawn_x(&game));
        assert(game.y == last_zone_spawn_y(&game));
        assert(last_zone_briefing(&game)[0] != '\0');
        int elites = 0;
        for (int i = 0; i < LAST_ZONE_ENEMIES; ++i) {
            if (!game.enemies[i].active) continue;
            if (game.enemies[i].elite) ++elites;
            assert(game.enemies[i].ai_state == LAST_ZONE_ENEMY_PATROL);
            float dx = game.enemies[i].x - game.x;
            float dy = game.enemies[i].y - game.y;
            assert(dx * dx + dy * dy > 64.0f);
            float cx = game.enemies[i].x - game.pickups[1].x;
            float cy = game.enemies[i].y - game.pickups[1].y;
            assert(cx * cx + cy * cy > 2.56f);
        }
        assert(elites == expected_elites[mission]);
        int props = 0, barrels = 0, doors = 0;
        for (int i = 0; i < LAST_ZONE_PICKUPS; ++i)
            assert(last_zone_cell(&game, (int)game.pickups[i].x,
                                  (int)game.pickups[i].y) == 0);
        for (int i = 0; i < LAST_ZONE_PROPS; ++i) {
            if (!game.props[i].active) continue;
            ++props;
            if (game.props[i].kind == 0) ++barrels;
            assert(last_zone_cell(&game, (int)game.props[i].x,
                                  (int)game.props[i].y) == 0);
        }
        for (int y = 0; y < LAST_ZONE_HEIGHT; ++y)
            for (int x = 0; x < LAST_ZONE_WIDTH; ++x)
                if (last_zone_cell(&game, x, y) == 4) ++doors;
        assert(props == expected_props[mission]);
        assert(barrels == expected_barrels[mission]);
        assert(doors >= 2);
        assert_mission_reachable(&game);
        assert(last_zone_cell(&game, (int)last_zone_extract_x(&game),
                              (int)last_zone_extract_y(&game)) == 5);
    }
    game.layout = 1;
    game.phase = LAST_ZONE_PHASE_DEAD;
    last_zone_confirm(&game);
    assert(game.layout == 1 && game.phase == LAST_ZONE_PHASE_PLAYING);
    game.phase = LAST_ZONE_PHASE_WON;
    last_zone_confirm(&game);
    assert(game.layout == 2 && game.phase == LAST_ZONE_PHASE_PLAYING);
    last_zone_reset(&game);
    last_zone_confirm(&game);

    game.armor = 1;
    for (int i = 1; i < LAST_ZONE_ENEMIES; ++i) game.enemies[i].active = false;
    game.enemies[0].x = game.x;
    game.enemies[0].y = game.y;
    game.enemies[0].active = true;
    game.enemy_shot_lock = 0;
    game.hurt_cooldown = 0;
    last_zone_update(&game);
    assert(game.armor == 0);
    assert(game.hp == LAST_ZONE_MAX_HP);

    last_zone_reset(&game);
    last_zone_move_radar(&game, 400, 400);
    assert(game.radar_y + LAST_ZONE_RADAR_SIZE <=
           LAST_ZONE_MOVE_Y - LAST_ZONE_MOVE_R - 8);
    assert(game.radar_x >= 4 && game.radar_x + LAST_ZONE_RADAR_SIZE <= 476);

    last_zone_reset(&game);
    last_zone_confirm(&game);
    for (int i = 1; i < LAST_ZONE_ENEMIES; ++i) game.enemies[i].active = false;
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
    assert(last_zone_fire(&game) == NEON_FIRE_KILL);
    assert(!game.props[0].active && game.props[0].blast_timer == 18);
    assert(!game.enemies[0].active && game.kills == 1);
    assert(game.shots_fired == 1 && game.shots_hit == 1);
    assert(game.last_blast && game.barrel_used);

    last_zone_reset(&game);
    last_zone_confirm(&game);
    uint16_t shots = game.shots_fired;
    last_zone_set_fire_held(&game, true);
    last_zone_update(&game);
    assert(game.last_fire == NEON_FIRE_NONE);
    assert(game.shots_fired == shots);
    last_zone_set_fire_held(&game, false);
    last_zone_update(&game);
    assert(game.last_fire == NEON_FIRE_SHOT);
    assert(game.shots_fired == shots + 1);

    last_zone_reset(&game);
    last_zone_confirm(&game);
    for (int i = 0; i < 10; ++i) {
        last_zone_set_fire_held(&game, true);
        last_zone_update(&game);
    }
    assert(game.holding_breath);

    last_zone_reset(&game);
    last_zone_confirm(&game);
    for (int i = 0; i < LAST_ZONE_ENEMIES; ++i) {
        game.enemies[i].active = false;
        game.enemies[i].hp = 0;
    }
    game.x = last_zone_extract_x(&game);
    game.y = last_zone_extract_y(&game);
    last_zone_update(&game);
    assert(game.phase == LAST_ZONE_PHASE_WON);
    assert(last_zone_on_extract(&game));

    last_zone_reset(&game);
    last_zone_confirm(&game);
    game.enemies[0].ai_state = LAST_ZONE_ENEMY_PATROL;
    game.enemies[0].aim_timer = 0;
    game.enemies[0].x = 8.5f;
    game.enemies[0].y = 2.5f;
    game.enemies[0].active = true;
    game.x = 6.5f;
    game.y = 2.5f;
    last_zone_set_motion(&game, 0, 0, 0);
    last_zone_set_sprint(&game, false);
    for (int i = 0; i < 4; ++i) last_zone_update(&game);
    assert(game.enemies[0].ai_state == LAST_ZONE_ENEMY_PATROL);
    last_zone_set_motion(&game, 1.0f, 0, 0);
    last_zone_set_sprint(&game, true);
    for (int i = 0; i < 8; ++i) last_zone_update(&game);
    assert(game.enemies[0].ai_state != LAST_ZONE_ENEMY_PATROL);
    assert(game.spotted);

    game.layout = 4;
    game.phase = LAST_ZONE_PHASE_WON;
    last_zone_confirm(&game);
    assert(game.layout == 0 && game.phase == LAST_ZONE_PHASE_PLAYING);

    puts("last zone model: ok");
    return 0;
}
