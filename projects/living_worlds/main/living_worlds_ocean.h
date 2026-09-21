// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define OCEAN_YAW_LIMIT 17.1887f
#define OCEAN_PITCH_LIMIT 8.8808f
#define OCEAN_FOCUS 9.0f
#define OCEAN_MOTE_CAP 36
#define OCEAN_JELLY_CAP 6
#define OCEAN_SHOAL_CAP 6
#define OCEAN_WANDERER_CAP 6

enum { OCEAN_WANDER_RAY=0, OCEAN_WANDER_TURTLE=1,
       OCEAN_WANDER_SEAHORSE=2, OCEAN_WANDER_WHALE=3 };

typedef struct {
    float x,y,z,phase,size,vy;
    uint8_t bubble;
} ocean_mote_t;

typedef struct {
    float x,y,z,home_x,home_y,home_z;
    float vx,vy,phase,freq,flash,roll,yaw,pulse,radius;
    uint8_t id;
} ocean_jelly_t;

typedef struct {
    float x,y,z;
    float phase,spread,speed,size,sway;
    int8_t dir,count;
    uint8_t r,g,b;
} ocean_shoal_t;

typedef struct {
    float x,y,z,size,speed,phase,alpha,drift;
    int8_t dir;
    uint8_t kind;
} ocean_wanderer_t;

typedef struct {
    uint32_t rng,tick;
    float flow_x,flow_y;
    ocean_mote_t motes[OCEAN_MOTE_CAP];
    ocean_jelly_t jellies[OCEAN_JELLY_CAP];
    ocean_shoal_t shoals[OCEAN_SHOAL_CAP];
    ocean_wanderer_t wanderers[OCEAN_WANDERER_CAP];
    uint8_t mote_count,jelly_count,shoal_count,wanderer_count;
} living_ocean_t;

void living_ocean_reset(living_ocean_t *ocean);
void living_ocean_look(float *yaw,float *pitch,float *yaw_velocity,
                           float *pitch_velocity,float dx,float dy);
void living_ocean_tap(living_ocean_t *ocean,float x,float y);
void living_ocean_update(living_ocean_t *ocean);
