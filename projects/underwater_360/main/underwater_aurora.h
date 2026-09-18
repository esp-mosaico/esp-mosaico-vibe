// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdint.h>

#define AURORA_YAW_LIMIT 11.4592f
#define AURORA_PITCH_LIMIT 6.8755f
#define AURORA_FOCUS 13.0f
#define AURORA_MOTE_CAP 36
#define AURORA_METEOR_CAP 12
#define AURORA_RIPPLE_CAP 4
#define AURORA_GLINT_CAP 20
#define AURORA_MARSH_CAP 16
#define AURORA_STREAM_CAP 6

typedef struct {
    float x,y,z,phase,size,vy;
} aurora_mote_t;

typedef struct {
    float x,y,z,vx,vy,age,life;
    uint8_t tail;
} aurora_meteor_t;

typedef struct {
    float x,y,z,age;
} aurora_ripple_t;

typedef struct {
    float x,y,z,phase,size,drift;
} aurora_glint_t;

typedef struct {
    float lane,phase,depth,amp,alpha;
} aurora_stream_t;

typedef struct {
    uint32_t rng,tick;
    float pulse,flow_x,flow_y;
    aurora_mote_t motes[AURORA_MOTE_CAP];
    aurora_meteor_t meteors[AURORA_METEOR_CAP];
    aurora_ripple_t ripples[AURORA_RIPPLE_CAP];
    aurora_glint_t glints[AURORA_GLINT_CAP];
    aurora_glint_t marsh[AURORA_MARSH_CAP];
    aurora_stream_t streams[AURORA_STREAM_CAP];
    uint16_t next_meteor,next_shower;
    uint8_t mote_count,meteor_count,ripple_count,glint_count,marsh_count,stream_count;
} underwater_aurora_t;

void underwater_aurora_reset(underwater_aurora_t *aurora);
void underwater_aurora_look(float *yaw,float *pitch,float *yaw_velocity,
                            float *pitch_velocity,float dx,float dy);
void underwater_aurora_tap(underwater_aurora_t *aurora,float x,float y);
void underwater_aurora_update(underwater_aurora_t *aurora);
