// SPDX-License-Identifier: Apache-2.0
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "host_asset_runtime.h"
#include "mosaico_game_2d.h"
#include "mosaico_game_module.h"
#include "mosaico_raylib_fast.h"
#include "underwater_view.h"
#include "underwater_world.h"

typedef struct {
    underwater_world_t world;
    underwater_view_atlases_t atlases;
    uint8_t loaded_volumes;
    bool paused;
} module_state_t;

static void unload_atlas(MosaicoAtlas *atlas)
{
    if(!atlas||!atlas->texture.id)return;
    UnloadMosaicoAtlas(*atlas);
    *atlas=(MosaicoAtlas){0};
}

static int load_atlas(MosaicoAtlas *atlas,const char *path)
{
    if(atlas->texture.id)return 0;
    *atlas=LoadMosaicoAtlas(path);
    return atlas->texture.id?0:-1;
}

static void unload_volumes(underwater_view_atlases_t *atlases)
{
    unload_atlas(&atlases->sunrise_cliff_front);
    unload_atlas(&atlases->sunrise_cliff_side);
    unload_atlas(&atlases->sunrise_cliff_rear);
    unload_atlas(&atlases->aurora_ice_front);
    unload_atlas(&atlases->aurora_ice_side);
    unload_atlas(&atlases->aurora_ice_rear);
    unload_atlas(&atlases->ocean_left_front);
    unload_atlas(&atlases->ocean_left_side);
    unload_atlas(&atlases->ocean_left_rear);
    unload_atlas(&atlases->ocean_right_front);
    unload_atlas(&atlases->ocean_right_side);
    unload_atlas(&atlases->ocean_right_rear);
}

static int acquire_volumes(module_state_t *s,uint8_t scene)
{
    if(s->loaded_volumes==scene)return 0;
    unload_volumes(&s->atlases);
    s->loaded_volumes=UINT8_MAX;
    int err=0;
    if(scene==UNDERWATER_SCENE_AURORA){
        err=load_atlas(&s->atlases.aurora_ice_front,"aurora_ice_front.atlas")||
            load_atlas(&s->atlases.aurora_ice_side,"aurora_ice_side.atlas")||
            load_atlas(&s->atlases.aurora_ice_rear,"aurora_ice_rear.atlas");
    }else if(scene==UNDERWATER_SCENE_SUNRISE){
        err=load_atlas(&s->atlases.sunrise_cliff_front,"sunrise_cliff_front.atlas")||
            load_atlas(&s->atlases.sunrise_cliff_side,"sunrise_cliff_side.atlas")||
            load_atlas(&s->atlases.sunrise_cliff_rear,"sunrise_cliff_rear.atlas");
    }else if(scene==UNDERWATER_SCENE_OCEAN){
        err=load_atlas(&s->atlases.ocean_left_front,"ocean_reef_left_front.atlas")||
            load_atlas(&s->atlases.ocean_left_side,"ocean_reef_left_side.atlas")||
            load_atlas(&s->atlases.ocean_left_rear,"ocean_reef_left_rear.atlas")||
            load_atlas(&s->atlases.ocean_right_front,"ocean_reef_right_front.atlas")||
            load_atlas(&s->atlases.ocean_right_side,"ocean_reef_right_side.atlas")||
            load_atlas(&s->atlases.ocean_right_rear,"ocean_reef_right_rear.atlas");
    }
    if(err){unload_volumes(&s->atlases);return -1;}
    s->loaded_volumes=scene;
    return 0;
}

static int initialize(void *value,const char *asset_root)
{
    module_state_t *s=value;
    mosaico_host_assets_set_root(asset_root);
    if(load_atlas(&s->atlases.aurora,"aurora.atlas")||
       load_atlas(&s->atlases.ocean,"ocean.atlas")||
       load_atlas(&s->atlases.sunrise,"sunrise.atlas")||
       load_atlas(&s->atlases.rainforest,"rainforest.atlas"))return -1;
    underwater_world_reset(&s->world);
    s->loaded_volumes=UINT8_MAX;
    if(acquire_volumes(s,s->world.scene))return -1;
    InitWindow(480,480,"Living Worlds");
    SetTargetFPS(30);
    return 0;
}

static void shutdown(void *value)
{
    module_state_t *s=value;
    if(!s)return;
    unload_volumes(&s->atlases);
    unload_atlas(&s->atlases.rainforest);
    unload_atlas(&s->atlases.sunrise);
    unload_atlas(&s->atlases.ocean);
    unload_atlas(&s->atlases.aurora);
}

static void input(void *value,const mosaico_host_input_v1_t *event)
{
    module_state_t *s=value;if(!s||!event)return;
    if(event->type==MOSAICO_HOST_INPUT_POINTER){
        uint8_t previous=s->world.scene;
        underwater_world_pointer(&s->world,(float)event->x,(float)event->y,event->pressed);
        if(s->world.scene!=previous&&acquire_volumes(s,s->world.scene)!=0)
            s->world.scene=previous;
    }else if(event->type==MOSAICO_HOST_INPUT_ACTION&&event->pressed){
        if(event->code==0)s->world.yaw-=8;else if(event->code==1)s->world.yaw+=8;
    }else if(event->type==MOSAICO_HOST_INPUT_CONTROL){
        if(event->code==MOSAICO_HOST_CONTROL_PAUSE)s->paused=true;
        else if(event->code==MOSAICO_HOST_CONTROL_RESUME)s->paused=false;
        else if(event->code==MOSAICO_HOST_CONTROL_RESET){
            underwater_world_reset(&s->world);
            acquire_volumes(s,s->world.scene);
        }
    }
}

static void update(void *value){module_state_t *s=value;if(!s->paused)underwater_world_update(&s->world);}
static int render(void *value){module_state_t *s=value;underwater_view_render(&s->world,&s->atlases);return 0;}
static uint32_t state_hash(const void *value){return underwater_world_hash(&((const module_state_t *)value)->world);}
static int state_json(const void *value,char *output,size_t capacity)
{
    const underwater_world_t *w=&((const module_state_t *)value)->world;
    return snprintf(output,capacity,
        "{\"scene\":%u,\"yaw\":%.2f,\"pitch\":%.2f,\"dragging\":%s,\"tick\":%lu,\"state_hash\":\"%08lx\"}",
        w->scene,w->yaw,w->pitch,w->dragging?"true":"false",(unsigned long)w->tick,
        (unsigned long)underwater_world_hash(w));
}
static const mosaico_game_module_v1_t MODULE={
    .descriptor={MOSAICO_HOST_GAME_ABI_V1,"underwater_360","Living Worlds",480,480,30,1},
    .state_size=sizeof(module_state_t),.initialize=initialize,.shutdown=shutdown,
    .input=input,.update=update,.render=render,.state_hash=state_hash,.state_json=state_json
};
const mosaico_game_module_v1_t *mosaico_game_module_v1(void){return &MODULE;}
