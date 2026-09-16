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
#include "neon_maze_save.h"
#include "neon_maze_view.h"
#include <math.h>

static neon_maze_game_t s_game;
static MosaicoAtlas s_enemies, s_weapon, s_environment, s_materials, s_controls, s_props;
static int32_t s_joystick_track = -1, s_look_track = -1, s_fire_track = -1;
static float s_move_forward, s_move_strafe, s_target_forward, s_target_strafe;
static int32_t s_look_x, s_look_y;
static Sound s_rifle_sound, s_impact_sound, s_confirm_sound, s_hurt_sound;
static Sound s_empty_sound, s_pickup_sound, s_alert_sound, s_step_l, s_step_r;
static uint8_t s_step_wait, s_enemy_step_wait;
static bool s_step_right;
static TimerHandle_t s_haptic_timer, s_haptic_second_timer;
static bool s_motor_ready;
static uint8_t s_previous_hp;
static neon_maze_phase_t s_previous_phase;
static uint8_t s_second_strength;
static uint16_t s_second_duration_ms;

#define JOYSTICK_RADIUS 58

extern const uint8_t _binary_enemy_atlas_start[], _binary_enemy_atlas_end[];
extern const uint8_t _binary_weapon_atlas_start[], _binary_weapon_atlas_end[];
extern const uint8_t _binary_controls_atlas_start[], _binary_controls_atlas_end[];
extern const uint8_t _binary_environment_atlas_start[];
extern const uint8_t _binary_environment_atlas_end[];
extern const uint8_t _binary_materials_atlas_start[];
extern const uint8_t _binary_materials_atlas_end[];
extern const uint8_t _binary_props_atlas_start[];
extern const uint8_t _binary_props_atlas_end[];
extern const uint8_t _binary_neon_rifle_start[], _binary_neon_rifle_end[];
extern const uint8_t _binary_neon_impact_start[], _binary_neon_impact_end[];
extern const uint8_t _binary_neon_confirm_start[], _binary_neon_confirm_end[];
extern const uint8_t _binary_neon_hurt_start[], _binary_neon_hurt_end[];
extern const uint8_t _binary_neon_empty_start[], _binary_neon_empty_end[];
extern const uint8_t _binary_neon_pickup_start[], _binary_neon_pickup_end[];
extern const uint8_t _binary_neon_alert_start[], _binary_neon_alert_end[];
extern const uint8_t _binary_neon_step_l_start[], _binary_neon_step_l_end[];
extern const uint8_t _binary_neon_step_r_start[], _binary_neon_step_r_end[];

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
    err = mosaico_game_asset_register_memory("empty.sound", _binary_neon_empty_start,
        (size_t)(_binary_neon_empty_end - _binary_neon_empty_start));
    if (err != ESP_OK) return err;
    err = mosaico_game_asset_register_memory("pickup.sound", _binary_neon_pickup_start,
        (size_t)(_binary_neon_pickup_end - _binary_neon_pickup_start));
    if (err != ESP_OK) return err;
    err = mosaico_game_asset_register_memory("alert.sound", _binary_neon_alert_start,
        (size_t)(_binary_neon_alert_end - _binary_neon_alert_start));
    if (err != ESP_OK) return err;
    err = mosaico_game_asset_register_memory("step_l.sound", _binary_neon_step_l_start,
        (size_t)(_binary_neon_step_l_end - _binary_neon_step_l_start));
    if (err != ESP_OK) return err;
    err = mosaico_game_asset_register_memory("step_r.sound", _binary_neon_step_r_start,
        (size_t)(_binary_neon_step_r_end - _binary_neon_step_r_start));
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
        "props.atlas", _binary_props_atlas_start,
        (size_t)(_binary_props_atlas_end - _binary_props_atlas_start));
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
    s_props = LoadMosaicoAtlas("props.atlas");
    return s_enemies.texture.id && s_weapon.texture.id && s_controls.texture.id &&
           s_environment.texture.id && s_materials.texture.id && s_props.texture.id
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
    s_empty_sound = LoadSound("empty.sound");
    s_pickup_sound = LoadSound("pickup.sound");
    s_alert_sound = LoadSound("alert.sound");
    s_step_l = LoadSound("step_l.sound");
    s_step_r = LoadSound("step_r.sound");
    SetSoundVolume(s_rifle_sound, .46f);
    SetSoundVolume(s_impact_sound, .38f);
    SetSoundVolume(s_confirm_sound, .32f);
    SetSoundVolume(s_hurt_sound, .62f);
    SetSoundVolume(s_empty_sound, .40f);
    SetSoundVolume(s_pickup_sound, .44f);
    SetSoundVolume(s_alert_sound, .48f);
    SetSoundVolume(s_step_l, .34f);
    SetSoundVolume(s_step_r, .34f);
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

static void apply_combat_feedback(void)
{
    switch (s_game.last_fire) {
    case NEON_FIRE_SHOT:
        PlaySound(s_rifle_sound);
        play_haptic(38, 18);
        break;
    case NEON_FIRE_HIT:
        PlaySound(s_rifle_sound);
        PlaySound(s_impact_sound);
        play_haptic(48, 22);
        break;
    case NEON_FIRE_KILL:
        PlaySound(s_rifle_sound);
        PlaySound(s_impact_sound);
        PlaySound(s_confirm_sound);
        play_haptic_pattern(72, 22, 58, 18, 28);
        break;
    case NEON_FIRE_DOOR:
        PlaySound(s_confirm_sound);
        play_haptic(58, 40);
        break;
    case NEON_FIRE_DRY:
        PlaySound(s_empty_sound);
        play_haptic(28, 18);
        break;
    default:
        break;
    }
    if (s_game.last_pickup) {
        PlaySound(s_pickup_sound);
        play_haptic(36, 24);
    }
    if (s_game.last_alert && !IsSoundPlaying(s_rifle_sound) &&
        !IsSoundPlaying(s_alert_sound)) PlaySound(s_alert_sound);
}

static void apply_footsteps(void)
{
    if (s_game.phase != NEON_MAZE_PHASE_PLAYING) {
        s_step_wait = 0;
        s_enemy_step_wait = 0;
        return;
    }
    float motion = sqrtf(s_game.move_forward * s_game.move_forward +
                         s_game.move_strafe * s_game.move_strafe);
    if (motion > 1.0f) motion = 1.0f;
    bool combat_busy = IsSoundPlaying(s_rifle_sound) || IsSoundPlaying(s_hurt_sound) ||
                       IsSoundPlaying(s_alert_sound);
    if (motion > .18f) {
        if (!s_step_wait) {
            if (!combat_busy) {
                SetSoundVolume(s_step_right ? s_step_r : s_step_l,
                               .28f + motion * .14f);
                PlaySound(s_step_right ? s_step_r : s_step_l);
                play_haptic((uint8_t)(14 + motion * 10.0f), 10);
            }
            s_step_right = !s_step_right;
            s_step_wait = s_game.sprinting ? 8 : (uint8_t)(13 - (int)(motion * 5.0f));
        } else {
            --s_step_wait;
        }
    } else {
        s_step_wait = 0;
    }
    float nearest = 99.0f;
    for (int i = 0; i < NEON_MAZE_ENEMIES; ++i) {
        if (!s_game.enemies[i].active) continue;
        if (s_game.enemies[i].ai_state == NEON_ENEMY_ALERT) continue;
        float dx = s_game.enemies[i].x - s_game.x;
        float dy = s_game.enemies[i].y - s_game.y;
        float dist = sqrtf(dx * dx + dy * dy);
        if (dist < nearest) nearest = dist;
    }
    if (nearest > .7f && nearest < 4.2f) {
        if (!s_enemy_step_wait) {
            if (!combat_busy && !IsSoundPlaying(s_step_l) && !IsSoundPlaying(s_step_r)) {
                float falloff = 1.0f - nearest / 4.2f;
                SetSoundVolume(s_step_l, .10f + falloff * .22f);
                PlaySound(s_step_l);
            }
            s_enemy_step_wait = 16;
        } else {
            --s_enemy_step_wait;
        }
    } else {
        s_enemy_step_wait = 0;
    }
}

static esp_err_t on_start(void)
{
    neon_maze_reset(&s_game);
    uint32_t best = 0;
    if (neon_maze_save_load(&best) == ESP_OK) neon_maze_set_best(&s_game, best);
    s_joystick_track = s_look_track = s_fire_track = -1;
    s_move_forward = s_move_strafe = 0.0f;
    s_target_forward = s_target_strafe = 0.0f;
    s_previous_hp = s_game.hp;
    s_previous_phase = s_game.phase;
    return ESP_OK;
}

static void update_joystick(int32_t x, int32_t y)
{
    float dx = (float)(x - NEON_MAZE_MOVE_X) / JOYSTICK_RADIUS;
    float dy = (float)(y - NEON_MAZE_MOVE_Y) / JOYSTICK_RADIUS;
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
        if (s_game.phase != NEON_MAZE_PHASE_PLAYING || !event->pressed) {
            s_joystick_track = s_look_track = s_fire_track = -1;
            s_target_forward = s_target_strafe = 0.0f;
            return;
        }
    }
    const int32_t track = event->value;
    if (!event->pressed) {
        if (track == s_joystick_track) {
            s_joystick_track = -1;
            s_target_forward = s_target_strafe = 0.0f;
        }
        if (track == s_look_track) s_look_track = -1;
        if (track == s_fire_track) s_fire_track = -1;
        return;
    }
    if (track == s_joystick_track) {
        update_joystick(event->x, event->y);
    } else if (track == s_look_track) {
        neon_maze_turn(&s_game, (float)(event->x - s_look_x) * .008f);
        neon_maze_look(&s_game, (float)(event->y - s_look_y) * -.09f);
        s_look_x = event->x;
        s_look_y = event->y;
    } else if (track == s_fire_track) {
        return;
    } else if (neon_maze_in_fire_zone(event->x, event->y) && s_fire_track < 0) {
        s_fire_track = track;
    } else if (neon_maze_in_move_zone(event->x, event->y) && s_joystick_track < 0) {
        s_joystick_track = track;
        update_joystick(event->x, event->y);
    } else if (event->x >= NEON_MAZE_LOOK_MIN_X && s_look_track < 0) {
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
    neon_maze_set_fire_held(&s_game, s_fire_track >= 0);
    if (s_look_track < 0) neon_maze_settle_look(&s_game);
    neon_maze_update(&s_game);
    if (s_previous_phase != NEON_MAZE_PHASE_WON &&
        s_game.phase == NEON_MAZE_PHASE_WON && s_game.best_updated) {
        (void)neon_maze_save_best(s_game.best_ticks);
        (void)neon_maze_save_flush();
    }
    s_previous_phase = s_game.phase;
    apply_combat_feedback();
    if (s_game.hp < s_previous_hp) {
        PlaySound(s_hurt_sound);
        if (s_game.hp) play_haptic_pattern(82, 35, 55, 25, 45);
        else play_haptic_pattern(96, 90, 70, 35, 120);
    }
    s_previous_hp = s_game.hp;
    apply_footsteps();
    if (s_game.phase == NEON_MAZE_PHASE_PLAYING && s_game.hp == 1 &&
        (s_game.tick % 24U) == 0U && !IsSoundPlaying(s_hurt_sound) &&
        !IsSoundPlaying(s_rifle_sound)) {
        SetSoundVolume(s_hurt_sound, .28f);
        PlaySound(s_hurt_sound);
        play_haptic(22, 14);
        SetSoundVolume(s_hurt_sound, .62f);
    }
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
                          s_controls, s_props);
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
