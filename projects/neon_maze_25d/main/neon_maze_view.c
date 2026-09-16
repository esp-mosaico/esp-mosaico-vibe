// SPDX-License-Identifier: Apache-2.0
#include "neon_maze_view.h"
#include <math.h>
#include "assets_ids.h"
#include "mosaico_raylib_fast.h"

#define NEON_MAZE_COLUMNS 120
#define NEON_MAZE_COLUMN_WIDTH 4
#define NEON_MAZE_HORIZON 205
#define NEON_MAZE_FOV 1.0471976f
#define NEON_MAZE_FLOOR_SCALE 165.0f

static float s_depth[NEON_MAZE_COLUMNS];
static uint16_t s_wall_bottom[NEON_MAZE_COLUMNS];
static uint8_t s_wall_type[NEON_MAZE_COLUMNS];
static uint8_t s_wall_side[NEON_MAZE_COLUMNS];
static uint8_t s_wall_u[NEON_MAZE_COLUMNS];
static int16_t s_wall_top[NEON_MAZE_COLUMNS];
static int16_t s_wall_height[NEON_MAZE_COLUMNS];

static int view_horizon(const neon_maze_game_t *game)
{
    float kick = game->hit_flash ? sinf((float)game->hit_flash * 2.15f) * 6.0f : 0.0f;
    int horizon = NEON_MAZE_HORIZON + (int)(game->look_pitch + kick);
    if (horizon < 150) horizon = 150;
    if (horizon > 260) horizon = 260;
    return horizon;
}

static unsigned distance_light(float corrected, bool side, bool door)
{
    float light = 286.0f / (1.0f + corrected * 0.26f);
    if (light > 256.0f) light = 256.0f;
    if (light < 72.0f) light = 72.0f;
    if (side) light *= 0.76f;
    if (door) light *= 0.82f;
    return (unsigned)light;
}

static void draw_panorama(const neon_maze_game_t *game, MosaicoAtlas environment)
{
    static const mosaico_asset_id_t ids[] = {MOSAICO_ASSET_ID_TACTICAL_PANORAMA_LEFT,
                                           MOSAICO_ASSET_ID_TACTICAL_PANORAMA_RIGHT};
    const float available = 504.0f, window = 300.0f;
    int horizon = view_horizon(game);
    float offset = fmodf(game->angle / 6.2831853f * available, available);
    float consumed = 0.0f;
    while (consumed < window) {
        float position = fmodf(offset + consumed, available);
        int half = position >= 252.0f ? 1 : 0;
        float local = position - half * 252.0f;
        float chunk = 252.0f - local;
        if (chunk > window - consumed) chunk = window - consumed;
        const MosaicoSpriteFrame *frame = MosaicoAtlasGetFrame(environment, ids[half]);
        if (!frame) return;
        float x = 480.0f * consumed / window, width = 480.0f * chunk / window;
        DrawTexturePro(environment.texture,
            (Rectangle){frame->source.x + 2 + local, frame->source.y + 2, chunk, 201},
            (Rectangle){x, 0, width, (float)horizon}, (Vector2){0, 0}, 0, WHITE);
        consumed += chunk;
    }
}

static void project_sprite(const neon_maze_game_t *game, float world_x, float world_y,
                           int *center, int *size, int *ground, float *distance_out)
{
    const float projection = 415.69f;
    float dx = world_x - game->x, dy = world_y - game->y;
    float relative = atan2f(dy, dx) - game->angle;
    while (relative > 3.1415927f) relative -= 6.2831853f;
    while (relative < -3.1415927f) relative += 6.2831853f;
    *center = 0;
    *size = 0;
    *ground = 0;
    *distance_out = 0;
    if (fabsf(relative) > .72f) return;
    float distance = sqrtf(dx * dx + dy * dy) * cosf(relative);
    if (distance <= .1f) return;
    *center = 240 + (int)(tanf(relative) * projection);
    *size = (int)(280.0f / distance);
    if (*size < 10) *size = 10;
    if (*size > 260) *size = 260;
    *ground = (int)((float)view_horizon(game) + 165.0f / distance);
    if (*ground > 480) *ground = 480;
    *distance_out = distance;
}

static bool column_visible(int screen, float distance)
{
    int column = screen / NEON_MAZE_COLUMN_WIDTH;
    if (screen < 0 || screen >= 480 || column < 0 || column >= NEON_MAZE_COLUMNS) return false;
    /* A sprite represents an area rather than an infinitely thin point.  The small
       margin and adjacent-column depth stop wall edges from making it blink while
       either the actor or camera moves. */
    float depth = s_depth[column];
    if (column > 0 && s_depth[column - 1] > depth) depth = s_depth[column - 1];
    if (column + 1 < NEON_MAZE_COLUMNS && s_depth[column + 1] > depth)
        depth = s_depth[column + 1];
    return distance < depth + .10f;
}

static void draw_enemy_effect(const neon_maze_game_t *game,
                              const neon_maze_enemy_t *enemy)
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

static void draw_enemies(const neon_maze_game_t *game, MosaicoAtlas atlas)
{
    static const mosaico_asset_id_t run_frames[] = {MOSAICO_ASSET_ID_ENEMY_RUN_0,
        MOSAICO_ASSET_ID_ENEMY_RUN_1, MOSAICO_ASSET_ID_ENEMY_RUN_2};
    int order[NEON_MAZE_ENEMIES];
    float distances[NEON_MAZE_ENEMIES];
    for (int i = 0; i < NEON_MAZE_ENEMIES; ++i) order[i] = i;
    for (int i = 0; i < NEON_MAZE_ENEMIES; ++i) {
        float dx = game->enemies[i].x - game->x;
        float dy = game->enemies[i].y - game->y;
        distances[i] = sqrtf(dx * dx + dy * dy);
    }
    for (int i = 0; i < NEON_MAZE_ENEMIES - 1; ++i)
        for (int j = i + 1; j < NEON_MAZE_ENEMIES; ++j)
            if (distances[order[i]] < distances[order[j]]) {
                int swap = order[i];
                order[i] = order[j];
                order[j] = swap;
            }
    for (int n = 0; n < NEON_MAZE_ENEMIES; ++n) {
        const neon_maze_enemy_t *enemy = &game->enemies[order[n]];
        if (!enemy->active) {
            if (enemy->death_timer) draw_enemy_effect(game, enemy);
            continue;
        }
        const MosaicoSpriteFrame *frame = MosaicoAtlasGetFrame(
            atlas, run_frames[((game->tick / 5U) + enemy->move_phase) % 3U]);
        if (!frame) continue;
        int center, size, ground;
        float distance;
        project_sprite(game, enemy->x, enemy->y, &center, &size, &ground, &distance);
        if (size <= 0) continue;
        int left = center - size / 2, top = ground - size, run_start = -1;
        bool visible_any = false;
        for (int x = 0; x < size + 4; x += 4) {
            int screen = left + x;
            bool visible = x < size && column_visible(screen, distance);
            if (visible) {
                visible_any = true;
                if (run_start < 0) run_start = x;
            }
            if (!visible && run_start >= 0) {
                int run_end = x < size ? x : size, run_width = run_end - run_start;
                int screen_left = left + run_start;
                float source_x = frame->source.x + frame->source.width * (float)run_start / size;
                float source_width = frame->source.width * (float)run_width / size;
                DrawTexturePro(atlas.texture,
                    (Rectangle){source_x, frame->source.y, source_width, frame->source.height},
                    (Rectangle){(float)screen_left, (float)top, (float)run_width, (float)size},
                    (Vector2){0, 0}, 0, enemy->hit_flash ? (Color){255, 100, 82, 255} : WHITE);
                run_start = -1;
            }
        }
        if (visible_any && (enemy->hit_flash || enemy->hp < 2)) {
            int bar_width = size < 48 ? size : 48, bar_x = center - bar_width / 2, bar_y = top - 7;
            DrawRectangle(bar_x, bar_y, bar_width, 4, (Color){28, 20, 18, 255});
            DrawRectangle(bar_x + 1, bar_y + 1, (bar_width - 2) * enemy->hp / 2, 2,
                          enemy->hp > 1 ? (Color){91, 218, 112, 255} : (Color){245, 77, 61, 255});
        }
        if (visible_any && enemy->ai_state == NEON_ENEMY_ALERT) {
            DrawCircle(center, top - 12, 9, (Color){255, 210, 55, 255});
            DrawText("!", center - 3, top - 19, 14, (Color){45, 28, 12, 255});
        } else if (visible_any && enemy->ai_state == NEON_ENEMY_SEARCH) {
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

static void draw_extract(const neon_maze_game_t *game)
{
    int center, size, ground;
    float distance;
    project_sprite(game, 22.5f, 22.5f, &center, &size, &ground, &distance);
    if (size <= 0) return;
    bool clear = neon_maze_enemies_alive(game) == 0;
    Color neon = clear ? (Color){72, 255, 214, 255} : (Color){255, 92, 70, 255};
    int left = center - size / 2, top = ground - size, run_start = -1;
    for (int x = 0; x < size + 4; x += 4) {
        int screen = left + x;
        bool visible = x < size && column_visible(screen, distance);
        if (visible && run_start < 0) run_start = x;
        if (!visible && run_start >= 0) {
            int run_width = (x < size ? x : size) - run_start;
            int screen_left = left + run_start;
            DrawRectangle(screen_left, top, 4, size, neon);
            if (run_width > 4) DrawRectangle(screen_left + run_width - 4, top, 4, size, neon);
            DrawRectangle(screen_left, top, run_width, 4, neon);
            DrawRectangle(screen_left, top + size - 4, run_width, 4, neon);
            run_start = -1;
        }
    }
}

static void raycast_world(const neon_maze_game_t *game)
{
    int horizon = view_horizon(game);
    for (int column = 0; column < NEON_MAZE_COLUMNS; ++column) {
        float ray = game->angle - NEON_MAZE_FOV * .5f +
                    NEON_MAZE_FOV * ((float)column + .5f) / NEON_MAZE_COLUMNS;
        float rx = cosf(ray), ry = sinf(ray);
        int map_x = (int)game->x, map_y = (int)game->y;
        int step_x = rx < 0 ? -1 : 1, step_y = ry < 0 ? -1 : 1;
        float delta_x = fabsf(1.0f / rx), delta_y = fabsf(1.0f / ry);
        float side_x = (rx < 0 ? game->x - map_x : map_x + 1.0f - game->x) * delta_x;
        float side_y = (ry < 0 ? game->y - map_y : map_y + 1.0f - game->y) * delta_y;
        uint8_t wall = 0;
        bool side = false;
        for (int step = 0; step < NEON_MAZE_WIDTH + NEON_MAZE_HEIGHT && !wall; ++step) {
            if (side_x < side_y) {
                side_x += delta_x;
                map_x += step_x;
                side = false;
            } else {
                side_y += delta_y;
                map_y += step_y;
                side = true;
            }
            uint8_t cell = neon_maze_cell(map_x, map_y);
            if (neon_maze_blocks(game, map_x, map_y)) wall = cell ? cell : 1;
        }
        float distance = side ? side_y - delta_y : side_x - delta_x;
        float corrected = distance * cosf(ray - game->angle);
        if (corrected < .08f) corrected = .08f;
        int height = (int)(330.0f / corrected);
        if (height > 356) height = 356;
        int top = horizon - height / 2;
        int bottom = top + height;
        if (bottom > 480) bottom = 480;
        if (bottom < horizon) bottom = horizon;
        float hit = side ? game->x + distance * rx : game->y + distance * ry;
        float u = hit - floorf(hit);
        s_depth[column] = corrected;
        s_wall_bottom[column] = (uint16_t)bottom;
        s_wall_top[column] = (int16_t)top;
        s_wall_height[column] = (int16_t)height;
        s_wall_type[column] = wall;
        s_wall_side[column] = side ? 1 : 0;
        s_wall_u[column] = (uint8_t)(u * 255.0f);
    }
}

static void draw_floor(const neon_maze_game_t *game, MosaicoAtlas materials,
                       const MosaicoSpriteFrame *tile)
{
    if (!tile) return;
    float dir_x = cosf(game->angle), dir_y = sinf(game->angle);
    int horizon = view_horizon(game);
    float plane = tanf(NEON_MAZE_FOV * .5f);
    float plane_x = -dir_y * plane, plane_y = dir_x * plane;
    float cam0 = (0.5f / (float)NEON_MAZE_COLUMNS) * 2.0f - 1.0f;
    float cam_step = 2.0f / (float)NEON_MAZE_COLUMNS;
    for (int y = horizon + 1; y < 480; ++y) {
        float dist = NEON_MAZE_FLOOR_SCALE / (float)(y - horizon);
        float ray_x = dir_x + plane_x * cam0, ray_y = dir_y + plane_y * cam0;
        float wx = game->x + ray_x * dist, wy = game->y + ray_y * dist;
        float dwx = (plane_x * cam_step) * dist, dwy = (plane_y * cam_step) * dist;
        int u_16 = (int)(wx * 128.0f * 65536.0f);
        int v_16 = (int)(wy * 128.0f * 65536.0f);
        int du_16 = (int)(dwx * 128.0f * 65536.0f);
        int dv_16 = (int)(dwy * 128.0f * 65536.0f);
        unsigned light = (unsigned)(240.0f / (1.0f + dist * 0.22f));
        if (light < 58U) light = 58U;
        if (light > 210U) light = 210U;
        Mosaico2DDrawFloorRow(materials.texture, tile->source, y, 0, NEON_MAZE_COLUMN_WIDTH,
                              NEON_MAZE_COLUMNS, s_wall_bottom, u_16, v_16, du_16, dv_16,
                              light);
    }
}

static void draw_walls(MosaicoAtlas materials, const MosaicoSpriteFrame *frames[3])
{
    for (int column = 0; column < NEON_MAZE_COLUMNS; ++column) {
        uint8_t wall = s_wall_type[column];
        if (!wall) continue;
        const MosaicoSpriteFrame *material = frames[wall == 4 ? 2 : (wall - 1U) % 3U];
        if (!material) continue;
        float u = (float)s_wall_u[column] / 255.0f;
        if (wall == 4) u = 0.18f + u * 0.64f;
        Mosaico2DDrawColumn(materials.texture,
            (Rectangle){material->source.x + u * (material->source.width - 2.0f),
                        material->source.y, 2, material->source.height},
            column * NEON_MAZE_COLUMN_WIDTH, s_wall_top[column], NEON_MAZE_COLUMN_WIDTH,
            s_wall_height[column],
            distance_light(s_depth[column], s_wall_side[column] != 0, wall == 4));
    }
}

static void draw_world(const neon_maze_game_t *game, MosaicoAtlas atlas,
                       MosaicoAtlas materials)
{
    const mosaico_asset_id_t material_ids[] = {MOSAICO_ASSET_ID_WALL_CONCRETE,
        MOSAICO_ASSET_ID_WALL_BRICK, MOSAICO_ASSET_ID_WALL_CONTAINER};
    const MosaicoSpriteFrame *material_frames[3];
    for (int i = 0; i < 3; ++i) material_frames[i] = MosaicoAtlasGetFrame(materials, material_ids[i]);
    raycast_world(game);
    draw_floor(game, materials, material_frames[1] ? material_frames[1] : material_frames[0]);
    draw_walls(materials, material_frames);
    draw_extract(game);
    draw_enemies(game, atlas);
}

static void draw_weapon(const neon_maze_game_t *game, MosaicoAtlas atlas)
{
    const MosaicoSpriteFrame *frame = MosaicoAtlasGetFrame(atlas, MOSAICO_ASSET_ID_K98_RIFLE);
    if (!frame) return;
    float motion = sqrtf(game->move_forward * game->move_forward +
                         game->move_strafe * game->move_strafe);
    if (motion > 1.0f) motion = 1.0f;
    float bob_x = sinf(game->move_phase) * 5.0f * motion;
    float bob_y = fabsf(cosf(game->move_phase)) * 5.0f * motion;
    float recoil_y = -18.0f * game->weapon_recoil;
    float recoil_x = 4.0f * game->weapon_recoil;
    DrawTexturePro(atlas.texture, frame->source,
        (Rectangle){100.0f + bob_x + recoil_x,
                    180.0f + game->look_pitch * .38f + bob_y + recoil_y,
                    frame->source.width, frame->source.height},
        (Vector2){0, 0}, 0, WHITE);
}

static void draw_radar(const neon_maze_game_t *game)
{
    const int left = 12, top = 64, scale = 3;
    DrawRectangle(left - 4, top - 4, NEON_MAZE_WIDTH * scale + 8,
                  NEON_MAZE_HEIGHT * scale + 8, (Color){3, 9, 20, 255});
    for (int y = 0; y < NEON_MAZE_HEIGHT; ++y)
        for (int x = 0; x < NEON_MAZE_WIDTH; ++x) {
            uint8_t cell = neon_maze_cell(x, y);
            if (!cell) continue;
            if (cell == 4 && game->door_open[y][x]) continue;
            static const Color colors[] = {{0, 0, 0, 0}, {34, 101, 119, 255},
                {132, 37, 91, 255}, {142, 107, 37, 255}, {220, 180, 60, 255},
                {80, 220, 200, 255}};
            if (cell > 5) continue;
            DrawRectangle(left + x * scale, top + y * scale, scale - 1, scale - 1, colors[cell]);
        }
    for (int i = 0; i < NEON_MAZE_ENEMIES; ++i)
        if (game->enemies[i].active)
            DrawRectangle(left + (int)(game->enemies[i].x * scale) - 1,
                          top + (int)(game->enemies[i].y * scale) - 1, 3, 3,
                          (Color){255, 52, 147, 255});
    int px = left + (int)(game->x * scale), py = top + (int)(game->y * scale);
    DrawRectangle(px - 1, py - 1, 3, 3, (Color){255, 235, 91, 255});
    DrawLine(px, py, px + (int)(cosf(game->angle) * 6), py + (int)(sinf(game->angle) * 6),
             (Color){255, 235, 91, 255});
}

static void draw_controls(const neon_maze_game_t *game, MosaicoAtlas controls)
{
    const int joy_x = 82, joy_y = 392;
    int thumb_x = joy_x + (int)(game->move_strafe * 32.0f);
    int thumb_y = joy_y - (int)(game->move_forward * 32.0f);
    const MosaicoSpriteFrame *joystick = MosaicoAtlasGetFrame(
        controls, MOSAICO_ASSET_ID_JOYSTICK_BASE);
    const MosaicoSpriteFrame *fire = MosaicoAtlasGetFrame(
        controls, MOSAICO_ASSET_ID_FIRE_BUTTON);
    if (joystick) DrawTexturePro(controls.texture, joystick->source,
        (Rectangle){32, 342, 100, 100}, (Vector2){0, 0}, 0, WHITE);
    DrawCircle(thumb_x, thumb_y, 12, (Color){186, 224, 217, 255});
    if (fire) DrawTexturePro(controls.texture, fire->source,
        (Rectangle){360, 342, 100, 100}, (Vector2){0, 0}, 0, WHITE);
    DrawText(TextFormat("%04u", game->score), 418, 18, 14, (Color){239, 242, 224, 255});
    DrawRectangle(229, 203, 22, 2, (Color){255, 220, 72, 255});
    DrawRectangle(239, 193, 2, 22, (Color){255, 220, 72, 255});
    DrawText("HP", 12, 38, 12, (Color){239, 242, 224, 255});
    DrawRectangle(34, 39, 112, 12, (Color){25, 28, 31, 255});
    int delayed_width = (int)(108.0f * game->display_hp / NEON_MAZE_MAX_HP);
    DrawRectangle(36, 41, delayed_width, 8, (Color){245, 211, 75, 255});
    DrawRectangle(36, 41, 108 * game->hp / NEON_MAZE_MAX_HP, 8,
                  game->hp > 1 ? (Color){65, 220, 116, 255} : (Color){255, 72, 72, 255});
    DrawText(TextFormat("%u/%u", game->hp, NEON_MAZE_MAX_HP), 150, 39, 12,
             (Color){239, 242, 224, 255});
    if (game->hit_marker) {
        Color marker = game->kill_flash ? (Color){255, 76, 65, 255} : WHITE;
        DrawLine(226, 191, 234, 199, marker); DrawLine(254, 191, 246, 199, marker);
        DrawLine(226, 219, 234, 211, marker); DrawLine(254, 219, 246, 211, marker);
    }
    if (game->kill_flash)
        DrawText("HOSTILE DOWN", 181, 232, 16, (Color){255, 214, 75, 255});
    if (game->hit_flash) {
        Color hurt = (Color){255, 58, 74, 255};
        int edge = 6 + game->hit_flash;
        DrawRectangle(0, 0, 480, edge, hurt); DrawRectangle(0, 480-edge, 480, edge, hurt);
        DrawRectangle(0, 0, edge, 480, hurt); DrawRectangle(480-edge, 0, edge, 480, hurt);
        int arrow_x = 240 + (int)(sinf(game->damage_angle) * 105.0f);
        DrawTriangle((Vector2){(float)arrow_x, 62}, (Vector2){(float)arrow_x - 9, 76},
                     (Vector2){(float)arrow_x + 9, 76}, hurt);
    }
}

static void draw_technical_stats(const neon_maze_game_t *game)
{
    int alive = neon_maze_enemies_alive(game);
    DrawRectangle(91, 8, 377, 25, (Color){8, 14, 18, 255});
    DrawText(TextFormat("L %.1f  D %.1f  R %.1fms  EN %02d  %02d:%02d",
        game->perf_logic_fps, game->perf_display_fps, game->perf_render_ms, alive,
        (int)game->x, (int)game->y), 99, 15, 11, (Color){192, 235, 214, 255});
}

static void draw_phase_overlay(const neon_maze_game_t *game)
{
    if (game->phase == NEON_MAZE_PHASE_PLAYING) {
        if (neon_maze_enemies_alive(game) == 0)
            DrawText("EXTRACT OPEN", 168, 40, 16, (Color){72, 255, 214, 255});
        return;
    }
    DrawRectangle(40, 150, 400, 168, (Color){8, 12, 18, 255});
    DrawRectangle(40, 150, 400, 4, (Color){72, 255, 214, 255});
    if (game->phase == NEON_MAZE_PHASE_START) {
        DrawText("NEON MAZE", 160, 168, 28, (Color){239, 242, 224, 255});
        DrawText("CLEAR HOSTILES, OPEN DOORS,", 92, 214, 16, (Color){192, 235, 214, 255});
        DrawText("REACH THE CYAN EXTRACT", 118, 236, 16, (Color){192, 235, 214, 255});
        DrawText("FIRE TO START", 168, 276, 18, (Color){255, 220, 72, 255});
    } else if (game->phase == NEON_MAZE_PHASE_WON) {
        DrawText("EXTRACT SECURE", 128, 184, 24, (Color){72, 255, 214, 255});
        DrawText(TextFormat("SCORE %04u   TIME %u.%us", game->score,
                            (unsigned)(game->tick / 30U), (unsigned)((game->tick % 30U) * 10U / 3U)),
                 110, 228, 16, (Color){239, 242, 224, 255});
        DrawText("FIRE TO REDEPLOY", 148, 272, 18, (Color){255, 220, 72, 255});
    } else {
        DrawText("DOWNED", 188, 184, 28, (Color){255, 92, 70, 255});
        DrawText(TextFormat("SCORE %04u", game->score), 186, 228, 16,
                 (Color){239, 242, 224, 255});
        DrawText("FIRE TO REDEPLOY", 148, 272, 18, (Color){255, 220, 72, 255});
    }
}

void neon_maze_view_render(const neon_maze_game_t *game, MosaicoAtlas enemies,
                           MosaicoAtlas weapon, MosaicoAtlas environment,
                           MosaicoAtlas materials, MosaicoAtlas controls)
{
    if (!game) return;
    BeginDrawing();
    draw_panorama(game, environment);
    draw_world(game, enemies, materials);
    draw_radar(game);
    draw_weapon(game, weapon);
    draw_controls(game, controls);
    draw_technical_stats(game);
    draw_phase_overlay(game);
    EndDrawing();
}
