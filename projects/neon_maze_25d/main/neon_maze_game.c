// SPDX-License-Identifier: Apache-2.0
#include "neon_maze_game.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static const char s_map[NEON_MAZE_HEIGHT][NEON_MAZE_WIDTH + 1] = {
    "111111111111111111111111",
    "100000211111111111111111",
    "100000211111111111111111",
    "100300000002111111111111",
    "101111110002111111111111",
    "101111110000000021111111",
    "101111110000300021111111",
    "100000011111110000000001",
    "100000011111000003000001",
    "103000011111000000200001",
    "111111111111441111111111",
    "111111111111000000030001",
    "111111111111000300000001",
    "111111111111001111110001",
    "111111111111000000000001",
    "111111111111030000300001",
    "111111111111111110011111",
    "111111111111111110000001",
    "111111111111111110000001",
    "111111111111111110000001",
    "111111111111111110030001",
    "111111111111111110000051",
    "111111111111111110000001",
    "111111111111111111111111",
};

static float angle_delta(float value);
static void emit_sfx(neon_maze_game_t *game,uint8_t id);

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

static bool layout_open(int x,int y)
{
    uint8_t cell=neon_maze_cell(x,y);
    return cell==0||cell==4||cell==5;
}

static bool adjacent_cover(int x,int y)
{
    static const int8_t dirs[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
    for(int i=0;i<4;++i){
        uint8_t cell=neon_maze_cell(x+dirs[i][0],y+dirs[i][1]);
        if(cell>=1&&cell<=3)return true;
    }
    return false;
}

static bool ally_at(const neon_maze_game_t *game,int ignore,float x,float y)
{
    for(int i=0;i<NEON_MAZE_ENEMIES;++i){
        if(i==ignore||!game->enemies[i].active)continue;
        float dx=game->enemies[i].x-x,dy=game->enemies[i].y-y;
        if(dx*dx+dy*dy<.72f)return true;
    }
    return false;
}

static void pick_hold_cell(neon_maze_game_t *game,int index,neon_maze_enemy_t *enemy)
{
    int ex=(int)enemy->x,ey=(int)enemy->y,best_x=ex,best_y=ey;
    float best=1e9f;
    for(int dy=-2;dy<=2;++dy)for(int dx=-2;dx<=2;++dx){
        int nx=ex+dx,ny=ey+dy;
        if(!layout_open(nx,ny)||neon_maze_blocks(game,nx,ny))continue;
        float cx=(float)nx+0.5f,cy=(float)ny+0.5f;
        if(ally_at(game,index,cx,cy))continue;
        if(!line_clear(game,cx,cy,game->x,game->y))continue;
        float px=cx-game->x,py=cy-game->y,dist=sqrtf(px*px+py*py);
        if(dist<2.2f||dist>5.4f)continue;
        float score=fabsf(dist-3.4f);
        if(adjacent_cover(nx,ny))score-=1.5f;
        if(score<best){best=score;best_x=nx;best_y=ny;}
    }
    enemy->hold_x=(int8_t)best_x;
    enemy->hold_y=(int8_t)best_y;
}

static bool blocked_at(const neon_maze_game_t *game,float x,float y)
{
    const float r=.10f;
    return neon_maze_blocks(game,(int)(x-r),(int)y)||neon_maze_blocks(game,(int)(x+r),(int)y)||
           neon_maze_blocks(game,(int)x,(int)(y-r))||neon_maze_blocks(game,(int)x,(int)(y+r));
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
    if(game->damage_taken<255)++game->damage_taken;
    game->hurt_cooldown=40;game->hit_flash=8;
    emit_sfx(game,9);
    game->damage_angle=angle_delta(atan2f(enemy->y-game->y,enemy->x-game->x)-game->angle);
    if(!game->hp)game->phase=NEON_MAZE_PHASE_DEAD;
}

static void mark_explored(neon_maze_game_t *game)
{
    int px=(int)game->x,py=(int)game->y;
    static const int8_t dirs[5][2]={{0,0},{1,0},{-1,0},{0,1},{0,-1}};
    for(int i=0;i<5;++i){
        int x=px+dirs[i][0],y=py+dirs[i][1];
        if(x>=0&&y>=0&&x<NEON_MAZE_WIDTH&&y<NEON_MAZE_HEIGHT)game->explored[y][x]=1;
    }
}

static void open_door_cluster(neon_maze_game_t *game,int x,int y)
{
    for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){
        int nx=x+dx,ny=y+dy;
        if(neon_maze_cell(nx,ny)==4&&!game->door_open[ny][nx]){
            game->door_open[ny][nx]=1;
            game->score+=15;
            game->door_flash=12;
        }
    }
}

static bool door_cell_ahead(const neon_maze_game_t *game,int *out_x,int *out_y)
{
    float fx=cosf(game->angle),fy=sinf(game->angle);
    for(float ray=.28f;ray<1.45f;ray+=.12f){
        int x=(int)(game->x+fx*ray),y=(int)(game->y+fy*ray);
        if(neon_maze_cell(x,y)==4&&!game->door_open[y][x]){
            if(out_x)*out_x=x;
            if(out_y)*out_y=y;
            return true;
        }
        if(neon_maze_blocks(game,x,y))return false;
    }
    return false;
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

bool neon_maze_door_ahead(const neon_maze_game_t *game)
{
    return game&&door_cell_ahead(game,NULL,NULL);
}

bool neon_maze_near_closed_door(const neon_maze_game_t *game)
{
    if(!game)return false;
    int px=(int)game->x,py=(int)game->y;
    for(int dy=-2;dy<=2;++dy)for(int dx=-2;dx<=2;++dx){
        int x=px+dx,y=py+dy;
        if(neon_maze_cell(x,y)==4&&!game->door_open[y][x])return true;
    }
    return false;
}

bool neon_maze_pickup_visible(const neon_maze_game_t *game,int index)
{
    if(!game||index<0||index>=NEON_MAZE_PICKUPS||game->pickups[index].taken)return false;
    int x=(int)game->pickups[index].x,y=(int)game->pickups[index].y;
    if(x<0||y<0||x>=NEON_MAZE_WIDTH||y>=NEON_MAZE_HEIGHT||!game->explored[y][x])return false;
    float dx=game->pickups[index].x-game->x,dy=game->pickups[index].y-game->y;
    return dx*dx+dy*dy<64.0f;
}

bool neon_maze_enemy_on_radar(const neon_maze_game_t *game,int index)
{
    if(!game||index<0||index>=NEON_MAZE_ENEMIES||!game->enemies[index].active)return false;
    const neon_maze_enemy_t *enemy=&game->enemies[index];
    float dx=enemy->x-game->x,dy=enemy->y-game->y,dist=sqrtf(dx*dx+dy*dy);
    if(dist>5.2f)return false;
    if(enemy->ai_state!=NEON_ENEMY_PATROL)return true;
    return line_clear(game,game->x,game->y,enemy->x,enemy->y);
}

int neon_maze_enemies_alive(const neon_maze_game_t *game)
{
    if(!game)return 0;
    int alive=0;
    for(int i=0;i<NEON_MAZE_ENEMIES;++i)if(game->enemies[i].active)++alive;
    return alive;
}

int neon_maze_last_enemy_index(const neon_maze_game_t *game)
{
    if(!game||neon_maze_enemies_alive(game)!=1)return -1;
    for(int i=0;i<NEON_MAZE_ENEMIES;++i)if(game->enemies[i].active)return i;
    return -1;
}

float neon_maze_extract_bearing(const neon_maze_game_t *game)
{
    if(!game)return 0;
    return angle_delta(atan2f(NEON_MAZE_EXTRACT_Y-game->y,NEON_MAZE_EXTRACT_X-game->x)-game->angle);
}

char neon_maze_grade(const neon_maze_game_t *game)
{
    if(!game||game->phase==NEON_MAZE_PHASE_DEAD)return 'D';
    unsigned seconds=game->tick/30U;
    if(game->hp>=4&&seconds<=90U)return 'S';
    if(game->hp>=3&&seconds<=140U)return 'A';
    if(seconds<=200U)return 'B';
    return 'C';
}

bool neon_maze_in_move_zone(int x,int y)
{
    int dx=x-NEON_MAZE_MOVE_X,dy=y-NEON_MAZE_MOVE_Y;
    return dx*dx+dy*dy<=NEON_MAZE_MOVE_R*NEON_MAZE_MOVE_R;
}

bool neon_maze_in_fire_zone(int x,int y)
{
    int dx=x-NEON_MAZE_FIRE_X,dy=y-NEON_MAZE_FIRE_Y;
    return dx*dx+dy*dy<=NEON_MAZE_FIRE_R*NEON_MAZE_FIRE_R;
}

static void flood_layout(uint8_t seen[NEON_MAZE_HEIGHT][NEON_MAZE_WIDTH],int sx,int sy)
{
    uint16_t queue[NEON_MAZE_WIDTH*NEON_MAZE_HEIGHT];
    static const int8_t dirs[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
    unsigned head=0,tail=0;
    memset(seen,0,sizeof(uint8_t)*NEON_MAZE_WIDTH*NEON_MAZE_HEIGHT);
    if(!layout_open(sx,sy))return;
    seen[sy][sx]=1;queue[tail++]=(uint16_t)(sy*NEON_MAZE_WIDTH+sx);
    while(head<tail){
        unsigned cell=queue[head++];
        int x=(int)(cell%NEON_MAZE_WIDTH),y=(int)(cell/NEON_MAZE_WIDTH);
        for(int i=0;i<4;++i){
            int nx=x+dirs[i][0],ny=y+dirs[i][1];
            if(nx<0||ny<0||nx>=NEON_MAZE_WIDTH||ny>=NEON_MAZE_HEIGHT||seen[ny][nx])continue;
            if(!layout_open(nx,ny))continue;
            seen[ny][nx]=1;queue[tail++]=(uint16_t)(ny*NEON_MAZE_WIDTH+nx);
        }
    }
}

static void snap_reachable(float *px,float *py,const uint8_t seen[NEON_MAZE_HEIGHT][NEON_MAZE_WIDTH])
{
    int x=(int)*px,y=(int)*py;
    if(x>=0&&y>=0&&x<NEON_MAZE_WIDTH&&y<NEON_MAZE_HEIGHT&&seen[y][x])return;
    int best=999,bx=2,by=3;
    for(int gy=1;gy<NEON_MAZE_HEIGHT-1;++gy)for(int gx=1;gx<NEON_MAZE_WIDTH-1;++gx){
        if(!seen[gy][gx])continue;
        int d=abs(gx-x)+abs(gy-y);
        if(d<best){best=d;bx=gx;by=gy;}
    }
    *px=(float)bx+0.5f;*py=(float)by+0.5f;
}

static void repair_layout(neon_maze_game_t *game)
{
    uint8_t seen[NEON_MAZE_HEIGHT][NEON_MAZE_WIDTH];
    flood_layout(seen,2,3);
    for(int i=0;i<NEON_MAZE_ENEMIES;++i)
        snap_reachable(&game->enemies[i].x,&game->enemies[i].y,seen);
    for(int i=0;i<NEON_MAZE_PICKUPS;++i)
        snap_reachable(&game->pickups[i].x,&game->pickups[i].y,seen);
    for(int i=0;i<NEON_MAZE_PROPS;++i)
        snap_reachable(&game->props[i].x,&game->props[i].y,seen);
    for(int i=0;i<NEON_MAZE_ENEMIES;++i){
        for(int j=i+1;j<NEON_MAZE_ENEMIES;++j){
            float dx=game->enemies[j].x-game->enemies[i].x;
            float dy=game->enemies[j].y-game->enemies[i].y;
            if(dx*dx+dy*dy>1.6f)continue;
            float nx=game->enemies[j].x+1.0f,ny=game->enemies[j].y+1.0f;
            int cx=(int)nx,cy=(int)game->enemies[j].y;
            if(layout_open(cx,cy)&&seen[cy][cx])
                game->enemies[j].x=(float)cx+0.5f;
            else if(layout_open((int)game->enemies[j].x,(int)ny)&&
                    seen[(int)ny][(int)game->enemies[j].x])
                game->enemies[j].y=(float)((int)ny)+0.5f;
        }
    }
}

static void place_layout(neon_maze_game_t *game,uint8_t layout)
{
    static const float enemy_xy[NEON_MAZE_LAYOUTS][NEON_MAZE_ENEMIES][2]={
        {{10.5f,6.5f},{16.5f,8.5f},{21.5f,9.5f},{4.5f,8.5f},{13.5f,12.5f},
         {21.5f,12.5f},{14.5f,15.5f},{21.5f,15.5f},{18.5f,18.5f},{20.5f,20.5f}},
        {{10.5f,5.5f},{16.5f,8.5f},{21.5f,8.5f},{4.5f,8.5f},{20.5f,11.5f},
         {13.5f,12.5f},{16.5f,15.5f},{13.5f,14.5f},{18.5f,18.5f},{19.5f,21.5f}},
        {{10.5f,6.5f},{13.5f,9.5f},{18.5f,8.5f},{1.5f,8.5f},{14.5f,12.5f},
         {21.5f,12.5f},{16.5f,14.5f},{21.5f,15.5f},{17.5f,18.5f},{20.5f,21.5f}}};
    static const float pickup_xy[NEON_MAZE_LAYOUTS][NEON_MAZE_PICKUPS][2]={
        {{9.5f,5.5f},{5.5f,8.5f},{19.5f,8.5f},{14.5f,12.5f},{16.5f,15.5f},{18.5f,20.5f}},
        {{10.5f,6.5f},{3.5f,8.5f},{20.5f,8.5f},{14.5f,12.5f},{20.5f,15.5f},{18.5f,21.5f}},
        {{9.5f,5.5f},{1.5f,8.5f},{17.5f,9.5f},{20.5f,12.5f},{14.5f,15.5f},{22.5f,20.5f}}};
    static const neon_maze_pickup_kind_t pickup_kind[NEON_MAZE_PICKUPS]={
        NEON_PICKUP_AMMO,NEON_PICKUP_HEALTH,NEON_PICKUP_AMMO,
        NEON_PICKUP_AMMO,NEON_PICKUP_AMMO,NEON_PICKUP_HEALTH};
    if(layout>=NEON_MAZE_LAYOUTS)layout=0;
    game->layout=layout;
    for(int i=0;i<NEON_MAZE_ENEMIES;++i){
        game->enemies[i].x=enemy_xy[layout][i][0];
        game->enemies[i].y=enemy_xy[layout][i][1];
        game->enemies[i].hp=2;
        game->enemies[i].move_phase=(uint8_t)(i*37U+layout*19U);
        game->enemies[i].last_seen_x=game->enemies[i].x;
        game->enemies[i].last_seen_y=game->enemies[i].y;
        game->enemies[i].hold_x=(int8_t)game->enemies[i].x;
        game->enemies[i].hold_y=(int8_t)game->enemies[i].y;
        game->enemies[i].active=true;
    }
    game->enemies[0].ai_state=NEON_ENEMY_ENGAGE;
    game->enemies[0].aim_timer=16;
    for(int i=0;i<NEON_MAZE_PICKUPS;++i){
        game->pickups[i].x=pickup_xy[layout][i][0];
        game->pickups[i].y=pickup_xy[layout][i][1];
        game->pickups[i].kind=pickup_kind[i];
        game->pickups[i].taken=false;
    }
    static const float prop_xy[NEON_MAZE_PROPS][2]={
        {1.5f,1.5f},{5.5f,3.5f},{15.5f,8.5f},{20.5f,8.5f},
        {13.5f,11.5f},{21.5f,14.5f},{17.5f,19.5f},{19.5f,22.5f}};
    for(int i=0;i<NEON_MAZE_PROPS;++i){
        game->props[i].x=prop_xy[i][0];
        game->props[i].y=prop_xy[i][1];
        game->props[i].kind=(uint8_t)(i&1);
    }
    repair_layout(game);
}

void neon_maze_reset(neon_maze_game_t *game)
{
    if(!game)return;
    uint32_t best=game->best_ticks;
    uint8_t layout=game->layout;
    memset(game,0,sizeof(*game));
    game->best_ticks=best;
    game->x=2.5f;game->y=3.5f;game->angle=.28f;
    game->hp=NEON_MAZE_MAX_HP;
    game->ammo=NEON_MAZE_AMMO_START;
    game->display_hp=(float)NEON_MAZE_MAX_HP;
    game->phase=NEON_MAZE_PHASE_START;
    place_layout(game,layout);
    game->explored[3][2]=1;
    game->perf_logic_fps=game->perf_display_fps=30.0f;
}

void neon_maze_set_best(neon_maze_game_t *game,uint32_t ticks)
{ if(game)game->best_ticks=ticks; }

void neon_maze_confirm(neon_maze_game_t *game)
{
    if(!game)return;
    if(game->phase==NEON_MAZE_PHASE_PLAYING)return;
    if(game->phase!=NEON_MAZE_PHASE_START){
        game->layout=(uint8_t)((game->layout+1U)%NEON_MAZE_LAYOUTS);
        neon_maze_reset(game);
    }
    game->phase=NEON_MAZE_PHASE_PLAYING;
}

void neon_maze_set_actions(neon_maze_game_t *game,bool left,bool right,bool forward,
                           bool backward)
{ if(game){game->left=left;game->right=right;game->forward=forward;game->backward=backward;
    game->turn_input=(right?1.0f:0.0f)-(left?1.0f:0.0f);
    game->move_forward=(forward?1.0f:0.0f)-(backward?1.0f:0.0f);game->move_strafe=0;} }

void neon_maze_set_motion(neon_maze_game_t *game,float forward,float strafe,float turn)
{ if(game){game->move_forward=forward;game->move_strafe=strafe;game->turn_input=turn;} }

void neon_maze_set_sprint(neon_maze_game_t *game,bool sprint)
{ if(game)game->sprint_held=sprint; }

static void emit_sfx(neon_maze_game_t *game,uint8_t id)
{
    game->sfx=id;
    game->sfx_hold=5;
}

const char *neon_maze_sfx_name(const neon_maze_game_t *game)
{
    static const char *names[]={"","rifle","impact","empty","confirm","alert",
                                "pickup","step_l","step_r","hurt"};
    uint8_t id=game?game->sfx:0;
    if(id>=sizeof(names)/sizeof(names[0]))id=0;
    return names[id];
}

void neon_maze_set_fire_held(neon_maze_game_t *game,bool held)
{
    if(!game)return;
    if(held&&!game->fire_held)game->fire_pressed=true;
    game->fire_held=held;
}

void neon_maze_turn(neon_maze_game_t *game,float radians)
{ if(game){game->angle+=radians;while(game->angle<0)game->angle+=6.2831853f;
    while(game->angle>=6.2831853f)game->angle-=6.2831853f;} }

void neon_maze_look(neon_maze_game_t *game,float pixels)
{
    if(!game)return;
    game->look_pitch+=pixels;
    if(game->look_pitch>22.0f)game->look_pitch=22.0f;
    if(game->look_pitch< -22.0f)game->look_pitch=-22.0f;
}

void neon_maze_settle_look(neon_maze_game_t *game)
{
    if(!game)return;
    game->look_pitch*=.82f;
    if(fabsf(game->look_pitch)<.4f)game->look_pitch=0;
}

void neon_maze_set_performance(neon_maze_game_t *game,float logic_fps,
                               float display_fps,float render_ms)
{ if(game){game->perf_logic_fps=logic_fps;game->perf_display_fps=display_fps;
    game->perf_render_ms=render_ms;} }

static void collect_pickups(neon_maze_game_t *game)
{
    for(int i=0;i<NEON_MAZE_PICKUPS;++i){
        neon_maze_pickup_t *item=&game->pickups[i];
        if(item->taken)continue;
        float dx=item->x-game->x,dy=item->y-game->y;
        if(dx*dx+dy*dy>.42f)continue;
        if(item->kind==NEON_PICKUP_HEALTH&&game->hp>=NEON_MAZE_MAX_HP)continue;
        if(item->kind==NEON_PICKUP_AMMO&&game->ammo>=NEON_MAZE_AMMO_MAX)continue;
        item->taken=true;
        game->last_pickup=true;
        game->pickup_flash=14;
        game->score+=20;
        if(item->kind==NEON_PICKUP_AMMO){
            game->ammo=(uint8_t)(game->ammo+6);
            if(game->ammo>NEON_MAZE_AMMO_MAX)game->ammo=NEON_MAZE_AMMO_MAX;
        }else if(game->hp<NEON_MAZE_MAX_HP){
            ++game->hp;
            game->display_hp=(float)game->hp;
        }
    }
}

void neon_maze_update(neon_maze_game_t *game)
{
    if(!game||game->phase!=NEON_MAZE_PHASE_PLAYING)return;
    const float turn=.085f;
    game->last_fire=NEON_FIRE_NONE;
    game->last_pickup=false;
    game->last_alert=false;
    game->sprinting=false;
    if(game->sfx_hold){if(!--game->sfx_hold)game->sfx=0;}
    if(game->fire_cooldown)--game->fire_cooldown;
    if(game->enemy_shot_lock)--game->enemy_shot_lock;
    if(game->fire_pressed)game->last_fire=neon_maze_fire(game);
    game->fire_pressed=false;
    if(game->last_fire==NEON_FIRE_DRY)emit_sfx(game,3);
    else if(game->last_fire==NEON_FIRE_DOOR)emit_sfx(game,4);
    else if(game->last_fire==NEON_FIRE_SHOT||game->last_fire==NEON_FIRE_HIT||
            game->last_fire==NEON_FIRE_KILL)emit_sfx(game,1);
    game->angle+=turn*game->turn_input;
    if(game->angle<0)game->angle+=6.2831853f;
    if(game->angle>=6.2831853f)game->angle-=6.2831853f;
    game->look_kick*=.68f;
    if(fabsf(game->look_kick)<.25f)game->look_kick=0;
    float forward=game->move_forward,strafe=game->move_strafe;
    float length=sqrtf(forward*forward+strafe*strafe);
    if(length>1.0f){forward/=length;strafe/=length;length=1.0f;}
    game->sprinting=(game->sprint_held&&forward>0.12f)||
                    (length>=NEON_MAZE_SPRINT&&forward>0.12f);
    if(game->sprint_held&&forward>0.12f&&length>0.2f&&length<1.0f){
        forward/=length;strafe/=length;length=1.0f;
    }
    float speed=game->sprinting?0.135f:(length>0.01f?0.055f+0.040f*length:0.0f);
    float wish_x=0,wish_y=0;
    if(length>.01f){
        wish_x=(cosf(game->angle)*forward-sinf(game->angle)*strafe)*speed;
        wish_y=(sinf(game->angle)*forward+cosf(game->angle)*strafe)*speed;
        game->move_phase+=game->sprinting?.62f:.44f*length;
        uint8_t beat=(uint8_t)(game->move_phase);
        if(beat!=game->step_beat){
            game->step_beat=beat;
            if(!game->sfx)emit_sfx(game,(uint8_t)(7U+(beat&1U)));
        }
    }
    if(game->sprinting)game->look_kick+=sinf(game->move_phase)*2.4f;
    game->vel_x=game->vel_x*.62f+wish_x*.38f;
    game->vel_y=game->vel_y*.62f+wish_y*.38f;
    if(fabsf(game->vel_x)<.002f)game->vel_x=0;
    if(fabsf(game->vel_y)<.002f)game->vel_y=0;
    float nx=game->x+game->vel_x,ny=game->y+game->vel_y;
    if(!blocked_at(game,nx,game->y))game->x=nx;
    else{
        game->vel_x*=.18f;
        if(!blocked_at(game,nx,game->y+.20f)){game->x=nx;game->y+=.10f;}
        else if(!blocked_at(game,nx,game->y-.20f)){game->x=nx;game->y-=.10f;}
    }
    if(!blocked_at(game,game->x,ny))game->y=ny;
    else{
        game->vel_y*=.18f;
        if(!blocked_at(game,game->x+.20f,ny)){game->y=ny;game->x+=.10f;}
        else if(!blocked_at(game,game->x-.20f,ny)){game->y=ny;game->x-=.10f;}
    }
    game->weapon_recoil*=.64f;
    if(game->weapon_recoil<.01f)game->weapon_recoil=0;
    if(game->hit_flash<=4&&game->display_hp>(float)game->hp){
        game->display_hp+=((float)game->hp-game->display_hp)*.22f;
        if(game->display_hp<(float)game->hp+.02f)game->display_hp=(float)game->hp;
    }
    mark_explored(game);
    collect_pickups(game);
    int last_enemy=neon_maze_last_enemy_index(game);
    for(int i=0;i<NEON_MAZE_ENEMIES;++i){
        neon_maze_enemy_t *enemy=&game->enemies[i];
        if(enemy->death_timer)--enemy->death_timer;
        if(!enemy->active)continue;
        if(enemy->hit_flash)--enemy->hit_flash;
        if(enemy->attack_flash)--enemy->attack_flash;
        if(enemy->attack_cooldown)--enemy->attack_cooldown;
        /* Bring the final hostile to the player instead of requiring a sweep
         * through every previously explored dead end. */
        if(i==last_enemy&&enemy->ai_state!=NEON_ENEMY_ENGAGE&&
           enemy->ai_state!=NEON_ENEMY_ALERT){
            enemy->ai_state=NEON_ENEMY_SEARCH;
            enemy->last_seen_x=game->x;
            enemy->last_seen_y=game->y;
            enemy->search_timer=151;
        }
        float to_x=game->x-enemy->x,to_y=game->y-enemy->y;
        float distance=sqrtf(to_x*to_x+to_y*to_y),heading=0;
        bool sees_player=distance<6.8f&&line_clear(game,enemy->x,enemy->y,game->x,game->y);
        if(sees_player){
            enemy->last_seen_x=game->x;enemy->last_seen_y=game->y;enemy->search_timer=150;
            if(enemy->ai_state==NEON_ENEMY_PATROL||
               (enemy->ai_state==NEON_ENEMY_SEARCH&&distance<3.8f)){
                enemy->ai_state=NEON_ENEMY_ALERT;enemy->alert_timer=NEON_MAZE_ALERT_TICKS;
                game->last_alert=true;
            }
        }else if((enemy->ai_state==NEON_ENEMY_ENGAGE||enemy->ai_state==NEON_ENEMY_ALERT)
                 &&enemy->search_timer){
            enemy->ai_state=NEON_ENEMY_SEARCH;enemy->aim_timer=0;
        }
        if(enemy->ai_state==NEON_ENEMY_ALERT){
            if(enemy->alert_timer)--enemy->alert_timer;
            else enemy->ai_state=NEON_ENEMY_ENGAGE;
        }else if(enemy->ai_state==NEON_ENEMY_SEARCH){
            if(enemy->search_timer)--enemy->search_timer;else enemy->ai_state=NEON_ENEMY_PATROL;
        }
        if(enemy->ai_state==NEON_ENEMY_ENGAGE&&sees_player&&distance>.9f&&distance<6.2f){
            if(!enemy->attack_cooldown&&!game->enemy_shot_lock){
                if(!enemy->aim_timer)enemy->aim_timer=NEON_MAZE_AIM_TICKS;
                else if(!--enemy->aim_timer){
                    enemy->attack_flash=4;enemy->attack_cooldown=NEON_MAZE_ATTACK_COOLDOWN;
                    game->enemy_shot_lock=12;
                    hurt_player(game,enemy);
                    if(game->phase==NEON_MAZE_PHASE_DEAD)return;
                }
            }
        }else enemy->aim_timer=0;
        if(distance<.40f&&!game->enemy_shot_lock){
            hurt_player(game,enemy);
            game->enemy_shot_lock=12;
            if(game->phase==NEON_MAZE_PHASE_DEAD)return;
        }
        if(enemy->ai_state==NEON_ENEMY_ENGAGE&&sees_player){
            if(((game->tick+(uint32_t)i)%12U)==0U)
                pick_hold_cell(game,i,enemy);
            bool at_hold=fabsf(enemy->x-((float)enemy->hold_x+0.5f))<.35f&&
                         fabsf(enemy->y-((float)enemy->hold_y+0.5f))<.35f;
            if(distance<2.0f){
                heading=atan2f(-to_y,-to_x);
            }else if(distance>4.6f){
                heading=atan2f(to_y,to_x);
            }else if(!at_hold){
                if(((game->tick+(uint32_t)i)%8U)==0U||(!enemy->nav_dx&&!enemy->nav_dy))
                    route_next(game,(int)enemy->x,(int)enemy->y,enemy->hold_x,
                               enemy->hold_y,&enemy->nav_dx,&enemy->nav_dy);
                if(!enemy->nav_dx&&!enemy->nav_dy)continue;
                heading=atan2f((float)enemy->nav_dy,(float)enemy->nav_dx);
            }else continue;
        }else{
            bool should_move=enemy->ai_state==NEON_ENEMY_PATROL||
                             enemy->ai_state==NEON_ENEMY_SEARCH;
            if(!should_move)continue;
            if(enemy->ai_state==NEON_ENEMY_PATROL)
                heading=((float)((game->tick/75U+enemy->move_phase)%16U))*0.3926991f;
            else{
                if(((game->tick+(uint32_t)i)%8U)==0U||(!enemy->nav_dx&&!enemy->nav_dy))
                    route_next(game,(int)enemy->x,(int)enemy->y,(int)enemy->last_seen_x,
                               (int)enemy->last_seen_y,&enemy->nav_dx,&enemy->nav_dy);
                if(!enemy->nav_dx&&!enemy->nav_dy)continue;
                heading=atan2f((float)enemy->nav_dy,(float)enemy->nav_dx);
            }
        }
        for(int j=0;j<NEON_MAZE_ENEMIES;++j){
            if(j==i||!game->enemies[j].active)continue;
            float ox=enemy->x-game->enemies[j].x,oy=enemy->y-game->enemies[j].y;
            if(ox*ox+oy*oy<1.6f)heading+=1.2f;
        }
        float step=enemy->ai_state==NEON_ENEMY_ENGAGE?.028f:.022f;
        float dx=cosf(heading)*step,dy=sinf(heading)*step;
        float nx=enemy->x+dx,ny=enemy->y+dy;
        if(!blocked_at(game,nx,enemy->y))enemy->x=nx;
        else enemy->move_phase=(uint8_t)(enemy->move_phase+5U);
        if(!blocked_at(game,enemy->x,ny))enemy->y=ny;
        else enemy->move_phase=(uint8_t)(enemy->move_phase+7U);
    }
    if(!game->sfx&&game->last_pickup)emit_sfx(game,6);
    if(!game->sfx&&game->last_alert)emit_sfx(game,5);
    if(neon_maze_enemies_alive(game)==0 &&
       neon_maze_cell((int)game->x,(int)game->y)==5){
        game->cells_reached=1;
        game->phase=NEON_MAZE_PHASE_WON;
        if(!game->best_ticks||game->tick<game->best_ticks){
            game->best_ticks=game->tick;
            game->best_updated=true;
        }
    }
    if(game->hit_flash)--game->hit_flash;
    if(game->hit_marker)--game->hit_marker;
    if(game->kill_flash)--game->kill_flash;
    if(game->hurt_cooldown)--game->hurt_cooldown;
    if(game->pickup_flash)--game->pickup_flash;
    if(game->door_flash)--game->door_flash;
    if(game->dry_flash)--game->dry_flash;
    ++game->tick;
}

static float angle_delta(float value)
{
    while(value>3.1415927f)value-=6.2831853f;
    while(value<-3.1415927f)value+=6.2831853f;
    return value;
}

neon_maze_fire_result_t neon_maze_fire(neon_maze_game_t *game)
{
    if(!game)return NEON_FIRE_NONE;
    if(game->phase!=NEON_MAZE_PHASE_PLAYING){neon_maze_confirm(game);return NEON_FIRE_NONE;}
    if(game->fire_cooldown)return NEON_FIRE_NONE;
    int door_x=0,door_y=0;
    if(door_cell_ahead(game,&door_x,&door_y)){
        open_door_cluster(game,door_x,door_y);
        game->fire_cooldown=NEON_MAZE_DOOR_COOLDOWN;
        return NEON_FIRE_DOOR;
    }
    if(!game->ammo){
        game->fire_cooldown=NEON_MAZE_DRY_COOLDOWN;
        game->dry_flash=10;
        return NEON_FIRE_DRY;
    }
    --game->ammo;
    ++game->shots_fired;
    game->fire_cooldown=NEON_MAZE_FIRE_COOLDOWN;
    game->weapon_recoil=1.0f;
    game->look_kick-=7.0f;
    int target=-1;float best=1000.0f;
    for(int i=0;i<NEON_MAZE_ENEMIES;++i){
        neon_maze_enemy_t *enemy=&game->enemies[i];if(!enemy->active)continue;
        float dx=enemy->x-game->x,dy=enemy->y-game->y;
        float distance=sqrtf(dx*dx+dy*dy);
        float delta=fabsf(angle_delta(atan2f(dy,dx)-game->angle));
        float slop=game->sprinting?0.10f:0.0f;
        if(delta>.08f+slop+(.16f/distance)||distance>=best)continue;
        if(!line_clear(game,game->x,game->y,enemy->x,enemy->y))continue;
        best=distance;target=i;
    }
    if(target<0)return NEON_FIRE_SHOT;
    neon_maze_enemy_t *enemy=&game->enemies[target];
    if(enemy->hp)--enemy->hp;
    enemy->hit_flash=12;
    float knock_x=enemy->x-game->x,knock_y=enemy->y-game->y;
    float knock_length=sqrtf(knock_x*knock_x+knock_y*knock_y);
    if(knock_length>.01f){
        float nx=enemy->x+knock_x/knock_length*.22f;
        float ny=enemy->y+knock_y/knock_length*.22f;
        if(!blocked_at(game,nx,enemy->y))enemy->x=nx;
        if(!blocked_at(game,enemy->x,ny))enemy->y=ny;
    }
    game->hit_marker=8;
    ++game->shots_hit;
    game->score+=25;
    if(!enemy->hp){enemy->active=false;enemy->death_timer=20;game->kill_flash=18;
        ++game->kills;game->score+=100;return NEON_FIRE_KILL;}
    return NEON_FIRE_HIT;
}

uint32_t neon_maze_state_hash(const neon_maze_game_t *game)
{
    if(!game)return 0;
    uint32_t hash=2166136261U, values[]={
        (uint32_t)(game->x*4096),(uint32_t)(game->y*4096),
        (uint32_t)(game->angle*4096),(uint32_t)((game->look_pitch+64.0f)*256.0f),
        game->tick,game->cells_reached,game->score,
        (uint32_t)game->phase,(uint32_t)game->hp,(uint32_t)game->ammo,
        (uint32_t)game->layout};
    for(unsigned i=0;i<sizeof(values)/sizeof(values[0]);++i){hash^=values[i];hash*=16777619U;}
    for(int i=0;i<NEON_MAZE_ENEMIES;++i){hash^=(uint32_t)game->enemies[i].hp;
        hash*=16777619U;}
    for(int i=0;i<NEON_MAZE_PICKUPS;++i){hash^=game->pickups[i].taken;hash*=16777619U;}
    for(int y=0;y<NEON_MAZE_HEIGHT;++y)for(int x=0;x<NEON_MAZE_WIDTH;++x){
        hash^=game->door_open[y][x];hash*=16777619U;}
    return hash;
}
