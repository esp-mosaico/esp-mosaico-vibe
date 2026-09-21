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
#include "last_zone_app.h"
#include "last_zone_game.h"
#include "last_zone_save.h"
#include "last_zone_view.h"
#include "mosaico_game_2d.h"
#include <math.h>

static last_zone_game_t s_game;
static MosaicoAtlas s_enemies, s_weapon, s_environment, s_materials, s_controls, s_props;
static int32_t s_joystick_track = -1, s_look_track = -1, s_fire_track = -1,
               s_radar_track = -1;
static float s_move_forward, s_move_strafe;
static int32_t s_stick_x, s_stick_y;
static int32_t s_look_x, s_look_y;
static int32_t s_radar_dx, s_radar_dy;
static Sound s_rifle_sound, s_bolt_sound, s_impact_sound, s_confirm_sound, s_hurt_sound;
static Sound s_empty_sound, s_pickup_sound, s_alert_sound, s_step_l, s_step_r;
static Sound s_explode_sound, s_extract_sound;
static Music s_music;
static uint8_t s_step_wait, s_enemy_step_wait;
static bool s_step_right;
static TimerHandle_t s_haptic_timer, s_haptic_second_timer;
static bool s_motor_ready;
static uint8_t s_previous_hp, s_previous_armor;
static uint8_t s_previous_fire_cooldown;
static last_zone_phase_t s_previous_phase;
static uint8_t s_second_strength;
static uint16_t s_second_duration_ms;

#define TAG "last_zone"
#define JOYSTICK_RADIUS 64

extern const uint8_t _binary_enemy_atlas_start[], _binary_enemy_atlas_end[];
extern const uint8_t _binary_weapon_atlas_start[], _binary_weapon_atlas_end[];
extern const uint8_t _binary_controls_atlas_start[], _binary_controls_atlas_end[];
extern const uint8_t _binary_environment_atlas_start[];
extern const uint8_t _binary_environment_atlas_end[];
extern const uint8_t _binary_materials_atlas_start[];
extern const uint8_t _binary_materials_atlas_end[];
extern const uint8_t _binary_props_atlas_start[];
extern const uint8_t _binary_props_atlas_end[];
extern const uint8_t _binary_last_zone_rifle_start[], _binary_last_zone_rifle_end[];
extern const uint8_t _binary_last_zone_bolt_start[], _binary_last_zone_bolt_end[];
extern const uint8_t _binary_last_zone_impact_start[], _binary_last_zone_impact_end[];
extern const uint8_t _binary_last_zone_confirm_start[], _binary_last_zone_confirm_end[];
extern const uint8_t _binary_last_zone_hurt_start[], _binary_last_zone_hurt_end[];
extern const uint8_t _binary_last_zone_empty_start[], _binary_last_zone_empty_end[];
extern const uint8_t _binary_last_zone_pickup_start[], _binary_last_zone_pickup_end[];
extern const uint8_t _binary_last_zone_alert_start[], _binary_last_zone_alert_end[];
extern const uint8_t _binary_last_zone_step_l_start[], _binary_last_zone_step_l_end[];
extern const uint8_t _binary_last_zone_step_r_start[], _binary_last_zone_step_r_end[];
extern const uint8_t _binary_last_zone_explode_start[], _binary_last_zone_explode_end[];
extern const uint8_t _binary_last_zone_extract_start[], _binary_last_zone_extract_end[];
extern const uint8_t _binary_last_zone_music_start[], _binary_last_zone_music_end[];

static esp_err_t embed(const char *name, const uint8_t *start, const uint8_t *end)
{
    esp_err_t err = mosaico_game_asset_register_memory(
        name, start, (size_t)(end - start));
    if (err != ESP_OK)
        ESP_LOGE(TAG, "embed %s failed: %s", name, esp_err_to_name(err));
    return err;
}

static esp_err_t before_display(void)
{
    const struct {
        const char *name;
        const uint8_t *start;
        const uint8_t *end;
    } assets[] = {
        {"enemy.atlas", _binary_enemy_atlas_start, _binary_enemy_atlas_end},
        {"rifle.sound", _binary_last_zone_rifle_start, _binary_last_zone_rifle_end},
        {"bolt.sound", _binary_last_zone_bolt_start, _binary_last_zone_bolt_end},
        {"impact.sound", _binary_last_zone_impact_start, _binary_last_zone_impact_end},
        {"confirm.sound", _binary_last_zone_confirm_start, _binary_last_zone_confirm_end},
        {"hurt.sound", _binary_last_zone_hurt_start, _binary_last_zone_hurt_end},
        {"empty.sound", _binary_last_zone_empty_start, _binary_last_zone_empty_end},
        {"pickup.sound", _binary_last_zone_pickup_start, _binary_last_zone_pickup_end},
        {"alert.sound", _binary_last_zone_alert_start, _binary_last_zone_alert_end},
        {"step_l.sound", _binary_last_zone_step_l_start, _binary_last_zone_step_l_end},
        {"step_r.sound", _binary_last_zone_step_r_start, _binary_last_zone_step_r_end},
        {"explode.sound", _binary_last_zone_explode_start, _binary_last_zone_explode_end},
        {"extract.sound", _binary_last_zone_extract_start, _binary_last_zone_extract_end},
        {"music.sound", _binary_last_zone_music_start, _binary_last_zone_music_end},
        {"controls.atlas", _binary_controls_atlas_start, _binary_controls_atlas_end},
        {"weapon.atlas", _binary_weapon_atlas_start, _binary_weapon_atlas_end},
        {"materials.atlas", _binary_materials_atlas_start, _binary_materials_atlas_end},
        {"props.atlas", _binary_props_atlas_start, _binary_props_atlas_end},
        {"environment.atlas", _binary_environment_atlas_start,
         _binary_environment_atlas_end},
    };
    for (size_t i = 0; i < sizeof(assets) / sizeof(assets[0]); ++i) {
        esp_err_t err = embed(assets[i].name, assets[i].start, assets[i].end);
        if (err != ESP_OK) return err;
    }
    s_enemies = LoadMosaicoAtlas("enemy.atlas");
    s_weapon = LoadMosaicoAtlas("weapon.atlas");
    s_controls = LoadMosaicoAtlas("controls.atlas");
    s_environment = LoadMosaicoAtlas("environment.atlas");
    s_materials = LoadMosaicoAtlas("materials.atlas");
    s_props = LoadMosaicoAtlas("props.atlas");
    if (!(s_enemies.texture.id && s_weapon.texture.id && s_controls.texture.id &&
          s_environment.texture.id && s_materials.texture.id && s_props.texture.id)) {
        ESP_LOGE(TAG,
                 "atlas load failed enemy=%u weapon=%u controls=%u env=%u mat=%u props=%u",
                 s_enemies.texture.id, s_weapon.texture.id, s_controls.texture.id,
                 s_environment.texture.id, s_materials.texture.id, s_props.texture.id);
        return ESP_ERR_NOT_FOUND;
    }
    return ESP_OK;
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
    s_bolt_sound = LoadSound("bolt.sound");
    s_impact_sound = LoadSound("impact.sound");
    s_confirm_sound = LoadSound("confirm.sound");
    s_hurt_sound = LoadSound("hurt.sound");
    s_empty_sound = LoadSound("empty.sound");
    s_pickup_sound = LoadSound("pickup.sound");
    s_alert_sound = LoadSound("alert.sound");
    s_step_l = LoadSound("step_l.sound");
    s_step_r = LoadSound("step_r.sound");
    s_explode_sound = LoadSound("explode.sound");
    s_extract_sound = LoadSound("extract.sound");
    s_music = LoadMusicStream("music.sound");
    SetSoundVolume(s_rifle_sound, .46f);
    SetSoundVolume(s_bolt_sound, .36f);
    SetSoundVolume(s_impact_sound, .38f);
    SetSoundVolume(s_confirm_sound, .32f);
    SetSoundVolume(s_hurt_sound, .62f);
    SetSoundVolume(s_empty_sound, .40f);
    SetSoundVolume(s_pickup_sound, .44f);
    SetSoundVolume(s_alert_sound, .48f);
    SetSoundVolume(s_step_l, .34f);
    SetSoundVolume(s_step_r, .34f);
    SetSoundVolume(s_explode_sound, .52f);
    SetSoundVolume(s_extract_sound, .40f);
    SetMusicVolume(s_music, .16f);
    PlayMusicStream(s_music);
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
    if (s_game.last_blast) {
        PlaySound(s_explode_sound);
        play_haptic_pattern(80, 30, 48, 16, 24);
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
    if (s_game.phase != LAST_ZONE_PHASE_PLAYING) {
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
    for (int i = 0; i < LAST_ZONE_ENEMIES; ++i) {
        if (!s_game.enemies[i].active) continue;
        if (s_game.enemies[i].ai_state == LAST_ZONE_ENEMY_ALERT) continue;
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
    last_zone_reset(&s_game);
    last_zone_campaign_t campaign = {0};
    if (last_zone_save_load(&campaign) == ESP_OK) {
        last_zone_campaign_apply(&s_game, &campaign);
        last_zone_reset(&s_game);
    }
    s_joystick_track = s_look_track = s_fire_track = s_radar_track = -1;
    s_move_forward = s_move_strafe = 0.0f;
    s_stick_x = LAST_ZONE_MOVE_X;
    s_stick_y = LAST_ZONE_MOVE_Y;
    s_previous_hp = s_game.hp;
    s_previous_armor = s_game.armor;
    s_previous_fire_cooldown = 0;
    s_previous_phase = s_game.phase;
    return ESP_OK;
}

static void update_joystick(int32_t x, int32_t y)
{
    float dx = (float)(x - s_stick_x) / (float)JOYSTICK_RADIUS;
    float dy = (float)(y - s_stick_y) / (float)JOYSTICK_RADIUS;
    float length = sqrtf(dx * dx + dy * dy);
    if (length > 1.0f) { dx /= length; dy /= length; }
    if (length < .08f) dx = dy = 0.0f;
    s_move_strafe = dx;
    s_move_forward = -dy;
}

static void begin_joystick(int32_t track, int32_t x, int32_t y)
{
    s_joystick_track = track;
    if (last_zone_in_move_zone(x, y)) {
        s_stick_x = LAST_ZONE_MOVE_X;
        s_stick_y = LAST_ZONE_MOVE_Y;
    } else {
        s_stick_x = x;
        s_stick_y = y;
    }
    update_joystick(x, y);
}

static void on_event(const mosaico_device_event_t *event)
{
    if (event->type != MOSAICO_DEVICE_EVENT_POINTER &&
        event->type != MOSAICO_DEVICE_EVENT_TOUCH) return;
    if (s_game.phase != LAST_ZONE_PHASE_PLAYING) {
        if (event->pressed) last_zone_confirm(&s_game);
        if (s_game.phase != LAST_ZONE_PHASE_PLAYING || !event->pressed) {
            s_joystick_track = s_look_track = s_fire_track = s_radar_track = -1;
            s_move_forward = s_move_strafe = 0.0f;
            return;
        }
    }
    const int32_t track = event->value;
    if (!event->pressed) {
        if (track == s_joystick_track) {
            s_joystick_track = -1;
            s_move_forward = s_move_strafe = 0.0f;
        }
        if (track == s_look_track) s_look_track = -1;
        if (track == s_fire_track) s_fire_track = -1;
        if (track == s_radar_track) s_radar_track = -1;
        return;
    }
    if (track == s_joystick_track) {
        update_joystick(event->x, event->y);
    } else if (track == s_radar_track) {
        last_zone_move_radar(&s_game,event->x-s_radar_dx,event->y-s_radar_dy);
    } else if (track == s_look_track) {
        last_zone_turn(&s_game, (float)(event->x - s_look_x) * .008f);
        last_zone_look(&s_game, (float)(event->y - s_look_y) * -.09f);
        s_look_x = event->x;
        s_look_y = event->y;
    } else if (track == s_fire_track) {
        return;
    } else if (last_zone_in_radar(&s_game,event->x,event->y) && s_radar_track < 0) {
        s_radar_track = track;
        s_radar_dx = event->x - s_game.radar_x;
        s_radar_dy = event->y - s_game.radar_y;
    } else if (last_zone_in_fire_zone(event->x, event->y) && s_fire_track < 0) {
        s_fire_track = track;
    } else if (last_zone_in_move_capture(event->x, event->y) && s_joystick_track < 0) {
        begin_joystick(track, event->x, event->y);
    } else if (event->x >= LAST_ZONE_LOOK_MIN_X && s_look_track < 0) {
        s_look_track = track;
        s_look_x = event->x;
        s_look_y = event->y;
    }
}

static void on_update(void)
{
    last_zone_set_motion(&s_game, s_move_forward, s_move_strafe, 0.0f);
    last_zone_set_fire_held(&s_game, s_fire_track >= 0);
    if (s_look_track < 0) last_zone_settle_look(&s_game);
    last_zone_update(&s_game);
    if (s_game.fire_cooldown == 11 && s_previous_fire_cooldown > 11)
        PlaySound(s_bolt_sound);
    if (s_previous_phase != LAST_ZONE_PHASE_WON &&
        s_game.phase == LAST_ZONE_PHASE_WON) {
        last_zone_campaign_t campaign;
        last_zone_campaign_from_game(&s_game, &campaign);
        campaign.layout = (uint8_t)((s_game.layout + 1U) % LAST_ZONE_LAYOUTS);
        if (campaign.unlocked <= s_game.layout)
            campaign.unlocked = (uint8_t)(s_game.layout + 1U);
        (void)last_zone_save_campaign(&campaign);
        (void)last_zone_save_flush();
        PlaySound(s_extract_sound);
        play_haptic_pattern(60, 28, 40, 18, 22);
    }
    if (s_game.last_alert)
        play_haptic(30, 16);
    UpdateMusicStream(s_music);
    SetMusicVolume(s_music, s_game.phase == LAST_ZONE_PHASE_PLAYING ? .14f : .10f);
    s_previous_phase = s_game.phase;
    apply_combat_feedback();
    if (s_game.hp < s_previous_hp) {
        PlaySound(s_hurt_sound);
        if (s_game.hp) play_haptic_pattern(82, 35, 55, 25, 45);
        else play_haptic_pattern(96, 90, 70, 35, 120);
    }
    if (s_game.armor < s_previous_armor) {
        PlaySound(s_impact_sound);
        play_haptic_pattern(52, 18, 34, 12, 20);
    }
    s_previous_hp = s_game.hp;
    s_previous_armor = s_game.armor;
    s_previous_fire_cooldown = s_game.fire_cooldown;
    apply_footsteps();
    if (s_game.phase == LAST_ZONE_PHASE_PLAYING && s_game.hp == 1 &&
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
        last_zone_set_performance(&s_game, stats.logic_fps, stats.display_fps,
                                   stats.render_us / 1000.0f);
    }
}

static void on_render(void)
{
    last_zone_view_render(&s_game, s_enemies, s_weapon, s_environment, s_materials,
                          s_controls, s_props);
}

static void on_stats(void)
{
    mosaico_game_2d_raster_stats_t raster = {0};
    last_zone_view_stats_t view = {0};
    mosaico_game_2d_get_raster_stats(&raster);
    last_zone_view_get_stats(&view);
    ESP_LOGI("last_zone_extraction", "pos=%.2f,%.2f score=%u frame=%uus acquire=%u ray=%u "
             "sky=%u floor=%u wall=%u grade=%u sprite=%u hud=%u submit=%u "
             "rays=%u refined=%u col=%u span=%u state_hash=%08lx",
             s_game.x, s_game.y, s_game.score, (unsigned)view.total_us,
             (unsigned)view.acquire_us, (unsigned)view.raycast_us,
             (unsigned)view.sky_us, (unsigned)view.floor_us,
             (unsigned)view.wall_us, (unsigned)view.grade_us,
             (unsigned)view.sprites_us, (unsigned)view.hud_us,
             (unsigned)view.submit_us, (unsigned)view.rays_cast,
             (unsigned)view.refined_columns,
             (unsigned)raster.column_pixels, (unsigned)raster.span_pixels,
             (unsigned long)last_zone_state_hash(&s_game));
}

static const mosaico_game_app_config_t s_config = {
    .tag = "last_zone_extraction",
    .window_title = "Last Zone: Extraction",
    .canvas_bind = GSP_LAST_ZONE_EXTRACTION_BIND_GAME_CANVAS,
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

const mosaico_game_app_config_t *last_zone_app_config(void)
{
    return &s_config;
}
