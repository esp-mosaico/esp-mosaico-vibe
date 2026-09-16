// SPDX-License-Identifier: Apache-2.0
#define GSP_BUNDLE_ENABLE_RAW_IDS 1
#include "bundle_gsp.h"
#include "esp_log.h"
#include "mosaico_game_assets.h"
#include "mosaico_game.h"
#include "mosaico_game_audio.h"
#include "bsp/esp_mosaico.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "neon_maze_app.h"
#include "neon_maze_game.h"
#include "neon_maze_view.h"
#include <math.h>

static neon_maze_game_t s_game;
static MosaicoAtlas s_enemies, s_weapon, s_environment, s_materials, s_controls;
static int32_t s_joystick_track = -1, s_look_track = -1;
static float s_move_forward, s_move_strafe, s_target_forward, s_target_strafe;
static int32_t s_look_x, s_look_y;
static Sound s_rifle_sound, s_impact_sound, s_confirm_sound, s_hurt_sound;
static TimerHandle_t s_haptic_timer, s_haptic_second_timer;
static bool s_motor_ready;
static uint8_t s_previous_hp;
static uint8_t s_second_strength;
static uint16_t s_second_duration_ms;

#define JOYSTICK_X 82
#define JOYSTICK_Y 392
#define JOYSTICK_RADIUS 58
#define FIRE_X 410
#define FIRE_Y 392
#define FIRE_RADIUS 48

extern const uint8_t _binary_enemy_atlas_start[], _binary_enemy_atlas_end[];
extern const uint8_t _binary_weapon_atlas_start[], _binary_weapon_atlas_end[];
extern const uint8_t _binary_controls_atlas_start[], _binary_controls_atlas_end[];
extern const uint8_t _binary_environment_atlas_start[];
extern const uint8_t _binary_environment_atlas_end[];
extern const uint8_t _binary_materials_atlas_start[];
extern const uint8_t _binary_materials_atlas_end[];
extern const uint8_t _binary_neon_rifle_start[], _binary_neon_rifle_end[];
extern const uint8_t _binary_neon_impact_start[], _binary_neon_impact_end[];
extern const uint8_t _binary_neon_confirm_start[], _binary_neon_confirm_end[];
extern const uint8_t _binary_neon_hurt_start[], _binary_neon_hurt_end[];

static esp_err_t before_display(void)
{
    esp_err_t err = mosaico_game_asset_register_memory(
        "enemy.atlas", _binary_enemy_atlas_start,
        (size_t)(_binary_enemy_atlas_end - _binary_enemy_atlas_start));
    if (err != ESP_OK) return err;
    err = mosaico_game_asset_register_memory("rifle.sound", _binary_neon_rifle_start,
        (size_t)(_binary_neon_rifle_end - _binary_neon_rifle_start));
    if (err != ESP_OK) return err;
    err = mosaico_game_asset_register_memory("impact.sound", _binary_neon_impact_start,
        (size_t)(_binary_neon_impact_end - _binary_neon_impact_start));
    if (err != ESP_OK) return err;
    err = mosaico_game_asset_register_memory("confirm.sound", _binary_neon_confirm_start,
        (size_t)(_binary_neon_confirm_end - _binary_neon_confirm_start));
    if (err != ESP_OK) return err;
    err = mosaico_game_asset_register_memory("hurt.sound", _binary_neon_hurt_start,
        (size_t)(_binary_neon_hurt_end - _binary_neon_hurt_start));
    if (err != ESP_OK) return err;
    err = mosaico_game_asset_register_memory("controls.atlas", _binary_controls_atlas_start,
        (size_t)(_binary_controls_atlas_end - _binary_controls_atlas_start));
    if (err != ESP_OK) return err;
    err = mosaico_game_asset_register_memory("weapon.atlas", _binary_weapon_atlas_start,
        (size_t)(_binary_weapon_atlas_end - _binary_weapon_atlas_start));
    if (err != ESP_OK) return err;
    err = mosaico_game_asset_register_memory(
        "materials.atlas", _binary_materials_atlas_start,
        (size_t)(_binary_materials_atlas_end - _binary_materials_atlas_start));
    if (err != ESP_OK) return err;
    err = mosaico_game_asset_register_memory(
        "environment.atlas", _binary_environment_atlas_start,
        (size_t)(_binary_environment_atlas_end - _binary_environment_atlas_start));
    if (err != ESP_OK) return err;
    s_enemies = LoadMosaicoAtlas("enemy.atlas");
    s_weapon = LoadMosaicoAtlas("weapon.atlas");
    s_controls = LoadMosaicoAtlas("controls.atlas");
    s_environment = LoadMosaicoAtlas("environment.atlas");
    s_materials = LoadMosaicoAtlas("materials.atlas");
    return s_enemies.texture.id && s_weapon.texture.id && s_controls.texture.id &&
           s_environment.texture.id && s_materials.texture.id
        ? ESP_OK : ESP_ERR_NOT_FOUND;
}

static void haptic_stop(TimerHandle_t timer)
{
    (void)timer;
    if (s_motor_ready) (void)bsp_motor_set(false);
}

static void haptic_second(TimerHandle_t timer)
{
    (void)timer;
    if (!s_motor_ready || !s_haptic_timer || !s_second_duration_ms) return;
    (void)bsp_motor_set_strength(s_second_strength);
    (void)bsp_motor_set(true);
    (void)xTimerChangePeriod(s_haptic_timer, pdMS_TO_TICKS(s_second_duration_ms), 0);
    (void)xTimerStart(s_haptic_timer, 0);
    s_second_duration_ms = 0;
}

static esp_err_t after_healthy(void)
{
    InitAudioDevice();
    s_rifle_sound = LoadSound("rifle.sound");
    s_impact_sound = LoadSound("impact.sound");
    s_confirm_sound = LoadSound("confirm.sound");
    s_hurt_sound = LoadSound("hurt.sound");
    SetSoundVolume(s_rifle_sound, .46f);
    SetSoundVolume(s_impact_sound, .38f);
    SetSoundVolume(s_confirm_sound, .32f);
    SetSoundVolume(s_hurt_sound, .62f);
    s_motor_ready = bsp_motor_init() == ESP_OK;
    if (s_motor_ready) {
        (void)bsp_motor_set(false);
        s_haptic_timer = xTimerCreate("maze_haptic", pdMS_TO_TICKS(35), pdFALSE,
                                     NULL, haptic_stop);
        s_haptic_second_timer = xTimerCreate("maze_haptic2", pdMS_TO_TICKS(60), pdFALSE,
                                            NULL, haptic_second);
    }
    return ESP_OK;
}

static void play_haptic(uint8_t strength, uint16_t duration_ms)
{
    if (!s_motor_ready || !s_haptic_timer) return;
    if (s_haptic_second_timer) (void)xTimerStop(s_haptic_second_timer, 0);
    s_second_duration_ms = 0;
    (void)bsp_motor_set_strength(strength);
    (void)xTimerStop(s_haptic_timer, 0);
    (void)xTimerChangePeriod(s_haptic_timer, pdMS_TO_TICKS(duration_ms), 0);
    (void)xTimerStart(s_haptic_timer, 0);
}

static void play_haptic_pattern(uint8_t first_strength, uint16_t first_ms,
                                uint8_t second_strength, uint16_t gap_ms,
                                uint16_t second_ms)
{
    play_haptic(first_strength, first_ms);
    if (!s_haptic_second_timer) return;
    s_second_strength = second_strength;
    s_second_duration_ms = second_ms;
    (void)xTimerChangePeriod(s_haptic_second_timer, pdMS_TO_TICKS(first_ms + gap_ms), 0);
    (void)xTimerStart(s_haptic_second_timer, 0);
}

static void fire_with_feedback(void)
{
    if (s_game.fire_cooldown) return;
    uint16_t score_before = s_game.score;
    neon_maze_fire(&s_game);
    PlaySound(s_rifle_sound);
    play_haptic(42, 30);
    if (s_game.score > score_before) {
        PlaySound(s_impact_sound);
        play_haptic(68, 52);
        if (s_game.score - score_before >= 100) {
            PlaySound(s_confirm_sound);
            play_haptic(86, 85);
        }
    }
}

static esp_err_t on_start(void)
{
    neon_maze_reset(&s_game);
    s_joystick_track = s_look_track = -1;
    s_move_forward = s_move_strafe = 0.0f;
    s_target_forward = s_target_strafe = 0.0f;
    s_previous_hp = s_game.hp;
    return ESP_OK;
}

static bool inside_circle(int32_t x, int32_t y, int32_t cx, int32_t cy, int32_t radius)
{
    const int32_t dx = x - cx, dy = y - cy;
    return dx * dx + dy * dy <= radius * radius;
}

static void update_joystick(int32_t x, int32_t y)
{
    float dx = (float)(x - JOYSTICK_X) / JOYSTICK_RADIUS;
    float dy = (float)(y - JOYSTICK_Y) / JOYSTICK_RADIUS;
    float length = sqrtf(dx * dx + dy * dy);
    if (length > 1.0f) { dx /= length; dy /= length; }
    if (length < .12f) dx = dy = 0.0f;
    s_target_strafe = dx;
    s_target_forward = -dy;
}

static void on_event(const mosaico_device_event_t *event)
{
    if (event->type != MOSAICO_DEVICE_EVENT_POINTER &&
        event->type != MOSAICO_DEVICE_EVENT_TOUCH) return;
    if (s_game.phase != NEON_MAZE_PHASE_PLAYING) {
        if (event->pressed) neon_maze_confirm(&s_game);
        return;
    }
    const int32_t track = event->value;
    if (!event->pressed) {
        if (track == s_joystick_track) {
            s_joystick_track = -1;
            s_target_forward = s_target_strafe = 0.0f;
        }
        if (track == s_look_track) s_look_track = -1;
        return;
    }
    if (track == s_joystick_track) {
        update_joystick(event->x, event->y);
    } else if (track == s_look_track) {
        neon_maze_turn(&s_game, (float)(event->x - s_look_x) * .008f);
        neon_maze_look(&s_game, (float)(event->y - s_look_y) * -.18f);
        s_look_x = event->x;
        s_look_y = event->y;
    } else if (inside_circle(event->x, event->y, FIRE_X, FIRE_Y, FIRE_RADIUS)) {
        fire_with_feedback();
    } else if (inside_circle(event->x, event->y, JOYSTICK_X, JOYSTICK_Y, 74) &&
               s_joystick_track < 0) {
        s_joystick_track = track;
        update_joystick(event->x, event->y);
    } else if (event->x >= 180 && s_look_track < 0) {
        s_look_track = track;
        s_look_x = event->x;
        s_look_y = event->y;
    }
}

static void on_update(void)
{
    s_move_forward += (s_target_forward - s_move_forward) * .38f;
    s_move_strafe += (s_target_strafe - s_move_strafe) * .38f;
    if (fabsf(s_move_forward) < .01f) s_move_forward = 0.0f;
    if (fabsf(s_move_strafe) < .01f) s_move_strafe = 0.0f;
    neon_maze_set_motion(&s_game, s_move_forward, s_move_strafe, 0.0f);
    neon_maze_update(&s_game);
    if (s_game.hp < s_previous_hp) {
        PlaySound(s_hurt_sound);
        if (s_game.hp) play_haptic_pattern(82, 35, 55, 25, 45);
        else play_haptic_pattern(96, 90, 70, 35, 120);
    }
    s_previous_hp = s_game.hp;
    if ((s_game.tick % 15U) == 0U) {
        mosaico_game_stats_t stats;
        MosaicoGameGetStats(&stats);
        neon_maze_set_performance(&s_game, stats.logic_fps, stats.display_fps,
                                   stats.render_us / 1000.0f);
    }
}

static void on_render(void)
{
    neon_maze_view_render(&s_game, s_enemies, s_weapon, s_environment, s_materials,
                          s_controls);
}

static void on_stats(void)
{
    ESP_LOGI("neon_maze", "pos=%.2f,%.2f score=%u state_hash=%08lx",
             s_game.x, s_game.y, s_game.score,
             (unsigned long)neon_maze_state_hash(&s_game));
}

static const mosaico_game_app_config_t s_config = {
    .tag = "neon_maze",
    .window_title = "Neon Maze 2.5D",
    .canvas_bind = GSP_NEON_MAZE_25D_BIND_GAME_CANVAS,
    .touch_points = 2,
    .enable_imu = false,
    .target_fps = 30,
    .gsp_bundle = gsp_bundle_config,
    .before_display = before_display,
    .after_healthy = after_healthy,
    .on_start = on_start,
    .on_event = on_event,
    .on_update = on_update,
    .on_render = on_render,
    .on_stats = on_stats,
};

const mosaico_game_app_config_t *neon_maze_app_config(void)
{
    return &s_config;
}
