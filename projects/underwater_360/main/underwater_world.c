// SPDX-License-Identifier: Apache-2.0
#include "underwater_world.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

#define SUNRISE_YAW_LIMIT 14.3239f
#define SUNRISE_PITCH_LIMIT 7.735f

static float sunrise_abs(float value){return value<0?-value:value;}

static float sunrise_sqrt(float value)
{
    if(value<=0)return 0;
    float root=value>1.0f?value:1.0f;
    for(int i=0;i<6;++i)root=.5f*(root+value/root);
    return root;
}

static float sunrise_sin(float angle)
{
    const float pi=3.14159265f,two_pi=6.2831853f;
    while(angle>pi)angle-=two_pi;
    while(angle<-pi)angle+=two_pi;
    float value=1.27323954f*angle-.405284735f*angle*sunrise_abs(angle);
    return .225f*(value*sunrise_abs(value)-value)+value;
}

static uint32_t sunrise_random(underwater_world_t *world)
{
    world->sunrise_rng=world->sunrise_rng*1664525U+1013904223U;
    return world->sunrise_rng;
}

static float sunrise_random_unit(underwater_world_t *world)
{
    return (float)(sunrise_random(world)>>8)*(1.0f/16777216.0f);
}

static float sunrise_random_range(underwater_world_t *world,float low,float high)
{
    return low+(high-low)*sunrise_random_unit(world);
}

static void sunrise_spawn_seed(underwater_world_t *world,
                               underwater_sunrise_seed_t *seed,bool gust)
{
    const float focal=480.0f*1.055f;
    float screen_x=gust?sunrise_random_range(world,-26.0f,145.0f):
                        sunrise_random_range(world,8.0f,472.0f);
    float screen_y=gust?sunrise_random_range(world,205.0f,445.0f):
                        sunrise_random_range(world,105.0f,450.0f);
    float z=gust?sunrise_random_range(world,3.7f,15.0f):
                 sunrise_random_range(world,3.3f,20.0f);
    memset(seed,0,sizeof(*seed));
    seed->x=(screen_x-240.0f)*z/focal;
    seed->y=(240.0f-screen_y)*z/focal;
    seed->z=z;
    seed->vx=sunrise_random_range(world,.055f,.18f);
    seed->vy=sunrise_random_range(world,.006f,.06f);
    seed->vz=sunrise_random_range(world,-.025f,.025f);
    seed->radius=sunrise_random_range(world,.10f,.19f);
    seed->rx=sunrise_random_range(world,-.70f,.70f);
    seed->ry=sunrise_random_range(world,0.0f,6.2831853f);
    seed->rz=sunrise_random_range(world,-.55f,.55f);
    seed->phase=sunrise_random_range(world,0.0f,6.2831853f);
    seed->life=(uint16_t)(820U+(sunrise_random(world)%460U));
    seed->age=gust?0U:(uint16_t)(sunrise_random(world)%(seed->life/2U));
    seed->active=1;
}

static void sunrise_particles_reset(underwater_world_t *world)
{
    memset(world->sunrise_seeds,0,sizeof(world->sunrise_seeds));
    world->sunrise_rng=0x51f15e31U;
    world->sunrise_seed_count=UNDERWATER_SUNRISE_SEED_AMBIENT;
    for(unsigned i=0;i<world->sunrise_seed_count;++i)
        sunrise_spawn_seed(world,&world->sunrise_seeds[i],false);
    world->sunrise_next_gust=(uint16_t)(114U+sunrise_random(world)%61U);
    world->sunrise_gust_ticks=0;
}

static void clamp_sunrise_cone(underwater_world_t *world)
{
    float nx=world->yaw/SUNRISE_YAW_LIMIT;
    float ny=world->pitch/SUNRISE_PITCH_LIMIT;
    float length=sunrise_sqrt(nx*nx+ny*ny);
    if(length<=1.0f)return;
    world->yaw/=length;
    world->pitch/=length;
    world->yaw_velocity*=.25f;
    world->pitch_velocity*=.25f;
}

static void sunrise_particles_update(underwater_world_t *world)
{
    if(world->sunrise_next_gust>0)--world->sunrise_next_gust;
    if(world->sunrise_next_gust==0){
        unsigned add=3;
        if(add>world->sunrise_seed_count)add=world->sunrise_seed_count;
        for(unsigned i=0;i<add;++i)
            sunrise_spawn_seed(world,&world->sunrise_seeds[i],true);
        world->sunrise_gust_ticks=200;
        world->sunrise_next_gust=(uint16_t)(114U+sunrise_random(world)%61U);
    }
    if(world->sunrise_gust_ticks>0)--world->sunrise_gust_ticks;
    float gust=(float)world->sunrise_gust_ticks/200.0f;
    for(unsigned i=0;i<world->sunrise_seed_count;++i){
        underwater_sunrise_seed_t *seed=&world->sunrise_seeds[i];
        float t=(float)seed->age/30.0f;
        float target_vx=.13f+gust*.38f;
        seed->vx+=(target_vx-seed->vx)*.018f;
        seed->vy+=sunrise_sin(t*.71f+seed->phase)*.00045f;
        seed->x+=(seed->vx+sunrise_sin(t*.83f+seed->phase)*.045f)/30.0f;
        seed->y+=(seed->vy+sunrise_sin(t*1.17f+seed->phase*.73f)*.035f)/30.0f;
        seed->z+=(seed->vz+sunrise_sin(t*.49f+seed->phase)*.012f)/30.0f;
        seed->ry+=.0063f;
        seed->rz+=sunrise_sin(t*.76f+seed->phase)*.0018f;
        ++seed->age;
        if(seed->age>=seed->life||seed->x>9.5f||seed->y>4.5f||seed->z<2.2f)
            sunrise_spawn_seed(world,seed,true);
    }
}

static float wrap_degrees(float value)
{
    while(value<0)value+=360.0f;
    while(value>=360.0f)value-=360.0f;
    return value;
}

static void enter_scene(underwater_world_t *world,uint8_t scene)
{
    world->scene=scene;
    world->yaw_velocity=0;world->pitch_velocity=0;
    if(scene==UNDERWATER_SCENE_SUNRISE){
        world->yaw=0;world->pitch=0;
        sunrise_particles_reset(world);
    }else if(scene==UNDERWATER_SCENE_AURORA){
        world->yaw=0;world->pitch=0;
        underwater_aurora_reset(&world->aurora);
    }else if(scene==UNDERWATER_SCENE_OCEAN){
        world->yaw=0;world->pitch=0;
        underwater_ocean_reset(&world->ocean);
    }else if(scene==UNDERWATER_SCENE_RAINFOREST){
        /* Left-edge yaw that frames the photographed creek, not the tree. */
        world->yaw=140.0f;world->pitch=0;
    }
}

void underwater_world_reset(underwater_world_t *world)
{
    memset(world,0,sizeof(*world));
    world->effects_level=1;
    enter_scene(world,UNDERWATER_SCENE_OCEAN);
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
        uint8_t next=world->scene;
        if(x>=22.0f&&x<132.0f)next=UNDERWATER_SCENE_AURORA;
        else if(x>=132.0f&&x<242.0f)next=UNDERWATER_SCENE_OCEAN;
        else if(x>=242.0f&&x<352.0f)next=UNDERWATER_SCENE_SUNRISE;
        else if(x>=352.0f&&x<=462.0f)next=UNDERWATER_SCENE_RAINFOREST;
        if(next!=world->scene)enter_scene(world,next);
        world->ui_touch=true;world->dragging=false;
        world->last_x=x;world->last_y=y;world->idle_ticks=0;return;
    }
    if(!pressed&&world->ui_touch){world->ui_touch=false;world->dragging=false;return;}
    if(!pressed&&world->dragging&&!world->ui_touch){
        /* Device release events may arrive as (0,0). Tap at the last contact. */
        float tap_x=world->last_x,tap_y=world->last_y;
        if(sunrise_abs(tap_x-world->press_x)+sunrise_abs(tap_y-world->press_y)<28.0f){
            if(world->scene==UNDERWATER_SCENE_AURORA)
                underwater_aurora_tap(&world->aurora,tap_x,tap_y);
            else if(world->scene==UNDERWATER_SCENE_OCEAN)
                underwater_ocean_tap(&world->ocean,tap_x,tap_y);
        }
    }
    if(pressed&&world->dragging&&!world->ui_touch){
        float dx=x-world->last_x,dy=y-world->last_y;
        if(world->scene==UNDERWATER_SCENE_SUNRISE){
            world->yaw_velocity=-dx*0.065f;
            world->pitch_velocity=dy*0.055f;
            world->yaw-=dx*0.065f;
            world->pitch+=dy*0.055f;
            clamp_sunrise_cone(world);
        }else if(world->scene==UNDERWATER_SCENE_AURORA){
            world->aurora.flow_x=fminf(2.5f,fmaxf(-2.5f,world->aurora.flow_x+dx*.025f));
            world->aurora.flow_y=fminf(2.0f,fmaxf(-2.0f,world->aurora.flow_y-dy*.02f));
            underwater_aurora_look(&world->yaw,&world->pitch,&world->yaw_velocity,
                                   &world->pitch_velocity,dx,dy);
        }else if(world->scene==UNDERWATER_SCENE_OCEAN){
            world->ocean.flow_x=fminf(2.5f,fmaxf(-2.5f,world->ocean.flow_x+dx*.025f));
            world->ocean.flow_y=fminf(2.0f,fmaxf(-2.0f,world->ocean.flow_y-dy*.02f));
            underwater_ocean_look(&world->yaw,&world->pitch,&world->yaw_velocity,
                                  &world->pitch_velocity,dx,dy);
        }else{
            world->yaw_velocity=-dx*0.38f;
            world->pitch_velocity=dy*0.20f;
            world->yaw=wrap_degrees(world->yaw-dx*0.38f);
            world->pitch+=dy*0.20f;
            if(world->pitch < -28.0f)world->pitch=-28.0f;
            if(world->pitch > 24.0f)world->pitch=24.0f;
        }
        if(world->yaw_velocity>6.0f)world->yaw_velocity=6.0f;
        if(world->yaw_velocity<-6.0f)world->yaw_velocity=-6.0f;
        if(world->pitch_velocity>3.0f)world->pitch_velocity=3.0f;
        if(world->pitch_velocity<-3.0f)world->pitch_velocity=-3.0f;
    }
    if(pressed&&!world->dragging&&!world->ui_touch){
        world->press_x=x;world->press_y=y;
    }
    world->dragging=pressed&&!world->ui_touch;world->last_x=x;world->last_y=y;world->idle_ticks=0;
}

void underwater_world_update(underwater_world_t *world)
{
    if(!world)return;
    ++world->tick;
    if(world->scene==UNDERWATER_SCENE_SUNRISE)sunrise_particles_update(world);
    else if(world->scene==UNDERWATER_SCENE_AURORA)underwater_aurora_update(&world->aurora);
    else if(world->scene==UNDERWATER_SCENE_OCEAN)underwater_ocean_update(&world->ocean);
    if(!world->dragging){
        ++world->idle_ticks;
        if(world->scene==UNDERWATER_SCENE_RAINFOREST)
            world->yaw=wrap_degrees(world->yaw+world->yaw_velocity);
        else world->yaw+=world->yaw_velocity;
        world->pitch+=world->pitch_velocity;
        world->yaw_velocity*=0.86f;world->pitch_velocity*=0.78f;
        if(world->yaw_velocity<.01f&&world->yaw_velocity>-.01f)world->yaw_velocity=0;
        if(world->pitch_velocity<.01f&&world->pitch_velocity>-.01f)world->pitch_velocity=0;
        if(world->scene==UNDERWATER_SCENE_SUNRISE){
            clamp_sunrise_cone(world);
            if(world->idle_ticks>90&&world->yaw_velocity==0){
                float phase=(float)(world->tick%400U)/100.0f;
                float triangle=phase<2.0f?phase-1.0f:3.0f-phase;
                world->yaw=triangle*SUNRISE_YAW_LIMIT*.72f;
                clamp_sunrise_cone(world);
            }
        }else if(world->scene==UNDERWATER_SCENE_AURORA){
            underwater_aurora_look(&world->yaw,&world->pitch,&world->yaw_velocity,
                                   &world->pitch_velocity,0,0);
            if(world->idle_ticks>90&&world->yaw_velocity==0){
                float phase=(float)(world->tick%400U)/100.0f;
                float triangle=phase<2.0f?phase-1.0f:3.0f-phase;
                world->yaw=triangle*AURORA_YAW_LIMIT*.72f;
                underwater_aurora_look(&world->yaw,&world->pitch,&world->yaw_velocity,
                                       &world->pitch_velocity,0,0);
            }
        }else if(world->scene==UNDERWATER_SCENE_OCEAN){
            underwater_ocean_look(&world->yaw,&world->pitch,&world->yaw_velocity,
                                  &world->pitch_velocity,0,0);
            if(world->idle_ticks>90&&world->yaw_velocity==0){
                float phase=(float)(world->tick%400U)/100.0f;
                float triangle=phase<2.0f?phase-1.0f:3.0f-phase;
                world->yaw=triangle*OCEAN_YAW_LIMIT*.72f;
                underwater_ocean_look(&world->yaw,&world->pitch,&world->yaw_velocity,
                                      &world->pitch_velocity,0,0);
            }
        }else{
            if(world->pitch < -28.0f)world->pitch=-28.0f;
            if(world->pitch > 24.0f)world->pitch=24.0f;
            if(world->idle_ticks>90)world->yaw=wrap_degrees(world->yaw+0.08f);
        }
    }else if(world->scene==UNDERWATER_SCENE_RAINFOREST)world->yaw=wrap_degrees(world->yaw);
}

uint32_t underwater_world_hash(const underwater_world_t *world)
{
    const uint8_t *bytes=(const uint8_t *)world;uint32_t hash=2166136261U;
    for(size_t i=0;i<sizeof(*world);++i)hash=(hash^bytes[i])*16777619U;
    return hash;
}
