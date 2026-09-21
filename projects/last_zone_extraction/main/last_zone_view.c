// SPDX-License-Identifier: Apache-2.0
#include "last_zone_view.h"
#include <math.h>
#include <stdlib.h>
#include <time.h>
#ifdef ESP_PLATFORM
#include "esp_timer.h"
#endif
#include "assets_ids.h"
#include "mosaico_raylib_fast.h"
#include "mosaico_rgb565.h"

#define LAST_ZONE_COLUMNS 120
#define LAST_ZONE_COLUMN_WIDTH 4
#define LAST_ZONE_SCREEN 480
#define LAST_ZONE_EDGE_RAY_BUDGET 64
#define LAST_ZONE_MAX_WALL_SAMPLES LAST_ZONE_SCREEN
#define LAST_ZONE_HORIZON 205
#define LAST_ZONE_CAMERA_PLANE 0.577350269f /* tan(60 degrees / 2) */
#define LAST_ZONE_FLOOR_SCALE 165.0f
#define LAST_ZONE_NEAR_WALL 3.6f

typedef struct {
    const char *name;
    Color sky;
    Color haze;
    Color accent;
    Color accent_dim;
} last_zone_look_t;

typedef struct {
    float depth, q, uq, u;
    int16_t top;
    int16_t height;
    uint16_t bottom;
    int16_t map_x;
    int16_t map_y;
    int16_t screen_x;
    uint8_t width;
    uint8_t wall;
    uint8_t side;
    uint8_t shift;
} last_zone_wall_sample_t;

static const last_zone_look_t s_looks[LAST_ZONE_LAYOUTS] = {
    {"DOCK", {255, 255, 255, 255}, {150, 174, 184, 255},
     {72, 255, 214, 255}, {72, 255, 214, 92}},
    {"DEPOT", {255, 255, 255, 255}, {186, 142, 88, 255},
     {255, 168, 64, 255}, {255, 168, 64, 100}},
    {"COMMAND", {255, 255, 255, 255}, {90, 116, 154, 255},
     {96, 170, 255, 255}, {96, 170, 255, 100}},
    {"GHOST", {255, 255, 255, 255}, {112, 136, 112, 255},
     {140, 220, 160, 255}, {140, 220, 160, 100}},
    {"RUN", {255, 255, 255, 255}, {166, 96, 122, 255},
     {255, 92, 140, 255}, {255, 92, 140, 100}},
};

static const last_zone_look_t *layout_look(const last_zone_game_t *game)
{
    unsigned layout = game && game->layout < LAST_ZONE_LAYOUTS ? game->layout : 0;
    return &s_looks[layout];
}

static uint16_t s_wall_bottom[LAST_ZONE_COLUMNS];
static uint8_t s_wall_type[LAST_ZONE_COLUMNS];
static int16_t s_wall_top[LAST_ZONE_COLUMNS];
static last_zone_wall_sample_t s_anchors[LAST_ZONE_COLUMNS];
static last_zone_wall_sample_t s_pixels[LAST_ZONE_SCREEN];
static float s_col_depth[LAST_ZONE_SCREEN];
static mosaico_raycast_wall_t s_wall_batch[LAST_ZONE_MAX_WALL_SAMPLES];
static float s_camera_dir_x;
static float s_camera_dir_y;
static float s_camera_plane_x;
static float s_camera_plane_y;
static last_zone_view_stats_t s_view_stats;
static MosaicoSpriteFrame s_material_copies[4];
static const MosaicoSpriteFrame *s_material_frames[4];

static int64_t view_now_us(void)
{
#ifdef ESP_PLATFORM
    return esp_timer_get_time();
#else
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (int64_t)ts.tv_sec * 1000000LL + (int64_t)(ts.tv_nsec / 1000);
#endif
}

void last_zone_view_get_stats(last_zone_view_stats_t *stats)
{
    if (stats) *stats = s_view_stats;
}

static int view_horizon(const last_zone_game_t *game)
{
    float kick = game->hit_flash ? sinf((float)game->hit_flash * 2.15f) * 6.0f : 0.0f;
    int horizon = LAST_ZONE_HORIZON + (int)(game->look_pitch + game->look_kick + kick);
    if (horizon < 150) horizon = 150;
    if (horizon > 260) horizon = 260;
    return horizon;
}

static unsigned distance_light(float corrected, bool side, bool door, bool window,
                               bool corner, float flash)
{
    float light = 300.0f / (1.0f + corrected * 0.18f);
    if (light > 250.0f) light = 250.0f;
    if (light < 158.0f) light = 158.0f;
    if (side) light *= 0.88f;
    if (window) light *= 1.08f;
    if (door) light *= 1.18f;
    if (corner) light *= 0.88f;
    light += flash * 48.0f;
    if (light > 256.0f) light = 256.0f;
    if (light >= 248.0f) return 256U;
    return ((unsigned)light + 8U) & ~15U;
}

static int panorama_height(const last_zone_game_t *game)
{
    int horizon = view_horizon(game);
    bool any_window = false;
    int sky = 0;
    for (int column = 0; column < LAST_ZONE_COLUMNS; ++column) {
        if (s_wall_type[column] == 2) any_window = true;
        if (s_wall_top[column] > sky) sky = s_wall_top[column];
    }
    if (any_window) return horizon;
    if (sky < 0) sky = 0;
    if (sky > horizon) sky = horizon;
    return sky;
}

static void draw_panorama(const last_zone_game_t *game, MosaicoAtlas environment,
                         int dest_height)
{
    static const mosaico_asset_id_t ids[LAST_ZONE_LAYOUTS][2] = {
        {MOSAICO_ASSET_ID_PANORAMA_DOCK_LEFT, MOSAICO_ASSET_ID_PANORAMA_DOCK_RIGHT},
        {MOSAICO_ASSET_ID_PANORAMA_DEPOT_LEFT, MOSAICO_ASSET_ID_PANORAMA_DEPOT_RIGHT},
        {MOSAICO_ASSET_ID_PANORAMA_COMMAND_LEFT, MOSAICO_ASSET_ID_PANORAMA_COMMAND_RIGHT},
        {MOSAICO_ASSET_ID_PANORAMA_GHOST_LEFT, MOSAICO_ASSET_ID_PANORAMA_GHOST_RIGHT},
        {MOSAICO_ASSET_ID_PANORAMA_RUN_LEFT, MOSAICO_ASSET_ID_PANORAMA_RUN_RIGHT},
    };
    if (dest_height <= 0) return;
    unsigned layout = game->layout < LAST_ZONE_LAYOUTS ? game->layout : 0;
    const MosaicoSpriteFrame *first_frame = MosaicoAtlasGetFrame(environment, ids[layout][0]);
    if (!first_frame) return;
    const float half_width = first_frame->source.width - 4.0f;
    const float available = half_width * 2.0f;
    const float window = available * (300.0f / 504.0f);
    float offset = fmodf(game->angle / 6.2831853f * available, available);
    float consumed = 0.0f;
    float max_height = first_frame->source.height - 4.0f;
    float src_h = dest_height > max_height ? max_height : (float)dest_height;
    while (consumed < window) {
        float position = fmodf(offset + consumed, available);
        int half = position >= half_width ? 1 : 0;
        float local = position - half * half_width;
        float chunk = half_width - local;
        if (chunk > window - consumed) chunk = window - consumed;
        const MosaicoSpriteFrame *frame = MosaicoAtlasGetFrame(environment, ids[layout][half]);
        if (!frame) return;
        float x = 480.0f * consumed / window, width = 480.0f * chunk / window;
        if (consumed + chunk >= window)
            width = 480.0f - x;
        if (width < 1.0f) break;
        DrawTexturePro(environment.texture,
            (Rectangle){frame->source.x + 2 + local, frame->source.y + 2, chunk, src_h},
            (Rectangle){x, 0, width, (float)dest_height}, (Vector2){0, 0}, 0,
            layout_look(game)->sky);
        consumed += chunk;
    }
}

static void project_sprite(const last_zone_game_t *game, float world_x, float world_y,
                           int *center, int *size, int *ground, float *distance_out)
{
    float dx = world_x - game->x, dy = world_y - game->y;
    *center = 0;
    *size = 0;
    *ground = 0;
    *distance_out = 0;
    float determinant = s_camera_plane_x * s_camera_dir_y -
                        s_camera_dir_x * s_camera_plane_y;
    if (fabsf(determinant) < .0001f) return;
    float inverse = 1.0f / determinant;
    float camera_x = inverse * (s_camera_dir_y * dx - s_camera_dir_x * dy);
    float distance = inverse * (-s_camera_plane_y * dx + s_camera_plane_x * dy);
    if (distance <= .1f) return;
    *center = (int)(240.0f * (1.0f + camera_x / distance));
    if (*center < -260 || *center > 740) return;
    *size = (int)(280.0f / distance);
    if (*size < 10) *size = 10;
    if (*size > 260) *size = 260;
    *ground = (int)((float)view_horizon(game) + 165.0f / distance);
    if (*ground > 480) *ground = 480;
    *distance_out = distance;
}

static bool column_visible(int screen, float distance)
{
    if (screen < 0 || screen >= LAST_ZONE_SCREEN) return false;
    float depth = s_col_depth[screen];
    if (screen > 0 && s_col_depth[screen - 1] > depth) depth = s_col_depth[screen - 1];
    if (screen + 1 < LAST_ZONE_SCREEN && s_col_depth[screen + 1] > depth)
        depth = s_col_depth[screen + 1];
    return distance < depth + .10f;
}

static void draw_enemy_effect(const last_zone_game_t *game,
                              const last_zone_enemy_t *enemy)
{
    int center, size, ground;
    float distance;
    project_sprite(game, enemy->x, enemy->y, &center, &size, &ground, &distance);
    if (size <= 0 || !column_visible(center, distance)) return;
    int scale = size / 28;
    if (scale < 2) scale = 2;
    if (scale > 8) scale = 8;
    if (enemy->hit_flash && enemy->active) {
        uint32_t phase = game->tick + enemy->move_phase;
        for (int i = 0; i < 5; ++i) {
            int ox = ((int)((phase * (uint32_t)(i + 3) * 7U) % 25U) - 12) * scale / 3;
            int oy = ((int)((phase * (uint32_t)(i + 5) * 5U) % 19U) - 9) * scale / 3;
            DrawRectangle(center + ox, ground - size / 2 + oy, scale, scale,
                          i & 1 ? (Color){255, 214, 75, 255} : (Color){255, 88, 54, 255});
        }
    }
    if (enemy->death_timer) {
        int age = 20 - enemy->death_timer;
        for (int i = 0; i < 7; ++i) {
            int spread = age + i * 3;
            int ox = ((i * 17 + enemy->move_phase) % 21 - 10) * spread / 12;
            int oy = age * (2 + i % 3) / 2 + i * 3;
            int radius = scale + (age + i) / 7;
            Color smoke = i & 1 ? (Color){76, 75, 70, 255} : (Color){112, 104, 91, 255};
            DrawCircle(center + ox, ground - size / 3 - oy, radius, smoke);
        }
    }
}

static void draw_enemies(const last_zone_game_t *game, MosaicoAtlas atlas)
{
    static const mosaico_asset_id_t run_frames[] = {MOSAICO_ASSET_ID_ENEMY_RUN_0,
        MOSAICO_ASSET_ID_ENEMY_RUN_1, MOSAICO_ASSET_ID_ENEMY_RUN_2};
    int order[LAST_ZONE_ENEMIES];
    float distances[LAST_ZONE_ENEMIES];
    for (int i = 0; i < LAST_ZONE_ENEMIES; ++i) order[i] = i;
    for (int i = 0; i < LAST_ZONE_ENEMIES; ++i) {
        float dx = game->enemies[i].x - game->x;
        float dy = game->enemies[i].y - game->y;
        distances[i] = sqrtf(dx * dx + dy * dy);
    }
    for (int i = 0; i < LAST_ZONE_ENEMIES - 1; ++i)
        for (int j = i + 1; j < LAST_ZONE_ENEMIES; ++j)
            if (distances[order[i]] < distances[order[j]]) {
                int swap = order[i];
                order[i] = order[j];
                order[j] = swap;
            }
    for (int n = 0; n < LAST_ZONE_ENEMIES; ++n) {
        const last_zone_enemy_t *enemy = &game->enemies[order[n]];
        bool corpse = !enemy->active && !enemy->hp;
        if (!enemy->active && !corpse) continue;
        mosaico_asset_id_t frame_id = MOSAICO_ASSET_ID_ENEMY_DOWN;
        if (!corpse) {
            frame_id = MOSAICO_ASSET_ID_ENEMY_RUN_1;
            if (enemy->hit_flash) frame_id = MOSAICO_ASSET_ID_ENEMY_HIT;
            else if (enemy->attack_flash) frame_id = MOSAICO_ASSET_ID_ENEMY_FIRE;
            else if (enemy->aim_timer) frame_id = MOSAICO_ASSET_ID_ENEMY_AIM;
            else frame_id = run_frames[((game->tick / 4U) + enemy->move_phase) % 3U];
        }
        const MosaicoSpriteFrame *frame = MosaicoAtlasGetFrame(atlas, frame_id);
        if (!frame) {
            if (corpse && enemy->death_timer) draw_enemy_effect(game, enemy);
            continue;
        }
        int center, size, ground;
        float distance;
        project_sprite(game, enemy->x, enemy->y, &center, &size, &ground, &distance);
        if (size <= 0) continue;
        int dest_h = corpse ? size * 3 / 5 : size;
        int dest_w = corpse ? size + size / 6 : size;
        if (dest_h < 12) dest_h = 12;
        if (!corpse && enemy->hp < 2) center += 3;
        Color tint = corpse ? (Color){210, 186, 168, 255}
                    : (enemy->hit_flash ? (Color){255, 92, 64, 255}
                    : (enemy->elite ? (Color){186, 164, 228, 255} : WHITE));
        int left = center - dest_w / 2, top = ground - dest_h, run_start = -1;
        bool visible_any = false;
        for (int x = 0; x < dest_w + 4; x += 4) {
            int screen = left + x;
            bool visible = x < dest_w && column_visible(screen, distance);
            if (visible) {
                visible_any = true;
                if (run_start < 0) run_start = x;
            }
            if (!visible && run_start >= 0) {
                int run_end = x < dest_w ? x : dest_w, run_width = run_end - run_start;
                float source_x = frame->source.x + frame->source.width * (float)run_start / dest_w;
                float source_width = frame->source.width * (float)run_width / dest_w;
                DrawTexturePro(atlas.texture,
                    (Rectangle){source_x, frame->source.y, source_width, frame->source.height},
                    (Rectangle){(float)(left + run_start), (float)top, (float)run_width,
                                (float)dest_h},
                    (Vector2){0, 0}, 0, tint);
                run_start = -1;
            }
        }
        if (corpse) {
            if (visible_any || enemy->death_timer) draw_enemy_effect(game, enemy);
            continue;
        }
        if (visible_any && enemy->hp < 2)
            DrawRectangle(center - 6, top - 4, 12, 3, (Color){255, 72, 48, 255});
        if (visible_any) {
            Color ident = enemy->move_phase % 3U == 0 ? (Color){72, 168, 214, 255}
                        : enemy->move_phase % 3U == 1 ? (Color){214, 168, 72, 255}
                        : (Color){168, 92, 72, 255};
            DrawRectangle(center - 3, top + size / 8, 6, 4, ident);
        }
        if (visible_any && enemy->ai_state == LAST_ZONE_ENEMY_ALERT) {
            DrawCircle(center, top - 12, 9, (Color){255, 210, 55, 255});
            DrawText("!", center - 3, top - 19, 14, (Color){45, 28, 12, 255});
        } else if (visible_any && enemy->ai_state == LAST_ZONE_ENEMY_SEARCH) {
            DrawText("?", center - 4, top - 18, 15, (Color){116, 210, 255, 255});
        }
        if (visible_any && enemy->aim_timer) {
            int width = 30 - (int)enemy->aim_timer;
            DrawRectangle(center - 15, top - 5, 30, 3, (Color){58, 25, 22, 255});
            DrawRectangle(center - 15, top - 5, width, 3, (Color){255, 66, 48, 255});
        }
        if (visible_any && enemy->attack_flash) {
            int muzzle_y = top + size * 43 / 100;
            DrawCircle(center + size / 5, muzzle_y, size / 18 + 2,
                       (Color){255, 226, 85, 255});
            DrawLine(center + size / 5, muzzle_y, 240, 205,
                     (Color){255, 118, 52, 255});
        }
        if (visible_any) draw_enemy_effect(game, enemy);
    }
}

static void draw_billboard_box(const last_zone_game_t *game, float world_x, float world_y,
                               Color color, int inset)
{
    int center, size, ground;
    float distance;
    project_sprite(game, world_x, world_y, &center, &size, &ground, &distance);
    if (size <= 0) return;
    size = size / 2 + inset;
    if (size < 10) size = 10;
    int left = center - size / 2, top = ground - size, run_start = -1;
    for (int x = 0; x < size + 4; x += 4) {
        int screen = left + x;
        bool visible = x < size && column_visible(screen, distance);
        if (visible && run_start < 0) run_start = x;
        if (!visible && run_start >= 0) {
            int run_width = (x < size ? x : size) - run_start;
            DrawRectangle(left + run_start, top, run_width, size, color);
            run_start = -1;
        }
    }
}

static void draw_billboard_sprite(const last_zone_game_t *game, MosaicoAtlas atlas,
                                  mosaico_asset_id_t frame_id, float world_x,
                                  float world_y, float scale)
{
    const MosaicoSpriteFrame *frame = MosaicoAtlasGetFrame(atlas, frame_id);
    if (!frame) return;
    int center, size, ground;
    float distance;
    project_sprite(game, world_x, world_y, &center, &size, &ground, &distance);
    if (size <= 0) return;
    size = (int)((float)size * scale);
    if (size < 12) size = 12;
    if (size > 120) size = 120;
    int left = center - size / 2, top = ground - size, run_start = -1;
    for (int x = 0; x < size + 4; x += 4) {
        int screen = left + x;
        bool visible = x < size && column_visible(screen, distance);
        if (visible) {
            if (run_start < 0) run_start = x;
        }
        if (!visible && run_start >= 0) {
            int run_end = x < size ? x : size, run_width = run_end - run_start;
            float source_x = frame->source.x + frame->source.width * (float)run_start / size;
            float source_width = frame->source.width * (float)run_width / size;
            DrawTexturePro(atlas.texture,
                (Rectangle){source_x, frame->source.y, source_width, frame->source.height},
                (Rectangle){(float)(left + run_start), (float)top, (float)run_width, (float)size},
                (Vector2){0, 0}, 0, WHITE);
            run_start = -1;
        }
    }
}

static void draw_pickups(const last_zone_game_t *game, MosaicoAtlas props)
{
    for (int i = 0; i < LAST_ZONE_PICKUPS; ++i) {
        if (game->pickups[i].taken) continue;
        mosaico_asset_id_t id = game->pickups[i].kind == NEON_PICKUP_HEALTH
            ? MOSAICO_ASSET_ID_PICKUP_HEALTH
            : (game->pickups[i].kind == NEON_PICKUP_ARMOR
                ? MOSAICO_ASSET_ID_PICKUP_ARMOR : MOSAICO_ASSET_ID_PICKUP_AMMO);
        if (props.texture.id)
            draw_billboard_sprite(game, props, id, game->pickups[i].x, game->pickups[i].y, 0.42f);
        else {
            Color color = game->pickups[i].kind == NEON_PICKUP_HEALTH
                ? (Color){255, 82, 96, 255}
                : (game->pickups[i].kind == NEON_PICKUP_ARMOR
                    ? (Color){64, 170, 255, 255} : (Color){255, 214, 75, 255});
            draw_billboard_box(game, game->pickups[i].x, game->pickups[i].y, color, 6);
        }
    }
    for (int i = 0; i < LAST_ZONE_PROPS; ++i) {
        if (!game->props[i].active) {
            if (game->props[i].blast_timer) {
                int age = 18 - game->props[i].blast_timer;
                Color blast = age < 7 ? (Color){255, 224, 92, 255}
                                      : (Color){232, 82, 34, 255};
                draw_billboard_box(game, game->props[i].x, game->props[i].y,
                                   blast, 14 + age);
            }
            continue;
        }
        mosaico_asset_id_t id = game->props[i].kind ? MOSAICO_ASSET_ID_PROP_BAG
                                                    : MOSAICO_ASSET_ID_PROP_BARREL;
        if (props.texture.id)
            draw_billboard_sprite(game, props, id, game->props[i].x, game->props[i].y, 0.48f);
        else {
            Color color = game->props[i].kind
                ? (Color){214, 186, 72, 255} : (Color){118, 78, 48, 255};
            draw_billboard_box(game, game->props[i].x, game->props[i].y, color,
                               game->props[i].kind ? 2 : 4);
        }
    }
}

static void draw_extract(const last_zone_game_t *game)
{
    /* Keep the locked objective from competing with last-hostile guidance. */
    if (last_zone_enemies_alive(game) != 0) return;
    int center, size, ground;
    float distance;
    project_sprite(game, last_zone_extract_x(game), last_zone_extract_y(game),
                   &center, &size, &ground, &distance);
    if (size <= 0) return;
    Color neon = (Color){72, 255, 214, 255};
    int pad_h = size / 5 + 6;
    if (pad_h < 10) pad_h = 10;
    int pad_w = size + size / 3;
    int left = center - pad_w / 2, top = ground - pad_h, run_start = -1;
    for (int x = 0; x < pad_w + 4; x += 4) {
        int screen = left + x;
        bool visible = x < pad_w && column_visible(screen, distance);
        if (visible && run_start < 0) run_start = x;
        if (!visible && run_start >= 0) {
            int run_width = (x < pad_w ? x : pad_w) - run_start;
            int screen_left = left + run_start;
            DrawRectangle(screen_left, top, run_width, pad_h, (Color){18, 42, 48, 255});
            DrawRectangle(screen_left, top, run_width, 3, neon);
            DrawRectangle(screen_left, top + pad_h - 3, run_width, 3, neon);
            run_start = -1;
        }
    }
    int chev_y = ground - pad_h - 8 - ((int)(game->tick / 6U) % 6);
    DrawTriangle((Vector2){(float)center, (float)(chev_y - 10)},
                 (Vector2){(float)(center - 14), (float)chev_y},
                 (Vector2){(float)(center + 14), (float)chev_y}, neon);
    DrawTriangle((Vector2){(float)center, (float)(chev_y + 4)},
                 (Vector2){(float)(center - 10), (float)(chev_y + 14)},
                 (Vector2){(float)(center + 10), (float)(chev_y + 14)}, neon);
    DrawRectangle(center - 2, top - 28, 4, 28, neon);
}

static void project_wall(last_zone_wall_sample_t *sample, int horizon)
{
    float distance = sample->depth;
    if (distance < .08f) distance = .08f;
    if (distance > 64.0f) distance = 64.0f;
    sample->depth = distance;
    sample->q = 1.0f / distance;
    sample->uq = sample->u * sample->q;
    int height = (int)(330.0f / distance);
    if (height < 1) height = 1;
    int top = horizon - height / 2;
    int bottom = top + height;
    if (bottom > LAST_ZONE_SCREEN) bottom = LAST_ZONE_SCREEN;
    if (bottom < horizon) bottom = horizon;
    if (bottom < 0) bottom = 0;
    sample->top = (int16_t)top;
    sample->height = (int16_t)height;
    sample->bottom = (uint16_t)bottom;
}

static void cast_wall_sample(const last_zone_game_t *game, int screen_x, int horizon,
                             last_zone_wall_sample_t *sample)
{
    float sample_x = (float)screen_x + 0.5f;
    float camera_x = sample_x / 240.0f - 1.0f;
    float rx = s_camera_dir_x + s_camera_plane_x * camera_x;
    float ry = s_camera_dir_y + s_camera_plane_y * camera_x;
    int map_x = (int)game->x, map_y = (int)game->y;
    int step_x = rx < 0 ? -1 : 1, step_y = ry < 0 ? -1 : 1;
    float delta_x = fabsf(rx) < .000001f ? 1.0e30f : fabsf(1.0f / rx);
    float delta_y = fabsf(ry) < .000001f ? 1.0e30f : fabsf(1.0f / ry);
    float side_x = (rx < 0 ? game->x - map_x : map_x + 1.0f - game->x) * delta_x;
    float side_y = (ry < 0 ? game->y - map_y : map_y + 1.0f - game->y) * delta_y;
    uint8_t wall = 0;
    bool side = false;
    for (int step = 0; step < LAST_ZONE_WIDTH + LAST_ZONE_HEIGHT && !wall; ++step) {
        if (side_x < side_y) {
            side_x += delta_x;
            map_x += step_x;
            side = false;
        } else {
            side_y += delta_y;
            map_y += step_y;
            side = true;
        }
        uint8_t cell = last_zone_cell(game, map_x, map_y);
        if (last_zone_blocks(game, map_x, map_y)) wall = cell ? cell : 1;
    }
    float distance = side ? side_y - delta_y : side_x - delta_x;
    if (!wall) distance = 64.0f;
    float hit = side ? game->x + distance * rx : game->y + distance * ry;
    float u = hit - floorf(hit);
    if (u < 0.0f) u += 1.0f;
    *sample = (last_zone_wall_sample_t){
        .depth = distance,
        .u = u,
        .map_x = (int16_t)map_x,
        .map_y = (int16_t)map_y,
        .screen_x = (int16_t)screen_x,
        .width = 1,
        .wall = wall,
        .side = side ? 1U : 0U,
        .shift = (uint8_t)((map_x * 37 + map_y * 13) & 31),
    };
    project_wall(sample, horizon);
}

static bool wall_boundary(const last_zone_wall_sample_t *a,
                          const last_zone_wall_sample_t *b)
{
    if (a->wall != b->wall || a->side != b->side ||
        a->map_x != b->map_x || a->map_y != b->map_y)
        return true;
    return fabsf(a->depth - b->depth) > .22f;
}

static void interp_wall(const last_zone_wall_sample_t *a,
                        const last_zone_wall_sample_t *b, float t, int screen_x,
                        int horizon, last_zone_wall_sample_t *out)
{
    float q = a->q + (b->q - a->q) * t;
    float uq = a->uq + (b->uq - a->uq) * t;
    if (q < 1.0f / 64.0f) q = 1.0f / 64.0f;
    *out = *a;
    out->screen_x = (int16_t)screen_x;
    out->width = 1;
    out->depth = 1.0f / q;
    out->u = uq * out->depth;
    out->u -= floorf(out->u);
    if (out->u < 0.0f) out->u += 1.0f;
    project_wall(out, horizon);
}

static void fill_interp_span(const last_zone_wall_sample_t *a,
                             const last_zone_wall_sample_t *b, int x0, int x1,
                             int horizon)
{
    if (x1 < x0) return;
    if (x0 < 0) x0 = 0;
    if (x1 >= LAST_ZONE_SCREEN) x1 = LAST_ZONE_SCREEN - 1;
    int span = x1 - x0;
    for (int x = x0; x <= x1; ++x) {
        float t = span > 0 ? (float)(x - x0) / (float)span : 0.0f;
        interp_wall(a, b, t, x, horizon, &s_pixels[x]);
    }
}

static int bisect_edge(const last_zone_game_t *game, int x0, int x1, int horizon,
                       last_zone_wall_sample_t *left, last_zone_wall_sample_t *right,
                       int *used)
{
    last_zone_wall_sample_t L = *left, R = *right;
    int lo = x0, hi = x1;
    while (hi - lo > 1 && *used < LAST_ZONE_EDGE_RAY_BUDGET) {
        int mid = (lo + hi) >> 1;
        last_zone_wall_sample_t mid_s;
        cast_wall_sample(game, mid, horizon, &mid_s);
        ++*used;
        if (!wall_boundary(&L, &mid_s)) {
            lo = mid;
            L = mid_s;
        } else {
            hi = mid;
            R = mid_s;
        }
    }
    *left = L;
    *right = R;
    return hi;
}

typedef struct {
    int x0;
    int x1;
    last_zone_wall_sample_t left;
    last_zone_wall_sample_t right;
    float near_depth;
} last_zone_wall_gap_t;

static int wall_gap_cmp(const void *a, const void *b)
{
    const last_zone_wall_gap_t *ga = a;
    const last_zone_wall_gap_t *gb = b;
    if (ga->near_depth < gb->near_depth) return -1;
    if (ga->near_depth > gb->near_depth) return 1;
    return ga->x0 - gb->x0;
}

static void fill_gap(const last_zone_game_t *game, last_zone_wall_gap_t *gap,
                     int horizon, int *extra)
{
    last_zone_wall_sample_t edge_l = gap->left, edge_r = gap->right;
    int split = bisect_edge(game, gap->x0, gap->x1, horizon, &edge_l, &edge_r, extra);
    fill_interp_span(&gap->left, &edge_l, gap->x0,
                     split > gap->x0 ? split - 1 : gap->x0, horizon);
    fill_interp_span(&edge_r, &gap->right, split, gap->x1, horizon);
}

static void raycast_world(const last_zone_game_t *game)
{
    static last_zone_wall_gap_t gaps[LAST_ZONE_COLUMNS];
    int gap_count = 0;
    int horizon = view_horizon(game);
    int extra = 0;
    s_camera_dir_x = cosf(game->angle);
    s_camera_dir_y = sinf(game->angle);
    s_camera_plane_x = -s_camera_dir_y * LAST_ZONE_CAMERA_PLANE;
    s_camera_plane_y = s_camera_dir_x * LAST_ZONE_CAMERA_PLANE;
    for (int column = 0; column < LAST_ZONE_COLUMNS; ++column) {
        int screen_x = column * LAST_ZONE_COLUMN_WIDTH;
        cast_wall_sample(game, screen_x, horizon, &s_anchors[column]);
    }
    last_zone_wall_sample_t tail;
    cast_wall_sample(game, LAST_ZONE_SCREEN - 1, horizon, &tail);
    ++extra;
    for (int column = 0; column < LAST_ZONE_COLUMNS; ++column) {
        int x0 = column * LAST_ZONE_COLUMN_WIDTH;
        int x1 = column + 1 < LAST_ZONE_COLUMNS ? x0 + LAST_ZONE_COLUMN_WIDTH
                                                : LAST_ZONE_SCREEN - 1;
        last_zone_wall_sample_t left = s_anchors[column];
        last_zone_wall_sample_t right = column + 1 < LAST_ZONE_COLUMNS
            ? s_anchors[column + 1] : tail;
        if (!wall_boundary(&left, &right)) {
            fill_interp_span(&left, &right, x0, x1, horizon);
            continue;
        }
        float near = left.depth < right.depth ? left.depth : right.depth;
        gaps[gap_count++] = (last_zone_wall_gap_t){
            .x0 = x0, .x1 = x1, .left = left, .right = right, .near_depth = near};
    }
    qsort(gaps, (size_t)gap_count, sizeof(gaps[0]), wall_gap_cmp);
    for (int i = 0; i < gap_count; ++i)
        fill_gap(game, &gaps[i], horizon, &extra);
    for (int x = 0; x < LAST_ZONE_SCREEN; ++x) s_col_depth[x] = s_pixels[x].depth;
    for (int column = 0; column < LAST_ZONE_COLUMNS; ++column) {
        int x0 = column * LAST_ZONE_COLUMN_WIDTH;
        uint16_t bottom = s_pixels[x0].bottom;
        int16_t top = s_pixels[x0].top;
        uint8_t wall = s_pixels[x0].wall;
        for (int x = x0; x < x0 + LAST_ZONE_COLUMN_WIDTH && x < LAST_ZONE_SCREEN; ++x) {
            if (s_pixels[x].bottom < bottom) bottom = s_pixels[x].bottom;
            if (s_pixels[x].top > top) top = s_pixels[x].top;
            if (s_pixels[x].wall == 2) wall = 2;
        }
        s_wall_bottom[column] = bottom;
        s_wall_top[column] = top;
        s_wall_type[column] = wall;
    }
    s_view_stats.refined_columns = (uint16_t)extra;
    s_view_stats.rays_cast = (uint16_t)(LAST_ZONE_COLUMNS + extra);
}

static int floor_kind_at(const last_zone_game_t *game, float wx, float wy)
{
    int mx = (int)wx, my = (int)wy;
    if (last_zone_cell(game, mx, my) == 5) return 2;
    int ex=(int)last_zone_extract_x(game),ey=(int)last_zone_extract_y(game);
    if (abs(mx-ex)<=1&&abs(my-ey)<=1) return 2;
    switch(game->layout){
        case 0: if(mx<8||my>=18)return 1;break;                 /* dock planks */
        case 1: if((mx<8&&my>8)||(mx>15&&my<9))return 1;break; /* twin stores */
        case 2: if(mx<7||mx>16)return 1;break;                 /* office wings */
        case 3: if(((my/5)&1)!=0)return 1;break;               /* silent bands */
        case 4: if(mx<6||my>14)return 1;break;                 /* service route */
        default: break;
    }
    return 0;
}

static void draw_floor(const last_zone_game_t *game, MosaicoAtlas materials,
                       const MosaicoSpriteFrame *tile)
{
    if (!tile) return;
    int horizon = view_horizon(game);
    float cam0 = (0.5f / (float)LAST_ZONE_COLUMNS) * 2.0f - 1.0f;
    float cam_step = 2.0f / (float)LAST_ZONE_COLUMNS;
    for (int y = horizon + 1; y < 480; y += 2) {
        float dist = LAST_ZONE_FLOOR_SCALE / (float)(y - horizon);
        float ray_x = s_camera_dir_x + s_camera_plane_x * cam0;
        float ray_y = s_camera_dir_y + s_camera_plane_y * cam0;
        float wx = game->x + ray_x * dist, wy = game->y + ray_y * dist;
        float dwx = (s_camera_plane_x * cam_step) * dist;
        float dwy = (s_camera_plane_y * cam_step) * dist;
        int u_16 = (int)(wx * 128.0f * 65536.0f);
        int v_16 = (int)(wy * 128.0f * 65536.0f);
        int du_16 = (int)(dwx * 128.0f * 65536.0f);
        int dv_16 = (int)(dwy * 128.0f * 65536.0f);
        unsigned light = (unsigned)(220.0f / (1.0f + dist * 0.18f));
        if (light < 118U) light = 118U;
        if (light > 210U) light = 210U;
        light += (unsigned)(game->weapon_recoil * 36.0f);
        int run_kind = -1, run_start = 0;
        for (int column = 0; column <= LAST_ZONE_COLUMNS; ++column) {
            int kind = 0;
            if (column < LAST_ZONE_COLUMNS)
                kind = floor_kind_at(game, wx + dwx * (float)column,
                                     wy + dwy * (float)column);
            if (column == 0) {
                run_kind = kind;
                run_start = 0;
                continue;
            }
            if (column < LAST_ZONE_COLUMNS && kind == run_kind) continue;
            Rectangle floor_src = tile->source;
            if (run_kind == 1) floor_src.y += 64.0f;
            floor_src.width = 64.0f;
            floor_src.height = 64.0f;
            unsigned run_light = light;
            if (run_kind == 2) run_light += 36U;
            if (run_light > 230U) run_light = 230U;
            int columns = column - run_start;
            Mosaico2DDrawFloorRows(materials.texture, floor_src, y,
                                  run_start * LAST_ZONE_COLUMN_WIDTH, LAST_ZONE_COLUMN_WIDTH,
                                  columns, s_wall_bottom + run_start,
                                  u_16 + du_16 * run_start, v_16 + dv_16 * run_start,
                                  du_16, dv_16, run_light, 2);
            run_kind = kind;
            run_start = column;
        }
    }
}

static void draw_wall_footing(int screen_x, int width, int top, int bottom)
{
    int height = bottom - top;
    if (height <= 0) return;
    if (bottom > 480) bottom = 480;
    if (bottom <= 0) return;
    int foot = height / 14;
    if (foot < 5) foot = 5;
    if (foot > 14) foot = 14;
    int y = bottom - foot;
    if (y < 0) y = 0;
    foot = bottom - y;
    if (foot <= 0) return;
    DrawRectangle(screen_x, y, width, foot, (Color){38, 32, 26, 255});
    int edge = foot < 2 ? foot : 2;
    DrawRectangle(screen_x, bottom - edge, width, edge, (Color){24, 20, 16, 255});
}

static bool wall_run_match(const last_zone_wall_sample_t *a,
                           const last_zone_wall_sample_t *b)
{
    if (wall_boundary(a, b)) return false;
    int dt = a->top - b->top;
    if (dt < 0) dt = -dt;
    return dt <= 1;
}

static int wall_dest_width(int x)
{
    const last_zone_wall_sample_t *sample = &s_pixels[x];
    if (sample->wall == 2 || sample->wall == 4) return 1;
    if (x + 1 >= LAST_ZONE_SCREEN || !wall_run_match(sample, &s_pixels[x + 1]))
        return 1;
    if (sample->depth >= LAST_ZONE_NEAR_WALL &&
        x + 3 < LAST_ZONE_SCREEN &&
        wall_run_match(sample, &s_pixels[x + 2]) &&
        wall_run_match(sample, &s_pixels[x + 3]))
        return 4;
    return 2;
}

static void draw_walls(const last_zone_game_t *game, MosaicoAtlas materials,
                       const MosaicoSpriteFrame *frames[4])
{
    float flash = game->weapon_recoil;
    int batch = 0;
    int horizon = view_horizon(game);
    for (int x = 0; x < LAST_ZONE_SCREEN; ) {
        const last_zone_wall_sample_t *sample = &s_pixels[x];
        uint8_t wall = sample->wall;
        if (!wall) {
            ++x;
            continue;
        }
        int width = wall_dest_width(x);
        int mat = 0;
        if (wall == 2 || wall == 4) mat = 1;
        else if (wall == 3) mat = 2;
        const MosaicoSpriteFrame *material = frames[mat];
        if (!material) {
            x += width;
            continue;
        }
        float inset = 4.0f;
        float inner = material->source.width - inset * 2.0f;
        if (inner < 4.0f) inner = material->source.width;
        float u = sample->u + (float)sample->shift / 48.0f;
        u = u - floorf(u);
        if (wall == 4) u = u * 0.26f;
        else if (wall == 2) u = 0.42f + u * 0.50f;
        bool corner = u < 0.08f || u > 0.92f;
        unsigned light = distance_light(sample->depth, sample->side != 0,
                                        wall == 4, wall == 2, corner, flash);
        if (wall == 4 && (game->tick % 20U) < 10U) light += 36U;
        if (light > 256U) light = 256U;
        int screen_x = sample->screen_x;
        int top = sample->top;
        int height = sample->height;
        if (height < 1) height = 1;
        int bottom = top + height;
        float src_w = (float)width;
        Rectangle src = {material->source.x + inset + u * (inner - src_w),
                         material->source.y + inset, src_w,
                         material->source.height - inset * 2.0f};
        if (src.height < 4.0f) src.height = material->source.height;
        if (wall == 4) {
            Color gold = (Color){214, 168, 48, 255};
            if (sample->side) gold = (Color){168, 128, 36, 255};
            if ((game->tick % 20U) < 10U) {
                gold.r = (unsigned char)(gold.r + 28 > 255 ? 255 : gold.r + 28);
                gold.g = (unsigned char)(gold.g + 20 > 255 ? 255 : gold.g + 20);
            }
            DrawRectangle(screen_x, top, width, height, gold);
            DrawRectangle(screen_x, top, width, 6, (Color){96, 64, 16, 255});
            DrawRectangle(screen_x, top + height * 55 / 100, width, 7,
                          (Color){255, 232, 128, 255});
            draw_wall_footing(screen_x, width, top, bottom);
            x += width;
            continue;
        }
        if (wall == 2) {
            int band = height * 22 / 100;
            if (band < 6) band = 6;
            if (band * 2 > height) band = height / 3;
            int open_top = top + band;
            int open_bot = bottom - band;
            if (open_bot <= open_top + 2 || open_bot <= horizon) {
                Mosaico2DDrawColumn(materials.texture, src, screen_x, top,
                                    width, height, light);
            } else {
                Mosaico2DDrawColumn(materials.texture,
                    (Rectangle){src.x, src.y, src.width, src.height * 0.22f},
                    screen_x, top, width, band, light);
                int haze_top = open_top < horizon ? horizon : open_top;
                if (open_bot > haze_top)
                    DrawRectangle(screen_x, haze_top, width,
                                  open_bot - haze_top, layout_look(game)->haze);
                int sill_h = bottom - open_bot;
                if (sill_h < 1) sill_h = 1;
                Mosaico2DDrawColumn(materials.texture,
                    (Rectangle){src.x, src.y + src.height * 0.78f, src.width, src.height * 0.22f},
                    screen_x, open_bot, width, sill_h, light);
            }
            draw_wall_footing(screen_x, width, top, bottom);
            x += width;
            continue;
        }
        if (batch < LAST_ZONE_MAX_WALL_SAMPLES) {
            s_wall_batch[batch++] = (mosaico_raycast_wall_t){
                screen_x, top, width, height,
                (int)src.x, (int)src.y, (int)src.width, (int)src.height, light};
        }
        x += width;
    }
    if (batch) Mosaico2DDrawRaycastWalls(materials.texture, s_wall_batch, batch);
    for (int x = 0; x < LAST_ZONE_SCREEN; ) {
        const last_zone_wall_sample_t *sample = &s_pixels[x];
        int width = wall_dest_width(x);
        if (sample->wall == 1 || sample->wall == 3)
            draw_wall_footing(sample->screen_x, width, sample->top, sample->bottom);
        x += sample->wall ? width : 1;
    }
}

static void load_material_frames(MosaicoAtlas materials)
{
    static const mosaico_asset_id_t material_ids[] = {MOSAICO_ASSET_ID_WALL_CONCRETE,
        MOSAICO_ASSET_ID_WALL_BRICK, MOSAICO_ASSET_ID_WALL_CONTAINER,
        MOSAICO_ASSET_ID_FLOOR_DIRT};
    for (int i = 0; i < 4; ++i) {
        if (mosaico_game_2d_atlas_get_frame(materials, material_ids[i],
                                            &s_material_copies[i]) == ESP_OK)
            s_material_frames[i] = &s_material_copies[i];
        else
            s_material_frames[i] = NULL;
    }
}

static void draw_weapon(const last_zone_game_t *game, MosaicoAtlas atlas)
{
    bool bolting = game->fire_cooldown > 4;
    mosaico_asset_id_t frame_id = bolting ? MOSAICO_ASSET_ID_TACTICAL_BOLT
                                         : MOSAICO_ASSET_ID_TACTICAL_RIFLE;
    const MosaicoSpriteFrame *frame = MosaicoAtlasGetFrame(atlas, frame_id);
    if (!frame) frame = MosaicoAtlasGetFrame(atlas, MOSAICO_ASSET_ID_TACTICAL_RIFLE);
    if (!frame) return;
    float motion = sqrtf(game->move_forward * game->move_forward +
                         game->move_strafe * game->move_strafe);
    if (motion > 1.0f) motion = 1.0f;
    float sprint = game->sprinting ? 1.7f : 1.0f;
    float back = game->move_forward < -.2f ? 1.35f : 1.0f;
    float bob_x = sinf(game->move_phase) * 5.0f * motion * sprint + game->turn_input * 10.0f;
    float bob_y = fabsf(cosf(game->move_phase)) * 5.0f * motion * sprint * back;
    float recoil_y = -18.0f * game->weapon_recoil + game->look_kick * .35f;
    float recoil_x = 4.0f * game->weapon_recoil;
    float bolt = game->fire_cooldown
        ? 8.0f * (float)game->fire_cooldown / LAST_ZONE_FIRE_COOLDOWN : 0.0f;
    float nearest = 8.0f;
    for (int i = 0; i < LAST_ZONE_ENEMIES; ++i) {
        if (!game->enemies[i].active) continue;
        float dx = game->enemies[i].x - game->x, dy = game->enemies[i].y - game->y;
        float dist = sqrtf(dx * dx + dy * dy);
        if (dist < nearest) nearest = dist;
    }
    float hip = nearest < 1.8f ? (1.8f - nearest) * 22.0f : 0.0f;
    DrawTexturePro(atlas.texture, frame->source,
        (Rectangle){128.0f + bob_x + recoil_x,
                    348.0f + hip + bolt * 0.45f + game->look_pitch * .22f + bob_y + recoil_y,
                    frame->source.width, frame->source.height},
        (Vector2){0, 0}, 0, WHITE);
    if (game->weapon_recoil > .35f)
        DrawCircle(240, 204 + (int)(game->look_kick * .2f),
                   7 + (int)(game->weapon_recoil * 9.0f), (Color){255, 226, 96, 255});
    if (game->fire_cooldown > 6) {
        int age = LAST_ZONE_FIRE_COOLDOWN - game->fire_cooldown;
        DrawRectangle(258 + age * 2, 372 - age * 2, 4, 3, (Color){255, 196, 82, 255});
    }
}

static void draw_radar(const last_zone_game_t *game)
{
    const int radius = 5, scale = 6;
    const int left = game->radar_x, top = game->radar_y;
    const int span = radius * 2 + 1;
    int origin_x = (int)game->x - radius, origin_y = (int)game->y - radius;
    DrawRectangle(left - 4, top - 4, span * scale + 8, span * scale + 8, (Color){3, 9, 20, 255});
    if (game->alert_flash)
        DrawRectangleLines(left - 4, top - 4, span * scale + 8, span * scale + 8,
                           (Color){255, 210, 55, 255});
    for (int y = 0; y < span; ++y)
        for (int x = 0; x < span; ++x) {
            int mx = origin_x + x, my = origin_y + y;
            if (mx < 0 || my < 0 || mx >= LAST_ZONE_WIDTH || my >= LAST_ZONE_HEIGHT) continue;
            if (!game->explored[my][mx]) continue;
            uint8_t cell = last_zone_cell(game, mx, my);
            Color color = (Color){58, 42, 28, 255};
            if (cell == 4 && !game->door_open[my][mx]) color = (Color){220, 180, 60, 255};
            else if (cell == 5) color = (Color){80, 220, 200, 255};
            else if (cell == 3) color = (Color){148, 92, 48, 255};
            else if (cell == 2) color = (Color){96, 140, 168, 255};
            else if (cell >= 1 && cell <= 3) color = (Color){186, 198, 208, 255};
            DrawRectangle(left + x * scale, top + y * scale, scale - 1, scale - 1, color);
        }
    int alive = last_zone_enemies_alive(game);
    bool extract_open = alive == 0;
    for (int i = 0; i < LAST_ZONE_PICKUPS; ++i) {
        if (game->pickups[i].taken) continue;
        int mx = (int)game->pickups[i].x, my = (int)game->pickups[i].y;
        if (mx < 0 || my < 0 || mx >= LAST_ZONE_WIDTH || my >= LAST_ZONE_HEIGHT) continue;
        if (!game->explored[my][mx]) continue;
        int ox = mx - origin_x, oy = my - origin_y;
        if (ox < 0 || oy < 0 || ox >= span || oy >= span) continue;
        Color color = game->pickups[i].kind == NEON_PICKUP_HEALTH
            ? (Color){255, 82, 96, 255}
            : (game->pickups[i].kind == NEON_PICKUP_ARMOR
                ? (Color){64, 170, 255, 255} : (Color){255, 214, 75, 255});
        DrawRectangle(left + ox * scale + 1, top + oy * scale + 1, 3, 3, color);
    }
    for (int i = 0; i < LAST_ZONE_ENEMIES; ++i)
        if (last_zone_enemy_on_radar(game, i) ||
            (alive == 1 && game->enemies[i].active)) {
            int ex = (int)game->enemies[i].x - origin_x, ey = (int)game->enemies[i].y - origin_y;
            if (ex >= 0 && ey >= 0 && ex < span && ey < span)
                DrawRectangle(left + ex * scale, top + ey * scale, 4, 4, (Color){255, 52, 147, 255});
        }
    if (extract_open) {
        int ex = (int)last_zone_extract_x(game) - origin_x;
        int ey = (int)last_zone_extract_y(game) - origin_y;
        if (ex >= 0 && ey >= 0 && ex < span && ey < span)
            DrawRectangle(left + ex * scale, top + ey * scale, 4, 4, (Color){72, 255, 214, 255});
    }
    int px = left + radius * scale, py = top + radius * scale;
    DrawRectangle(px - 1, py - 1, 3, 3, (Color){255, 235, 91, 255});
    DrawLine(px, py, px + (int)(cosf(game->angle) * 7), py + (int)(sinf(game->angle) * 7),
             (Color){255, 235, 91, 255});
}

static void draw_compass(const last_zone_game_t *game)
{
    static const char *marks[] = {"E", "SE", "S", "SW", "W", "NW", "N", "NE"};
    const int cx = 240, top = 5, half = 96;
    const Rectangle panel = {136, 4, 208, 50};
    const last_zone_look_t *look = layout_look(game);
    DrawRectangleRounded(panel, 0.18f, 4, (Color){7, 13, 18, 224});
    DrawRectangleRoundedLines(panel, 0.18f, 4, look->accent_dim);
    DrawRectangle(148, 22, 184, 1, (Color){82, 112, 108, 135});
    DrawRectangle(140, 8, 4, 42, look->accent);
    DrawText(look->name, 208, 40, 10, look->accent);
    DrawRectangle(cx - 1, top, 2, 16, (Color){255, 220, 72, 255});
    for (int i = 0; i < 8; ++i) {
        float relative = (float)i * 0.7853982f - game->angle;
        while (relative > 3.1415927f) relative -= 6.2831853f;
        while (relative < -3.1415927f) relative += 6.2831853f;
        int x = cx + (int)(relative * 58.0f);
        if (x < cx - half + 8 || x > cx + half - 10) continue;
        bool cardinal = (i % 2) == 0;
        DrawText(marks[i], x - (cardinal ? 4 : 6), top + 1, cardinal ? 12 : 10,
                 cardinal ? (Color){239, 242, 224, 255} : (Color){140, 160, 150, 255});
    }
}

static void format_clock_buf(uint32_t ticks, char out[8])
{
    unsigned seconds = ticks / 30U;
    out[0] = (char)('0' + (seconds / 60U) / 10U);
    out[1] = (char)('0' + (seconds / 60U) % 10U);
    out[2] = ':';
    out[3] = (char)('0' + (seconds % 60U) / 10U);
    out[4] = (char)('0' + (seconds % 60U) % 10U);
    out[5] = 0;
}

static const char *format_clock(uint32_t ticks)
{
    static char buffer[8];
    format_clock_buf(ticks, buffer);
    return buffer;
}

static unsigned accuracy_pct(const last_zone_game_t *game)
{
    if (!game->shots_fired) return 0;
    return (unsigned)game->shots_hit * 100U / (unsigned)game->shots_fired;
}

static void draw_bearing_marker(float bearing, Color color, const char *label)
{
    float clamped = bearing;
    if (clamped > 1.05f) clamped = 1.05f;
    if (clamped < -1.05f) clamped = -1.05f;
    int x = 240 + (int)(clamped * 118.0f);
    DrawTriangle((Vector2){(float)x, 70}, (Vector2){(float)(x - 8), 86},
                 (Vector2){(float)(x + 8), 86}, color);
    DrawText(label, x - 18, 88, 12, color);
}

static void draw_status_hud(const last_zone_game_t *game)
{
    int alive = last_zone_enemies_alive(game);
    DrawText(format_clock(game->tick), 222, 26, 12, (Color){239, 242, 224, 255});
    for (int i = 0; i < LAST_ZONE_MAX_HP; ++i) {
        Color pip = i < game->hp ? (game->hp > 2 ? (Color){65, 220, 116, 255}
                                                 : (Color){255, 72, 72, 255})
                                 : (Color){36, 40, 44, 255};
        DrawRectangle(148 + i * 13, 29, 11, 7, pip);
    }
    for (int i = 0; i < LAST_ZONE_MAX_ARMOR; ++i)
        DrawRectangle(148 + i * 9, 41, 7, 5,
                      i < game->armor ? (Color){64, 170, 255, 255}
                                      : (Color){28, 44, 58, 255});
    int ammo_shown = game->ammo > 10 ? 10 : (int)game->ammo;
    for (int i = 0; i < 10; ++i) {
        Color tick = i < ammo_shown ? (Color){255, 214, 75, 255} : (Color){36, 40, 44, 255};
        DrawRectangle(285 + i * 3, 29, 2, 7, tick);
    }
    DrawText(TextFormat("%u", (unsigned)game->ammo), 319, 27, 11,
             (Color){255, 214, 75, 255});
    DrawText(TextFormat("EN %d", alive), 285, 40, 9, (Color){255, 92, 140, 255});
}

static void draw_guidance(const last_zone_game_t *game)
{
    int alive = last_zone_enemies_alive(game);
    int last = last_zone_last_enemy_index(game);
    if (alive == 0) {
        float dx = last_zone_extract_x(game) - game->x;
        float dy = last_zone_extract_y(game) - game->y;
        unsigned meters = (unsigned)sqrtf(dx * dx + dy * dy);
        draw_bearing_marker(last_zone_extract_bearing(game), (Color){72, 255, 214, 255},
                            TextFormat("EX %um", meters));
        DrawText("EXTRACT OPEN", 176, 104, 14, (Color){72, 255, 214, 255});
    } else if (last >= 0) {
        float dx = game->enemies[last].x - game->x, dy = game->enemies[last].y - game->y;
        unsigned meters = (unsigned)sqrtf(dx * dx + dy * dy);
        float bearing = atan2f(dy, dx) - game->angle;
        while (bearing > 3.1415927f) bearing -= 6.2831853f;
        while (bearing < -3.1415927f) bearing += 6.2831853f;
        draw_bearing_marker(bearing, (Color){255, 92, 140, 255}, TextFormat("EN %um", meters));
        DrawText("LAST HOSTILE INBOUND", 132, 104, 14, (Color){255, 92, 140, 255});
    }
    if (!game->ammo && alive > 1) {
        int nearest = -1;
        float best = 1e9f;
        for (int i = 0; i < LAST_ZONE_PICKUPS; ++i) {
            if (game->pickups[i].taken || game->pickups[i].kind != NEON_PICKUP_AMMO) continue;
            float dx = game->pickups[i].x - game->x, dy = game->pickups[i].y - game->y;
            float dist = dx * dx + dy * dy;
            if (dist < best) { best = dist; nearest = i; }
        }
        if (nearest >= 0) {
            float dx = game->pickups[nearest].x - game->x;
            float dy = game->pickups[nearest].y - game->y;
            float bearing = atan2f(dy, dx) - game->angle;
            while (bearing > 3.1415927f) bearing -= 6.2831853f;
            while (bearing < -3.1415927f) bearing += 6.2831853f;
            draw_bearing_marker(bearing, (Color){255, 214, 75, 255}, "AMMO");
        }
    }
}

static void draw_damage_frame(int thickness, Color color)
{
    enum { SIZE = 480, SEGMENTS = 8 };
    if (thickness < 1) return;
    if (thickness > 14) thickness = 14;
    for (int layer = 0; layer < thickness; ++layer) {
        float inset = (float)layer;
        DrawRectangleRoundedLines(
            (Rectangle){inset, inset, (float)SIZE - inset * 2.0f,
                        (float)SIZE - inset * 2.0f},
            0.26f, SEGMENTS, color);
    }
}

static void draw_controls(const last_zone_game_t *game, MosaicoAtlas controls)
{
    int thumb_x = LAST_ZONE_MOVE_X + (int)(game->move_strafe * 28.0f);
    int thumb_y = LAST_ZONE_MOVE_Y - (int)(game->move_forward * 28.0f);
    const MosaicoSpriteFrame *joystick = MosaicoAtlasGetFrame(
        controls, MOSAICO_ASSET_ID_JOYSTICK_BASE);
    const MosaicoSpriteFrame *fire = MosaicoAtlasGetFrame(
        controls, MOSAICO_ASSET_ID_FIRE_BUTTON);
    if (joystick) DrawTexturePro(controls.texture, joystick->source,
        (Rectangle){42, 352, joystick->source.width, joystick->source.height},
        (Vector2){0, 0}, 0, WHITE);
    DrawCircle(thumb_x, thumb_y, 9, game->sprinting ? (Color){255, 220, 72, 255}
                                                    : (Color){186, 224, 217, 255});
    if (fire) DrawTexturePro(controls.texture, fire->source,
        (Rectangle){358, 352, fire->source.width, fire->source.height},
        (Vector2){0, 0}, 0, WHITE);
    if (game->fire_held)
        DrawCircle(LAST_ZONE_FIRE_X, LAST_ZONE_FIRE_Y, 16, (Color){255, 220, 72, 255});
    DrawText(game->sprinting ? "SPRINT" : "MOVE", 54, 338, 10,
             game->sprinting ? (Color){255, 220, 72, 255} : (Color){192, 235, 214, 255});
    const char *fire_label = !game->ammo ? "DRY" : (game->fire_cooldown ? "BOLT" : "FIRE");
    Color fire_color = !game->ammo ? (Color){255, 92, 70, 255} : (Color){255, 220, 72, 255};
    DrawText(fire_label, 376, 338, 10, fire_color);
    Color cross = game->holding_breath ? (Color){72, 255, 214, 255}
                                      : (Color){255, 220, 72, 255};
    int cross_w = game->holding_breath ? 16 : 22;
    int cross_h = game->holding_breath ? 16 : 22;
    DrawRectangle(240 - cross_w / 2, 203, cross_w, 2, cross);
    DrawRectangle(239, 204 - cross_h / 2, 2, cross_h, cross);
    if (game->holding_breath)
        DrawText("HOLD", 226, 176, 12, (Color){72, 255, 214, 255});
    draw_status_hud(game);
    draw_guidance(game);
    if (game->hit_marker) {
        Color marker = game->kill_flash ? (Color){255, 76, 65, 255} : WHITE;
        DrawLine(226, 191, 234, 199, marker); DrawLine(254, 191, 246, 199, marker);
        DrawLine(226, 219, 234, 211, marker); DrawLine(254, 219, 246, 211, marker);
    }
    if (game->kill_flash)
        DrawRectangle(236, 148, 8, 8, (Color){255, 214, 75, 255});
    if (game->last_fire == NEON_FIRE_SHOT) {
        DrawCircle(248, 198, 3, (Color){255, 214, 96, 255});
        DrawCircle(232, 212, 2, (Color){255, 168, 72, 255});
    }
    if (game->pickup_flash)
        DrawText("SECURED", 204, 248, 16, (Color){72, 255, 214, 255});
    if (game->door_flash)
        DrawText("GATE OPEN", 196, 248, 16, (Color){255, 220, 72, 255});
    if (game->dry_flash)
        DrawText("NO AMMO", 198, 264, 16, (Color){255, 92, 70, 255});
    if (last_zone_door_ahead(game))
        DrawText("FIRE TO OPEN GATE", 148, 278, 14, (Color){255, 220, 72, 255});
    else if (last_zone_near_closed_door(game))
        DrawText("FACE GATE, THEN FIRE", 136, 278, 14, (Color){255, 220, 72, 255});
    if (game->hp == 1 && game->phase == LAST_ZONE_PHASE_PLAYING) {
        int pulse = 5 + (int)(sinf((float)game->tick * 0.28f) * 3.0f);
        draw_damage_frame(pulse, (Color){120, 18, 28, 255});
    }
    if (game->hit_flash) {
        Color hurt = (Color){255, 58, 74, 255};
        int edge = 6 + game->hit_flash;
        draw_damage_frame(edge, hurt);
        int arrow_x = 240 + (int)(sinf(game->damage_angle) * 105.0f);
        DrawTriangle((Vector2){(float)arrow_x, 62}, (Vector2){(float)arrow_x - 9, 76},
                     (Vector2){(float)arrow_x + 9, 76}, hurt);
    }
}

static void draw_round_stats(const last_zone_game_t *game, int y)
{
    char time_buf[8], best_buf[8];
    format_clock_buf(game->tick, time_buf);
    format_clock_buf(game->best_ticks, best_buf);
    DrawText(TextFormat("TIME %s   KILLS %u/%d", time_buf,
                        (unsigned)game->kills, last_zone_enemy_total(game)), 118, y, 14,
             (Color){239, 242, 224, 255});
    DrawText(TextFormat("HIT %u%%   DMG %u   HP %u  AMMO %u", accuracy_pct(game),
                        (unsigned)game->damage_taken, (unsigned)game->hp,
                        (unsigned)game->ammo), 110, y + 20, 14,
             (Color){192, 235, 214, 255});
    DrawText(TextFormat("GRADE %c   %s %s", last_zone_grade(game),
                        game->best_updated ? "NEW BEST" : "BEST",
                        game->best_ticks ? best_buf : "--:--"),
             118, y + 40, 14, (Color){72, 255, 214, 255});
    DrawText(TextFormat("%s  %s  %s", game->spotted ? "SPOTTED" : "CLEAN",
                        game->barrel_used ? "BLAST" : "RIFLE",
                        game->armor_hit ? "ARMOR" : "FLESH"),
             126, y + 60, 12, (Color){192, 235, 214, 255});
}

static void draw_phase_overlay(const last_zone_game_t *game)
{
    if (game->phase == LAST_ZONE_PHASE_PLAYING) return;
    const last_zone_look_t *look = layout_look(game);
    const Rectangle panel = {58, 96, 364, 248};
    DrawRectangleRounded(panel, 0.16f, 6, (Color){8, 12, 18, 255});
    DrawRectangleRoundedLines(panel, 0.16f, 6, look->accent);
    if (game->phase == LAST_ZONE_PHASE_START) {
        DrawText("LAST ZONE", 166, 112, 28, (Color){239, 242, 224, 255});
        const char *briefing=last_zone_briefing(game);
        int briefing_x=240-MeasureText(briefing,14)/2;
        DrawText(briefing, briefing_x, 156, 14, (Color){192, 235, 214, 255});
        DrawText("TAP FIRE TO SHOOT / OPEN GATE", 82, 178, 16, (Color){192, 235, 214, 255});
        DrawText("HOLD FIRE TO STEADY, STAND SILENT", 78, 200, 14, (Color){192, 235, 214, 255});
        DrawText("CLEAR ALL, THEN THE EXTRACT PAD", 86, 222, 14, (Color){255, 220, 72, 255});
        char best_buf[8];
        format_clock_buf(game->best_ticks, best_buf);
        DrawText(TextFormat("MISSION %u/%u %s", (unsigned)game->layout + 1U,
                            (unsigned)LAST_ZONE_LAYOUTS, look->name),
                 118, 250, 14, look->accent);
        DrawText(TextFormat("BEST %s", game->best_ticks ? best_buf : "--:--"),
                 190, 270, 12, (Color){192, 235, 214, 255});
        DrawText("TAP TO DEPLOY", 168, 300, 16, (Color){255, 220, 72, 255});
    } else if (game->phase == LAST_ZONE_PHASE_WON) {
        DrawText("EXTRACT SECURE", 128, 112, 24, (Color){72, 255, 214, 255});
        draw_round_stats(game, 160);
        DrawText("TAP TO REDEPLOY", 152, 292, 16, (Color){255, 220, 72, 255});
    } else {
        DrawText("DOWNED", 188, 112, 28, (Color){255, 92, 70, 255});
        draw_round_stats(game, 160);
        DrawText("TAP TO REDEPLOY", 152, 292, 16, (Color){255, 220, 72, 255});
    }
}

void last_zone_view_render(const last_zone_game_t *game, MosaicoAtlas enemies,
                           MosaicoAtlas weapon, MosaicoAtlas environment,
                           MosaicoAtlas materials, MosaicoAtlas controls,
                           MosaicoAtlas props)
{
    if (!game) return;
    mosaico_shade_lut_init();
    s_view_stats = (last_zone_view_stats_t){0};
    load_material_frames(materials);
    int64_t t0 = view_now_us();
    BeginDrawing();
    int64_t t1 = view_now_us();
    raycast_world(game);
    int64_t t2 = view_now_us();
    draw_panorama(game, environment, panorama_height(game));
    int64_t t3 = view_now_us();
    draw_floor(game, materials, s_material_frames[3] ? s_material_frames[3] : s_material_frames[1]);
    int64_t t4 = view_now_us();
    draw_walls(game, materials, s_material_frames);
    int64_t t5 = view_now_us();
    int64_t t6 = t5;
    draw_extract(game);
    draw_pickups(game, props);
    draw_enemies(game, enemies);
    int64_t t7 = view_now_us();
    draw_radar(game);
    draw_compass(game);
    draw_weapon(game, weapon);
    draw_controls(game, controls);
    draw_phase_overlay(game);
    int64_t t8 = view_now_us();
    EndDrawing();
    int64_t t9 = view_now_us();
    s_view_stats.acquire_us = (uint32_t)(t1 - t0);
    s_view_stats.raycast_us = (uint32_t)(t2 - t1);
    s_view_stats.sky_us = (uint32_t)(t3 - t2);
    s_view_stats.floor_us = (uint32_t)(t4 - t3);
    s_view_stats.wall_us = (uint32_t)(t5 - t4);
    s_view_stats.grade_us = (uint32_t)(t6 - t5);
    s_view_stats.sprites_us = (uint32_t)(t7 - t6);
    s_view_stats.hud_us = (uint32_t)(t8 - t7);
    s_view_stats.submit_us = (uint32_t)(t9 - t8);
    s_view_stats.total_us = (uint32_t)(t9 - t0);
    mosaico_game_2d_set_phase_us(s_view_stats.sky_us, s_view_stats.floor_us,
                                 s_view_stats.wall_us, s_view_stats.sprites_us,
                                 s_view_stats.hud_us);
}
