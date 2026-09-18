// SPDX-License-Identifier: Apache-2.0
#include "underwater_aurora.h"
#include <math.h>
#include <stdbool.h>
#include <string.h>
#ifndef UNDERWATER_SCENE_SIM_ONLY
#include "aurora_depth.h"
#include "aurora_ice_volume.h"
#include "mosaico_raylib_fast.h"
#include "underwater_aurora_draw.h"
#include "underwater_volume.h"
#endif

#define AURORA_DT (1.0f/30.0f)
#define AURORA_FOCAL (480.0f*1.055f)


static uint32_t aurora_random(underwater_aurora_t *aurora)
{
    aurora->rng=aurora->rng*1664525U+1013904223U;
    return aurora->rng;
}

static float aurora_unit(underwater_aurora_t *aurora)
{
    return (float)(aurora_random(aurora)>>8)*(1.0f/16777216.0f);
}

static float aurora_range(underwater_aurora_t *aurora,float low,float high)
{
    return low+(high-low)*aurora_unit(aurora);
}

static void aurora_unproject(float sx,float sy,float z,float *x,float *y)
{
    *x=(sx-240.0f)*z/AURORA_FOCAL;
    *y=(240.0f-sy)*z/AURORA_FOCAL;
}

static void aurora_spawn_mote(underwater_aurora_t *aurora,aurora_mote_t *mote,
                              float sx,float sy,float z)
{
    aurora_unproject(sx,sy,z,&mote->x,&mote->y);
    mote->z=z;
    mote->phase=aurora_range(aurora,0,6.2831853f);
    mote->size=aurora_range(aurora,.007f,.025f);
    mote->vy=aurora_range(aurora,-.04f,-.012f);
}

static void aurora_spawn_meteor(underwater_aurora_t *aurora,float sx,float sy,bool hero)
{
    if(aurora->meteor_count>=AURORA_METEOR_CAP){
        memmove(aurora->meteors,aurora->meteors+1,
                (AURORA_METEOR_CAP-1U)*sizeof(aurora->meteors[0]));
        aurora->meteor_count=AURORA_METEOR_CAP-1;
    }
    aurora_meteor_t *meteor=&aurora->meteors[aurora->meteor_count++];
    float z=aurora_range(aurora,65.0f,105.0f);
    aurora_unproject(sx,sy,z,&meteor->x,&meteor->y);
    meteor->z=z;
    float speed=aurora_range(aurora,24.0f,36.0f);
    meteor->x+=speed*.4f;meteor->y+=speed*.4f;
    meteor->vx=-speed;meteor->vy=-speed*.56f;
    meteor->age=0;meteor->life=1.7f;
    meteor->tail=hero?15:(uint8_t)(8U+aurora_random(aurora)%6U);
}

void underwater_aurora_reset(underwater_aurora_t *aurora)
{
    memset(aurora,0,sizeof(*aurora));
    aurora->rng=826721U;
    aurora->mote_count=AURORA_MOTE_CAP;
    aurora->glint_count=AURORA_GLINT_CAP;
    aurora->marsh_count=AURORA_MARSH_CAP;
    aurora->stream_count=AURORA_STREAM_CAP;
    aurora->pulse=.5f;
    aurora->next_meteor=26;
    aurora->next_shower=150;
    for(unsigned i=0;i<aurora->mote_count;++i)
        aurora_spawn_mote(aurora,&aurora->motes[i],aurora_range(aurora,0,480),
                          aurora_range(aurora,5,475),aurora_range(aurora,3.2f,90.0f));
    for(unsigned i=0;i<aurora->glint_count;++i){
        float z=aurora_range(aurora,62.0f,108.0f);
        aurora_unproject(aurora_range(aurora,18,462),aurora_range(aurora,18,206),z,
                         &aurora->glints[i].x,&aurora->glints[i].y);
        aurora->glints[i].z=z;
        aurora->glints[i].phase=aurora_range(aurora,0,6.2831853f);
        aurora->glints[i].size=aurora_range(aurora,.012f,.03f);
        aurora->glints[i].drift=aurora_range(aurora,.02f,.06f);
    }
    for(unsigned i=0;i<aurora->marsh_count;++i){
        float z=aurora_range(aurora,8.0f,28.0f);
        aurora_unproject(aurora_range(aurora,18,462),aurora_range(aurora,272,438),z,
                         &aurora->marsh[i].x,&aurora->marsh[i].y);
        aurora->marsh[i].z=z;
        aurora->marsh[i].phase=aurora_range(aurora,0,6.2831853f);
        aurora->marsh[i].size=aurora_range(aurora,.016f,.04f);
        aurora->marsh[i].drift=aurora_range(aurora,.03f,.08f);
    }
    for(unsigned i=0;i<aurora->stream_count;++i){
        aurora->streams[i].lane=i/5.0f;
        aurora->streams[i].phase=aurora_range(aurora,0,6.2831853f);
        aurora->streams[i].depth=aurora_range(aurora,70,98);
        aurora->streams[i].amp=aurora_range(aurora,14,28);
        aurora->streams[i].alpha=aurora_range(aurora,.9f,1.4f);
    }
    aurora_spawn_meteor(aurora,340.0f,95.0f,true);
}

void underwater_aurora_look(float *yaw,float *pitch,float *yaw_velocity,
                            float *pitch_velocity,float dx,float dy)
{
    if(dx!=0||dy!=0){
        *yaw_velocity=-dx*0.050f;
        *pitch_velocity=dy*0.042f;
        *yaw-=dx*0.050f;
        *pitch+=dy*0.042f;
    }
    float nx=*yaw/AURORA_YAW_LIMIT,ny=*pitch/AURORA_PITCH_LIMIT;
    float length=sqrtf(nx*nx+ny*ny);
    if(length>1.0f){*yaw/=length;*pitch/=length;*yaw_velocity*=.25f;*pitch_velocity*=.25f;}
}

void underwater_aurora_tap(underwater_aurora_t *aurora,float x,float y)
{
    if(y<240.0f){
        aurora_spawn_meteor(aurora,x,y,true);
        if(aurora_unit(aurora)<.65f)
            aurora_spawn_meteor(aurora,x+aurora_range(aurora,-34,34),
                                y+aurora_range(aurora,-18,18),false);
    }else if(aurora->ripple_count<AURORA_RIPPLE_CAP){
        aurora_ripple_t *ripple=&aurora->ripples[aurora->ripple_count++];
        aurora_unproject(x,y,18.0f,&ripple->x,&ripple->y);
        ripple->z=18.0f;ripple->age=0;
    }
}

void underwater_aurora_update(underwater_aurora_t *aurora)
{
    ++aurora->tick;
    float t=aurora->tick*AURORA_DT;
    aurora->flow_x*=.962f;aurora->flow_y*=.962f;
    aurora->pulse=.45f+.42f*(.5f+.5f*sinf(t*.6f))+.15f*sinf(t*2.0f);
    for(unsigned i=0;i<aurora->glint_count;++i)
        aurora->glints[i].phase+=AURORA_DT*(1.0f+aurora->glints[i].drift*1.6f);
    for(unsigned i=0;i<aurora->marsh_count;++i)
        aurora->marsh[i].phase+=AURORA_DT*(1.2f+aurora->marsh[i].drift*1.4f);
    if(aurora->next_meteor>0)--aurora->next_meteor;
    if(aurora->next_shower>0)--aurora->next_shower;
    if(aurora->next_meteor==0){
        aurora_spawn_meteor(aurora,aurora_range(aurora,70,460),
                            aurora_range(aurora,42,180),aurora_unit(aurora)<.28f);
        if(aurora_unit(aurora)<.58f)
            aurora_spawn_meteor(aurora,aurora_range(aurora,85,448),
                                aurora_range(aurora,42,165),false);
        aurora->next_meteor=(uint16_t)(20U+aurora_random(aurora)%30U);
    }
    if(aurora->next_shower==0){
        for(int i=0;i<7;++i)
            aurora_spawn_meteor(aurora,aurora_range(aurora,65,458),
                                aurora_range(aurora,42,160),i==0);
        aurora->next_shower=(uint16_t)(135U+aurora_random(aurora)%75U);
    }
    unsigned kept=0;
    for(unsigned i=0;i<aurora->meteor_count;++i){
        aurora_meteor_t meteor=aurora->meteors[i];
        meteor.x+=meteor.vx*AURORA_DT;
        meteor.y+=meteor.vy*AURORA_DT;
        meteor.age+=AURORA_DT;
        if(meteor.age<meteor.life)aurora->meteors[kept++]=meteor;
    }
    aurora->meteor_count=(uint8_t)kept;
    kept=0;
    for(unsigned i=0;i<aurora->ripple_count;++i){
        aurora->ripples[i].age+=AURORA_DT;
        if(aurora->ripples[i].age<3.5f)aurora->ripples[kept++]=aurora->ripples[i];
    }
    aurora->ripple_count=(uint8_t)kept;
    for(unsigned i=0;i<aurora->mote_count;++i){
        aurora_mote_t *mote=&aurora->motes[i];
        mote->x+=(sinf(t*.34f+mote->phase)*.018f+aurora->flow_x*.07f)*AURORA_DT;
        mote->y+=(mote->vy+aurora->flow_y*.03f)*AURORA_DT;
        float sx=240.0f+mote->x*AURORA_FOCAL/mote->z;
        float sy=240.0f-mote->y*AURORA_FOCAL/mote->z;
        if(sy<-12.0f||sy>492.0f||sx<-15.0f||sx>495.0f)
            aurora_spawn_mote(aurora,mote,aurora_range(aurora,3,477),
                              mote->vy>0?490.0f:-8.0f,mote->z);
    }
}

#ifndef UNDERWATER_SCENE_SIM_ONLY
static void aurora_clamp_cone(float *yaw,float *pitch)
{
    float nx=*yaw/AURORA_YAW_LIMIT,ny=*pitch/AURORA_PITCH_LIMIT;
    float length=sqrtf(nx*nx+ny*ny);
    if(length>1.0f){*yaw/=length;*pitch/=length;}
}

static float aurora_mirror(float value)
{
    value=fmodf(value,2.0f);
    if(value<0)value+=2.0f;
    return value<=1.0f?value:2.0f-value;
}

static int aurora_depth_index(int value)
{
    if(value<0)return 0;
    if(value>AURORA_DEPTH_GRID)return AURORA_DEPTH_GRID;
    return value;
}

static bool aurora_project_depth(const living_camera_t *camera,float u,float v,
                                 uint16_t raw,Vector2 *out)
{
    float inverse=(float)raw/65535.0f*.3f;
    float z=1.0f/fmaxf(.007f,inverse);
    return living_project_xyz(camera,(u-.5f)*480.0f*z/AURORA_FOCAL*1.14f,
                              (.5f-v)*480.0f*z/AURORA_FOCAL*1.14f,z,out);
}

static void draw_aurora_light_field(const underwater_aurora_t *aurora,
                                    const living_camera_t *camera,MosaicoAtlas space)
{
    if(!space.texture.id||!camera)return;
    const int n=AURORA_DEPTH_GRID;
    enum { X0=-4, Y0=-2, GW=17, GH=13 };
    static Vector2 mesh[GW*GH];
    static uint8_t ok[GW*GH];
    static uint16_t rawz[GW*GH];
    static float tu[GW],tv[GH],uu[GW],vv[GH];
    static float cached_tw=-1.0f,cached_th=-1.0f;
    float tex_w=(float)space.texture.width-1.0f,tex_h=(float)space.texture.height-1.0f;
    if(cached_tw!=tex_w||cached_th!=tex_h){
        for(int ix=0;ix<GW;++ix){
            uu[ix]=(float)(X0+ix)/n;
            tu[ix]=aurora_mirror(uu[ix])*tex_w;
        }
        for(int iy=0;iy<GH;++iy){
            vv[iy]=(float)(Y0+iy)/n;
            tv[iy]=aurora_mirror(vv[iy])*tex_h;
        }
        for(int iy=0;iy<GH;++iy){
            int dj=aurora_depth_index(Y0+iy);
            for(int ix=0;ix<GW;++ix)
                rawz[iy*GW+ix]=AURORA_DEPTH[dj*(n+1)+aurora_depth_index(X0+ix)];
        }
        cached_tw=tex_w;cached_th=tex_h;
    }
    for(int iy=0;iy<GH;++iy)
    for(int ix=0;ix<GW;++ix){
        int idx=iy*GW+ix;
        ok[idx]=(uint8_t)aurora_project_depth(camera,uu[ix],vv[iy],rawz[idx],&mesh[idx]);
    }
    enum { QUAD_CAP=(GW-1)*(GH-1) };
    static uint16_t band_ix[8][QUAD_CAP];
    static uint16_t band_iy[8][QUAD_CAP];
    int band_n[8];
    memset(band_n,0,sizeof band_n);
    for(int iy=0;iy<GH-1;++iy)
    for(int ix=0;ix<GW-1;++ix){
        int a=iy*GW+ix,b=a+1,c=a+GW,d=c+1;
        if(!ok[a]||!ok[b]||!ok[c]||!ok[d])continue;
        unsigned average=((unsigned)rawz[a]+rawz[b]+rawz[c]+rawz[d])/4U;
        int band=(int)(average*8U/65536U);
        if(band<0)band=0;
        if(band>7)band=7;
        float left=fminf(fminf(mesh[a].x,mesh[b].x),fminf(mesh[c].x,mesh[d].x));
        float right=fmaxf(fmaxf(mesh[a].x,mesh[b].x),fmaxf(mesh[c].x,mesh[d].x));
        float top=fminf(fminf(mesh[a].y,mesh[b].y),fminf(mesh[c].y,mesh[d].y));
        float bottom=fmaxf(fmaxf(mesh[a].y,mesh[b].y),fmaxf(mesh[c].y,mesh[d].y));
        if(right<0||left>480||bottom<0||top>480)continue;
        int slot=band_n[band]++;
        band_ix[band][slot]=(uint16_t)ix;
        band_iy[band][slot]=(uint16_t)iy;
    }
    for(int band=0;band<8;++band){
        for(int i=0;i<band_n[band];++i){
            int ix=band_ix[band][i],iy=band_iy[band][i];
            int a=iy*GW+ix,b=a+1,c=a+GW,d=c+1;
            mosaico_textured_vertex_t va={mesh[a].x,mesh[a].y,tu[ix],tv[iy]};
            mosaico_textured_vertex_t vb={mesh[b].x,mesh[b].y,tu[ix+1],tv[iy]};
            mosaico_textured_vertex_t vc={mesh[c].x,mesh[c].y,tu[ix],tv[iy+1]};
            mosaico_textured_vertex_t vd={mesh[d].x,mesh[d].y,tu[ix+1],tv[iy+1]};
            Mosaico2DDrawTexturedTriangle(space.texture,va,vc,vb,256);
            Mosaico2DDrawTexturedTriangle(space.texture,vb,vc,vd,256);
        }
    }
    (void)aurora;
}

static void draw_aurora_ice(const living_camera_t *camera,MosaicoAtlas front,MosaicoAtlas side,
                            MosaicoAtlas rear)
{
    float fw=(float)front.texture.width,fh=(float)front.texture.height;
    living_draw_volume(camera,AURORA_ICE_REAR_VERTICES,AURORA_ICE_REAR_VERTEX_COUNT,
        AURORA_ICE_REAR_FACES,AURORA_ICE_REAR_FACE_COUNT,rear,512,512,3);
    living_draw_volume_uv(camera,AURORA_ICE_SIDE_VERTICES,AURORA_ICE_SIDE_VERTEX_COUNT,
        AURORA_ICE_SIDE_FACES,AURORA_ICE_SIDE_FACE_COUNT,front,1024,256,
        0.0f,fh*.58f,fw,fh,2);
    living_draw_volume(camera,AURORA_ICE_FRONT_VERTICES,AURORA_ICE_FRONT_VERTEX_COUNT,
        AURORA_ICE_FRONT_FACES,AURORA_ICE_FRONT_FACE_COUNT,front,768,768,1);
    (void)side;
}

static void aurora_dot(const living_camera_t *camera,float x,float y,float z,
                       float size,Color color)
{
    Vector2 screen;
    if(!living_project_xyz(camera,x,y,z,&screen))return;
    if(screen.x<-8||screen.x>488||screen.y<-8||screen.y>488)return;
    float radius=fmaxf(1.0f,size*AURORA_FOCAL/fmaxf(z,.8f));
    DrawCircle((int)screen.x,(int)screen.y,radius,color);
}

void underwater_aurora_draw(const underwater_aurora_t *aurora,float yaw,float pitch,
                            uint8_t effects_level,MosaicoAtlas space,
                            MosaicoAtlas ice_front,MosaicoAtlas ice_side,
                            MosaicoAtlas ice_rear)
{
    aurora_clamp_cone(&yaw,&pitch);
    living_camera_t camera=living_camera_orbit(yaw,pitch,AURORA_FOCUS);
    draw_aurora_light_field(aurora,&camera,space);
    draw_aurora_ice(&camera,ice_front,ice_side,ice_rear);
    float t=aurora->tick*AURORA_DT;
    int streams=effects_level==0?3:(effects_level==2?6:5);
    if(streams>aurora->stream_count)streams=aurora->stream_count;
    for(int i=0;i<streams;++i){
        const aurora_stream_t *stream=&aurora->streams[i];
        Vector2 prev={0,0};bool has_prev=false;
        for(int s=0;s<18;++s){
            float f=s/17.0f;
            float sx=36.0f+stream->lane*78.0f+sinf(t*.23f+stream->phase+f*2.2f)*stream->amp+f*24.0f;
            float sy=22.0f+f*182.0f+sinf(t*.58f+stream->phase+f*4.9f)*14.0f;
            float wx,wy;aurora_unproject(sx,sy,stream->depth+f*2.8f,&wx,&wy);
            Vector2 screen;
            if(!living_project_xyz(&camera,wx,wy,stream->depth+f*2.8f,&screen)){
                has_prev=false;continue;
            }
            if(has_prev)
                DrawLine((int)prev.x,(int)prev.y,(int)screen.x,(int)screen.y,
                         (Color){145,255,225,(unsigned char)((18+28*aurora->pulse)*
                         stream->alpha*(1.0f-f*.72f))});
            prev=screen;has_prev=true;
        }
    }
    for(unsigned i=0;i<aurora->glint_count;++i){
        const aurora_glint_t *glint=&aurora->glints[i];
        unsigned char alpha=(unsigned char)(28+70*(.5f+.5f*sinf(t*1.9f+glint->phase)));
        aurora_dot(&camera,glint->x+sinf(t*.25f+glint->phase)*.12f,
                   glint->y+cosf(t*.32f+glint->phase)*.06f,glint->z,glint->size,
                   (Color){220,255,246,alpha});
    }
    for(unsigned i=0;i<aurora->marsh_count;++i){
        const aurora_glint_t *marsh=&aurora->marsh[i];
        unsigned char alpha=(unsigned char)((40+36*(.5f+.5f*sinf(t*1.4f+marsh->phase)))*
                                            (.9f+.55f*aurora->pulse));
        aurora_dot(&camera,marsh->x+sinf(t*.45f+marsh->phase)*.06f,
                   marsh->y+cosf(t*.4f+marsh->phase)*.04f,marsh->z,marsh->size,
                   (Color){168,237,223,alpha});
    }
    int meteors=aurora->meteor_count;
    if(effects_level==0&&meteors>4)meteors=4;
    for(int i=0;i<meteors;++i){
        const aurora_meteor_t *meteor=&aurora->meteors[i];
        float alpha=fminf(1.0f,meteor->age*6.0f)*fminf(1.0f,(meteor->life-meteor->age)*3.0f);
        float norm=sqrtf(meteor->vx*meteor->vx+meteor->vy*meteor->vy);
        if(norm<.01f)continue;
        float len=meteor->tail*fminf(1.0f,meteor->age*5.0f);
        Vector2 prev;bool has_prev=false;
        for(int s=0;s<12;++s){
            float d=s/11.0f*len;
            Vector2 screen;
            if(!living_project_xyz(&camera,meteor->x-meteor->vx/norm*d,
                                   meteor->y-meteor->vy/norm*d,meteor->z,&screen)){
                has_prev=false;continue;
            }
            if(has_prev)
                DrawLine((int)prev.x,(int)prev.y,(int)screen.x,(int)screen.y,
                         (Color){169,206,239,(unsigned char)(alpha*(1.0f-s/11.0f)*190.0f)});
            prev=screen;has_prev=true;
        }
        aurora_dot(&camera,meteor->x,meteor->y,meteor->z,.17f,
                   (Color){255,255,255,(unsigned char)(alpha*230.0f)});
    }
    for(unsigned i=0;i<aurora->ripple_count;++i){
        const aurora_ripple_t *ripple=&aurora->ripples[i];
        for(int ring=0;ring<3;++ring){
            float age=ripple->age-ring*.20f;
            if(age<0)continue;
            float rad=age*.9f;
            unsigned char alpha=(unsigned char)((1.0f-age/3.5f)*120.0f);
            Vector2 prev;bool has_prev=false;
            for(int s=0;s<=24;++s){
                float a=s/24.0f*6.2831853f;
                Vector2 screen;
                if(!living_project_xyz(&camera,ripple->x+cosf(a)*rad,ripple->y+.007f,
                                       ripple->z+sinf(a)*rad,&screen)){
                    has_prev=false;continue;
                }
                if(has_prev)
                    DrawLineEx(prev,screen,1.35f,(Color){145,203,197,alpha});
                prev=screen;has_prev=true;
            }
        }
    }
    int motes=effects_level==0?16:(effects_level==2?aurora->mote_count:26);
    if(motes>aurora->mote_count)motes=aurora->mote_count;
    for(int i=0;i<motes;++i){
        const aurora_mote_t *mote=&aurora->motes[i];
        unsigned char alpha=(unsigned char)(50+40*(.5f+.5f*sinf(t*.8f+mote->phase)));
        aurora_dot(&camera,mote->x,mote->y,mote->z,mote->size,(Color){232,247,251,alpha});
    }
}
#endif
