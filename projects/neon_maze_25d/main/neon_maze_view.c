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
static uint8_t s_wall_shift[NEON_MAZE_COLUMNS];
static int16_t s_wall_top[NEON_MAZE_COLUMNS];
static int16_t s_wall_height[NEON_MAZE_COLUMNS];

static int view_horizon(const neon_maze_game_t *game)
{
    float kick = game->hit_flash ? sinf((float)game->hit_flash * 2.15f) * 6.0f : 0.0f;
    int horizon = NEON_MAZE_HORIZON + (int)(game->look_pitch + game->look_kick + kick);
    if (horizon < 150) horizon = 150;
    if (horizon > 260) horizon = 260;
    return horizon;
}

static unsigned distance_light(float corrected, bool side, bool door, bool window,
                               bool corner, float flash)
{
    float light = 300.0f / (1.0f + corrected * 0.22f);
    if (light > 250.0f) light = 250.0f;
    if (light < 118.0f) light = 118.0f;
    if (side) light *= 0.74f;
    if (window) light *= 1.08f;
    if (door) light *= 1.18f;
    if (corner) light *= 0.68f;
    light += flash * 48.0f;
    if (light > 255.0f) light = 255.0f;
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
        mosaico_asset_id_t frame_id = MOSAICO_ASSET_ID_ENEMY_RUN_1;
        if (enemy->hit_flash) frame_id = MOSAICO_ASSET_ID_ENEMY_HIT;
        else if (enemy->attack_flash) frame_id = MOSAICO_ASSET_ID_ENEMY_FIRE;
        else if (enemy->aim_timer) frame_id = MOSAICO_ASSET_ID_ENEMY_AIM;
        else frame_id = run_frames[((game->tick / 4U) + enemy->move_phase) % 3U];
        const MosaicoSpriteFrame *frame = MosaicoAtlasGetFrame(atlas, frame_id);
        if (!frame) continue;
        int center, size, ground;
        float distance;
        project_sprite(game, enemy->x, enemy->y, &center, &size, &ground, &distance);
        if (size <= 0) continue;
        if (enemy->hp < 2) center += 3;
        Color tint = enemy->hit_flash ? (Color){255, 92, 64, 255} : WHITE;
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
                    (Vector2){0, 0}, 0, tint);
                run_start = -1;
            }
        }
        if (visible_any && enemy->hp < 2)
            DrawRectangle(center - 6, top - 4, 12, 3, (Color){255, 72, 48, 255});
        if (visible_any) {
            Color ident = enemy->move_phase % 3U == 0 ? (Color){72, 168, 214, 255}
                        : enemy->move_phase % 3U == 1 ? (Color){214, 168, 72, 255}
                        : (Color){168, 92, 72, 255};
            DrawRectangle(center - 3, top + size / 8, 6, 4, ident);
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

static void draw_billboard_box(const neon_maze_game_t *game, float world_x, float world_y,
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

static void draw_billboard_sprite(const neon_maze_game_t *game, MosaicoAtlas atlas,
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

static void draw_pickups(const neon_maze_game_t *game, MosaicoAtlas props)
{
    for (int i = 0; i < NEON_MAZE_PICKUPS; ++i) {
        if (game->pickups[i].taken) continue;
        mosaico_asset_id_t id = game->pickups[i].kind == NEON_PICKUP_HEALTH
            ? MOSAICO_ASSET_ID_PICKUP_HEALTH : MOSAICO_ASSET_ID_PICKUP_AMMO;
        if (props.texture.id)
            draw_billboard_sprite(game, props, id, game->pickups[i].x, game->pickups[i].y, 0.42f);
        else {
            Color color = game->pickups[i].kind == NEON_PICKUP_HEALTH
                ? (Color){255, 82, 96, 255} : (Color){255, 214, 75, 255};
            draw_billboard_box(game, game->pickups[i].x, game->pickups[i].y, color, 6);
        }
    }
    for (int i = 0; i < NEON_MAZE_PROPS; ++i) {
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

static void draw_extract(const neon_maze_game_t *game)
{
    int center, size, ground;
    float distance;
    project_sprite(game, NEON_MAZE_EXTRACT_X, NEON_MAZE_EXTRACT_Y, &center, &size, &ground, &distance);
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
        s_wall_shift[column] = (uint8_t)((map_x * 37 + map_y * 13) & 31);
    }
}

static int floor_kind_at(float wx, float wy)
{
    int mx = (int)wx, my = (int)wy;
    if (neon_maze_cell(mx, my) == 5) return 2;
    if (mx <= 11 && my <= 9) return 1;
    return 0;
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
        unsigned light = (unsigned)(200.0f / (1.0f + dist * 0.26f));
        if (light < 56U) light = 56U;
        if (light > 198U) light = 198U;
        light += (unsigned)(game->weapon_recoil * 36.0f);
        int run_kind = -1, run_start = 0;
        for (int column = 0; column <= NEON_MAZE_COLUMNS; ++column) {
            int kind = 0;
            if (column < NEON_MAZE_COLUMNS)
                kind = floor_kind_at(wx + dwx * (float)column, wy + dwy * (float)column);
            if (column == 0) {
                run_kind = kind;
                run_start = 0;
                continue;
            }
            if (column < NEON_MAZE_COLUMNS && kind == run_kind) continue;
            Rectangle floor_src = tile->source;
            if (run_kind == 1) {
                floor_src.y += 64.0f;
                floor_src.height = 64.0f;
            } else {
                floor_src.height = 64.0f;
            }
            floor_src.x += 6.0f;
            floor_src.y += 4.0f;
            floor_src.width -= 12.0f;
            floor_src.height -= 8.0f;
            unsigned run_light = light;
            if (run_kind == 2) run_light += 36U;
            if (run_light > 230U) run_light = 230U;
            int columns = column - run_start;
            Mosaico2DDrawFloorRow(materials.texture, floor_src, y,
                                  run_start * NEON_MAZE_COLUMN_WIDTH, NEON_MAZE_COLUMN_WIDTH,
                                  columns, s_wall_bottom,
                                  u_16 + du_16 * run_start, v_16 + dv_16 * run_start,
                                  du_16, dv_16, run_light);
            run_kind = kind;
            run_start = column;
        }
    }
}

static void draw_walls(const neon_maze_game_t *game, MosaicoAtlas materials,
                       const MosaicoSpriteFrame *frames[4])
{
    float flash = game->weapon_recoil;
    for (int column = 0; column < NEON_MAZE_COLUMNS; ++column) {
        uint8_t wall = s_wall_type[column];
        if (!wall) continue;
        int mat = 0;
        if (wall == 2 || wall == 4) mat = 1;
        else if (wall == 3) mat = 2;
        const MosaicoSpriteFrame *material = frames[mat];
        if (!material) continue;
        float inset = 6.0f;
        float inner = material->source.width - inset * 2.0f;
        float u = (float)s_wall_u[column] / 255.0f + (float)s_wall_shift[column] / 48.0f;
        u = u - floorf(u);
        if (wall == 4) u = 0.12f + u * 0.52f;
        bool corner = u < 0.08f || u > 0.92f;
        unsigned light = distance_light(s_depth[column], s_wall_side[column] != 0,
                                        wall == 4, wall == 2, corner, flash);
        if (wall == 4 && (game->tick % 20U) < 10U) light += 28U;
        if (light > 255U) light = 255U;
        Mosaico2DDrawColumn(materials.texture,
            (Rectangle){material->source.x + inset + u * (inner - 2.0f),
                        material->source.y + inset, 2,
                        material->source.height - inset * 2.0f},
            column * NEON_MAZE_COLUMN_WIDTH, s_wall_top[column], NEON_MAZE_COLUMN_WIDTH,
            s_wall_height[column], light);
    }
}

static void draw_world(const neon_maze_game_t *game, MosaicoAtlas atlas,
                       MosaicoAtlas materials, MosaicoAtlas props)
{
    const mosaico_asset_id_t material_ids[] = {MOSAICO_ASSET_ID_WALL_CONCRETE,
        MOSAICO_ASSET_ID_WALL_BRICK, MOSAICO_ASSET_ID_WALL_CONTAINER,
        MOSAICO_ASSET_ID_FLOOR_DIRT};
    const MosaicoSpriteFrame *material_frames[4];
    for (int i = 0; i < 4; ++i) material_frames[i] = MosaicoAtlasGetFrame(materials, material_ids[i]);
    raycast_world(game);
    draw_floor(game, materials, material_frames[3] ? material_frames[3] : material_frames[1]);
    draw_walls(game, materials, material_frames);
    draw_extract(game);
    draw_pickups(game, props);
    draw_enemies(game, atlas);
}

static void draw_weapon(const neon_maze_game_t *game, MosaicoAtlas atlas)
{
    const MosaicoSpriteFrame *frame = MosaicoAtlasGetFrame(atlas, MOSAICO_ASSET_ID_K98_RIFLE);
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
        ? 10.0f * (float)game->fire_cooldown / NEON_MAZE_FIRE_COOLDOWN : 0.0f;
    float nearest = 8.0f;
    for (int i = 0; i < NEON_MAZE_ENEMIES; ++i) {
        if (!game->enemies[i].active) continue;
        float dx = game->enemies[i].x - game->x, dy = game->enemies[i].y - game->y;
        float dist = sqrtf(dx * dx + dy * dy);
        if (dist < nearest) nearest = dist;
    }
    float hip = nearest < 1.8f ? (1.8f - nearest) * 56.0f : 0.0f;
    float scale = 0.86f;
    DrawTexturePro(atlas.texture, frame->source,
        (Rectangle){128.0f + bob_x + recoil_x,
                    198.0f + hip + bolt + game->look_pitch * .38f + bob_y + recoil_y,
                    frame->source.width * scale, frame->source.height * scale},
        (Vector2){0, 0}, 0, WHITE);
    if (game->weapon_recoil > .35f)
        DrawCircle(268 + (int)recoil_x, 238 + (int)(game->look_pitch * .38f + recoil_y),
                   8 + (int)(game->weapon_recoil * 10.0f), (Color){255, 226, 96, 255});
    if (game->fire_cooldown > 6) {
        int age = NEON_MAZE_FIRE_COOLDOWN - game->fire_cooldown;
        DrawRectangle(292 + age * 3, 220 - age * 4, 5, 3, (Color){255, 196, 82, 255});
    }
}

static void draw_radar(const neon_maze_game_t *game)
{
    const int radius = 5, scale = 6, left = 88, top = 86;
    const int span = radius * 2 + 1;
    int origin_x = (int)game->x - radius, origin_y = (int)game->y - radius;
    DrawRectangle(left - 4, top - 4, span * scale + 8, span * scale + 8, (Color){3, 9, 20, 255});
    for (int y = 0; y < span; ++y)
        for (int x = 0; x < span; ++x) {
            int mx = origin_x + x, my = origin_y + y;
            if (mx < 0 || my < 0 || mx >= NEON_MAZE_WIDTH || my >= NEON_MAZE_HEIGHT) continue;
            if (!game->explored[my][mx]) continue;
            uint8_t cell = neon_maze_cell(mx, my);
            Color color = (Color){58, 42, 28, 255};
            if (cell == 4 && !game->door_open[my][mx]) color = (Color){220, 180, 60, 255};
            else if (cell == 5) color = (Color){80, 220, 200, 255};
            else if (cell == 3) color = (Color){148, 92, 48, 255};
            else if (cell == 2) color = (Color){96, 140, 168, 255};
            else if (cell >= 1 && cell <= 3) color = (Color){186, 198, 208, 255};
            DrawRectangle(left + x * scale, top + y * scale, scale - 1, scale - 1, color);
        }
    int alive = neon_maze_enemies_alive(game);
    bool extract_open = alive == 0;
    for (int i = 0; i < NEON_MAZE_PICKUPS; ++i) {
        if (game->pickups[i].taken) continue;
        int mx = (int)game->pickups[i].x, my = (int)game->pickups[i].y;
        if (mx < 0 || my < 0 || mx >= NEON_MAZE_WIDTH || my >= NEON_MAZE_HEIGHT) continue;
        if (!game->explored[my][mx]) continue;
        int ox = mx - origin_x, oy = my - origin_y;
        if (ox < 0 || oy < 0 || ox >= span || oy >= span) continue;
        Color color = game->pickups[i].kind == NEON_PICKUP_HEALTH
            ? (Color){255, 82, 96, 255} : (Color){255, 214, 75, 255};
        DrawRectangle(left + ox * scale + 1, top + oy * scale + 1, 3, 3, color);
    }
    for (int i = 0; i < NEON_MAZE_ENEMIES; ++i)
        if (neon_maze_enemy_on_radar(game, i) ||
            (alive == 1 && game->enemies[i].active)) {
            int ex = (int)game->enemies[i].x - origin_x, ey = (int)game->enemies[i].y - origin_y;
            if (ex >= 0 && ey >= 0 && ex < span && ey < span)
                DrawRectangle(left + ex * scale, top + ey * scale, 4, 4, (Color){255, 52, 147, 255});
        }
    if (extract_open) {
        int ex = 22 - origin_x, ey = 22 - origin_y;
        if (ex >= 0 && ey >= 0 && ex < span && ey < span)
            DrawRectangle(left + ex * scale, top + ey * scale, 4, 4, (Color){72, 255, 214, 255});
    }
    int px = left + radius * scale, py = top + radius * scale;
    DrawRectangle(px - 1, py - 1, 3, 3, (Color){255, 235, 91, 255});
    DrawLine(px, py, px + (int)(cosf(game->angle) * 7), py + (int)(sinf(game->angle) * 7),
             (Color){255, 235, 91, 255});
}

static void draw_compass(const neon_maze_game_t *game)
{
    static const char *marks[] = {"E", "SE", "S", "SW", "W", "NW", "N", "NE"};
    const int cx = 240, top = 2, half = 84;
    DrawRectangle(cx - half, top, half * 2, 16, (Color){8, 12, 18, 255});
    DrawRectangle(cx - 1, top, 2, 16, (Color){255, 220, 72, 255});
    for (int i = 0; i < 8; ++i) {
        float relative = (float)i * 0.7853982f - game->angle;
        while (relative > 3.1415927f) relative -= 6.2831853f;
        while (relative < -3.1415927f) relative += 6.2831853f;
        int x = cx + (int)(relative * 52.0f);
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

static unsigned accuracy_pct(const neon_maze_game_t *game)
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

static void draw_status_hud(const neon_maze_game_t *game)
{
    int alive = neon_maze_enemies_alive(game);
    DrawRectangle(150, 20, 180, 44, (Color){8, 12, 18, 255});
    DrawText(format_clock(game->tick), 214, 22, 14, (Color){239, 242, 224, 255});
    for (int i = 0; i < NEON_MAZE_MAX_HP; ++i) {
        Color pip = i < game->hp ? (game->hp > 2 ? (Color){65, 220, 116, 255}
                                                 : (Color){255, 72, 72, 255})
                                 : (Color){36, 40, 44, 255};
        DrawRectangle(158 + i * 14, 40, 12, 8, pip);
    }
    int ammo_shown = game->ammo > 10 ? 10 : (int)game->ammo;
    for (int i = 0; i < 10; ++i) {
        Color tick = i < ammo_shown ? (Color){255, 214, 75, 255} : (Color){36, 40, 44, 255};
        DrawRectangle(292 + i * 3, 40, 2, 8, tick);
    }
    DrawText(TextFormat("%u", (unsigned)game->ammo), 326, 38, 12, (Color){255, 214, 75, 255});
    DrawText(TextFormat("x%d", alive), 158, 50, 10, (Color){255, 92, 140, 255});
    if (game->best_ticks)
        DrawText(TextFormat("BEST %s", format_clock(game->best_ticks)), 318, 6, 10,
                 (Color){72, 255, 214, 255});
}

static void draw_guidance(const neon_maze_game_t *game)
{
    int alive = neon_maze_enemies_alive(game);
    int last = neon_maze_last_enemy_index(game);
    if (alive == 0) {
        float dx = NEON_MAZE_EXTRACT_X - game->x, dy = NEON_MAZE_EXTRACT_Y - game->y;
        unsigned meters = (unsigned)sqrtf(dx * dx + dy * dy);
        draw_bearing_marker(neon_maze_extract_bearing(game), (Color){72, 255, 214, 255},
                            TextFormat("EX %um", meters));
        DrawText("EXTRACT OPEN", 176, 104, 14, (Color){72, 255, 214, 255});
    } else if (last >= 0) {
        float dx = game->enemies[last].x - game->x, dy = game->enemies[last].y - game->y;
        unsigned meters = (unsigned)sqrtf(dx * dx + dy * dy);
        float bearing = atan2f(dy, dx) - game->angle;
        while (bearing > 3.1415927f) bearing -= 6.2831853f;
        while (bearing < -3.1415927f) bearing += 6.2831853f;
        draw_bearing_marker(bearing, (Color){255, 92, 140, 255}, TextFormat("EN %um", meters));
    }
    if (!game->ammo && alive > 1) {
        int nearest = -1;
        float best = 1e9f;
        for (int i = 0; i < NEON_MAZE_PICKUPS; ++i) {
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

static void draw_controls(const neon_maze_game_t *game, MosaicoAtlas controls)
{
    int thumb_x = NEON_MAZE_MOVE_X + (int)(game->move_strafe * 28.0f);
    int thumb_y = NEON_MAZE_MOVE_Y - (int)(game->move_forward * 28.0f);
    const MosaicoSpriteFrame *joystick = MosaicoAtlasGetFrame(
        controls, MOSAICO_ASSET_ID_JOYSTICK_BASE);
    const MosaicoSpriteFrame *fire = MosaicoAtlasGetFrame(
        controls, MOSAICO_ASSET_ID_FIRE_BUTTON);
    if (joystick) DrawTexturePro(controls.texture, joystick->source,
        (Rectangle){42, 352, 80, 80}, (Vector2){0, 0}, 0, WHITE);
    DrawCircle(thumb_x, thumb_y, 9, game->sprinting ? (Color){255, 220, 72, 255}
                                                    : (Color){186, 224, 217, 255});
    if (fire) DrawTexturePro(controls.texture, fire->source,
        (Rectangle){358, 352, 80, 80}, (Vector2){0, 0}, 0, WHITE);
    if (game->fire_held)
        DrawCircle(NEON_MAZE_FIRE_X, NEON_MAZE_FIRE_Y, 16, (Color){255, 220, 72, 255});
    DrawText(game->sprinting ? "SPRINT" : "MOVE", 54, 338, 10,
             game->sprinting ? (Color){255, 220, 72, 255} : (Color){192, 235, 214, 255});
    const char *fire_label = !game->ammo ? "DRY" : (game->fire_cooldown ? "BOLT" : "FIRE");
    Color fire_color = !game->ammo ? (Color){255, 92, 70, 255} : (Color){255, 220, 72, 255};
    DrawText(fire_label, 376, 338, 10, fire_color);
    DrawRectangle(229, 203, 22, 2, (Color){255, 220, 72, 255});
    DrawRectangle(239, 193, 2, 22, (Color){255, 220, 72, 255});
    draw_status_hud(game);
    draw_guidance(game);
    if (game->hit_marker) {
        Color marker = game->kill_flash ? (Color){255, 76, 65, 255} : WHITE;
        DrawLine(226, 191, 234, 199, marker); DrawLine(254, 191, 246, 199, marker);
        DrawLine(226, 219, 234, 211, marker); DrawLine(254, 219, 246, 211, marker);
    }
    if (game->kill_flash)
        DrawText("HOSTILE DOWN", 181, 148, 16, (Color){255, 214, 75, 255});
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
    if (neon_maze_door_ahead(game))
        DrawText("FIRE TO OPEN GATE", 148, 278, 14, (Color){255, 220, 72, 255});
    else if (neon_maze_near_closed_door(game))
        DrawText("FACE GATE, THEN FIRE", 136, 278, 14, (Color){255, 220, 72, 255});
    if (game->hp == 1 && game->phase == NEON_MAZE_PHASE_PLAYING) {
        int pulse = 5 + (int)(sinf((float)game->tick * 0.28f) * 3.0f);
        Color breath = (Color){120, 18, 28, 255};
        DrawRectangle(0, 0, 480, pulse, breath);
        DrawRectangle(0, 480 - pulse, 480, pulse, breath);
        DrawRectangle(0, 0, pulse, 480, breath);
        DrawRectangle(480 - pulse, 0, pulse, 480, breath);
    }
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

static void draw_round_stats(const neon_maze_game_t *game, int y)
{
    char time_buf[8], best_buf[8];
    format_clock_buf(game->tick, time_buf);
    format_clock_buf(game->best_ticks, best_buf);
    DrawText(TextFormat("TIME %s   KILLS %u/%d", time_buf,
                        (unsigned)game->kills, NEON_MAZE_ENEMIES), 118, y, 14,
             (Color){239, 242, 224, 255});
    DrawText(TextFormat("HIT %u%%   DMG %u   HP %u  AMMO %u", accuracy_pct(game),
                        (unsigned)game->damage_taken, (unsigned)game->hp,
                        (unsigned)game->ammo), 110, y + 20, 14,
             (Color){192, 235, 214, 255});
    DrawText(TextFormat("GRADE %c   %s %s", neon_maze_grade(game),
                        game->best_updated ? "NEW BEST" : "BEST",
                        game->best_ticks ? best_buf : "--:--"),
             118, y + 40, 14, (Color){72, 255, 214, 255});
}

static void draw_phase_overlay(const neon_maze_game_t *game)
{
    if (game->phase == NEON_MAZE_PHASE_PLAYING) return;
    DrawRectangle(58, 96, 364, 248, (Color){8, 12, 18, 255});
    DrawRectangle(58, 96, 364, 4, (Color){72, 255, 214, 255});
    if (game->phase == NEON_MAZE_PHASE_START) {
        DrawText("NEON MAZE", 160, 112, 28, (Color){239, 242, 224, 255});
        DrawText("TURN THE CORNER TO CONTACT", 98, 156, 16, (Color){192, 235, 214, 255});
        DrawText("TAP FIRE TO SHOOT / OPEN GATE", 82, 178, 16, (Color){192, 235, 214, 255});
        DrawText("SIDE CACHE IS OPTIONAL", 118, 200, 16, (Color){192, 235, 214, 255});
        DrawText("CLEAR ALL, THEN FOLLOW EXTRACT", 86, 222, 14, (Color){255, 220, 72, 255});
        char best_buf[8];
        format_clock_buf(game->best_ticks, best_buf);
        DrawText(TextFormat("SHIFT %u/%u  BEST %s", (unsigned)game->layout + 1U,
                            (unsigned)NEON_MAZE_LAYOUTS,
                            game->best_ticks ? best_buf : "--:--"),
                 118, 250, 16, (Color){72, 255, 214, 255});
        DrawText("TAP TO DEPLOY", 168, 286, 16, (Color){255, 220, 72, 255});
    } else if (game->phase == NEON_MAZE_PHASE_WON) {
        DrawText("EXTRACT SECURE", 128, 112, 24, (Color){72, 255, 214, 255});
        draw_round_stats(game, 160);
        DrawText("TAP TO REDEPLOY", 152, 292, 16, (Color){255, 220, 72, 255});
    } else {
        DrawText("DOWNED", 188, 112, 28, (Color){255, 92, 70, 255});
        draw_round_stats(game, 160);
        DrawText("TAP TO REDEPLOY", 152, 292, 16, (Color){255, 220, 72, 255});
    }
}

void neon_maze_view_render(const neon_maze_game_t *game, MosaicoAtlas enemies,
                           MosaicoAtlas weapon, MosaicoAtlas environment,
                           MosaicoAtlas materials, MosaicoAtlas controls,
                           MosaicoAtlas props)
{
    if (!game) return;
    BeginDrawing();
    draw_panorama(game, environment);
    draw_world(game, enemies, materials, props);
    draw_radar(game);
    draw_compass(game);
    draw_weapon(game, weapon);
    draw_controls(game, controls);
    draw_phase_overlay(game);
    EndDrawing();
}
