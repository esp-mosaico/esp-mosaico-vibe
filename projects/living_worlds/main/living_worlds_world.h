// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "living_worlds_aurora.h"
#include "living_worlds_ocean.h"

#define LIVING_SUNRISE_SEED_CAP 12
#define LIVING_SUNRISE_SEED_AMBIENT 10

typedef struct {
    float x,y,z;
    float vx,vy,vz;
    float radius,rx,ry,rz,phase;
    uint16_t age,life;
    uint8_t active;
} living_sunrise_seed_t;

typedef struct {
    float yaw, pitch, last_x, last_y, press_x, press_y;
    float yaw_velocity, pitch_velocity;
    uint32_t tick;
    uint32_t sunrise_rng;
    living_sunrise_seed_t sunrise_seeds[LIVING_SUNRISE_SEED_CAP];
    living_aurora_t aurora;
    living_ocean_t ocean;
    uint16_t sunrise_next_gust,sunrise_gust_ticks;
    uint8_t sunrise_seed_count;
    uint16_t idle_ticks;
    uint8_t scene,effects_level;
    bool dragging,ui_touch;
} living_world_t;

enum { LIVING_SCENE_AURORA=0, LIVING_SCENE_OCEAN=1,
       LIVING_SCENE_SUNRISE=2, LIVING_SCENE_RAINFOREST=3 };

void living_world_reset(living_world_t *world);
void living_world_pointer(living_world_t *world,float x,float y,bool pressed);
void living_world_update(living_world_t *world);
uint32_t living_world_hash(const living_world_t *world);
