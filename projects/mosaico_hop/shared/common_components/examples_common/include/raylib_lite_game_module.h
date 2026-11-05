// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RAYLIB_LITE_GAME_MODULE_ABI_V1 1U

typedef enum {
    RAYLIB_LITE_GAME_INPUT_ACTION = 1,
    RAYLIB_LITE_GAME_INPUT_POINTER = 2,
    RAYLIB_LITE_GAME_INPUT_CONTROL = 3,
    RAYLIB_LITE_GAME_INPUT_IMU = 4,
} raylib_lite_game_input_type_t;

typedef enum {
    RAYLIB_LITE_GAME_CONTROL_PAUSE = 1,
    RAYLIB_LITE_GAME_CONTROL_RESUME,
    RAYLIB_LITE_GAME_CONTROL_RESET,
} raylib_lite_game_control_t;

typedef struct {
    uint32_t type;
    int32_t code;
    int32_t x;
    int32_t y;
    int32_t track_id;
    bool pressed;
    float value_x;
    float value_y;
    float value_z;
} raylib_lite_game_input_v1_t;

typedef struct {
    uint32_t abi_version;
    const char *game_id;
    const char *title;
    uint16_t width;
    uint16_t height;
    uint16_t tick_hz;
    uint16_t max_pointers;
} raylib_lite_game_descriptor_v1_t;

/* Shared example Host/native module contract. Product ELF builds translate
 * this vocabulary in raylib_lite_game_module_contract.h. */
typedef struct {
    raylib_lite_game_descriptor_v1_t descriptor;
    size_t state_size;
    int (*initialize)(void *state, const char *asset_root);
    void (*shutdown)(void *state);
    void (*input)(void *state, const raylib_lite_game_input_v1_t *input);
    void (*update)(void *state);
    int (*render)(void *state);
    uint32_t (*state_hash)(const void *state);
    int (*state_json)(const void *state, char *output, size_t capacity);
} raylib_lite_game_module_v1_t;

const raylib_lite_game_module_v1_t *raylib_lite_game_module_v1(void);
