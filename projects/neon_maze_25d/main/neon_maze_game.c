// SPDX-License-Identifier: Apache-2.0
#include "neon_maze_game.h"
#include <math.h>
#include <string.h>

static const char s_map[NEON_MAZE_HEIGHT][NEON_MAZE_WIDTH + 1] = {
    "111111111111111111111111",
    "100000000000000000000001",
    "100000011100000033300001",
    "100000011100000033300001",
    "100000000000000000000001",
    "100222000011100000330001",
    "100222000011100000330001",
    "100000000000000000000001",
    "100003330000000222000001",
    "100003330000000222000001",
    "100000000000000000000001",
    "104110000033300004110001",
    "104110000033300004110001",
    "100000000000000000000001",
    "100033300011100022200001",
    "100033300011100022200001",
    "100000000000000000000001",
    "100041100033300000111001",
    "100041100033300000111001",
    "100000000000000000000001",
    "100222000011100033300001",
    "100000000011100000000001",
    "100000000000000000000051",
    "111111111111111111111111",
};

static float angle_delta(float value);

static bool line_clear(const neon_maze_game_t *game,float x0,float y0,float x1,float y1)
{
    float dx=x1-x0,dy=y1-y0,distance=sqrtf(dx*dx+dy*dy);
    if(distance<.01f)return true;
    int steps=(int)(distance/.14f);
    for(int i=1;i<steps;++i)
        if(neon_maze_blocks(game,(int)(x0+dx*(float)i/steps),(int)(y0+dy*(float)i/steps)))
            return false;
    return true;
}

static void route_next(const neon_maze_game_t *game,int sx,int sy,int gx,int gy,
                       int8_t *out_dx,int8_t *out_dy)
{
    int16_t distance[NEON_MAZE_HEIGHT][NEON_MAZE_WIDTH];
    uint16_t queue[NEON_MAZE_WIDTH*NEON_MAZE_HEIGHT];
    static const int8_t dirs[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
    *out_dx=*out_dy=0;
    if(neon_maze_blocks(game,sx,sy)||neon_maze_blocks(game,gx,gy))return;
    for(int y=0;y<NEON_MAZE_HEIGHT;++y)for(int x=0;x<NEON_MAZE_WIDTH;++x)
        distance[y][x]=-1;
    unsigned head=0,tail=0;
    distance[gy][gx]=0;queue[tail++]=(uint16_t)(gy*NEON_MAZE_WIDTH+gx);
    while(head<tail){
        unsigned cell=queue[head++];int x=(int)(cell%NEON_MAZE_WIDTH),y=(int)(cell/NEON_MAZE_WIDTH);
        if(x==sx&&y==sy)break;
        for(int i=0;i<4;++i){int nx=x+dirs[i][0],ny=y+dirs[i][1];
            if(nx<0||ny<0||nx>=NEON_MAZE_WIDTH||ny>=NEON_MAZE_HEIGHT||
               distance[ny][nx]>=0||neon_maze_blocks(game,nx,ny))continue;
            distance[ny][nx]=(int16_t)(distance[y][x]+1);
            queue[tail++]=(uint16_t)(ny*NEON_MAZE_WIDTH+nx);
        }
    }
    int best=distance[sy][sx];
    if(best<=0)return;
    for(int i=0;i<4;++i){int nx=sx+dirs[i][0],ny=sy+dirs[i][1];
        if(nx>=0&&ny>=0&&nx<NEON_MAZE_WIDTH&&ny<NEON_MAZE_HEIGHT&&
           distance[ny][nx]>=0&&distance[ny][nx]<best){
            best=distance[ny][nx];*out_dx=dirs[i][0];*out_dy=dirs[i][1];
        }
    }
}

static void hurt_player(neon_maze_game_t *game,const neon_maze_enemy_t *enemy)
{
    if(game->hurt_cooldown)return;
    if(game->hp)--game->hp;
    game->hurt_cooldown=22;game->hit_flash=8;
    game->damage_angle=angle_delta(atan2f(enemy->y-game->y,enemy->x-game->x)-game->angle);
    if(!game->hp)game->phase=NEON_MAZE_PHASE_DEAD;
}

uint8_t neon_maze_cell(int x,int y)
{
    if(x<0||y<0||x>=NEON_MAZE_WIDTH||y>=NEON_MAZE_HEIGHT)return 1;
    return (uint8_t)(s_map[y][x]-'0');
}

bool neon_maze_blocks(const neon_maze_game_t *game,int x,int y)
{
    uint8_t cell=neon_maze_cell(x,y);
    if(cell==0||cell==5)return false;
    if(cell==4)return !game||!game->door_open[y][x];
    return true;
}

int neon_maze_enemies_alive(const neon_maze_game_t *game)
{
    if(!game)return 0;
    int alive=0;
    for(int i=0;i<NEON_MAZE_ENEMIES;++i)if(game->enemies[i].active)++alive;
    return alive;
}

void neon_maze_reset(neon_maze_game_t *game)
{
    if(!game)return;
    memset(game,0,sizeof(*game));
    game->x=1.5f;game->y=1.5f;game->angle=.15f;
    game->hp=NEON_MAZE_MAX_HP;
    game->display_hp=(float)NEON_MAZE_MAX_HP;
    game->phase=NEON_MAZE_PHASE_START;
    const neon_maze_enemy_t enemies[NEON_MAZE_ENEMIES]={
        {.x=7.5f,.y=1.5f,.hp=2,.move_phase=0,.active=true},
        {.x=15.5f,.y=2.5f,.hp=2,.move_phase=19,.active=true},
        {.x=4.5f,.y=7.5f,.hp=2,.move_phase=41,.active=true},
        {.x=18.5f,.y=7.5f,.hp=2,.move_phase=67,.active=true},
        {.x=8.5f,.y=10.5f,.hp=2,.move_phase=83,.active=true},
        {.x=20.5f,.y=12.5f,.hp=2,.move_phase=101,.active=true},
        {.x=3.5f,.y=16.5f,.hp=2,.move_phase=127,.active=true},
        {.x=13.5f,.y=16.5f,.hp=2,.move_phase=149,.active=true},
        {.x=7.5f,.y=21.5f,.hp=2,.move_phase=173,.active=true},
        {.x=20.5f,.y=21.5f,.hp=2,.move_phase=211,.active=true}};
    memcpy(game->enemies,enemies,sizeof(enemies));
    game->perf_logic_fps=game->perf_display_fps=30.0f;
}

void neon_maze_confirm(neon_maze_game_t *game)
{
    if(!game)return;
    if(game->phase==NEON_MAZE_PHASE_PLAYING)return;
    if(game->phase!=NEON_MAZE_PHASE_START)neon_maze_reset(game);
    game->phase=NEON_MAZE_PHASE_PLAYING;
}

void neon_maze_set_actions(neon_maze_game_t *game,bool left,bool right,bool forward,
                           bool backward)
{ if(game){game->left=left;game->right=right;game->forward=forward;game->backward=backward;
    game->turn_input=(right?1.0f:0.0f)-(left?1.0f:0.0f);
    game->move_forward=(forward?1.0f:0.0f)-(backward?1.0f:0.0f);game->move_strafe=0;} }

void neon_maze_set_motion(neon_maze_game_t *game,float forward,float strafe,float turn)
{ if(game){game->move_forward=forward;game->move_strafe=strafe;game->turn_input=turn;} }

void neon_maze_turn(neon_maze_game_t *game,float radians)
{ if(game){game->angle+=radians;while(game->angle<0)game->angle+=6.2831853f;
    while(game->angle>=6.2831853f)game->angle-=6.2831853f;} }

void neon_maze_look(neon_maze_game_t *game,float pixels)
{
    if(!game)return;
    game->look_pitch+=pixels;
    if(game->look_pitch>42.0f)game->look_pitch=42.0f;
    if(game->look_pitch< -42.0f)game->look_pitch=-42.0f;
}

void neon_maze_set_performance(neon_maze_game_t *game,float logic_fps,
                               float display_fps,float render_ms)
{ if(game){game->perf_logic_fps=logic_fps;game->perf_display_fps=display_fps;
    game->perf_render_ms=render_ms;} }

static void open_nearby_doors(neon_maze_game_t *game)
{
    int px=(int)game->x,py=(int)game->y;
    for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){
        int x=px+dx,y=py+dy;
        if(neon_maze_cell(x,y)!=4||game->door_open[y][x])continue;
        float cx=x+0.5f-game->x,cy=y+0.5f-game->y;
        if(cx*cx+cy*cy>2.1f)continue;
        game->door_open[y][x]=1;
        game->score+=15;
    }
}

void neon_maze_update(neon_maze_game_t *game)
{
    if(!game||game->phase!=NEON_MAZE_PHASE_PLAYING)return;
    const float turn=.085f, speed=.09f;
    game->angle+=turn*game->turn_input;
    if(game->angle<0)game->angle+=6.2831853f;
    if(game->angle>=6.2831853f)game->angle-=6.2831853f;
    if(fabsf(game->move_forward)>.01f||fabsf(game->move_strafe)>.01f){
        float forward=game->move_forward,strafe=game->move_strafe;
        float length=sqrtf(forward*forward+strafe*strafe);
        if(length>1.0f){forward/=length;strafe/=length;}
        float dx=cosf(game->angle)*forward-sinf(game->angle)*strafe;
        float dy=sinf(game->angle)*forward+cosf(game->angle)*strafe;
        float nx=game->x+dx*speed;
        float ny=game->y+dy*speed;
        if(!neon_maze_blocks(game,(int)nx,(int)game->y))game->x=nx;
        if(!neon_maze_blocks(game,(int)game->x,(int)ny))game->y=ny;
        game->move_phase+=.44f*length;
    }
    game->weapon_recoil*=.64f;
    if(game->weapon_recoil<.01f)game->weapon_recoil=0;
    if(game->hit_flash<=4&&game->display_hp>(float)game->hp){
        game->display_hp+=((float)game->hp-game->display_hp)*.22f;
        if(game->display_hp<(float)game->hp+.02f)game->display_hp=(float)game->hp;
    }
    open_nearby_doors(game);
    for(int i=0;i<NEON_MAZE_ENEMIES;++i){
        neon_maze_enemy_t *enemy=&game->enemies[i];
        if(enemy->death_timer)--enemy->death_timer;
        if(!enemy->active)continue;
        if(enemy->hit_flash)--enemy->hit_flash;
        if(enemy->attack_flash)--enemy->attack_flash;
        if(enemy->attack_cooldown)--enemy->attack_cooldown;
        float to_x=game->x-enemy->x,to_y=game->y-enemy->y;
        float distance=sqrtf(to_x*to_x+to_y*to_y),heading=0;
        bool sees_player=distance<10.0f&&line_clear(game,enemy->x,enemy->y,game->x,game->y);
        if(sees_player){
            enemy->last_seen_x=game->x;enemy->last_seen_y=game->y;enemy->search_timer=105;
            if(enemy->ai_state==NEON_ENEMY_PATROL||enemy->ai_state==NEON_ENEMY_SEARCH){
                enemy->ai_state=NEON_ENEMY_ALERT;enemy->alert_timer=18;
            }
        }else if(enemy->ai_state==NEON_ENEMY_ENGAGE||enemy->ai_state==NEON_ENEMY_ALERT){
            enemy->ai_state=NEON_ENEMY_SEARCH;enemy->aim_timer=0;
        }
        if(enemy->ai_state==NEON_ENEMY_ALERT){
            if(enemy->alert_timer)--enemy->alert_timer;
            else enemy->ai_state=NEON_ENEMY_ENGAGE;
        }else if(enemy->ai_state==NEON_ENEMY_SEARCH){
            if(enemy->search_timer)--enemy->search_timer;else enemy->ai_state=NEON_ENEMY_PATROL;
        }
        if(enemy->ai_state==NEON_ENEMY_ENGAGE&&sees_player&&distance>.9f&&distance<9.0f){
            if(!enemy->attack_cooldown){
                if(!enemy->aim_timer)enemy->aim_timer=16;
                else if(!--enemy->aim_timer){
                    enemy->attack_flash=4;enemy->attack_cooldown=42;
                    hurt_player(game,enemy);
                    if(game->phase==NEON_MAZE_PHASE_DEAD)return;
                }
            }
        }else enemy->aim_timer=0;
        if(distance<.52f){
            hurt_player(game,enemy);
            if(game->phase==NEON_MAZE_PHASE_DEAD)return;
        }
        bool should_move=enemy->ai_state==NEON_ENEMY_PATROL||
                         enemy->ai_state==NEON_ENEMY_SEARCH||
                         (enemy->ai_state==NEON_ENEMY_ENGAGE&&distance>3.2f);
        if(!should_move)continue;
        if(enemy->ai_state==NEON_ENEMY_PATROL)
            heading=((float)((game->tick/75U+enemy->move_phase)%16U))*0.3926991f;
        else if(sees_player)heading=atan2f(to_y,to_x);
        else {
            if(((game->tick+(uint32_t)i)%8U)==0U||(!enemy->nav_dx&&!enemy->nav_dy))
                route_next(game,(int)enemy->x,(int)enemy->y,(int)enemy->last_seen_x,
                           (int)enemy->last_seen_y,&enemy->nav_dx,&enemy->nav_dy);
            if(!enemy->nav_dx&&!enemy->nav_dy)continue;
            heading=atan2f((float)enemy->nav_dy,(float)enemy->nav_dx);
        }
        float step=enemy->ai_state==NEON_ENEMY_ENGAGE?.028f:.022f;
        float dx=cosf(heading)*step,dy=sinf(heading)*step;
        float nx=enemy->x+dx,ny=enemy->y+dy;
        if(!neon_maze_blocks(game,(int)nx,(int)enemy->y))enemy->x=nx;
        else enemy->move_phase=(uint8_t)(enemy->move_phase+5U);
        if(!neon_maze_blocks(game,(int)enemy->x,(int)ny))enemy->y=ny;
        else enemy->move_phase=(uint8_t)(enemy->move_phase+7U);
    }
    if(neon_maze_enemies_alive(game)==0 &&
       neon_maze_cell((int)game->x,(int)game->y)==5){
        game->cells_reached=1;
        game->phase=NEON_MAZE_PHASE_WON;
    }
    if(game->fire_cooldown)--game->fire_cooldown;
    if(game->hit_flash)--game->hit_flash;
    if(game->hit_marker)--game->hit_marker;
    if(game->kill_flash)--game->kill_flash;
    if(game->hurt_cooldown)--game->hurt_cooldown;
    ++game->tick;
}

static float angle_delta(float value)
{
    while(value>3.1415927f)value-=6.2831853f;
    while(value<-3.1415927f)value+=6.2831853f;
    return value;
}

void neon_maze_fire(neon_maze_game_t *game)
{
    if(!game)return;
    if(game->phase!=NEON_MAZE_PHASE_PLAYING){neon_maze_confirm(game);return;}
    if(game->fire_cooldown)return;
    game->fire_cooldown=7;
    game->weapon_recoil=1.0f;
    int target=-1;float best=1000.0f;
    for(int i=0;i<NEON_MAZE_ENEMIES;++i){
        neon_maze_enemy_t *enemy=&game->enemies[i];if(!enemy->active)continue;
        float dx=enemy->x-game->x,dy=enemy->y-game->y;
        float distance=sqrtf(dx*dx+dy*dy);
        float delta=fabsf(angle_delta(atan2f(dy,dx)-game->angle));
        if(delta>.08f+(.16f/distance)||distance>=best)continue;
        bool blocked=false;
        for(float ray=.12f;ray<distance-.18f;ray+=.08f)
            if(neon_maze_blocks(game,(int)(game->x+dx*ray/distance),
                                (int)(game->y+dy*ray/distance))){blocked=true;break;}
        if(!blocked){best=distance;target=i;}
    }
    if(target>=0){
        neon_maze_enemy_t *enemy=&game->enemies[target];
        if(enemy->hp)--enemy->hp;
        enemy->hit_flash=6;
        float knock_x=enemy->x-game->x,knock_y=enemy->y-game->y;
        float knock_length=sqrtf(knock_x*knock_x+knock_y*knock_y);
        if(knock_length>.01f){
            float nx=enemy->x+knock_x/knock_length*.22f;
            float ny=enemy->y+knock_y/knock_length*.22f;
            if(!neon_maze_blocks(game,(int)nx,(int)enemy->y))enemy->x=nx;
            if(!neon_maze_blocks(game,(int)enemy->x,(int)ny))enemy->y=ny;
        }
        game->hit_marker=5;
        game->score+=25;
        if(!enemy->hp){enemy->active=false;enemy->death_timer=20;game->kill_flash=18;
            game->score+=100;}
    }
}

uint32_t neon_maze_state_hash(const neon_maze_game_t *game)
{
    if(!game)return 0;
    uint32_t hash=2166136261U, values[]={
        (uint32_t)(game->x*4096),(uint32_t)(game->y*4096),
        (uint32_t)(game->angle*4096),(uint32_t)((game->look_pitch+64.0f)*256.0f),
        game->tick,game->cells_reached,game->score,
        (uint32_t)game->phase,(uint32_t)game->hp};
    for(unsigned i=0;i<sizeof(values)/sizeof(values[0]);++i){hash^=values[i];hash*=16777619U;}
    for(int i=0;i<NEON_MAZE_ENEMIES;++i){hash^=(uint32_t)game->enemies[i].hp;
        hash*=16777619U;}
    for(int y=0;y<NEON_MAZE_HEIGHT;++y)for(int x=0;x<NEON_MAZE_WIDTH;++x){
        hash^=game->door_open[y][x];hash*=16777619U;}
    return hash;
}
