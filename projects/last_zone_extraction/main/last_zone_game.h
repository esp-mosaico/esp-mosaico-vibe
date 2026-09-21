// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define LAST_ZONE_WIDTH 24
#define LAST_ZONE_HEIGHT 24
#define LAST_ZONE_ENEMIES 12
#define LAST_ZONE_PICKUPS 6
#define LAST_ZONE_LAYOUTS 5
#define LAST_ZONE_MAX_HP 5
#define LAST_ZONE_AMMO_MAX 22
#define LAST_ZONE_AMMO_START 14
#define LAST_ZONE_FIRE_COOLDOWN 18
#define LAST_ZONE_DOOR_COOLDOWN 8
#define LAST_ZONE_DRY_COOLDOWN 12
#define LAST_ZONE_AIM_TICKS 24
#define LAST_ZONE_ALERT_TICKS 18
#define LAST_ZONE_ATTACK_COOLDOWN 72
#define LAST_ZONE_BREATH_TICKS 8
#define LAST_ZONE_HEAR_SPRINT 4.6f
#define LAST_ZONE_HEAR_WALK 2.2f
#define LAST_ZONE_SHOT_NOISE 7.0f
#define LAST_ZONE_PROPS 12
#define LAST_ZONE_MAX_ARMOR 3
#define LAST_ZONE_SPRINT 0.78f
#define LAST_ZONE_MOVE_X 82
#define LAST_ZONE_MOVE_Y 392
#define LAST_ZONE_MOVE_R 64
#define LAST_ZONE_FIRE_X 398
#define LAST_ZONE_FIRE_Y 392
#define LAST_ZONE_FIRE_R 56
#define LAST_ZONE_LOOK_MIN_X 188
#define LAST_ZONE_RADAR_DEFAULT_X 88
#define LAST_ZONE_RADAR_DEFAULT_Y 86
#define LAST_ZONE_RADAR_SIZE 74

typedef enum {
    LAST_ZONE_PHASE_START = 0,
    LAST_ZONE_PHASE_PLAYING,
    LAST_ZONE_PHASE_WON,
    LAST_ZONE_PHASE_DEAD,
} last_zone_phase_t;

typedef enum {
    LAST_ZONE_ENEMY_PATROL = 0,
    LAST_ZONE_ENEMY_ALERT,
    LAST_ZONE_ENEMY_ENGAGE,
    LAST_ZONE_ENEMY_SEARCH,
} last_zone_enemy_state_t;

typedef enum {
    NEON_FIRE_NONE = 0,
    NEON_FIRE_SHOT,
    NEON_FIRE_HIT,
    NEON_FIRE_KILL,
    NEON_FIRE_DRY,
    NEON_FIRE_DOOR,
} last_zone_fire_result_t;

typedef enum {
    NEON_PICKUP_AMMO = 0,
    NEON_PICKUP_HEALTH,
    NEON_PICKUP_ARMOR,
} last_zone_pickup_kind_t;

typedef struct {
    float x,y;
    uint8_t hp,move_phase,hit_flash,death_timer;
    uint8_t ai_state,alert_timer,search_timer,aim_timer,attack_cooldown,attack_flash;
    int8_t nav_dx,nav_dy,hold_x,hold_y;
    float last_seen_x,last_seen_y;
    bool active,elite;
} last_zone_enemy_t;

typedef struct {
    float x,y;
    uint8_t kind;
    bool taken;
} last_zone_pickup_t;

typedef struct {
    float x,y;
    uint8_t kind,blast_timer;
    bool active;
} last_zone_prop_t;

typedef struct {
    float x, y, angle;
    float look_pitch,look_kick,weapon_recoil,move_phase,display_hp,vel_x,vel_y;
    uint32_t tick,best_ticks,layout_best[LAST_ZONE_LAYOUTS];
    uint16_t cells_reached,score,shots_fired,shots_hit,kills;
    uint8_t fire_cooldown,hit_flash,hit_marker,kill_flash,hp,armor,hurt_cooldown,ammo;
    uint8_t pickup_flash,door_flash,dry_flash,layout,damage_taken,enemy_shot_lock;
    uint8_t unlocked,breath_hold,alert_flash;
    int16_t radar_x,radar_y;
    float damage_angle;
    last_zone_phase_t phase;
    bool left, right, forward, backward, fire_held, fire_pressed, fire_released, best_updated;
    bool last_pickup,last_alert,sprinting,sprint_held,holding_breath;
    bool spotted,barrel_used,armor_hit,last_blast;
    uint8_t sfx,sfx_hold,step_beat;
    last_zone_fire_result_t last_fire;
    float move_forward, move_strafe, turn_input;
    float perf_logic_fps,perf_display_fps,perf_render_ms;
    uint8_t door_open[LAST_ZONE_HEIGHT][LAST_ZONE_WIDTH];
    uint8_t explored[LAST_ZONE_HEIGHT][LAST_ZONE_WIDTH];
    last_zone_enemy_t enemies[LAST_ZONE_ENEMIES];
    last_zone_pickup_t pickups[LAST_ZONE_PICKUPS];
    last_zone_prop_t props[LAST_ZONE_PROPS];
} last_zone_game_t;

void last_zone_reset(last_zone_game_t *game);
void last_zone_set_best(last_zone_game_t *game,uint32_t ticks);
void last_zone_confirm(last_zone_game_t *game);
void last_zone_set_actions(last_zone_game_t *game,bool left,bool right,bool forward,
                           bool backward);
void last_zone_set_motion(last_zone_game_t *game,float forward,float strafe,float turn);
void last_zone_set_sprint(last_zone_game_t *game,bool sprint);
void last_zone_set_fire_held(last_zone_game_t *game,bool held);
const char *last_zone_sfx_name(const last_zone_game_t *game);
void last_zone_turn(last_zone_game_t *game,float radians);
void last_zone_look(last_zone_game_t *game,float pixels);
void last_zone_settle_look(last_zone_game_t *game);
void last_zone_set_performance(last_zone_game_t *game,float logic_fps,
                               float display_fps,float render_ms);
void last_zone_update(last_zone_game_t *game);
last_zone_fire_result_t last_zone_fire(last_zone_game_t *game);
uint8_t last_zone_cell(const last_zone_game_t *game,int x,int y);
bool last_zone_blocks(const last_zone_game_t *game,int x,int y);
bool last_zone_door_ahead(const last_zone_game_t *game);
bool last_zone_near_closed_door(const last_zone_game_t *game);
bool last_zone_pickup_visible(const last_zone_game_t *game,int index);
bool last_zone_enemy_on_radar(const last_zone_game_t *game,int index);
int last_zone_enemies_alive(const last_zone_game_t *game);
int last_zone_enemy_total(const last_zone_game_t *game);
int last_zone_last_enemy_index(const last_zone_game_t *game);
float last_zone_extract_bearing(const last_zone_game_t *game);
float last_zone_extract_x(const last_zone_game_t *game);
float last_zone_extract_y(const last_zone_game_t *game);
float last_zone_spawn_x(const last_zone_game_t *game);
float last_zone_spawn_y(const last_zone_game_t *game);
const char *last_zone_briefing(const last_zone_game_t *game);
char last_zone_grade(const last_zone_game_t *game);
uint32_t last_zone_state_hash(const last_zone_game_t *game);
bool last_zone_in_move_zone(int x,int y);
bool last_zone_in_move_capture(int x,int y);
bool last_zone_in_fire_zone(int x,int y);
bool last_zone_in_radar(const last_zone_game_t *game,int x,int y);
void last_zone_move_radar(last_zone_game_t *game,int x,int y);
bool last_zone_on_extract(const last_zone_game_t *game);
