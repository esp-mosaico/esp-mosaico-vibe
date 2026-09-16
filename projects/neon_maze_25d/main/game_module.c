// SPDX-License-Identifier: Apache-2.0
#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <time.h>
#include "mosaico_game_module.h"
#include "host_asset_runtime.h"
#include "mosaico_raylib_fast.h"
#include "neon_maze_game.h"
#include "neon_maze_view.h"

typedef struct {
    neon_maze_game_t game;
    MosaicoAtlas enemies,weapon,environment,materials,controls;
    bool paused,left,right,forward,backward,fire,fire_was_down;
    int32_t joystick_track,look_track,look_x,look_y;
    float move_forward,move_strafe,target_forward,target_strafe;
} neon_maze_module_t;

#define JOYSTICK_X 82
#define JOYSTICK_Y 392
#define JOYSTICK_RADIUS 58
#define FIRE_X 410
#define FIRE_Y 392
#define FIRE_RADIUS 48

static bool inside_circle(int x,int y,int cx,int cy,int radius)
{int dx=x-cx,dy=y-cy;return dx*dx+dy*dy<=radius*radius;}
static void update_joystick(neon_maze_module_t *state,int x,int y)
{
    float dx=(float)(x-JOYSTICK_X)/JOYSTICK_RADIUS;
    float dy=(float)(y-JOYSTICK_Y)/JOYSTICK_RADIUS;
    float length=sqrtf(dx*dx+dy*dy);
    if(length>1.0f){dx/=length;dy/=length;}
    if(length<.12f)dx=dy=0;
    state->target_strafe=dx;state->target_forward=-dy;
}

static int initialize(void *value,const char *asset_root)
{
    neon_maze_module_t *state=value;
    mosaico_host_assets_set_root(asset_root);
    state->enemies=LoadMosaicoAtlas("enemy.atlas");
    state->weapon=LoadMosaicoAtlas("weapon.atlas");
    state->controls=LoadMosaicoAtlas("controls.atlas");
    state->environment=LoadMosaicoAtlas("environment.atlas");
    state->materials=LoadMosaicoAtlas("materials.atlas");
    if(!state->enemies.texture.id||!state->weapon.texture.id||!state->controls.texture.id||
       !state->environment.texture.id||
       !state->materials.texture.id)return -1;
    neon_maze_reset(&state->game);
    state->joystick_track=state->look_track=-1;
    InitWindow(480,480,"Neon Maze 2.5D");SetTargetFPS(30);return 0;
}
static void shutdown(void *value){neon_maze_module_t *state=value;if(state){
    UnloadMosaicoAtlas(state->enemies);UnloadMosaicoAtlas(state->weapon);
    UnloadMosaicoAtlas(state->controls);
    UnloadMosaicoAtlas(state->environment);
    UnloadMosaicoAtlas(state->materials);}}
static void input(void *value,const mosaico_host_input_v1_t *event)
{
    neon_maze_module_t *state=value;if(!state||!event)return;
    if(state->game.phase!=NEON_MAZE_PHASE_PLAYING){
        if(event->pressed&&(event->type==MOSAICO_HOST_INPUT_ACTION||
                            event->type==MOSAICO_HOST_INPUT_POINTER))
            neon_maze_confirm(&state->game);
        if(event->type==MOSAICO_HOST_INPUT_CONTROL){
            if(event->code==MOSAICO_HOST_CONTROL_PAUSE)state->paused=true;
            else if(event->code==MOSAICO_HOST_CONTROL_RESUME)state->paused=false;
            else if(event->code==MOSAICO_HOST_CONTROL_RESET)neon_maze_reset(&state->game);
        }
        return;
    }
    if(event->type==MOSAICO_HOST_INPUT_ACTION){
        if(event->code==0)state->left=event->pressed;
        else if(event->code==1)state->right=event->pressed;
        else if(event->code==2)state->forward=event->pressed;
        else if(event->code==5)state->backward=event->pressed;
        else if(event->code==6)state->fire=event->pressed;
    }else if(event->type==MOSAICO_HOST_INPUT_POINTER){
        int track=event->track_id;
        if(!event->pressed){
            if(track==state->joystick_track){state->joystick_track=-1;
                state->target_forward=state->target_strafe=0;}
            if(track==state->look_track)state->look_track=-1;
        }else if(track==state->joystick_track)update_joystick(state,event->x,event->y);
        else if(track==state->look_track){
            neon_maze_turn(&state->game,(float)(event->x-state->look_x)*.008f);
            neon_maze_look(&state->game,(float)(event->y-state->look_y)*-.18f);
            state->look_x=event->x;state->look_y=event->y;
        }else if(inside_circle(event->x,event->y,FIRE_X,FIRE_Y,FIRE_RADIUS))
            neon_maze_fire(&state->game);
        else if(inside_circle(event->x,event->y,JOYSTICK_X,JOYSTICK_Y,74)&&
                state->joystick_track<0){
            state->joystick_track=track;update_joystick(state,event->x,event->y);
        }else if(event->x>=180&&state->look_track<0){
            state->look_track=track;state->look_x=event->x;state->look_y=event->y;
        }
    }else if(event->type==MOSAICO_HOST_INPUT_CONTROL){
        if(event->code==MOSAICO_HOST_CONTROL_PAUSE)state->paused=true;
        else if(event->code==MOSAICO_HOST_CONTROL_RESUME)state->paused=false;
        else if(event->code==MOSAICO_HOST_CONTROL_RESET)neon_maze_reset(&state->game);
    }
}
static void update(void *value)
{
    neon_maze_module_t *state=value;if(!state||state->paused)return;
    state->move_forward+=(state->target_forward-state->move_forward)*.38f;
    state->move_strafe+=(state->target_strafe-state->move_strafe)*.38f;
    if(fabsf(state->move_forward)<.01f)state->move_forward=0;
    if(fabsf(state->move_strafe)<.01f)state->move_strafe=0;
    float forward=state->move_forward;
    float strafe=state->move_strafe;
    if(state->forward||state->backward)forward=(state->forward?1.0f:0.0f)-
                                                (state->backward?1.0f:0.0f);
    neon_maze_set_motion(&state->game,forward,strafe,
                         (state->right?1.0f:0.0f)-(state->left?1.0f:0.0f));
    if(state->fire&&!state->fire_was_down)neon_maze_fire(&state->game);
    state->fire_was_down=state->fire;
    neon_maze_update(&state->game);
}
static int render(void *value){neon_maze_module_t *state=value;
    struct timespec started,ended;timespec_get(&started,TIME_UTC);
    neon_maze_view_render(&state->game,state->enemies,state->weapon,state->environment,
                          state->materials,state->controls);
    timespec_get(&ended,TIME_UTC);
    float elapsed=(float)(ended.tv_sec-started.tv_sec)*1000.0f+
                  (float)(ended.tv_nsec-started.tv_nsec)/1000000.0f;
    neon_maze_set_performance(&state->game,30.0f,30.0f,elapsed);return 0;}
static uint32_t state_hash(const void *value)
{return neon_maze_state_hash(&((const neon_maze_module_t*)value)->game);}
static int state_json(const void *value,char *output,size_t capacity)
{
    const neon_maze_game_t *g=&((const neon_maze_module_t*)value)->game;
    static const char *phases[]={"start","playing","won","dead"};
    const char *phase=g->phase<=NEON_MAZE_PHASE_DEAD?phases[g->phase]:"playing";
    return snprintf(output,capacity,"{\"phase\":\"%s\",\"x\":%.2f,\"y\":%.2f,"
        "\"heading\":%d,\"pitch\":%.1f,\"score\":%u,\"hp\":%u,\"alive\":%d,\"tick\":%lu,"
        "\"state_hash\":\"%08lx\"}",phase,g->x,g->y,
        (int)(g->angle*57.29578f),g->look_pitch,g->score,g->hp,neon_maze_enemies_alive(g),
        (unsigned long)g->tick,(unsigned long)neon_maze_state_hash(g));
}
static const mosaico_game_module_v1_t s_module={
    .descriptor={MOSAICO_HOST_GAME_ABI_V1,"neon_maze_25d","Neon Maze 2.5D",480,480,30,2},
    .state_size=sizeof(neon_maze_module_t),.initialize=initialize,.shutdown=shutdown,
    .input=input,.update=update,.render=render,.state_hash=state_hash,.state_json=state_json};
const mosaico_game_module_v1_t *mosaico_game_module_v1(void){return &s_module;}
