// SPDX-License-Identifier: Apache-2.0
#include "underwater_world.h"
#include <stddef.h>
#include <string.h>

static float wrap_degrees(float value)
{
    while(value<0)value+=360.0f;
    while(value>=360.0f)value-=360.0f;
    return value;
}

void underwater_world_reset(underwater_world_t *world)
{
    memset(world,0,sizeof(*world));
    world->yaw=18.0f;world->pitch=-3.0f;world->scene=UNDERWATER_SCENE_OCEAN;
}

void underwater_world_pointer(underwater_world_t *world,float x,float y,bool pressed)
{
    if(!world)return;
    if(pressed&&!world->dragging&&y>=420.0f){
        if(x>=92.0f&&x<186.0f)world->scene=UNDERWATER_SCENE_AURORA;
        else if(x>=186.0f&&x<280.0f)world->scene=UNDERWATER_SCENE_OCEAN;
        else if(x>=280.0f&&x<=374.0f)world->scene=UNDERWATER_SCENE_SUNRISE;
        world->ui_touch=true;world->dragging=false;
        world->last_x=x;world->last_y=y;world->idle_ticks=0;return;
    }
    if(!pressed&&world->ui_touch){world->ui_touch=false;world->dragging=false;return;}
    if(pressed&&world->dragging&&!world->ui_touch){
        world->yaw=wrap_degrees(world->yaw-(x-world->last_x)*0.38f);
        world->pitch+=(y-world->last_y)*0.20f;
        if(world->pitch < -28.0f)world->pitch=-28.0f;
        if(world->pitch > 24.0f)world->pitch=24.0f;
    }
    world->dragging=pressed&&!world->ui_touch;world->last_x=x;world->last_y=y;world->idle_ticks=0;
}

void underwater_world_update(underwater_world_t *world)
{
    if(!world)return;
    ++world->tick;
    if(!world->dragging&&++world->idle_ticks>90)
        world->yaw=wrap_degrees(world->yaw+0.08f);
    else
        world->yaw=wrap_degrees(world->yaw);
}

uint32_t underwater_world_hash(const underwater_world_t *world)
{
    const uint8_t *bytes=(const uint8_t *)world;uint32_t hash=2166136261U;
    for(size_t i=0;i<sizeof(*world);++i)hash=(hash^bytes[i])*16777619U;
    return hash;
}
