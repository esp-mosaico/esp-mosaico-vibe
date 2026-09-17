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
    world->effects_level=1;
}

void underwater_world_pointer(underwater_world_t *world,float x,float y,bool pressed)
{
    if(!world)return;
    if(pressed&&!world->dragging&&y<=60.0f&&x<=310.0f){
        world->effects_level=(uint8_t)((world->effects_level+1U)%3U);
        world->ui_touch=true;world->dragging=false;
        world->last_x=x;world->last_y=y;world->idle_ticks=0;return;
    }
    if(pressed&&!world->dragging&&y>=420.0f){
        if(x>=22.0f&&x<132.0f)world->scene=UNDERWATER_SCENE_AURORA;
        else if(x>=132.0f&&x<242.0f)world->scene=UNDERWATER_SCENE_OCEAN;
        else if(x>=242.0f&&x<352.0f)world->scene=UNDERWATER_SCENE_SUNRISE;
        else if(x>=352.0f&&x<=462.0f)world->scene=UNDERWATER_SCENE_RAINFOREST;
        world->ui_touch=true;world->dragging=false;
        world->last_x=x;world->last_y=y;world->idle_ticks=0;return;
    }
    if(!pressed&&world->ui_touch){world->ui_touch=false;world->dragging=false;return;}
    if(pressed&&world->dragging&&!world->ui_touch){
        float dx=x-world->last_x,dy=y-world->last_y;
        world->yaw_velocity=-dx*0.38f;
        world->pitch_velocity=dy*0.20f;
        world->yaw=wrap_degrees(world->yaw-dx*0.38f);
        world->pitch+=dy*0.20f;
        if(world->yaw_velocity>6.0f)world->yaw_velocity=6.0f;
        if(world->yaw_velocity<-6.0f)world->yaw_velocity=-6.0f;
        if(world->pitch_velocity>3.0f)world->pitch_velocity=3.0f;
        if(world->pitch_velocity<-3.0f)world->pitch_velocity=-3.0f;
        if(world->pitch < -28.0f)world->pitch=-28.0f;
        if(world->pitch > 24.0f)world->pitch=24.0f;
    }
    world->dragging=pressed&&!world->ui_touch;world->last_x=x;world->last_y=y;world->idle_ticks=0;
}

void underwater_world_update(underwater_world_t *world)
{
    if(!world)return;
    ++world->tick;
    if(!world->dragging){
        ++world->idle_ticks;
        world->yaw=wrap_degrees(world->yaw+world->yaw_velocity);
        world->pitch+=world->pitch_velocity;
        world->yaw_velocity*=0.86f;world->pitch_velocity*=0.78f;
        if(world->yaw_velocity<.01f&&world->yaw_velocity>-.01f)world->yaw_velocity=0;
        if(world->pitch_velocity<.01f&&world->pitch_velocity>-.01f)world->pitch_velocity=0;
        if(world->pitch < -28.0f)world->pitch=-28.0f;
        if(world->pitch > 24.0f)world->pitch=24.0f;
        if(world->idle_ticks>90)world->yaw=wrap_degrees(world->yaw+0.08f);
    }else world->yaw=wrap_degrees(world->yaw);
}

uint32_t underwater_world_hash(const underwater_world_t *world)
{
    const uint8_t *bytes=(const uint8_t *)world;uint32_t hash=2166136261U;
    for(size_t i=0;i<sizeof(*world);++i)hash=(hash^bytes[i])*16777619U;
    return hash;
}
