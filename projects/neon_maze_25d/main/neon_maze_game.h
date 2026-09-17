// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define NEON_MAZE_WIDTH 24
#define NEON_MAZE_HEIGHT 24
#define NEON_MAZE_ENEMIES 10
#define NEON_MAZE_PICKUPS 6
#define NEON_MAZE_LAYOUTS 3
#define NEON_MAZE_MAX_HP 5
#define NEON_MAZE_AMMO_MAX 22
#define NEON_MAZE_AMMO_START 14
#define NEON_MAZE_FIRE_COOLDOWN 18
#define NEON_MAZE_DOOR_COOLDOWN 8
#define NEON_MAZE_DRY_COOLDOWN 12
#define NEON_MAZE_AIM_TICKS 20
#define NEON_MAZE_ALERT_TICKS 14
#define NEON_MAZE_ATTACK_COOLDOWN 64
#define NEON_MAZE_EXTRACT_X 21.5f
#define NEON_MAZE_EXTRACT_Y 21.5f
#define NEON_MAZE_PROPS 8
#define NEON_MAZE_MAX_ARMOR 3
#define NEON_MAZE_SPRINT 0.78f
#define NEON_MAZE_MOVE_X 82
#define NEON_MAZE_MOVE_Y 392
#define NEON_MAZE_MOVE_R 64
#define NEON_MAZE_FIRE_X 398
#define NEON_MAZE_FIRE_Y 392
#define NEON_MAZE_FIRE_R 56
#define NEON_MAZE_LOOK_MIN_X 188

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

typedef enum {
    NEON_FIRE_NONE = 0,
    NEON_FIRE_SHOT,
    NEON_FIRE_HIT,
    NEON_FIRE_KILL,
    NEON_FIRE_DRY,
    NEON_FIRE_DOOR,
} neon_maze_fire_result_t;

typedef enum {
    NEON_PICKUP_AMMO = 0,
    NEON_PICKUP_HEALTH,
    NEON_PICKUP_ARMOR,
} neon_maze_pickup_kind_t;

typedef struct {
    float x,y;
    uint8_t hp,move_phase,hit_flash,death_timer;
    uint8_t ai_state,alert_timer,search_timer,aim_timer,attack_cooldown,attack_flash;
    int8_t nav_dx,nav_dy,hold_x,hold_y;
    float last_seen_x,last_seen_y;
    bool active;
} neon_maze_enemy_t;

typedef struct {
    float x,y;
    uint8_t kind;
    bool taken;
} neon_maze_pickup_t;

typedef struct {
    float x,y;
    uint8_t kind,blast_timer;
    bool active;
} neon_maze_prop_t;

typedef struct {
    float x, y, angle;
    float look_pitch,look_kick,weapon_recoil,move_phase,display_hp,vel_x,vel_y;
    uint32_t tick,best_ticks;
    uint16_t cells_reached,score,shots_fired,shots_hit,kills;
    uint8_t fire_cooldown,hit_flash,hit_marker,kill_flash,hp,armor,hurt_cooldown,ammo;
    uint8_t pickup_flash,door_flash,dry_flash,layout,damage_taken,enemy_shot_lock;
    float damage_angle;
    neon_maze_phase_t phase;
    bool left, right, forward, backward, fire_held, fire_pressed, best_updated;
    bool last_pickup,last_alert,sprinting,sprint_held;
    uint8_t sfx,sfx_hold,step_beat;
    neon_maze_fire_result_t last_fire;
    float move_forward, move_strafe, turn_input;
    float perf_logic_fps,perf_display_fps,perf_render_ms;
    uint8_t door_open[NEON_MAZE_HEIGHT][NEON_MAZE_WIDTH];
    uint8_t explored[NEON_MAZE_HEIGHT][NEON_MAZE_WIDTH];
    neon_maze_enemy_t enemies[NEON_MAZE_ENEMIES];
    neon_maze_pickup_t pickups[NEON_MAZE_PICKUPS];
    neon_maze_prop_t props[NEON_MAZE_PROPS];
} neon_maze_game_t;

void neon_maze_reset(neon_maze_game_t *game);
void neon_maze_set_best(neon_maze_game_t *game,uint32_t ticks);
void neon_maze_confirm(neon_maze_game_t *game);
void neon_maze_set_actions(neon_maze_game_t *game,bool left,bool right,bool forward,
                           bool backward);
void neon_maze_set_motion(neon_maze_game_t *game,float forward,float strafe,float turn);
void neon_maze_set_sprint(neon_maze_game_t *game,bool sprint);
void neon_maze_set_fire_held(neon_maze_game_t *game,bool held);
const char *neon_maze_sfx_name(const neon_maze_game_t *game);
void neon_maze_turn(neon_maze_game_t *game,float radians);
void neon_maze_look(neon_maze_game_t *game,float pixels);
void neon_maze_settle_look(neon_maze_game_t *game);
void neon_maze_set_performance(neon_maze_game_t *game,float logic_fps,
                               float display_fps,float render_ms);
void neon_maze_update(neon_maze_game_t *game);
neon_maze_fire_result_t neon_maze_fire(neon_maze_game_t *game);
uint8_t neon_maze_cell(const neon_maze_game_t *game,int x,int y);
bool neon_maze_blocks(const neon_maze_game_t *game,int x,int y);
bool neon_maze_door_ahead(const neon_maze_game_t *game);
bool neon_maze_near_closed_door(const neon_maze_game_t *game);
bool neon_maze_pickup_visible(const neon_maze_game_t *game,int index);
bool neon_maze_enemy_on_radar(const neon_maze_game_t *game,int index);
int neon_maze_enemies_alive(const neon_maze_game_t *game);
int neon_maze_enemy_total(const neon_maze_game_t *game);
int neon_maze_last_enemy_index(const neon_maze_game_t *game);
float neon_maze_extract_bearing(const neon_maze_game_t *game);
char neon_maze_grade(const neon_maze_game_t *game);
uint32_t neon_maze_state_hash(const neon_maze_game_t *game);
bool neon_maze_in_move_zone(int x,int y);
bool neon_maze_in_fire_zone(int x,int y);
