// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define NEON_MAZE_WIDTH 24
#define NEON_MAZE_HEIGHT 24
#define NEON_MAZE_ENEMIES 10
#define NEON_MAZE_MAX_HP 3

typedef enum {
    NEON_MAZE_PHASE_START = 0,
    NEON_MAZE_PHASE_PLAYING,
    NEON_MAZE_PHASE_WON,
    NEON_MAZE_PHASE_DEAD,
} neon_maze_phase_t;

typedef enum {
    NEON_ENEMY_PATROL = 0,
    NEON_ENEMY_ALERT,
    NEON_ENEMY_ENGAGE,
    NEON_ENEMY_SEARCH,
} neon_maze_enemy_state_t;

typedef struct {
    float x,y;
    uint8_t hp,move_phase,hit_flash,death_timer;
    uint8_t ai_state,alert_timer,search_timer,aim_timer,attack_cooldown,attack_flash;
    int8_t nav_dx,nav_dy;
    float last_seen_x,last_seen_y;
    bool active;
} neon_maze_enemy_t;

typedef struct {
    float x, y, angle;
    float look_pitch,weapon_recoil,move_phase,display_hp;
    uint32_t tick;
    uint16_t cells_reached,score;
    uint8_t fire_cooldown,hit_flash,hit_marker,kill_flash,hp,hurt_cooldown;
    float damage_angle;
    neon_maze_phase_t phase;
    bool left, right, forward, backward;
    float move_forward, move_strafe, turn_input;
    float perf_logic_fps,perf_display_fps,perf_render_ms;
    uint8_t door_open[NEON_MAZE_HEIGHT][NEON_MAZE_WIDTH];
    neon_maze_enemy_t enemies[NEON_MAZE_ENEMIES];
} neon_maze_game_t;

void neon_maze_reset(neon_maze_game_t *game);
void neon_maze_confirm(neon_maze_game_t *game);
void neon_maze_set_actions(neon_maze_game_t *game,bool left,bool right,bool forward,
                           bool backward);
void neon_maze_set_motion(neon_maze_game_t *game,float forward,float strafe,float turn);
void neon_maze_turn(neon_maze_game_t *game,float radians);
void neon_maze_look(neon_maze_game_t *game,float pixels);
void neon_maze_set_performance(neon_maze_game_t *game,float logic_fps,
                               float display_fps,float render_ms);
void neon_maze_update(neon_maze_game_t *game);
void neon_maze_fire(neon_maze_game_t *game);
uint8_t neon_maze_cell(int x,int y);
bool neon_maze_blocks(const neon_maze_game_t *game,int x,int y);
int neon_maze_enemies_alive(const neon_maze_game_t *game);
uint32_t neon_maze_state_hash(const neon_maze_game_t *game);
