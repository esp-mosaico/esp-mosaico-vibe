// SPDX-License-Identifier: Apache-2.0
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "host_asset_runtime.h"
#include "mosaico_game_2d.h"
#include "mosaico_raylib_fast.h"
#include "mosaico_raylib_port.h"

static uint16_t framebuffer[480 * 480];

int main(int argc, char **argv)
{
    assert(argc == 2);
    mosaico_host_assets_set_root(argv[1]);
    mosaico_host_raylib_set_target(framebuffer, 480, 480, 480);
    InitWindow(480, 480, "test");
    SetTargetFPS(60);
    assert(IsWindowReady());
    assert(GetScreenWidth() == 480 && GetScreenHeight() == 480);
    MosaicoFastInjectAction(0, true);
    assert(IsKeyPressed(KEY_LEFT) && IsKeyDown(KEY_A));
    MosaicoFastInjectPointer(7, 123, 234, true);
    assert(GetTouchPointCount() == 1 && GetTouchPointId(0) == 7);
    assert(GetTouchX() == 123 && GetTouchY() == 234);
    MosaicoFastConsumeInputEdges();
    assert(!IsKeyPressed(KEY_LEFT) && IsKeyDown(KEY_A));
    assert(GetTouchPointCount() == 1 && GetTouchPointId(0) == 7);

    MosaicoFastInjectImu(0.25f, -0.5f, 1.0f);
    Vector3 imu = MosaicoFastGetImuAcceleration();
    assert(imu.x == 0.25f && imu.y == -0.5f && imu.z == 1.0f);

    BeginDrawing();
    assert(MosaicoFastFrameAvailable());
    ClearBackground(BLACK);
    BeginScissorMode(10, 10, 20, 20);
    DrawRectangle(0, 0, 40, 40, RED);
    DrawLineEx((Vector2){10, 15}, (Vector2){29, 15}, 3, WHITE);
    DrawEllipse(20, 20, 5, 8, BLUE);
    EndScissorMode();
    DrawRectangleGradientH(40, 40, 20, 10, GREEN, BLUE);
    DrawPoly((Vector2){80, 80}, 6, 12, 0, YELLOW);
    DrawText("HI", 100, 100, 16, WHITE);
    EndDrawing();

    assert(framebuffer[5 * 480 + 5] == 0);
    assert(framebuffer[12 * 480 + 12] != 0);
    assert(framebuffer[45 * 480 + 45] != 0);
    assert(framebuffer[100 * 480 + 100] != 0);
    assert(framebuffer[106 * 480 + 108] != 0);
    assert(fabs(GetFrameTime() - (1.0f / 60.0f)) < 0.0001f);
    assert(fabs(GetTime() - (1.0 / 60.0)) < 0.0001);

    MosaicoAtlas scale_atlas = LoadMosaicoAtlas("scale.atlas");
    Texture2D scale_texture = scale_atlas.texture;
    assert(scale_texture.id != 0);
    mosaico_game_2d_reset_raster_stats();
    assert(MosaicoAtlasGetFrame(scale_atlas, 0x12345678));
    assert(MosaicoAtlasGetFrame(scale_atlas, 0x12345678));
    mosaico_game_2d_raster_stats_t lookup_stats = {0};
    mosaico_game_2d_get_raster_stats(&lookup_stats);
    assert(lookup_stats.frame_lookup_misses == 1);
    assert(lookup_stats.frame_lookup_hits == 1);
    mosaico_game_2d_reset_raster_stats();
    BeginDrawing();
    ClearBackground(BLACK);
    DrawTexturePro(scale_texture, (Rectangle){0, 0, 4, 3},
                   (Rectangle){20, 20, 7, 5}, (Vector2){0, 0}, 0, WHITE);
    EndDrawing();
    for (int y = 0; y < 5; ++y) {
        for (int x = 0; x < 7; ++x) {
            uint16_t expected = (uint16_t)(1 + (y * 3 / 5) * 4 + x * 4 / 7);
            assert(framebuffer[(20 + y) * 480 + 20 + x] == expected);
        }
    }
    mosaico_game_2d_raster_stats_t raster = {0};
    mosaico_game_2d_get_raster_stats(&raster);
    assert(raster.opaque_scale_calls == 1);
    assert(raster.opaque_scale_pixels == 35);

    mosaico_game_2d_reset_raster_stats();
    BeginDrawing();
    ClearBackground(BLACK);
    Mosaico2DDrawColumn(scale_texture, (Rectangle){0, 0, 1, 3},
                        20, 20, 4, 6, 256);
    uint16_t bottoms[2] = {0, 100};
    Mosaico2DDrawFloorRow(scale_texture, (Rectangle){0, 0, 4, 3},
                          30, 40, 4, 2, bottoms, 0, 0, 65536, 0, 256);
    EndDrawing();
    assert(framebuffer[20 * 480 + 20] == 1);
    assert(framebuffer[20 * 480 + 23] == 1);
    assert(framebuffer[22 * 480 + 20] == 5);
    assert(framebuffer[24 * 480 + 20] == 9);
    assert(framebuffer[30 * 480 + 40] == 1);
    assert(framebuffer[30 * 480 + 44] == 0);
    mosaico_game_2d_get_raster_stats(&raster);
    assert(raster.column_calls == 1);
    assert(raster.column_pixels == 24);
    assert(raster.span_calls == 1);
    assert(raster.span_pixels == 4);

    mosaico_game_2d_reset_raster_stats();
    BeginDrawing();
    ClearBackground(BLACK);
    mosaico_raycast_wall_t wall = {20, 20, 4, 6, 0, 0, 1, 3, 256};
    Mosaico2DDrawRaycastWalls(scale_texture, &wall, 1);
    Mosaico2DDrawFloorRows(scale_texture, (Rectangle){0, 0, 4, 3},
                           30, 40, 4, 2, bottoms, 0, 0, 65536, 0, 256, 2);
    Mosaico2DCopyScanline(30, 32);
    EndDrawing();
    assert(framebuffer[20 * 480 + 20] == 1);
    assert(framebuffer[20 * 480 + 23] == 1);
    assert(framebuffer[22 * 480 + 20] == 5);
    assert(framebuffer[24 * 480 + 20] == 9);
    assert(framebuffer[30 * 480 + 40] == 1);
    assert(framebuffer[31 * 480 + 40] == 1);
    assert(framebuffer[30 * 480 + 44] == 0);
    assert(framebuffer[32 * 480 + 40] == 1);
    mosaico_game_2d_get_raster_stats(&raster);
    assert(raster.column_calls == 1);
    assert(raster.column_pixels == 24);
    assert(raster.span_pixels == 8);

    {
        static const mosaico_raycast_wall_t cases[] = {
            {16, 8, 4, 48, 0, 0, 1, 3, 256},
            {40, -12, 4, 80, 1, 0, 2, 3, 192},
            {80, 200, 4, 220, 0, 1, 1, 2, 160},
            {200, 30, 8, 17, 2, 0, 2, 3, 256},
            {300, 100, 4, 6, 0, 0, 1, 3, 128},
        };
        const int case_count = (int)(sizeof(cases) / sizeof(cases[0]));
        uint16_t *expected = malloc(480 * 480 * sizeof(uint16_t));
        assert(expected);
        BeginDrawing();
        ClearBackground((Color){17, 34, 51, 255});
        for (int i = 0; i < case_count; ++i) {
            const mosaico_raycast_wall_t *c = &cases[i];
            Mosaico2DDrawColumn(scale_texture,
                (Rectangle){(float)c->src_x, (float)c->src_y,
                            (float)c->src_width, (float)c->src_height},
                c->dest_x, c->dest_y, c->dest_width, c->dest_height, c->light256);
        }
        EndDrawing();
        memcpy(expected, framebuffer, 480 * 480 * sizeof(uint16_t));
        BeginDrawing();
        ClearBackground((Color){17, 34, 51, 255});
        Mosaico2DDrawRaycastWalls(scale_texture, cases, case_count);
        EndDrawing();
        assert(memcmp(framebuffer, expected, 480 * 480 * sizeof(uint16_t)) == 0);
        free(expected);
    }

    mosaico_game_2d_reset_raster_stats();
    BeginDrawing();
    DrawTexture(scale_texture, 30, 30, WHITE);
    EndDrawing();
    mosaico_game_2d_get_raster_stats(&raster);
    assert(raster.opaque_copy_calls == 1);
    assert(raster.opaque_copy_pixels == 12);
    UnloadTexture(scale_texture);

    Texture2D mask_texture = Mosaico2DLoadTexture("mask.atlas");
    assert(mask_texture.id != 0);
    mosaico_game_2d_reset_raster_stats();
    BeginDrawing();
    ClearBackground(BLACK);
    DrawTexturePro(mask_texture, (Rectangle){0, 0, 4, 3},
                   (Rectangle){40, 40, 7, 5}, (Vector2){0, 0}, 0, WHITE);
    EndDrawing();
    for (int y = 0; y < 5; ++y) {
        for (int x = 0; x < 7; ++x) {
            int source_x = x * 4 / 7;
            int source_y = y * 3 / 5;
            static const uint8_t alpha[12] = {
                255, 0, 255, 0, 0, 255, 0, 255, 255, 255, 0, 0};
            uint16_t expected = alpha[source_y * 4 + source_x]
                ? (uint16_t)(1 + source_y * 4 + source_x) : 0;
            assert(framebuffer[(40 + y) * 480 + 40 + x] == expected);
        }
    }
    mosaico_game_2d_get_raster_stats(&raster);
    assert(raster.binary_alpha_calls == 1);
    assert(raster.binary_scale_calls == 1 && raster.binary_copy_calls == 0);
    assert(raster.binary_alpha_pixels > 0 && raster.alpha_calls == 0);

    mosaico_game_2d_reset_raster_stats();
    BeginDrawing();
    ClearBackground((Color){255, 0, 255, 255});
    DrawTexturePro(mask_texture, (Rectangle){0, 0, 4, 3},
                   (Rectangle){400, 400, 80, 80}, (Vector2){0, 0}, 0, WHITE);
    EndDrawing();
    assert(framebuffer[479 * 480 + 479] == framebuffer[0]);

    mosaico_game_2d_reset_raster_stats();
    BeginDrawing();
    ClearBackground(BLACK);
    DrawTexture(mask_texture, 60, 60, WHITE);
    EndDrawing();
    mosaico_game_2d_get_raster_stats(&raster);
    assert(raster.binary_alpha_calls == 1);
    assert(raster.binary_copy_calls == 1 && raster.binary_scale_calls == 0);
    assert(raster.binary_alpha_pixels == 6);
    UnloadTexture(mask_texture);

    Texture2D smooth_texture = Mosaico2DLoadTexture("smooth.atlas");
    assert(smooth_texture.id != 0);
    mosaico_game_2d_reset_raster_stats();
    BeginDrawing();
    ClearBackground(BLACK);
    DrawTexturePro(smooth_texture, (Rectangle){0, 0, 4, 3},
                   (Rectangle){80, 80, 7, 5}, (Vector2){0, 0}, 0, WHITE);
    EndDrawing();
    mosaico_game_2d_get_raster_stats(&raster);
    assert(raster.alpha_calls == 1);
    assert(raster.alpha_pixels == 35);
    assert(framebuffer[80 * 480 + 80] == 1);
    assert(framebuffer[80 * 480 + 83] != 2);
    assert(framebuffer[80 * 480 + 85] == 0);
    UnloadTexture(smooth_texture);

    assert(CheckCollisionCircleRec((Vector2){10, 10}, 5,
                                   (Rectangle){12, 12, 5, 5}));
    assert(CheckCollisionPointTriangle((Vector2){2, 2}, (Vector2){0, 0},
                                       (Vector2){10, 0}, (Vector2){0, 10}));
    Rectangle overlap = GetCollisionRec((Rectangle){0, 0, 10, 10},
                                        (Rectangle){5, 5, 10, 10});
    assert(overlap.x == 5 && overlap.y == 5 && overlap.width == 5 && overlap.height == 5);
    assert(Fade(RED, 0.5f).a == 128);
    assert(!IsKeyPressed(KEY_LEFT) && IsKeyDown(KEY_LEFT));
    MosaicoFastInjectAction(0, false);
    MosaicoFastInjectPointer(7, 123, 234, false);
    assert(IsKeyReleased(KEY_LEFT) && GetTouchPointCount() == 0);
    assert(IsMouseButtonReleased(MOUSE_BUTTON_LEFT));
    CloseWindow();
    assert(WindowShouldClose() && !IsWindowReady());
    return 0;
}
