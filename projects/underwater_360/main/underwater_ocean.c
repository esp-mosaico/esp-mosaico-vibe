// SPDX-License-Identifier: Apache-2.0
#include "underwater_ocean.h"
#include <math.h>
#include <stdbool.h>
#include <string.h>
#ifndef UNDERWATER_SCENE_SIM_ONLY
#include "mosaico_raylib_fast.h"
#include "ocean_depth.h"
#include "ocean_left_volume.h"
#include "ocean_right_volume.h"
#include "underwater_ocean_draw.h"
#include "underwater_volume.h"
#endif

#define OCEAN_DT (1.0f/30.0f)
#define OCEAN_FOCAL (480.0f*1.055f)
#define OCEAN_RENDER_GRID 16

static float ocean_clamp(float value,float low,float high)
{
    if(value<low)return low;
    if(value>high)return high;
    return value;
}

static uint32_t ocean_random(underwater_ocean_t *ocean)
{
    ocean->rng=ocean->rng*1664525U+1013904223U;
    return ocean->rng;
}

static float ocean_unit(underwater_ocean_t *ocean)
{
    return (float)(ocean_random(ocean)>>8)*(1.0f/16777216.0f);
}

static float ocean_range(underwater_ocean_t *ocean,float low,float high)
{
    return low+(high-low)*ocean_unit(ocean);
}

static void ocean_unproject(float sx,float sy,float z,float *x,float *y)
{
    *x=(sx-240.0f)*z/OCEAN_FOCAL;
    *y=(240.0f-sy)*z/OCEAN_FOCAL;
}

static void ocean_spawn_mote(underwater_ocean_t *ocean,ocean_mote_t *mote,
                             float sx,float sy,float z)
{
    ocean_unproject(sx,sy,z,&mote->x,&mote->y);
    mote->z=z;
    mote->phase=ocean_range(ocean,0,6.2831853f);
    mote->size=ocean_range(ocean,.007f,.025f);
    mote->vy=ocean_range(ocean,.015f,.07f);
    mote->bubble=(uint8_t)(ocean_unit(ocean)<.13f);
}

static void ocean_spawn_wanderer(underwater_ocean_t *ocean,ocean_wanderer_t *item,
                                 uint8_t kind,float sx,float sy,float z,float size,int dir)
{
    ocean_unproject(sx,sy,z,&item->x,&item->y);
    item->z=z;item->kind=kind;item->size=size;item->dir=(int8_t)dir;
    item->speed=kind==OCEAN_WANDER_WHALE?ocean_range(ocean,.05f,.08f):
                kind==OCEAN_WANDER_RAY?ocean_range(ocean,.16f,.22f):
                kind==OCEAN_WANDER_TURTLE?ocean_range(ocean,.08f,.13f):
                ocean_range(ocean,.11f,.16f);
    item->phase=ocean_range(ocean,0,6.2831853f);
    item->alpha=ocean_range(ocean,.58f,.9f);
    item->drift=ocean_range(ocean,.04f,.09f);
}

static void ocean_recycle_wanderer(underwater_ocean_t *ocean,ocean_wanderer_t *item)
{
    bool from_right=item->dir<0;
    float sx,sy,z,size;
    if(item->kind==OCEAN_WANDER_RAY){
        sx=from_right?520.0f:-40.0f;sy=ocean_range(ocean,160,230);
        z=ocean_range(ocean,16,24);size=ocean_range(ocean,.46f,.62f);
    }else if(item->kind==OCEAN_WANDER_TURTLE){
        sx=from_right?-35.0f:515.0f;sy=ocean_range(ocean,250,330);
        z=ocean_range(ocean,12,20);size=ocean_range(ocean,.36f,.5f);
    }else if(item->kind==OCEAN_WANDER_WHALE){
        sx=from_right?548.0f:-60.0f;sy=ocean_range(ocean,176,232);
        z=ocean_range(ocean,34,42);size=ocean_range(ocean,.62f,.78f);
    }else{
        sx=from_right?520.0f:-30.0f;sy=ocean_range(ocean,300,360);
        z=ocean_range(ocean,18,26);size=ocean_range(ocean,.24f,.34f);
    }
    ocean_spawn_wanderer(ocean,item,item->kind,sx,sy,z,size,item->dir);
}

void underwater_ocean_reset(underwater_ocean_t *ocean)
{
    memset(ocean,0,sizeof(*ocean));
    ocean->rng=826721U+521U;
    ocean->mote_count=OCEAN_MOTE_CAP;
    ocean->jelly_count=OCEAN_JELLY_CAP;
    ocean->shoal_count=OCEAN_SHOAL_CAP;
    ocean->wanderer_count=OCEAN_WANDERER_CAP;
    static const float jelly_spec[][4]={
        {333,146,6.7f,.82f},{94,122,13.5f,.69f},{163,68,30.0f,.71f},
        {416,100,25.0f,.78f},{171,258,19.0f,.65f},{284,249,27.0f,.59f}
    };
    for(unsigned i=0;i<ocean->jelly_count;++i){
        ocean_jelly_t *jelly=&ocean->jellies[i];
        ocean_unproject(jelly_spec[i][0],jelly_spec[i][1],jelly_spec[i][2],&jelly->x,&jelly->y);
        jelly->z=jelly_spec[i][2];
        jelly->home_x=jelly->x;jelly->home_y=jelly->y;jelly->home_z=jelly->z;
        jelly->radius=jelly_spec[i][3];jelly->id=(uint8_t)i;
        jelly->phase=ocean_range(ocean,0,6.2831853f);
        jelly->freq=ocean_range(ocean,1.35f,1.8f);
        jelly->roll=ocean_range(ocean,-.15f,.16f);
        jelly->yaw=ocean_range(ocean,-.6f,.6f);
    }
    static const float shoal_spec[][8]={
        {346,176,22,20,-1,1.05f,.40f,.16f},{132,233,13,17,1,.96f,.33f,.14f},
        {404,296,18,14,-1,.86f,.27f,.12f},{76,146,28,12,1,.72f,.23f,.11f},
        {438,118,31,10,-1,.56f,.18f,.10f},{220,352,10,22,1,1.20f,.43f,.10f}
    };
    static const uint8_t shoal_color[][3]={
        {146,214,238},{121,189,224},{159,231,215},
        {134,200,231},{174,224,244},{153,205,232}
    };
    for(unsigned i=0;i<ocean->shoal_count;++i){
        ocean_shoal_t *shoal=&ocean->shoals[i];
        ocean_unproject(shoal_spec[i][0],shoal_spec[i][1],shoal_spec[i][2],&shoal->x,&shoal->y);
        shoal->z=shoal_spec[i][2];
        shoal->count=(int8_t)shoal_spec[i][3];
        shoal->dir=(int8_t)shoal_spec[i][4];
        shoal->spread=shoal_spec[i][5];
        shoal->speed=shoal_spec[i][6];
        shoal->size=shoal_spec[i][7];
        shoal->phase=ocean_range(ocean,0,6.2831853f);
        shoal->sway=ocean_range(ocean,.8f,1.6f);
        shoal->r=shoal_color[i][0];shoal->g=shoal_color[i][1];shoal->b=shoal_color[i][2];
    }
    ocean_spawn_wanderer(ocean,&ocean->wanderers[0],OCEAN_WANDER_RAY,402,198,20,.56f,-1);
    ocean_spawn_wanderer(ocean,&ocean->wanderers[1],OCEAN_WANDER_TURTLE,78,280,15,.46f,1);
    ocean_spawn_wanderer(ocean,&ocean->wanderers[2],OCEAN_WANDER_SEAHORSE,448,326,23,.30f,-1);
    ocean_spawn_wanderer(ocean,&ocean->wanderers[3],OCEAN_WANDER_RAY,70,138,27,.40f,1);
    ocean_spawn_wanderer(ocean,&ocean->wanderers[4],OCEAN_WANDER_TURTLE,432,340,29,.34f,-1);
    ocean_spawn_wanderer(ocean,&ocean->wanderers[5],OCEAN_WANDER_WHALE,460,208,36,.68f,-1);
    for(unsigned i=0;i<ocean->mote_count;++i)
        ocean_spawn_mote(ocean,&ocean->motes[i],ocean_range(ocean,0,480),
                         ocean_range(ocean,0,480),ocean_range(ocean,3,38));
}

void underwater_ocean_look(float *yaw,float *pitch,float *yaw_velocity,
                           float *pitch_velocity,float dx,float dy)
{
    if(dx!=0||dy!=0){
        *yaw_velocity=-dx*0.072f;
        *pitch_velocity=dy*0.050f;
        *yaw-=dx*0.072f;
        *pitch+=dy*0.050f;
    }
    float nx=*yaw/OCEAN_YAW_LIMIT,ny=*pitch/OCEAN_PITCH_LIMIT;
    float length=sqrtf(nx*nx+ny*ny);
    if(length>1.0f){*yaw/=length;*pitch/=length;*yaw_velocity*=.25f;*pitch_velocity*=.25f;}
}

void underwater_ocean_tap(underwater_ocean_t *ocean,float x,float y)
{
    unsigned first=ocean->mote_count,add=8;
    if(first+add>OCEAN_MOTE_CAP)add=OCEAN_MOTE_CAP-first;
    for(unsigned i=0;i<add;++i)
        ocean_spawn_mote(ocean,&ocean->motes[first+i],x+ocean_range(ocean,-13,13),
                         y+ocean_range(ocean,-8,8),ocean_range(ocean,5,12));
    ocean->mote_count=(uint8_t)(first+add);
    if(ocean->jelly_count){
        ocean_jelly_t *jelly=&ocean->jellies[ocean_random(ocean)%ocean->jelly_count];
        jelly->flash=1;jelly->vy+=.5f;jelly->phase=1.0471976f;
    }
}

void underwater_ocean_update(underwater_ocean_t *ocean)
{
    ++ocean->tick;
    float t=ocean->tick*OCEAN_DT;
    ocean->flow_x*=.962f;ocean->flow_y*=.962f;
    for(unsigned i=0;i<ocean->jelly_count;++i){
        ocean_jelly_t *jelly=&ocean->jellies[i];
        jelly->phase+=OCEAN_DT*jelly->freq;
        float pulse=sinf(jelly->phase);
        if(pulse<0)pulse=0;
        jelly->pulse=pulse*pulse;
        jelly->flash*=.932f;
        float ax=(jelly->home_x+sinf(t*.24f+jelly->id)*.16f-jelly->x)*.35f+ocean->flow_x*.24f;
        float ay=(jelly->home_y+sinf(t*.35f+jelly->id)*.16f-jelly->y)*.35f+
                 jelly->pulse*.11f+ocean->flow_y*.15f;
        jelly->vx=(jelly->vx+ax*OCEAN_DT)*.974f;
        jelly->vy=(jelly->vy+ay*OCEAN_DT)*.974f;
        jelly->x+=jelly->vx*OCEAN_DT;
        jelly->y+=jelly->vy*OCEAN_DT;
        jelly->z=jelly->home_z+sinf(t*.24f+jelly->id)*.2f;
        jelly->roll+=(-.24f+ocean_clamp(-jelly->vx*.45f,-.35f,.35f)+
                      sinf(t*.3f+jelly->id)*.10f-jelly->roll)*.074f;
        jelly->yaw+=(sinf(t*.13f+jelly->id)*.36f+jelly->vx*.25f-jelly->yaw)*.033f;
    }
    for(unsigned i=0;i<ocean->shoal_count;++i){
        ocean_shoal_t *shoal=&ocean->shoals[i];
        shoal->phase+=OCEAN_DT*(1.2f+shoal->speed);
        shoal->x+=shoal->speed*shoal->dir*OCEAN_DT;
        shoal->y+=sinf(t*.55f+shoal->phase)*.018f*OCEAN_DT;
        float screen_x=240.0f+shoal->x*OCEAN_FOCAL/shoal->z;
        if((shoal->dir<0&&screen_x<-110.0f)||(shoal->dir>0&&screen_x>590.0f)){
            ocean_unproject(shoal->dir<0?520.0f:-36.0f,ocean_range(ocean,125,345),
                            shoal->z,&shoal->x,&shoal->y);
            shoal->phase=ocean_range(ocean,0,6.2831853f);
        }
    }
    for(unsigned i=0;i<ocean->wanderer_count;++i){
        ocean_wanderer_t *item=&ocean->wanderers[i];
        float factor=item->kind==OCEAN_WANDER_WHALE?.65f:1.0f;
        item->phase+=OCEAN_DT*((item->kind==OCEAN_WANDER_RAY?.9f:1.2f)*factor);
        item->x+=item->speed*item->dir*OCEAN_DT;
        item->y+=sinf(t*.55f+item->phase)*item->drift*OCEAN_DT*factor;
        float screen_x=240.0f+item->x*OCEAN_FOCAL/item->z;
        if((item->dir<0&&screen_x<-120.0f)||(item->dir>0&&screen_x>600.0f))
            ocean_recycle_wanderer(ocean,item);
    }
    for(unsigned i=0;i<ocean->mote_count;++i){
        ocean_mote_t *mote=&ocean->motes[i];
        mote->x+=(sinf(t*.34f+mote->phase)*.018f+ocean->flow_x*.07f)*OCEAN_DT;
        mote->y+=(mote->vy+ocean->flow_y*.03f)*OCEAN_DT;
        float sx=240.0f+mote->x*OCEAN_FOCAL/mote->z;
        float sy=240.0f-mote->y*OCEAN_FOCAL/mote->z;
        if(sy<-12.0f||sy>492.0f||sx<-15.0f||sx>495.0f)
            ocean_spawn_mote(ocean,mote,ocean_range(ocean,3,477),
                             mote->vy>0?490.0f:-8.0f,mote->z);
    }
}

#ifndef UNDERWATER_SCENE_SIM_ONLY
static bool ocean_project(const living_camera_t *camera,float x,float y,float z,Vector2 *out)
{
    return living_project_xyz(camera,x,y,z,out);
}

static float ocean_mirror(float value)
{
    value=fmodf(value,2.0f);
    if(value<0)value+=2.0f;
    return value<=1.0f?value:2.0f-value;
}

static int ocean_depth_index(int value)
{
    if(value<0)return 0;
    if(value>OCEAN_DEPTH_GRID)return OCEAN_DEPTH_GRID;
    return value;
}

static float ocean_depth_sample(float u,float v)
{
    const int n=OCEAN_DEPTH_GRID;
    float gx=ocean_clamp(u,0.0f,1.0f)*n;
    float gy=ocean_clamp(v,0.0f,1.0f)*n;
    int x0=(int)gx,y0=(int)gy;
    int x1=ocean_depth_index(x0+1),y1=ocean_depth_index(y0+1);
    float fx=gx-x0,fy=gy-y0;
    float top=OCEAN_DEPTH[y0*(n+1)+x0]*(1.0f-fx)+
              OCEAN_DEPTH[y0*(n+1)+x1]*fx;
    float bottom=OCEAN_DEPTH[y1*(n+1)+x0]*(1.0f-fx)+
                 OCEAN_DEPTH[y1*(n+1)+x1]*fx;
    return top*(1.0f-fy)+bottom*fy;
}

static float ocean_mask_sample(float u,float v)
{
    const int n=OCEAN_DEPTH_GRID;
    float gx=ocean_clamp(u,0.0f,1.0f)*n;
    float gy=ocean_clamp(v,0.0f,1.0f)*n;
    int x0=(int)gx,y0=(int)gy;
    int x1=ocean_depth_index(x0+1),y1=ocean_depth_index(y0+1);
    float fx=gx-x0,fy=gy-y0;
    float top=OCEAN_MASK_R[y0*(n+1)+x0]*(1.0f-fx)+
              OCEAN_MASK_R[y0*(n+1)+x1]*fx;
    float bottom=OCEAN_MASK_R[y1*(n+1)+x0]*(1.0f-fx)+
                 OCEAN_MASK_R[y1*(n+1)+x1]*fx;
    return top*(1.0f-fy)+bottom*fy;
}

static bool ocean_project_depth(const living_camera_t *camera,float u,float v,
                                uint16_t raw,Vector2 *out)
{
    const float xy_scale=480.0f*1.14f/OCEAN_FOCAL;
    float inverse=(float)raw*(.3f/65535.0f);
    float z=1.0f/fmaxf(.007f,inverse);
    float scale=z*xy_scale;
    return living_project_xyz(camera,(u-.5f)*scale,(.5f-v)*scale,z,out);
}

static void draw_ocean_water(const underwater_ocean_t *ocean,const living_camera_t *camera,
                             MosaicoAtlas water)
{
    if(!water.texture.id||!camera)return;
    const int n=OCEAN_RENDER_GRID;
    enum { X0=-8, Y0=-4, GW=33, GH=25 };
    static Vector2 mesh[GW*GH];
    static uint8_t ok[GW*GH];
    static float depth[GW*GH];
    static float mask[GW*GH];
    static float tu[GW],tv[GH],uu[GW],vv[GH];
    static float cached_tw=-1.0f,cached_th=-1.0f;
    float tex_w=(float)water.texture.width-1.0f,tex_h=(float)water.texture.height-1.0f;
    if(cached_tw!=tex_w||cached_th!=tex_h){
        for(int ix=0;ix<GW;++ix){
            uu[ix]=(float)(X0+ix)/n;
            tu[ix]=ocean_mirror(uu[ix])*tex_w;
        }
        for(int iy=0;iy<GH;++iy){
            vv[iy]=(float)(Y0+iy)/n;
            tv[iy]=ocean_mirror(vv[iy])*tex_h;
        }
        for(int iy=0;iy<GH;++iy)
        for(int ix=0;ix<GW;++ix){
            int idx=iy*GW+ix;
            depth[idx]=ocean_depth_sample(uu[ix],vv[iy]);
            mask[idx]=ocean_mask_sample(uu[ix],vv[iy]);
        }
        cached_tw=tex_w;cached_th=tex_h;
    }
    for(int iy=0;iy<GH;++iy)
    for(int ix=0;ix<GW;++ix){
        int idx=iy*GW+ix;
        ok[idx]=(uint8_t)ocean_project_depth(camera,uu[ix],vv[iy],(uint16_t)depth[idx],&mesh[idx]);
    }
    enum { QUAD_CAP=(GW-1)*(GH-1) };
    static uint16_t band_ix[8][QUAD_CAP];
    static uint16_t band_iy[8][QUAD_CAP];
    static uint16_t band_light[8][QUAD_CAP];
    int band_n[8];
    memset(band_n,0,sizeof band_n);
    for(int iy=0;iy<GH-1;++iy)
    for(int ix=0;ix<GW-1;++ix){
        int a=iy*GW+ix,b=a+1,c=a+GW,d=c+1;
        if(!ok[a]||!ok[b]||!ok[c]||!ok[d])continue;
        unsigned average=(unsigned)((depth[a]+depth[b]+depth[c]+depth[d])*.25f);
        int band=(int)(average*8U/65536U);
        if(band<0)band=0;
        if(band>7)band=7;
        float left=fminf(fminf(mesh[a].x,mesh[b].x),fminf(mesh[c].x,mesh[d].x));
        float right=fmaxf(fmaxf(mesh[a].x,mesh[b].x),fmaxf(mesh[c].x,mesh[d].x));
        float top=fminf(fminf(mesh[a].y,mesh[b].y),fminf(mesh[c].y,mesh[d].y));
        float bottom=fmaxf(fmaxf(mesh[a].y,mesh[b].y),fmaxf(mesh[c].y,mesh[d].y));
        if(right<0||left>480||bottom<0||top>480)continue;
        unsigned reef=(unsigned)((mask[a]+mask[b]+mask[c]+mask[d])*.25f);
        int slot=band_n[band]++;
        band_ix[band][slot]=(uint16_t)ix;
        band_iy[band][slot]=(uint16_t)iy;
        band_light[band][slot]=(uint16_t)(reef<40U?232U:256U);
    }
    for(int band=0;band<8;++band){
        for(int i=0;i<band_n[band];++i){
            int ix=band_ix[band][i],iy=band_iy[band][i];
            int a=iy*GW+ix,b=a+1,c=a+GW,d=c+1;
            unsigned background_light=band_light[band][i];
            mosaico_textured_vertex_t va={mesh[a].x,mesh[a].y,tu[ix],tv[iy]};
            mosaico_textured_vertex_t vb={mesh[b].x,mesh[b].y,tu[ix+1],tv[iy]};
            mosaico_textured_vertex_t vc={mesh[c].x,mesh[c].y,tu[ix],tv[iy+1]};
            mosaico_textured_vertex_t vd={mesh[d].x,mesh[d].y,tu[ix+1],tv[iy+1]};
            Mosaico2DDrawTexturedQuad(water.texture,va,vb,vc,vd,background_light);
        }
    }
    (void)ocean;
}

static void draw_ocean_reefs(const living_camera_t *camera,float yaw,MosaicoAtlas left_front,
                             MosaicoAtlas left_side,MosaicoAtlas left_rear,
                             MosaicoAtlas right_front,MosaicoAtlas right_side,
                             MosaicoAtlas right_rear)
{
    /* Painter order: farther reef first.  Reef fronts sit over the continuous
       water mesh.  The authored side UVs stretch into detached diagonal slabs
       at oblique angles, so both side walls and rear caps stay omitted. */
    if(yaw<0){
        living_draw_volume(camera,OCEAN_RIGHT_FRONT_VERTICES,OCEAN_RIGHT_FRONT_VERTEX_COUNT,
            OCEAN_RIGHT_FRONT_FACES,OCEAN_RIGHT_FRONT_FACE_COUNT,right_front,768,768,1);
        living_draw_volume(camera,OCEAN_LEFT_FRONT_VERTICES,OCEAN_LEFT_FRONT_VERTEX_COUNT,
            OCEAN_LEFT_FRONT_FACES,OCEAN_LEFT_FRONT_FACE_COUNT,left_front,768,768,1);
    }else{
        living_draw_volume(camera,OCEAN_LEFT_FRONT_VERTICES,OCEAN_LEFT_FRONT_VERTEX_COUNT,
            OCEAN_LEFT_FRONT_FACES,OCEAN_LEFT_FRONT_FACE_COUNT,left_front,768,768,1);
        living_draw_volume(camera,OCEAN_RIGHT_FRONT_VERTICES,OCEAN_RIGHT_FRONT_VERTEX_COUNT,
            OCEAN_RIGHT_FRONT_FACES,OCEAN_RIGHT_FRONT_FACE_COUNT,right_front,768,768,1);
    }
    (void)left_side;(void)left_rear;(void)right_side;(void)right_rear;
}

static void ocean_tri(const living_camera_t *camera,float ax,float ay,float az,
                      float bx,float by,float bz,float cx,float cy,float cz,Color color)
{
    Vector2 a,b,c;
    if(!ocean_project(camera,ax,ay,az,&a)||!ocean_project(camera,bx,by,bz,&b)||
       !ocean_project(camera,cx,cy,cz,&c))return;
    DrawTriangle(a,b,c,color);
}

static void draw_ocean_fish(const living_camera_t *camera,float x,float y,float z,
                            float size,int dir,Color color,float alpha)
{
    Color body=color;body.a=(unsigned char)(180*alpha);
    Color shade=color;shade.r=(unsigned char)(color.r*.78f);shade.g=(unsigned char)(color.g*.78f);
    shade.b=(unsigned char)(color.b*.78f);shade.a=(unsigned char)(150*alpha);
    Color tail=(Color){195,238,255,(unsigned char)(108*alpha)};
    ocean_tri(camera,x+dir*size*1.08f,y,z,x,y+size*.34f,z,x,y-size*.28f,z,body);
    ocean_tri(camera,x-dir*size*.82f,y,z,x,y+size*.34f,z,x,y-size*.28f,z,shade);
    ocean_tri(camera,x-dir*size*.82f,y,z,x-dir*size*1.34f,y+size*.36f,z,
              x-dir*size*1.34f,y-size*.36f,z,tail);
}

static void rotate_local(float x,float y,float z,float roll,float yaw,
                         float *ox,float *oy,float *oz)
{
    float c=cosf(roll),s=sinf(roll),cy=cosf(yaw),sy=sinf(yaw);
    float a=x*c-y*s,b=x*s+y*c;
    *ox=a*cy+z*sy;*oy=b;*oz=-a*sy+z*cy;
}

static void jelly_world(const ocean_jelly_t *jelly,float x,float y,float z,
                        float *wx,float *wy,float *wz)
{
    float lx,ly,lz;rotate_local(x,y,z,jelly->roll,jelly->yaw,&lx,&ly,&lz);
    *wx=jelly->x+lx*jelly->radius;*wy=jelly->y+ly*jelly->radius;*wz=jelly->z+lz*jelly->radius;
}

static void draw_ocean_jelly(const living_camera_t *camera,const ocean_jelly_t *jelly,float t)
{
    float contract=1.0f-jelly->pulse*.145f;
    float h=.76f*(1.0f+jelly->pulse*.15f);
    float wx,wy,wz;
    Vector2 top,left,right;
    jelly_world(jelly,0,h-.04f,0,&wx,&wy,&wz);
    bool cap_valid=ocean_project(camera,wx,wy,wz,&top);
    jelly_world(jelly,-contract,-.04f,0,&wx,&wy,&wz);
    cap_valid=cap_valid&&ocean_project(camera,wx,wy,wz,&left);
    jelly_world(jelly,contract,-.04f,0,&wx,&wy,&wz);
    cap_valid=cap_valid&&ocean_project(camera,wx,wy,wz,&right);
    if(cap_valid){
        float rim_x=(left.x+right.x)*.5f;
        float rim_y=(left.y+right.y)*.5f;
        float height=rim_y-top.y;
        float radius=fabsf(right.x-left.x)*.5f;
        if(height>1.0f&&radius>1.0f){
            int first_x=(int)(rim_x-radius);if((float)first_x<rim_x-radius)++first_x;
            int last_x=(int)(rim_x+radius);if((float)last_x>rim_x+radius)--last_x;
            for(int x=first_x;x<=last_x;++x){
                float q=ocean_clamp(((float)x+.5f-rim_x)/radius,-1.0f,1.0f);
                float curve=sqrtf(fmaxf(0.0f,1.0f-q*q));
                float top_y=rim_y-height*curve;
                float bot_y=rim_y+height*.055f*curve*curve;
                int first_y=(int)top_y;if((float)first_y<top_y)++first_y;
                int last_y=(int)bot_y;if((float)last_y>bot_y)--last_y;
                unsigned char alpha=(unsigned char)(96+(1.0f-curve)*24+jelly->flash*42);
                Color fill=(Color){92,190,229,alpha};
                if(last_y>=first_y)DrawRectangle(x,first_y,1,last_y-first_y+1,fill);
            }
        }
    }
    int tentacles=jelly->id==0?10:7;
    for(int i=0;i<tentacles;++i){
        float ang=i/(float)tentacles*6.2831853f;
        Vector2 last;bool has=false;
        for(int s=0;s<=8;++s){
            float f=s/8.0f;
            float wx,wy,wz;
            jelly_world(jelly,cosf(ang)*.24f+sinf(t*.95f-f*5.5f+i)*.18f*f,
                        -.13f-f*3.1f,sinf(ang)*.22f+sinf(f*4.0f-t*.8f+i)*.23f*f,
                        &wx,&wy,&wz);
            Vector2 screen;
            if(!ocean_project(camera,wx,wy,wz,&screen)){has=false;continue;}
            if(has)DrawLine((int)last.x,(int)last.y,(int)screen.x,(int)screen.y,
                            (Color){69,155,209,(unsigned char)(70+40*(1.0f-f))});
            last=screen;has=true;
        }
    }
}

static void draw_ocean_wanderer(const living_camera_t *camera,const ocean_wanderer_t *item,float t)
{
    float s=item->size,dir=item->dir,x=item->x,y=item->y,z=item->z,a=item->alpha;
    if(item->kind==OCEAN_WANDER_RAY){
        float w=s*(1.25f+.14f*sinf(t*1.1f+item->phase));
        Color shade=(Color){105,186,210,(unsigned char)(120*a)};
        ocean_tri(camera,x+dir*s*1.1f,y,z,x-dir*s*.1f,y+s*.34f,z-w,x,y,z,shade);
        ocean_tri(camera,x+dir*s*1.1f,y,z,x-dir*s*.1f,y+s*.34f,z+w,x,y,z,shade);
    }else if(item->kind==OCEAN_WANDER_TURTLE){
        float flap=sinf(t*2.1f+item->phase)*s*.15f;
        ocean_tri(camera,x+dir*s*.96f,y+s*.02f,z,x,y+s*.42f,z,x,y-s*.34f,z,
                  (Color){128,192,171,(unsigned char)(130*a)});
        ocean_tri(camera,x+s*.1f,y+s*.1f,z,x+s*.58f,y+s*.38f+flap,z+s*.18f,
                  x+s*.18f,y+s*.08f,z+s*.3f,(Color){112,178,160,(unsigned char)(86*a)});
    }else if(item->kind==OCEAN_WANDER_SEAHORSE){
        Vector2 head,belly,tail,snout;
        if(ocean_project(camera,x+dir*s*.42f,y+s*.18f,z,&head)&&
           ocean_project(camera,x,y,z,&belly)&&
           ocean_project(camera,x-dir*s*.18f,y-s*.46f,z,&tail)&&
           ocean_project(camera,x+dir*s*.62f,y+s*.16f,z,&snout)){
            Color color=(Color){165,231,243,(unsigned char)(90*a)};
            DrawLine((int)head.x,(int)head.y,(int)belly.x,(int)belly.y,color);
            DrawLine((int)belly.x,(int)belly.y,(int)tail.x,(int)tail.y,color);
            DrawLine((int)head.x,(int)head.y,(int)snout.x,(int)snout.y,color);
        }
    }else{
        ocean_tri(camera,x+dir*s*1.26f,y+s*.02f,z,x,y+s*.30f,z,x,y-s*.22f,z,
                  (Color){88,123,152,(unsigned char)(90*a)});
        ocean_tri(camera,x-dir*s*1.18f,y,z,x,y+s*.30f,z,x,y-s*.22f,z,
                  (Color){65,96,124,(unsigned char)(80*a)});
    }
}

void underwater_ocean_draw(const underwater_ocean_t *ocean,float yaw,float pitch,
                           uint8_t effects_level,MosaicoAtlas water,
                           MosaicoAtlas left_front,MosaicoAtlas left_side,
                           MosaicoAtlas left_rear,MosaicoAtlas right_front,
                           MosaicoAtlas right_side,MosaicoAtlas right_rear)
{
    float nx=yaw/OCEAN_YAW_LIMIT,ny=pitch/OCEAN_PITCH_LIMIT,length=sqrtf(nx*nx+ny*ny);
    if(length>1.0f){yaw/=length;pitch/=length;}
    living_camera_t camera=living_camera_orbit(yaw,pitch,OCEAN_FOCUS);
    draw_ocean_water(ocean,&camera,water);
    draw_ocean_reefs(&camera,yaw,left_front,left_side,left_rear,right_front,right_side,right_rear);
    float t=ocean->tick*OCEAN_DT;
    int shoals=effects_level==0?3:ocean->shoal_count;
    int wanderers=effects_level==0?3:ocean->wanderer_count;
    int jellies=effects_level==0?3:ocean->jelly_count;
    uint8_t order[OCEAN_SHOAL_CAP+OCEAN_WANDERER_CAP+OCEAN_JELLY_CAP];
    uint8_t kind[OCEAN_SHOAL_CAP+OCEAN_WANDERER_CAP+OCEAN_JELLY_CAP];
    float depth[OCEAN_SHOAL_CAP+OCEAN_WANDERER_CAP+OCEAN_JELLY_CAP];
    int items=0;
    for(int i=0;i<shoals;++i){order[items]=(uint8_t)i;kind[items]=0;depth[items]=ocean->shoals[i].z;++items;}
    for(int i=0;i<wanderers;++i){order[items]=(uint8_t)i;kind[items]=1;depth[items]=ocean->wanderers[i].z;++items;}
    for(int i=0;i<jellies;++i){order[items]=(uint8_t)i;kind[items]=2;depth[items]=ocean->jellies[i].z;++items;}
    for(int i=1;i<items;++i){
        int j=i;
        while(j>0&&depth[j-1]<depth[j]){
            uint8_t so=order[j-1],sk=kind[j-1];float sd=depth[j-1];
            order[j-1]=order[j];kind[j-1]=kind[j];depth[j-1]=depth[j];
            order[j]=so;kind[j]=sk;depth[j]=sd;--j;
        }
    }
    for(int i=0;i<items;++i){
        if(kind[i]==0){
            const ocean_shoal_t *shoal=&ocean->shoals[order[i]];
            int count=effects_level==0?shoal->count/2:shoal->count;
            for(int f=0;f<count;++f){
                float u=count>1?f/(float)(count-1)-.5f:0;
                float wob=sinf(t*(1.2f+shoal->sway)+shoal->phase+f*.72f);
                draw_ocean_fish(&camera,shoal->x+u*shoal->spread*shoal->dir*.95f,
                                shoal->y+sinf(t*1.1f+shoal->phase+f*.65f)*.04f+wob*.012f,
                                shoal->z+cosf(f*.8f+shoal->phase)*.22f,
                                shoal->size*(1.0f-.35f*fabsf(u)*2.0f),shoal->dir,
                                (Color){shoal->r,shoal->g,shoal->b,255},
                                .72f+.22f*(1.0f-fabsf(u)*1.6f));
            }
        }else if(kind[i]==1){
            draw_ocean_wanderer(&camera,&ocean->wanderers[order[i]],t);
        }else{
            draw_ocean_jelly(&camera,&ocean->jellies[order[i]],t);
        }
    }
    int motes=effects_level==0?14:(effects_level==2?ocean->mote_count:24);
    if(motes>ocean->mote_count)motes=ocean->mote_count;
    for(int i=0;i<motes;++i){
        const ocean_mote_t *mote=&ocean->motes[i];
        Vector2 screen;
        if(!ocean_project(&camera,mote->x,mote->y,mote->z,&screen))continue;
        if(mote->bubble){
            DrawCircleLines((int)screen.x,(int)screen.y,2+(i%3),(Color){114,197,225,90});
        }else{
            DrawCircle((int)screen.x,(int)screen.y,1,(Color){195,238,255,80});
        }
    }
}
#endif
