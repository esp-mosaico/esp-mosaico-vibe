// SPDX-License-Identifier: Apache-2.0
#include "last_zone_game.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static const char s_maps[LAST_ZONE_LAYOUTS][LAST_ZONE_HEIGHT][LAST_ZONE_WIDTH + 1] = {
{
    "111111111111111111111111",
    "100000010000000000000001",
    "100000020000000000000001",
    "100000020000000000000001",
    "100000010000100000000001",
    "100030010000100000000001",
    "100000010000100000000001",
    "111111001111100000000001",
    "100000000030000000000001",
    "100000000000200000000001",
    "111111111111144111111111",
    "100000000000000001000001",
    "100000000000000001000001",
    "100000000000000301000001",
    "100000000000000000000001",
    "100000000000000001000001",
    "100000000000000001003001",
    "100000000000000001000001",
    "100000000000000001110011",
    "100000000000000000000001",
    "100000000000000000005501",
    "100000000000000000000001",
    "100000000000000000000001",
    "111111111111111111111111",
},
{
    "111111111111111111111111",
    "100000001000000000000001",
    "100000001000000000005501",
    "100000001000000000000001",
    "100000002003000000300001",
    "100030002000000000000001",
    "100000001000000000000001",
    "100000001000000000000001",
    "100000001111114411111111",
    "100000000000000100000001",
    "100000000000000100000001",
    "100030000003000100000001",
    "100000000000000100030001",
    "100000000000000000000001",
    "100000000000000100000001",
    "111111114411111100000001",
    "100000000000000100000001",
    "100000000000000100000001",
    "100000000000000100000001",
    "100030000003000000030001",
    "100000000000000100000001",
    "100000000000000100000001",
    "100000000000000100000001",
    "111111111111111111111111",
},
{
    "111111111111111111111111",
    "100000010000000010000001",
    "100000010000000010000001",
    "100000010000000010000001",
    "100000020000000020000001",
    "100000020030030020000001",
    "100000010000000010000001",
    "100000010000000010000001",
    "100000040040040040000001",
    "100000010000000010000001",
    "100000010000000010000001",
    "100000020000000020005501",
    "100000020030030020000001",
    "100000010000000010000001",
    "100000010000000010000001",
    "100030010000000010030001",
    "100000010000000010000001",
    "100000040004400040000001",
    "100000010000000010000001",
    "100000010000000010000001",
    "111101111111111111101111",
    "100000010000000010000001",
    "100000010000000010000001",
    "111111111111111111111111",
},
{
    "111111111111111111111111",
    "100000000000000000000001",
    "100000000000000000000001",
    "100000000003000000000001",
    "100000000000000000000001",
    "101100111111111110011101",
    "100000100000000000000001",
    "100000000000000000000001",
    "100000000000300000000001",
    "100000100000000000000001",
    "101111110011111111100101",
    "100000000000000010000001",
    "100030000000000000000001",
    "100000000000000000000001",
    "100000000000000010000001",
    "101001111111110011111101",
    "100000010000000000000001",
    "100000000000000000030001",
    "100000000000000000000001",
    "111111111111114411111111",
    "100000000000000000000001",
    "100000000000000000005501",
    "100000000000000000000001",
    "111111111111111111111111",
},
{
    "111111111111111111111111",
    "100000000000000000100001",
    "100000000000000000105501",
    "100000000000000000400001",
    "100000000000000000200001",
    "100000000000000000100001",
    "111122111114411111111111",
    "100000000000000000130001",
    "100000000000000000000001",
    "100000030000000030000001",
    "100000000000000000100001",
    "100000000000030000100001",
    "100000000000000000000001",
    "100000000030000000000001",
    "100001111001111111100111",
    "100000000000000000000001",
    "100000030000000000000001",
    "100000000000000000000001",
    "100030000000003000003001",
    "100000000000000000000001",
    "100000000000000000000001",
    "100000000000000000000001",
    "100000000000000000000001",
    "111111111111111111111111",
}
};

static float angle_delta(float value);
static void emit_sfx(last_zone_game_t *game,uint8_t id);

typedef struct {
    float spawn_x,spawn_y,spawn_angle;
    float extract_x,extract_y;
    uint8_t enemy_total,start_ammo,start_armor;
    uint16_t elite_mask;
    const char *briefing;
} last_zone_layout_info_t;

static const last_zone_layout_info_t s_layouts[LAST_ZONE_LAYOUTS]={
    {2.5f,3.5f,0.00f,20.5f,20.5f,5,20,2,0x000,"OBSERVE. CLEAR. OPEN THE GATE."},
    {2.5f,20.5f,-1.05f,20.5f,2.5f,7,18,2,0x000,"CONTROL THE ROOM. USE THE BARRELS."},
    {2.5f,11.5f,0.00f,20.5f,11.5f,8,17,1,0x0c0,"COUNT THEM. CHOOSE A FLANK."},
    {2.5f,2.5f,0.78f,20.5f,21.5f,8,17,1,0x000,"WALK QUIET. STRIKE FIRST."},
    {2.5f,21.5f,-0.78f,20.5f,2.5f,9,16,0,0x1c0,"THE PAD IS AHEAD. EARN THE EXIT."},
};

static const last_zone_layout_info_t *layout_info(const last_zone_game_t *game)
{
    unsigned layout=game&&game->layout<LAST_ZONE_LAYOUTS?game->layout:0;
    return &s_layouts[layout];
}

static bool line_clear(const last_zone_game_t *game,float x0,float y0,float x1,float y1)
{
    float dx=x1-x0,dy=y1-y0,distance=sqrtf(dx*dx+dy*dy);
    if(distance<.01f)return true;
    int steps=(int)(distance/.14f);
    for(int i=1;i<steps;++i)
        if(last_zone_blocks(game,(int)(x0+dx*(float)i/steps),(int)(y0+dy*(float)i/steps)))
            return false;
    return true;
}

static bool layout_open(const last_zone_game_t *game,int x,int y)
{
    uint8_t cell=last_zone_cell(game,x,y);
    return cell==0||cell==4||cell==5;
}

static bool adjacent_cover(const last_zone_game_t *game,int x,int y)
{
    static const int8_t dirs[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
    for(int i=0;i<4;++i){
        uint8_t cell=last_zone_cell(game,x+dirs[i][0],y+dirs[i][1]);
        if(cell>=1&&cell<=3)return true;
    }
    return false;
}

static bool ally_at(const last_zone_game_t *game,int ignore,float x,float y)
{
    for(int i=0;i<LAST_ZONE_ENEMIES;++i){
        if(i==ignore||!game->enemies[i].active)continue;
        float dx=game->enemies[i].x-x,dy=game->enemies[i].y-y;
        if(dx*dx+dy*dy<.72f)return true;
    }
    return false;
}

static void pick_hold_cell(last_zone_game_t *game,int index,last_zone_enemy_t *enemy)
{
    int ex=(int)enemy->x,ey=(int)enemy->y,best_x=ex,best_y=ey;
    float best=1e9f;
    for(int dy=-2;dy<=2;++dy)for(int dx=-2;dx<=2;++dx){
        int nx=ex+dx,ny=ey+dy;
        if(!layout_open(game,nx,ny)||last_zone_blocks(game,nx,ny))continue;
        float cx=(float)nx+0.5f,cy=(float)ny+0.5f;
        if(ally_at(game,index,cx,cy))continue;
        if(!line_clear(game,cx,cy,game->x,game->y))continue;
        float px=cx-game->x,py=cy-game->y,dist=sqrtf(px*px+py*py);
        if(dist<2.2f||dist>5.4f)continue;
        float score=fabsf(dist-3.4f);
        if(adjacent_cover(game,nx,ny))score-=1.5f;
        if(score<best){best=score;best_x=nx;best_y=ny;}
    }
    enemy->hold_x=(int8_t)best_x;
    enemy->hold_y=(int8_t)best_y;
}

static bool blocked_at(const last_zone_game_t *game,float x,float y)
{
    const float r=.10f;
    return last_zone_blocks(game,(int)(x-r),(int)y)||last_zone_blocks(game,(int)(x+r),(int)y)||
           last_zone_blocks(game,(int)x,(int)(y-r))||last_zone_blocks(game,(int)x,(int)(y+r));
}

static void route_next(const last_zone_game_t *game,int sx,int sy,int gx,int gy,
                       int8_t *out_dx,int8_t *out_dy)
{
    int16_t distance[LAST_ZONE_HEIGHT][LAST_ZONE_WIDTH];
    uint16_t queue[LAST_ZONE_WIDTH*LAST_ZONE_HEIGHT];
    static const int8_t dirs[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
    *out_dx=*out_dy=0;
    if(last_zone_blocks(game,sx,sy)||last_zone_blocks(game,gx,gy))return;
    for(int y=0;y<LAST_ZONE_HEIGHT;++y)for(int x=0;x<LAST_ZONE_WIDTH;++x)
        distance[y][x]=-1;
    unsigned head=0,tail=0;
    distance[gy][gx]=0;queue[tail++]=(uint16_t)(gy*LAST_ZONE_WIDTH+gx);
    while(head<tail){
        unsigned cell=queue[head++];int x=(int)(cell%LAST_ZONE_WIDTH),y=(int)(cell/LAST_ZONE_WIDTH);
        if(x==sx&&y==sy)break;
        for(int i=0;i<4;++i){int nx=x+dirs[i][0],ny=y+dirs[i][1];
            if(nx<0||ny<0||nx>=LAST_ZONE_WIDTH||ny>=LAST_ZONE_HEIGHT||
               distance[ny][nx]>=0||last_zone_blocks(game,nx,ny))continue;
            distance[ny][nx]=(int16_t)(distance[y][x]+1);
            queue[tail++]=(uint16_t)(ny*LAST_ZONE_WIDTH+nx);
        }
    }
    int best=distance[sy][sx];
    if(best<=0)return;
    for(int i=0;i<4;++i){int nx=sx+dirs[i][0],ny=sy+dirs[i][1];
        if(nx>=0&&ny>=0&&nx<LAST_ZONE_WIDTH&&ny<LAST_ZONE_HEIGHT&&
           distance[ny][nx]>=0&&distance[ny][nx]<best){
            best=distance[ny][nx];*out_dx=dirs[i][0];*out_dy=dirs[i][1];
        }
    }
}

static void hurt_player(last_zone_game_t *game,const last_zone_enemy_t *enemy)
{
    if(game->hurt_cooldown)return;
    if(game->armor){--game->armor;game->armor_hit=true;}
    else if(game->hp)--game->hp;
    if(game->damage_taken<255)++game->damage_taken;
    game->hurt_cooldown=40;game->hit_flash=8;
    emit_sfx(game,9);
    game->damage_angle=angle_delta(atan2f(enemy->y-game->y,enemy->x-game->x)-game->angle);
    if(!game->hp)game->phase=LAST_ZONE_PHASE_DEAD;
}

static void mark_explored(last_zone_game_t *game)
{
    int px=(int)game->x,py=(int)game->y;
    static const int8_t dirs[5][2]={{0,0},{1,0},{-1,0},{0,1},{0,-1}};
    for(int i=0;i<5;++i){
        int x=px+dirs[i][0],y=py+dirs[i][1];
        if(x>=0&&y>=0&&x<LAST_ZONE_WIDTH&&y<LAST_ZONE_HEIGHT)game->explored[y][x]=1;
    }
}

static void open_door_cluster(last_zone_game_t *game,int x,int y)
{
    for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){
        int nx=x+dx,ny=y+dy;
        if(last_zone_cell(game,nx,ny)==4&&!game->door_open[ny][nx]){
            game->door_open[ny][nx]=1;
            game->score+=15;
            game->door_flash=12;
        }
    }
}

static bool door_cell_ahead(const last_zone_game_t *game,int *out_x,int *out_y)
{
    float fx=cosf(game->angle),fy=sinf(game->angle);
    for(float ray=.28f;ray<1.45f;ray+=.12f){
        int x=(int)(game->x+fx*ray),y=(int)(game->y+fy*ray);
        if(last_zone_cell(game,x,y)==4&&!game->door_open[y][x]){
            if(out_x)*out_x=x;
            if(out_y)*out_y=y;
            return true;
        }
        if(last_zone_blocks(game,x,y))return false;
    }
    return false;
}

uint8_t last_zone_cell(const last_zone_game_t *game,int x,int y)
{
    if(x<0||y<0||x>=LAST_ZONE_WIDTH||y>=LAST_ZONE_HEIGHT)return 1;
    unsigned level=game&&game->layout<LAST_ZONE_LAYOUTS?game->layout:0;
    return (uint8_t)(s_maps[level][y][x]-'0');
}

bool last_zone_blocks(const last_zone_game_t *game,int x,int y)
{
    uint8_t cell=last_zone_cell(game,x,y);
    if(cell==0||cell==5)return false;
    if(cell==4)return !game||!game->door_open[y][x];
    return true;
}

bool last_zone_door_ahead(const last_zone_game_t *game)
{
    return game&&door_cell_ahead(game,NULL,NULL);
}

bool last_zone_near_closed_door(const last_zone_game_t *game)
{
    if(!game)return false;
    int px=(int)game->x,py=(int)game->y;
    for(int dy=-2;dy<=2;++dy)for(int dx=-2;dx<=2;++dx){
        int x=px+dx,y=py+dy;
        if(last_zone_cell(game,x,y)==4&&!game->door_open[y][x])return true;
    }
    return false;
}

bool last_zone_pickup_visible(const last_zone_game_t *game,int index)
{
    if(!game||index<0||index>=LAST_ZONE_PICKUPS||game->pickups[index].taken)return false;
    int x=(int)game->pickups[index].x,y=(int)game->pickups[index].y;
    if(x<0||y<0||x>=LAST_ZONE_WIDTH||y>=LAST_ZONE_HEIGHT||!game->explored[y][x])return false;
    float dx=game->pickups[index].x-game->x,dy=game->pickups[index].y-game->y;
    return dx*dx+dy*dy<64.0f;
}

bool last_zone_enemy_on_radar(const last_zone_game_t *game,int index)
{
    if(!game||index<0||index>=LAST_ZONE_ENEMIES||!game->enemies[index].active)return false;
    const last_zone_enemy_t *enemy=&game->enemies[index];
    float dx=enemy->x-game->x,dy=enemy->y-game->y,dist=sqrtf(dx*dx+dy*dy);
    if(dist>5.2f)return false;
    if(enemy->ai_state!=LAST_ZONE_ENEMY_PATROL)return true;
    return line_clear(game,game->x,game->y,enemy->x,enemy->y);
}

int last_zone_enemies_alive(const last_zone_game_t *game)
{
    if(!game)return 0;
    int alive=0;
    for(int i=0;i<LAST_ZONE_ENEMIES;++i)if(game->enemies[i].active)++alive;
    return alive;
}

int last_zone_enemy_total(const last_zone_game_t *game)
{
    if(!game)return 0;
    return layout_info(game)->enemy_total;
}

float last_zone_extract_x(const last_zone_game_t *game){return layout_info(game)->extract_x;}
float last_zone_extract_y(const last_zone_game_t *game){return layout_info(game)->extract_y;}
float last_zone_spawn_x(const last_zone_game_t *game){return layout_info(game)->spawn_x;}
float last_zone_spawn_y(const last_zone_game_t *game){return layout_info(game)->spawn_y;}
const char *last_zone_briefing(const last_zone_game_t *game){return layout_info(game)->briefing;}

bool last_zone_on_extract(const last_zone_game_t *game)
{
    if(!game)return false;
    if(last_zone_cell(game,(int)game->x,(int)game->y)==5)return true;
    float dx=game->x-last_zone_extract_x(game),dy=game->y-last_zone_extract_y(game);
    return dx*dx+dy*dy<.36f;
}

int last_zone_last_enemy_index(const last_zone_game_t *game)
{
    if(!game||last_zone_enemies_alive(game)!=1)return -1;
    for(int i=0;i<LAST_ZONE_ENEMIES;++i)if(game->enemies[i].active)return i;
    return -1;
}

float last_zone_extract_bearing(const last_zone_game_t *game)
{
    if(!game)return 0;
    return angle_delta(atan2f(last_zone_extract_y(game)-game->y,
                             last_zone_extract_x(game)-game->x)-game->angle);
}

char last_zone_grade(const last_zone_game_t *game)
{
    if(!game||game->phase==LAST_ZONE_PHASE_DEAD)return 'D';
    unsigned seconds=game->tick/30U;
    bool mission_s=false;
    switch(game->layout){
        case 0: mission_s=game->shots_fired&&game->shots_hit*100U>=game->shots_fired*70U&&
                         game->damage_taken<=1&&seconds<=105U;break;
        case 1: mission_s=game->barrel_used&&game->damage_taken<=1&&seconds<=120U;break;
        case 2: mission_s=game->armor_hit&&game->damage_taken<=2&&seconds<=135U;break;
        case 3: mission_s=game->damage_taken==0&&seconds<=150U;break;
        case 4: mission_s=game->hp>=4&&seconds<=150U;break;
        default: break;
    }
    if(mission_s)return 'S';
    if(game->hp>=4&&seconds<=90U)return 'A';
    if(game->hp>=3&&seconds<=140U)return 'A';
    if(seconds<=200U)return 'B';
    return 'C';
}

bool last_zone_in_move_zone(int x,int y)
{
    int dx=x-LAST_ZONE_MOVE_X,dy=y-LAST_ZONE_MOVE_Y;
    return dx*dx+dy*dy<=LAST_ZONE_MOVE_R*LAST_ZONE_MOVE_R;
}

bool last_zone_in_move_capture(int x,int y)
{
    if(last_zone_in_move_zone(x,y))return true;
    if(x<0||x>=LAST_ZONE_LOOK_MIN_X||y<260)return false;
    return true;
}

bool last_zone_in_fire_zone(int x,int y)
{
    int dx=x-LAST_ZONE_FIRE_X,dy=y-LAST_ZONE_FIRE_Y;
    return dx*dx+dy*dy<=LAST_ZONE_FIRE_R*LAST_ZONE_FIRE_R;
}

bool last_zone_in_radar(const last_zone_game_t *game,int x,int y)
{
    if(!game)return false;
    return x>=game->radar_x-4&&x<game->radar_x+LAST_ZONE_RADAR_SIZE&&
           y>=game->radar_y-4&&y<game->radar_y+LAST_ZONE_RADAR_SIZE;
}

void last_zone_move_radar(last_zone_game_t *game,int x,int y)
{
    if(!game)return;
    int max_x=480-LAST_ZONE_RADAR_SIZE-4;
    int max_y=LAST_ZONE_MOVE_Y-LAST_ZONE_MOVE_R-LAST_ZONE_RADAR_SIZE-8;
    if(x<4)x=4;
    if(x>max_x)x=max_x;
    if(y<52)y=52;
    if(y>max_y)y=max_y;
    game->radar_x=(int16_t)x;game->radar_y=(int16_t)y;
}

static void flood_layout(const last_zone_game_t *game,
                         uint8_t seen[LAST_ZONE_HEIGHT][LAST_ZONE_WIDTH],int sx,int sy)
{
    uint16_t queue[LAST_ZONE_WIDTH*LAST_ZONE_HEIGHT];
    static const int8_t dirs[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
    unsigned head=0,tail=0;
    memset(seen,0,sizeof(uint8_t)*LAST_ZONE_WIDTH*LAST_ZONE_HEIGHT);
    if(!layout_open(game,sx,sy))return;
    seen[sy][sx]=1;queue[tail++]=(uint16_t)(sy*LAST_ZONE_WIDTH+sx);
    while(head<tail){
        unsigned cell=queue[head++];
        int x=(int)(cell%LAST_ZONE_WIDTH),y=(int)(cell/LAST_ZONE_WIDTH);
        for(int i=0;i<4;++i){
            int nx=x+dirs[i][0],ny=y+dirs[i][1];
            if(nx<0||ny<0||nx>=LAST_ZONE_WIDTH||ny>=LAST_ZONE_HEIGHT||seen[ny][nx])continue;
            if(!layout_open(game,nx,ny))continue;
            seen[ny][nx]=1;queue[tail++]=(uint16_t)(ny*LAST_ZONE_WIDTH+nx);
        }
    }
}

static void snap_reachable(float *px,float *py,const uint8_t seen[LAST_ZONE_HEIGHT][LAST_ZONE_WIDTH])
{
    int x=(int)*px,y=(int)*py;
    if(x>=0&&y>=0&&x<LAST_ZONE_WIDTH&&y<LAST_ZONE_HEIGHT&&seen[y][x])return;
    int best=999,bx=2,by=3;
    for(int gy=1;gy<LAST_ZONE_HEIGHT-1;++gy)for(int gx=1;gx<LAST_ZONE_WIDTH-1;++gx){
        if(!seen[gy][gx])continue;
        int d=abs(gx-x)+abs(gy-y);
        if(d<best){best=d;bx=gx;by=gy;}
    }
    *px=(float)bx+0.5f;*py=(float)by+0.5f;
}

static void repair_layout(last_zone_game_t *game)
{
    uint8_t seen[LAST_ZONE_HEIGHT][LAST_ZONE_WIDTH];
    flood_layout(game,seen,(int)last_zone_spawn_x(game),(int)last_zone_spawn_y(game));
    for(int i=0;i<LAST_ZONE_ENEMIES;++i)
        snap_reachable(&game->enemies[i].x,&game->enemies[i].y,seen);
    for(int i=0;i<LAST_ZONE_PICKUPS;++i)
        snap_reachable(&game->pickups[i].x,&game->pickups[i].y,seen);
    for(int i=0;i<LAST_ZONE_PROPS;++i)
        snap_reachable(&game->props[i].x,&game->props[i].y,seen);
    for(int i=0;i<LAST_ZONE_ENEMIES;++i){
        for(int j=i+1;j<LAST_ZONE_ENEMIES;++j){
            float dx=game->enemies[j].x-game->enemies[i].x;
            float dy=game->enemies[j].y-game->enemies[i].y;
            if(dx*dx+dy*dy>1.6f)continue;
            float nx=game->enemies[j].x+1.0f,ny=game->enemies[j].y+1.0f;
            int cx=(int)nx,cy=(int)game->enemies[j].y;
            if(layout_open(game,cx,cy)&&seen[cy][cx])
                game->enemies[j].x=(float)cx+0.5f;
            else if(layout_open(game,(int)game->enemies[j].x,(int)ny)&&
                    seen[(int)ny][(int)game->enemies[j].x])
                game->enemies[j].y=(float)((int)ny)+0.5f;
        }
    }
}

static void place_layout(last_zone_game_t *game,uint8_t layout)
{
    static const float enemy_xy[LAST_ZONE_LAYOUTS][LAST_ZONE_ENEMIES][2]={
        {{10.5f,8.5f},{15.5f,8.5f},{15.5f,13.5f},{20.5f,15.5f},{20.5f,19.5f},
         {3.5f,20.5f},{6.5f,20.5f},{9.5f,20.5f},{12.5f,20.5f},{15.5f,20.5f},
         {18.5f,20.5f},{21.5f,20.5f}},
        {{4.5f,11.5f},{11.5f,19.5f},{19.5f,19.5f},{11.5f,11.5f},{18.5f,12.5f},
         {10.5f,4.5f},{19.5f,4.5f},{3.5f,3.5f},{6.5f,3.5f},{12.5f,3.5f},
         {16.5f,3.5f},{21.5f,3.5f}},
        {{11.5f,2.5f},{12.5f,6.5f},{20.5f,5.5f},{20.5f,15.5f},{11.5f,12.5f},
         {13.5f,15.5f},{18.5f,15.5f},{20.5f,18.5f},{3.5f,3.5f},{4.5f,18.5f},
         {18.5f,3.5f},{21.5f,21.5f}},
        {{12.5f,3.5f},{18.5f,4.5f},{12.5f,8.5f},{4.5f,12.5f},{20.5f,13.5f},
         {4.5f,17.5f},{12.5f,18.5f},{20.5f,20.5f},{3.5f,21.5f},{6.5f,21.5f},
         {10.5f,21.5f},{16.5f,21.5f}},
        {{4.5f,11.5f},{10.5f,18.5f},{16.5f,18.5f},{19.5f,17.5f},{8.5f,12.5f},
         {14.5f,12.5f},{20.5f,12.5f},{14.5f,8.5f},{20.5f,7.5f},{4.5f,3.5f},
         {8.5f,3.5f},{12.5f,3.5f}}};
    static const float pickup_xy[LAST_ZONE_LAYOUTS][LAST_ZONE_PICKUPS][2]={
        {{5.5f,5.5f},{3.5f,9.5f},{10.5f,9.5f},{15.5f,12.5f},{20.5f,14.5f},{19.5f,21.5f}},
        {{4.5f,18.5f},{3.5f,13.5f},{11.5f,18.5f},{18.5f,18.5f},{20.5f,10.5f},{18.5f,3.5f}},
        {{4.5f,9.5f},{4.5f,18.5f},{10.5f,3.5f},{13.5f,13.5f},{19.5f,19.5f},{21.5f,9.5f}},
        {{4.5f,3.5f},{3.5f,13.5f},{10.5f,8.5f},{19.5f,8.5f},{12.5f,17.5f},{18.5f,21.5f}},
        {{4.5f,20.5f},{3.5f,16.5f},{10.5f,17.5f},{16.5f,12.5f},{6.5f,17.5f},{19.5f,3.5f}}};
    static const last_zone_pickup_kind_t pickup_kind[LAST_ZONE_LAYOUTS][LAST_ZONE_PICKUPS]={
        {NEON_PICKUP_AMMO,NEON_PICKUP_HEALTH,NEON_PICKUP_AMMO,
         NEON_PICKUP_AMMO,NEON_PICKUP_AMMO,NEON_PICKUP_HEALTH},
        {NEON_PICKUP_AMMO,NEON_PICKUP_HEALTH,NEON_PICKUP_AMMO,
         NEON_PICKUP_AMMO,NEON_PICKUP_ARMOR,NEON_PICKUP_HEALTH},
        {NEON_PICKUP_AMMO,NEON_PICKUP_ARMOR,NEON_PICKUP_AMMO,
         NEON_PICKUP_AMMO,NEON_PICKUP_ARMOR,NEON_PICKUP_HEALTH},
        {NEON_PICKUP_AMMO,NEON_PICKUP_HEALTH,NEON_PICKUP_AMMO,
         NEON_PICKUP_AMMO,NEON_PICKUP_AMMO,NEON_PICKUP_HEALTH},
        {NEON_PICKUP_AMMO,NEON_PICKUP_HEALTH,NEON_PICKUP_AMMO,
         NEON_PICKUP_AMMO,NEON_PICKUP_ARMOR,NEON_PICKUP_HEALTH}};
    if(layout>=LAST_ZONE_LAYOUTS)layout=0;
    game->layout=layout;
    for(int i=0;i<LAST_ZONE_ENEMIES;++i){
        game->enemies[i].x=enemy_xy[layout][i][0];
        game->enemies[i].y=enemy_xy[layout][i][1];
        game->enemies[i].elite=(s_layouts[layout].elite_mask&(1U<<i))!=0;
        game->enemies[i].hp=(uint8_t)(game->enemies[i].elite?3:2);
        game->enemies[i].move_phase=(uint8_t)(i*37U+layout*19U);
        game->enemies[i].last_seen_x=game->enemies[i].x;
        game->enemies[i].last_seen_y=game->enemies[i].y;
        game->enemies[i].hold_x=(int8_t)game->enemies[i].x;
        game->enemies[i].hold_y=(int8_t)game->enemies[i].y;
        game->enemies[i].active=i<last_zone_enemy_total(game);
        game->enemies[i].ai_state=LAST_ZONE_ENEMY_PATROL;
    }
    for(int i=0;i<LAST_ZONE_PICKUPS;++i){
        game->pickups[i].x=pickup_xy[layout][i][0];
        game->pickups[i].y=pickup_xy[layout][i][1];
        game->pickups[i].kind=pickup_kind[layout][i];
        game->pickups[i].taken=false;
    }
    static const float prop_xy[LAST_ZONE_LAYOUTS][LAST_ZONE_PROPS][2]={
        {{14.5f,12.5f},{3.5f,5.5f},{9.5f,8.5f},{19.5f,15.5f},{3.5f,21.5f},{6.5f,21.5f},
         {9.5f,21.5f},{12.5f,21.5f},{15.5f,21.5f},{18.5f,21.5f},{21.5f,21.5f},{2.5f,18.5f}},
        {{5.5f,11.5f},{12.5f,11.5f},{20.5f,12.5f},{3.5f,18.5f},{10.5f,19.5f},{18.5f,19.5f},
         {3.5f,3.5f},{6.5f,3.5f},{9.5f,3.5f},{12.5f,3.5f},{16.5f,3.5f},{21.5f,3.5f}},
        {{12.5f,4.5f},{18.5f,12.5f},{4.5f,10.5f},{11.5f,13.5f},{19.5f,16.5f},{20.5f,6.5f},
         {3.5f,3.5f},{5.5f,3.5f},{18.5f,21.5f},{20.5f,21.5f},{3.5f,21.5f},{6.5f,21.5f}},
        {{18.5f,17.5f},{3.5f,8.5f},{12.5f,13.5f},{20.5f,8.5f},{3.5f,21.5f},{6.5f,21.5f},
         {9.5f,21.5f},{12.5f,21.5f},{15.5f,21.5f},{18.5f,21.5f},{21.5f,18.5f},{21.5f,16.5f}},
        {{16.5f,8.5f},{19.5f,12.5f},{4.5f,18.5f},{8.5f,16.5f},{13.5f,12.5f},{20.5f,16.5f},
         {3.5f,3.5f},{6.5f,3.5f},{9.5f,3.5f},{12.5f,3.5f},{15.5f,3.5f},{21.5f,21.5f}}};
    static const uint16_t prop_active[LAST_ZONE_LAYOUTS]={0x00f,0x03f,0x03f,0x007,0x07f};
    static const uint8_t barrel_count[LAST_ZONE_LAYOUTS]={1,3,2,1,2};
    for(int i=0;i<LAST_ZONE_PROPS;++i){
        game->props[i].x=prop_xy[layout][i][0];
        game->props[i].y=prop_xy[layout][i][1];
        game->props[i].kind=(uint8_t)(i>=barrel_count[layout]);
        game->props[i].active=(prop_active[layout]&(1U<<i))!=0;
        game->props[i].blast_timer=0;
    }
    repair_layout(game);
}

void last_zone_reset(last_zone_game_t *game)
{
    if(!game)return;
    uint32_t best=game->best_ticks;
    uint32_t layout_best[LAST_ZONE_LAYOUTS];
    uint8_t layout=game->layout,unlocked=game->unlocked;
    memcpy(layout_best,game->layout_best,sizeof(layout_best));
    memset(game,0,sizeof(*game));
    game->best_ticks=best;
    memcpy(game->layout_best,layout_best,sizeof(layout_best));
    game->unlocked=unlocked;
    const last_zone_layout_info_t *info=&s_layouts[layout<LAST_ZONE_LAYOUTS?layout:0];
    game->x=info->spawn_x;game->y=info->spawn_y;game->angle=info->spawn_angle;
    game->radar_x=LAST_ZONE_RADAR_DEFAULT_X;
    game->radar_y=LAST_ZONE_RADAR_DEFAULT_Y;
    game->hp=LAST_ZONE_MAX_HP;
    game->armor=info->start_armor;
    game->ammo=info->start_ammo;
    game->display_hp=(float)LAST_ZONE_MAX_HP;
    game->phase=LAST_ZONE_PHASE_START;
    place_layout(game,layout);
    game->explored[(int)game->y][(int)game->x]=1;
    game->perf_logic_fps=game->perf_display_fps=30.0f;
}

void last_zone_set_best(last_zone_game_t *game,uint32_t ticks)
{ if(game)game->best_ticks=ticks; }

void last_zone_confirm(last_zone_game_t *game)
{
    if(!game)return;
    if(game->phase==LAST_ZONE_PHASE_PLAYING)return;
    if(game->phase!=LAST_ZONE_PHASE_START){
        if(game->phase==LAST_ZONE_PHASE_WON){
            uint8_t next=(uint8_t)((game->layout+1U)%LAST_ZONE_LAYOUTS);
            if(game->unlocked<next)game->unlocked=next;
            if(game->unlocked<LAST_ZONE_LAYOUTS&&next==0)
                game->unlocked=LAST_ZONE_LAYOUTS;
            game->layout=next;
        }
        last_zone_reset(game);
    }
    game->phase=LAST_ZONE_PHASE_PLAYING;
}

void last_zone_set_actions(last_zone_game_t *game,bool left,bool right,bool forward,
                           bool backward)
{ if(game){game->left=left;game->right=right;game->forward=forward;game->backward=backward;
    game->turn_input=(right?1.0f:0.0f)-(left?1.0f:0.0f);
    game->move_forward=(forward?1.0f:0.0f)-(backward?1.0f:0.0f);game->move_strafe=0;} }

void last_zone_set_motion(last_zone_game_t *game,float forward,float strafe,float turn)
{ if(game){game->move_forward=forward;game->move_strafe=strafe;game->turn_input=turn;} }

void last_zone_set_sprint(last_zone_game_t *game,bool sprint)
{ if(game)game->sprint_held=sprint; }

static void emit_sfx(last_zone_game_t *game,uint8_t id)
{
    game->sfx=id;
    game->sfx_hold=5;
}

const char *last_zone_sfx_name(const last_zone_game_t *game)
{
    static const char *names[]={"","rifle","impact","empty","confirm","alert",
                                "pickup","step_l","step_r","hurt","explode","extract"};
    uint8_t id=game?game->sfx:0;
    if(id>=sizeof(names)/sizeof(names[0]))id=0;
    return names[id];
}

void last_zone_set_fire_held(last_zone_game_t *game,bool held)
{
    if(!game)return;
    if(held&&!game->fire_held)game->fire_pressed=true;
    if(!held&&game->fire_held)game->fire_released=true;
    game->fire_held=held;
}

void last_zone_turn(last_zone_game_t *game,float radians)
{ if(game){game->angle+=radians;while(game->angle<0)game->angle+=6.2831853f;
    while(game->angle>=6.2831853f)game->angle-=6.2831853f;} }

void last_zone_look(last_zone_game_t *game,float pixels)
{
    if(!game)return;
    game->look_pitch+=pixels;
    if(game->look_pitch>22.0f)game->look_pitch=22.0f;
    if(game->look_pitch< -22.0f)game->look_pitch=-22.0f;
}

void last_zone_settle_look(last_zone_game_t *game)
{
    if(!game)return;
    game->look_pitch*=.82f;
    if(fabsf(game->look_pitch)<.4f)game->look_pitch=0;
}

void last_zone_set_performance(last_zone_game_t *game,float logic_fps,
                               float display_fps,float render_ms)
{ if(game){game->perf_logic_fps=logic_fps;game->perf_display_fps=display_fps;
    game->perf_render_ms=render_ms;} }

static void collect_pickups(last_zone_game_t *game)
{
    for(int i=0;i<LAST_ZONE_PICKUPS;++i){
        last_zone_pickup_t *item=&game->pickups[i];
        if(item->taken)continue;
        float dx=item->x-game->x,dy=item->y-game->y;
        if(dx*dx+dy*dy>.42f)continue;
        if(item->kind==NEON_PICKUP_HEALTH&&game->hp>=LAST_ZONE_MAX_HP)continue;
        if(item->kind==NEON_PICKUP_ARMOR&&game->armor>=LAST_ZONE_MAX_ARMOR)continue;
        if(item->kind==NEON_PICKUP_AMMO&&game->ammo>=LAST_ZONE_AMMO_MAX)continue;
        item->taken=true;
        game->last_pickup=true;
        game->pickup_flash=14;
        game->score+=20;
        if(item->kind==NEON_PICKUP_AMMO){
            game->ammo=(uint8_t)(game->ammo+6);
            if(game->ammo>LAST_ZONE_AMMO_MAX)game->ammo=LAST_ZONE_AMMO_MAX;
        }else if(item->kind==NEON_PICKUP_HEALTH&&game->hp<LAST_ZONE_MAX_HP){
            ++game->hp;
            game->display_hp=(float)game->hp;
        }else if(item->kind==NEON_PICKUP_ARMOR){
            game->armor=(uint8_t)(game->armor+2U);
            if(game->armor>LAST_ZONE_MAX_ARMOR)game->armor=LAST_ZONE_MAX_ARMOR;
        }
    }
}

void last_zone_update(last_zone_game_t *game)
{
    if(!game||game->phase!=LAST_ZONE_PHASE_PLAYING)return;
    const float turn=.085f;
    game->last_fire=NEON_FIRE_NONE;
    game->last_pickup=false;
    game->last_alert=false;
    game->last_blast=false;
    game->sprinting=false;
    if(game->sfx_hold){if(!--game->sfx_hold)game->sfx=0;}
    if(game->fire_cooldown)--game->fire_cooldown;
    if(game->enemy_shot_lock)--game->enemy_shot_lock;
    if(game->alert_flash)game->alert_flash--;
    if(game->fire_held){
        if(game->breath_hold<255)game->breath_hold++;
        game->holding_breath=game->breath_hold>=LAST_ZONE_BREATH_TICKS;
    }else if(game->fire_released&&(game->breath_hold||game->fire_pressed)){
        game->last_fire=last_zone_fire(game);
        game->breath_hold=0;
        game->holding_breath=false;
    }else{
        game->breath_hold=0;
        game->holding_breath=false;
    }
    game->fire_pressed=false;
    game->fire_released=false;
    if(game->last_blast)emit_sfx(game,10);
    else if(game->last_fire==NEON_FIRE_DRY)emit_sfx(game,3);
    else if(game->last_fire==NEON_FIRE_DOOR)emit_sfx(game,4);
    else if(game->last_fire==NEON_FIRE_SHOT||game->last_fire==NEON_FIRE_HIT||
            game->last_fire==NEON_FIRE_KILL)emit_sfx(game,1);
    if(game->last_fire==NEON_FIRE_SHOT||game->last_fire==NEON_FIRE_HIT||
       game->last_fire==NEON_FIRE_KILL){
        game->spotted=true;
        game->alert_flash=18;
        for(int i=0;i<LAST_ZONE_ENEMIES;++i){
            last_zone_enemy_t *enemy=&game->enemies[i];
            if(!enemy->active)continue;
            float dx=enemy->x-game->x,dy=enemy->y-game->y;
            if(dx*dx+dy*dy>LAST_ZONE_SHOT_NOISE*LAST_ZONE_SHOT_NOISE)continue;
            enemy->last_seen_x=game->x;enemy->last_seen_y=game->y;
            enemy->search_timer=150;
            if(enemy->ai_state==LAST_ZONE_ENEMY_PATROL){
                enemy->ai_state=LAST_ZONE_ENEMY_ALERT;
                enemy->alert_timer=LAST_ZONE_ALERT_TICKS;
                game->last_alert=true;
            }
        }
    }
    game->angle+=turn*game->turn_input;
    if(game->angle<0)game->angle+=6.2831853f;
    if(game->angle>=6.2831853f)game->angle-=6.2831853f;
    game->look_kick*=.68f;
    if(fabsf(game->look_kick)<.25f)game->look_kick=0;
    float forward=game->move_forward,strafe=game->move_strafe;
    float length=sqrtf(forward*forward+strafe*strafe);
    if(length>1.0f){forward/=length;strafe/=length;length=1.0f;}
    game->sprinting=(game->sprint_held&&forward>0.12f)||
                    (length>=LAST_ZONE_SPRINT&&forward>0.12f);
    if(game->sprint_held&&forward>0.12f&&length>0.2f&&length<1.0f){
        forward/=length;strafe/=length;length=1.0f;
    }
    float speed=game->sprinting?0.135f:(length>0.01f?0.055f+0.040f*length:0.0f);
    if(game->holding_breath)speed*=0.55f;
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
    game->vel_x=game->vel_x*.18f+wish_x*.82f;
    game->vel_y=game->vel_y*.18f+wish_y*.82f;
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
    for(int i=0;i<LAST_ZONE_PROPS;++i)if(game->props[i].blast_timer)--game->props[i].blast_timer;
    int last_enemy=last_zone_last_enemy_index(game);
    for(int i=0;i<LAST_ZONE_ENEMIES;++i){
        last_zone_enemy_t *enemy=&game->enemies[i];
        if(enemy->death_timer)--enemy->death_timer;
        if(!enemy->active)continue;
        if(enemy->hit_flash)--enemy->hit_flash;
        if(enemy->attack_flash)--enemy->attack_flash;
        if(enemy->attack_cooldown)--enemy->attack_cooldown;
        /* Bring the final hostile to the player instead of requiring a sweep
         * through every previously explored dead end. */
        if(i==last_enemy&&enemy->ai_state!=LAST_ZONE_ENEMY_ENGAGE&&
           enemy->ai_state!=LAST_ZONE_ENEMY_ALERT){
            enemy->ai_state=LAST_ZONE_ENEMY_SEARCH;
            enemy->last_seen_x=last_zone_extract_x(game);
            enemy->last_seen_y=last_zone_extract_y(game);
            enemy->search_timer=180;
        }
        float to_x=game->x-enemy->x,to_y=game->y-enemy->y;
        float distance=sqrtf(to_x*to_x+to_y*to_y),heading=0;
        bool sees_player=distance<6.8f&&line_clear(game,enemy->x,enemy->y,game->x,game->y);
        bool hears_player=false;
        if(!sees_player){
            if(game->sprinting&&distance<LAST_ZONE_HEAR_SPRINT)hears_player=true;
            else if(length>0.18f&&distance<LAST_ZONE_HEAR_WALK&&
                    line_clear(game,enemy->x,enemy->y,game->x,game->y))
                hears_player=true;
        }
        if(sees_player||hears_player){
            enemy->last_seen_x=game->x;enemy->last_seen_y=game->y;enemy->search_timer=150;
            if(enemy->ai_state==LAST_ZONE_ENEMY_PATROL||
               (enemy->ai_state==LAST_ZONE_ENEMY_SEARCH&&(sees_player?distance<3.8f:true))){
                enemy->ai_state=LAST_ZONE_ENEMY_ALERT;enemy->alert_timer=LAST_ZONE_ALERT_TICKS;
                game->last_alert=true;
                game->alert_flash=18;
                if(sees_player||hears_player)game->spotted=true;
            }
        }else if((enemy->ai_state==LAST_ZONE_ENEMY_ENGAGE||enemy->ai_state==LAST_ZONE_ENEMY_ALERT)
                 &&enemy->search_timer){
            enemy->ai_state=LAST_ZONE_ENEMY_SEARCH;enemy->aim_timer=0;
        }
        if(enemy->ai_state==LAST_ZONE_ENEMY_ALERT){
            if(enemy->alert_timer)--enemy->alert_timer;
            else enemy->ai_state=LAST_ZONE_ENEMY_ENGAGE;
        }else if(enemy->ai_state==LAST_ZONE_ENEMY_SEARCH){
            if(enemy->search_timer)--enemy->search_timer;else enemy->ai_state=LAST_ZONE_ENEMY_PATROL;
        }
        if(enemy->ai_state==LAST_ZONE_ENEMY_ENGAGE&&sees_player&&distance>.9f&&distance<6.2f){
            if(!enemy->attack_cooldown&&!game->enemy_shot_lock){
                if(!enemy->aim_timer){
                    static const uint8_t aim_ticks[LAST_ZONE_LAYOUTS]={30,27,24,22,20};
                    enemy->aim_timer=game->layout<LAST_ZONE_LAYOUTS?aim_ticks[game->layout]:20;
                }else if(!--enemy->aim_timer){
                    static const uint8_t cool_ticks[LAST_ZONE_LAYOUTS]={96,86,76,70,64};
                    enemy->attack_flash=4;
                    enemy->attack_cooldown=game->layout<LAST_ZONE_LAYOUTS?cool_ticks[game->layout]:64;
                    game->enemy_shot_lock=12;
                    hurt_player(game,enemy);
                    if(game->phase==LAST_ZONE_PHASE_DEAD)return;
                }
            }
        }else enemy->aim_timer=0;
        if(distance<.40f&&!game->enemy_shot_lock){
            hurt_player(game,enemy);
            game->enemy_shot_lock=12;
            if(game->phase==LAST_ZONE_PHASE_DEAD)return;
        }
        if(enemy->ai_state==LAST_ZONE_ENEMY_ENGAGE&&sees_player){
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
            bool should_move=enemy->ai_state==LAST_ZONE_ENEMY_PATROL||
                             enemy->ai_state==LAST_ZONE_ENEMY_SEARCH;
            if(!should_move)continue;
            if(enemy->ai_state==LAST_ZONE_ENEMY_PATROL)
                heading=((float)((game->tick/75U+enemy->move_phase)%16U))*0.3926991f;
            else{
                if(((game->tick+(uint32_t)i)%8U)==0U||(!enemy->nav_dx&&!enemy->nav_dy))
                    route_next(game,(int)enemy->x,(int)enemy->y,(int)enemy->last_seen_x,
                               (int)enemy->last_seen_y,&enemy->nav_dx,&enemy->nav_dy);
                if(!enemy->nav_dx&&!enemy->nav_dy)continue;
                heading=atan2f((float)enemy->nav_dy,(float)enemy->nav_dx);
            }
        }
        for(int j=0;j<LAST_ZONE_ENEMIES;++j){
            if(j==i||!game->enemies[j].active)continue;
            float ox=enemy->x-game->enemies[j].x,oy=enemy->y-game->enemies[j].y;
            if(ox*ox+oy*oy<1.6f)heading+=1.2f;
        }
        float step=enemy->ai_state==LAST_ZONE_ENEMY_ENGAGE?.028f:.022f;
        float dx=cosf(heading)*step,dy=sinf(heading)*step;
        float nx=enemy->x+dx,ny=enemy->y+dy;
        if(!blocked_at(game,nx,enemy->y))enemy->x=nx;
        else enemy->move_phase=(uint8_t)(enemy->move_phase+5U);
        if(!blocked_at(game,enemy->x,ny))enemy->y=ny;
        else enemy->move_phase=(uint8_t)(enemy->move_phase+7U);
    }
    if(!game->sfx&&game->last_pickup)emit_sfx(game,6);
    if(!game->sfx&&game->last_alert)emit_sfx(game,5);
    if(last_zone_enemies_alive(game)==0&&last_zone_on_extract(game)){
        game->cells_reached=1;
        game->phase=LAST_ZONE_PHASE_WON;
        emit_sfx(game,11);
        if(!game->layout_best[game->layout]||game->tick<game->layout_best[game->layout])
            game->layout_best[game->layout]=game->tick;
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

last_zone_fire_result_t last_zone_fire(last_zone_game_t *game)
{
    if(!game)return NEON_FIRE_NONE;
    if(game->phase!=LAST_ZONE_PHASE_PLAYING){last_zone_confirm(game);return NEON_FIRE_NONE;}
    if(game->fire_cooldown)return NEON_FIRE_NONE;
    int door_x=0,door_y=0;
    if(door_cell_ahead(game,&door_x,&door_y)){
        open_door_cluster(game,door_x,door_y);
        game->fire_cooldown=LAST_ZONE_DOOR_COOLDOWN;
        return NEON_FIRE_DOOR;
    }
    if(!game->ammo){
        game->fire_cooldown=LAST_ZONE_DRY_COOLDOWN;
        game->dry_flash=10;
        return NEON_FIRE_DRY;
    }
    --game->ammo;
    ++game->shots_fired;
    game->fire_cooldown=LAST_ZONE_FIRE_COOLDOWN;
    game->weapon_recoil=1.0f;
    game->look_kick-=7.0f;
    int target=-1,barrel=-1;float best=1000.0f;
    for(int i=0;i<LAST_ZONE_ENEMIES;++i){
        last_zone_enemy_t *enemy=&game->enemies[i];if(!enemy->active)continue;
        float dx=enemy->x-game->x,dy=enemy->y-game->y;
        float distance=sqrtf(dx*dx+dy*dy);
        float delta=fabsf(angle_delta(atan2f(dy,dx)-game->angle));
        float slop=game->sprinting&&!game->holding_breath?0.10f:0.0f;
        float cone=game->holding_breath?0.045f:0.08f;
        float fall=game->holding_breath?0.10f:0.16f;
        if(delta>cone+slop+(fall/distance)||distance>=best)continue;
        if(!line_clear(game,game->x,game->y,enemy->x,enemy->y))continue;
        best=distance;target=i;
    }
    for(int i=0;i<LAST_ZONE_PROPS;++i){
        last_zone_prop_t *prop=&game->props[i];
        if(!prop->active||prop->kind!=0)continue;
        float dx=prop->x-game->x,dy=prop->y-game->y;
        float distance=sqrtf(dx*dx+dy*dy);
        float delta=fabsf(angle_delta(atan2f(dy,dx)-game->angle));
        if(delta>.08f+(.14f/distance)||distance>=best)continue;
        if(!line_clear(game,game->x,game->y,prop->x,prop->y))continue;
        best=distance;target=-1;barrel=i;
    }
    if(barrel>=0){
        last_zone_prop_t *prop=&game->props[barrel];
        prop->active=false;prop->blast_timer=18;
        ++game->shots_hit;
        int killed=0;
        for(int i=0;i<LAST_ZONE_ENEMIES;++i){
            last_zone_enemy_t *enemy=&game->enemies[i];if(!enemy->active)continue;
            float dx=enemy->x-prop->x,dy=enemy->y-prop->y;
            if(dx*dx+dy*dy>6.25f||
               !line_clear(game,prop->x,prop->y,enemy->x,enemy->y))continue;
            enemy->hp=0;enemy->active=false;enemy->death_timer=20;
            ++game->kills;++killed;game->score+=100;
        }
        game->hit_marker=10;game->kill_flash=killed?18:0;game->score+=40;
        game->last_blast=true;game->barrel_used=true;
        return killed?NEON_FIRE_KILL:NEON_FIRE_HIT;
    }
    if(target<0)return NEON_FIRE_SHOT;
    last_zone_enemy_t *enemy=&game->enemies[target];
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

uint32_t last_zone_state_hash(const last_zone_game_t *game)
{
    if(!game)return 0;
    uint32_t hash=2166136261U, values[]={
        (uint32_t)(game->x*4096),(uint32_t)(game->y*4096),
        (uint32_t)(game->angle*4096),(uint32_t)((game->look_pitch+64.0f)*256.0f),
        game->tick,game->cells_reached,game->score,
        (uint32_t)game->phase,(uint32_t)game->hp,(uint32_t)game->armor,(uint32_t)game->ammo,
        (uint32_t)game->layout,(uint32_t)game->spotted,(uint32_t)game->holding_breath,
        (uint32_t)game->unlocked};
    for(unsigned i=0;i<sizeof(values)/sizeof(values[0]);++i){hash^=values[i];hash*=16777619U;}
    for(int i=0;i<LAST_ZONE_ENEMIES;++i){hash^=(uint32_t)game->enemies[i].hp;
        hash*=16777619U;}
    for(int i=0;i<LAST_ZONE_PICKUPS;++i){hash^=game->pickups[i].taken;hash*=16777619U;}
    for(int i=0;i<LAST_ZONE_PROPS;++i){hash^=game->props[i].active;hash*=16777619U;
        hash^=game->props[i].blast_timer;hash*=16777619U;}
    for(int y=0;y<LAST_ZONE_HEIGHT;++y)for(int x=0;x<LAST_ZONE_WIDTH;++x){
        hash^=game->door_open[y][x];hash*=16777619U;}
    return hash;
}
