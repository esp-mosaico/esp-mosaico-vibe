// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "underwater_aurora.h"
#include "underwater_ocean.h"

#define UNDERWATER_SUNRISE_SEED_CAP 12
#define UNDERWATER_SUNRISE_SEED_AMBIENT 10

typedef struct {
    float x,y,z;
    float vx,vy,vz;
    float radius,rx,ry,rz,phase;
    uint16_t age,life;
    uint8_t active;
} underwater_sunrise_seed_t;

typedef struct {
    float yaw, pitch, last_x, last_y, press_x, press_y;
    float yaw_velocity, pitch_velocity;
    uint32_t tick;
    uint32_t sunrise_rng;
    underwater_sunrise_seed_t sunrise_seeds[UNDERWATER_SUNRISE_SEED_CAP];
    underwater_aurora_t aurora;
    underwater_ocean_t ocean;
    uint16_t sunrise_next_gust,sunrise_gust_ticks;
    uint8_t sunrise_seed_count;
    uint16_t idle_ticks;
    uint8_t scene,effects_level;
    bool dragging,ui_touch;
} underwater_world_t;

enum { UNDERWATER_SCENE_AURORA=0, UNDERWATER_SCENE_OCEAN=1,
       UNDERWATER_SCENE_SUNRISE=2, UNDERWATER_SCENE_RAINFOREST=3 };

void underwater_world_reset(underwater_world_t *world);
void underwater_world_pointer(underwater_world_t *world,float x,float y,bool pressed);
void underwater_world_update(underwater_world_t *world);
uint32_t underwater_world_hash(const underwater_world_t *world);
