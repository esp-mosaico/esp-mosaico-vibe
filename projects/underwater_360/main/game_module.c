// SPDX-License-Identifier: Apache-2.0
#include <stdbool.h>
#include <stdio.h>
#include "host_asset_runtime.h"
#include "mosaico_game_2d.h"
#include "mosaico_game_module.h"
#include "mosaico_raylib_fast.h"
#include "underwater_view.h"
#include "underwater_world.h"

typedef struct { underwater_world_t world;MosaicoAtlas panorama,fish,creatures,aurora,sunrise;bool paused; } module_state_t;
static int initialize(void *value,const char *asset_root)
{ module_state_t *s=value;mosaico_host_assets_set_root(asset_root);s->panorama=LoadMosaicoAtlas("panorama.atlas");s->fish=LoadMosaicoAtlas("reef_fish.atlas");s->creatures=LoadMosaicoAtlas("marine_creatures.atlas");s->aurora=LoadMosaicoAtlas("aurora.atlas");s->sunrise=LoadMosaicoAtlas("sunrise.atlas");if(!s->panorama.texture.id||!s->fish.texture.id||!s->creatures.texture.id||!s->aurora.texture.id||!s->sunrise.texture.id)return -1;underwater_world_reset(&s->world);InitWindow(480,480,"Living Worlds");SetTargetFPS(30);return 0; }
static void shutdown(void *value){module_state_t *s=value;if(s){UnloadMosaicoAtlas(s->sunrise);UnloadMosaicoAtlas(s->aurora);UnloadMosaicoAtlas(s->creatures);UnloadMosaicoAtlas(s->fish);UnloadMosaicoAtlas(s->panorama);}}
static void input(void *value,const mosaico_host_input_v1_t *event)
{
 module_state_t *s=value;if(!s||!event)return;
 if(event->type==MOSAICO_HOST_INPUT_POINTER)underwater_world_pointer(&s->world,(float)event->x,(float)event->y,event->pressed);
 else if(event->type==MOSAICO_HOST_INPUT_ACTION&&event->pressed){if(event->code==0)s->world.yaw-=8;else if(event->code==1)s->world.yaw+=8;}
 else if(event->type==MOSAICO_HOST_INPUT_CONTROL){if(event->code==MOSAICO_HOST_CONTROL_PAUSE)s->paused=true;else if(event->code==MOSAICO_HOST_CONTROL_RESUME)s->paused=false;else if(event->code==MOSAICO_HOST_CONTROL_RESET)underwater_world_reset(&s->world);}
}
static void update(void *value){module_state_t *s=value;if(!s->paused)underwater_world_update(&s->world);}
static int render(void *value){module_state_t *s=value;underwater_view_render(&s->world,s->panorama,s->fish,s->creatures,s->aurora,s->sunrise);return 0;}
static uint32_t state_hash(const void *value){return underwater_world_hash(&((const module_state_t *)value)->world);}
static int state_json(const void *value,char *output,size_t capacity)
{
 const underwater_world_t *w=&((const module_state_t *)value)->world;
 return snprintf(output,capacity,"{\"scene\":%u,\"yaw\":%.2f,\"pitch\":%.2f,\"dragging\":%s,\"tick\":%lu,\"state_hash\":\"%08lx\"}",w->scene,w->yaw,w->pitch,w->dragging?"true":"false",(unsigned long)w->tick,(unsigned long)underwater_world_hash(w));
}
static const mosaico_game_module_v1_t MODULE={.descriptor={MOSAICO_HOST_GAME_ABI_V1,"underwater_360","Living Worlds",480,480,30,1},.state_size=sizeof(module_state_t),.initialize=initialize,.shutdown=shutdown,.input=input,.update=update,.render=render,.state_hash=state_hash,.state_json=state_json};
const mosaico_game_module_v1_t *mosaico_game_module_v1(void){return &MODULE;}
