// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float yaw, pitch, last_x, last_y;
    uint32_t tick;
    uint16_t idle_ticks;
    uint8_t scene;
    bool dragging,ui_touch;
} underwater_world_t;

enum { UNDERWATER_SCENE_AURORA=0, UNDERWATER_SCENE_OCEAN=1, UNDERWATER_SCENE_SUNRISE=2 };

void underwater_world_reset(underwater_world_t *world);
void underwater_world_pointer(underwater_world_t *world,float x,float y,bool pressed);
void underwater_world_update(underwater_world_t *world);
uint32_t underwater_world_hash(const underwater_world_t *world);
