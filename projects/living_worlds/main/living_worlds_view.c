// SPDX-License-Identifier: Apache-2.0
#include "living_worlds_view.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "mosaico_raylib_fast.h"
#include "sunrise_depth.h"
#include "sunrise_volume.h"
#include "living_worlds_aurora_draw.h"
#include "living_worlds_ocean_draw.h"
#include "living_worlds_volume.h"

#define PANORAMA_WIDTH 1600.0f
#define PANORAMA_HEIGHT 800.0f
#define VIEW_WIDTH 500.0f
#define VIEW_HEIGHT 520.0f
#define VIEW_FOV_DEG (360.0f*VIEW_WIDTH/PANORAMA_WIDTH)

static float signed_angle(float angle)
{
    while(angle>180.0f)angle-=360.0f;
    while(angle<-180.0f)angle+=360.0f;
    return angle;
}

static float world_to_screen_x(float longitude,float camera_yaw)
{
    return 240.0f+signed_angle(longitude-camera_yaw)*(480.0f/VIEW_FOV_DEG);
}

static int effect_count(const living_world_t *world,int calm,int living,int vivid)
{
    return world->effects_level==0?calm:(world->effects_level==2?vivid:living);
}

static void draw_panorama_band(MosaicoAtlas panorama,float source_x,float source_y,
                               float source_width,float source_height,
                               float dest_y,float dest_height)
{
    float panorama_width=(float)panorama.texture.width;
    float first=panorama_width-source_x;
    if(first>source_width)first=source_width;
    float first_dest=first*(480.0f/source_width);
    DrawTexturePro(panorama.texture,
                   (Rectangle){source_x,source_y,first,source_height},
                   (Rectangle){0,dest_y,first_dest,dest_height},
                   (Vector2){0,0},0,WHITE);
    if(first<source_width){
        float second=source_width-first;
        DrawTexturePro(panorama.texture,
                       (Rectangle){0,source_y,second,source_height},
                       (Rectangle){first_dest,dest_y,480-first_dest,dest_height},
                       (Vector2){0,0},0,WHITE);
    }
}

static void draw_panorama(const living_world_t *world,MosaicoAtlas panorama)
{
    float panorama_width=(float)panorama.texture.width;
    float panorama_height=(float)panorama.texture.height;
    float view_width=panorama_width*(VIEW_WIDTH/PANORAMA_WIDTH);
    float view_height=panorama_height*(VIEW_HEIGHT/PANORAMA_HEIGHT);
    float source_x=world->yaw*(panorama_width/360.0f);
    /* Keep equal angular density on both axes. The previous 150-degree vertical
       view compressed cliffs and valleys into a flat postcard. */
    float neutral_y=(panorama_height-view_height)*.5f;
    float pitch_pixels=world->pitch*(panorama_height/PANORAMA_HEIGHT)*4.0f;
    /* A horizon-anchored vertical projection: distant sky moves least while
       close ground moves most. Boundary-derived source coordinates keep every
       strip continuous, and pitch zero remains the original one-pass path. */
    if(fabsf(world->pitch)<.25f){
        draw_panorama_band(panorama,source_x,neutral_y,view_width,view_height,0,480);
        return;
    }
    enum { VERTICAL_BANDS=16 };
    for(int band=0;band<VERTICAL_BANDS;++band){
        float v0=(float)band/VERTICAL_BANDS;
        float v1=(float)(band+1)/VERTICAL_BANDS;
        float factor0=.65f+.75f*v0;
        float factor1=.65f+.75f*v1;
        float y0=neutral_y+view_height*v0+pitch_pixels*factor0;
        float y1=neutral_y+view_height*v1+pitch_pixels*factor1;
        if(y0<0)y0=0;
        if(y1>panorama_height)y1=panorama_height;
        draw_panorama_band(panorama,source_x,y0,view_width,y1-y0,
                           480.0f*v0,480.0f*(v1-v0)+.25f);
    }
}

typedef struct {
    float x,y,z;
    float m[9];
} sunrise_camera_t;

static sunrise_camera_t sunrise_camera(const living_world_t *world)
{
    const float focus=10.5f;
    float yaw=world->yaw*.01745329252f,pitch=world->pitch*.01745329252f;
    float cp=cosf(pitch),radius=focus;
    sunrise_camera_t camera={.x=radius*sinf(yaw)*cp,
        .y=radius*sinf(pitch),.z=focus-radius*cosf(yaw)*cp};
    float dx=-camera.x,dy=-camera.y,dz=focus-camera.z;
    float length=sqrtf(dx*dx+dy*dy+dz*dz);
    float fx=dx/length,fy=dy/length,fz=dz/length;
    float right_length=sqrtf(fz*fz+fx*fx),rx=fz/right_length,rz=-fx/right_length;
    camera.m[0]=rx;camera.m[1]=0;camera.m[2]=rz;
    camera.m[3]=fy*rz;camera.m[4]=fz*rx-fx*rz;camera.m[5]=-fy*rx;
    camera.m[6]=fx;camera.m[7]=fy;camera.m[8]=fz;
    return camera;
}

static bool sunrise_project(const sunrise_camera_t *camera,float u,float v,
                            uint16_t raw_depth,Vector2 *out)
{
    const float focal=480.0f*1.055f;
    const float xy_scale=480.0f*1.14f/focal;
    float inverse_depth=(float)raw_depth*(.3f/65535.0f);
    float z=1.0f/fmaxf(.007f,inverse_depth);
    float scale=z*xy_scale;
    float x=(u-.5f)*scale;
    float y=(.5f-v)*scale;
    float dx=x-camera->x,dy=y-camera->y,dz=z-camera->z;
    float view_z=dx*camera->m[6]+dy*camera->m[7]+dz*camera->m[8];
    if(view_z<.2f)return false;
    float inv=focal/view_z;
    out->x=240.0f+inv*(dx*camera->m[0]+dz*camera->m[2]);
    out->y=240.0f-inv*(dx*camera->m[3]+dy*camera->m[4]+dz*camera->m[5]);
    return true;
}

static bool sunrise_project_xyz(const sunrise_camera_t *camera,float x,float y,
                                float z,Vector2 *out)
{
    const float focal=480.0f*1.055f;
    float dx=x-camera->x,dy=y-camera->y,dz=z-camera->z;
    float view_z=dx*camera->m[6]+dy*camera->m[7]+dz*camera->m[8];
    if(view_z<.2f)return false;
    float inv=focal/view_z;
    out->x=240.0f+inv*(dx*camera->m[0]+dz*camera->m[2]);
    out->y=240.0f-inv*(dx*camera->m[3]+dy*camera->m[4]+dz*camera->m[5]);
    return true;
}

static float mirror_unit(float value)
{
    value=fmodf(value,2.0f);
    if(value<0)value+=2.0f;
    return value<=1.0f?value:2.0f-value;
}

static int sunrise_depth_index(int value)
{
    if(value<0)return 0;
    if(value>SUNRISE_DEPTH_GRID)return SUNRISE_DEPTH_GRID;
    return value;
}

static void draw_sunrise_orbit(const living_world_t *world,MosaicoAtlas sunrise)
{
    sunrise_camera_t camera=sunrise_camera(world);
    const int n=SUNRISE_DEPTH_GRID;
    enum {
        HORIZONTAL_EDGE_TILES=4,
        VERTICAL_EDGE_TILES=2,
        X0=-HORIZONTAL_EDGE_TILES,
        Y0=-VERTICAL_EDGE_TILES,
        GW=SUNRISE_DEPTH_GRID+HORIZONTAL_EDGE_TILES*2+1,
        GH=SUNRISE_DEPTH_GRID+VERTICAL_EDGE_TILES*2+1
    };
    static Vector2 mesh[GW*GH];
    static uint8_t ok[GW*GH];
    static uint16_t rawz[GW*GH];
    static float tu[GW],tv[GH],uu[GW],vv[GH];
    static float cached_tw=-1.0f,cached_th=-1.0f;
    float tex_w=(float)sunrise.texture.width-1.0f;
    float tex_h=(float)sunrise.texture.height-1.0f;
    if(cached_tw!=tex_w||cached_th!=tex_h){
        for(int ix=0;ix<GW;++ix){
            uu[ix]=(float)(X0+ix)/n;
            tu[ix]=mirror_unit(uu[ix])*tex_w;
        }
        for(int iy=0;iy<GH;++iy){
            vv[iy]=(float)(Y0+iy)/n;
            tv[iy]=mirror_unit(vv[iy])*tex_h;
        }
        for(int iy=0;iy<GH;++iy){
            int dj=sunrise_depth_index(Y0+iy);
            for(int ix=0;ix<GW;++ix)
                rawz[iy*GW+ix]=SUNRISE_DEPTH[dj*(n+1)+sunrise_depth_index(X0+ix)];
        }
        cached_tw=tex_w;cached_th=tex_h;
    }
    for(int iy=0;iy<GH;++iy)
    for(int ix=0;ix<GW;++ix){
        int idx=iy*GW+ix;
        ok[idx]=(uint8_t)sunrise_project(&camera,uu[ix],vv[iy],rawz[idx],&mesh[idx]);
    }
    /* Same 8 painter bands and the same 16x12 tiles; vertices are projected once. */
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
        Vector2 p0=mesh[a],p1=mesh[b],p2=mesh[c],p3=mesh[d];
        float left=fminf(fminf(p0.x,p1.x),fminf(p2.x,p3.x));
        float right=fmaxf(fmaxf(p0.x,p1.x),fmaxf(p2.x,p3.x));
        float top=fminf(fminf(p0.y,p1.y),fminf(p2.y,p3.y));
        float bottom=fmaxf(fmaxf(p0.y,p1.y),fmaxf(p2.y,p3.y));
        if(right<0||left>=480||bottom<0||top>=480)continue;
        if((right-left)*(bottom-top)<1.5f)continue;
        if(living_cover_quad(p0,p1,p2,p3))continue;
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
            Mosaico2DDrawTexturedQuad(sunrise.texture,va,vb,vc,vd,256);
        }
    }
}

static Vector2 sunrise_volume_screen[SUNRISE_FRONT_VERTEX_COUNT];
static uint8_t sunrise_volume_valid[SUNRISE_FRONT_VERTEX_COUNT];

static void draw_sunrise_volume_part(const sunrise_camera_t *camera,
    const sunrise_volume_vertex_t *vertices,int vertex_count,
    const sunrise_volume_face_t *faces,int face_count,MosaicoAtlas atlas,
    float authored_width,float authored_height,int part,int cover_only)
{
    if(vertex_count>SUNRISE_FRONT_VERTEX_COUNT)return;
    if(!cover_only&&!atlas.texture.id)return;
    for(int i=0;i<vertex_count;++i){
        const sunrise_volume_vertex_t *v=&vertices[i];
        sunrise_volume_valid[i]=(uint8_t)sunrise_project_xyz(camera,v->x*.001f,
            v->y*.001f,v->z*.001f,&sunrise_volume_screen[i]);
    }
    float scale_u=atlas.texture.width/authored_width;
    float scale_v=atlas.texture.height/authored_height;
    for(int i=0;i<face_count;++i){
        unsigned ia=faces[i].a,ib=faces[i].b,ic=faces[i].c;
        if(!sunrise_volume_valid[ia]||!sunrise_volume_valid[ib]||
           !sunrise_volume_valid[ic])continue;
        const sunrise_volume_vertex_t *a=&vertices[ia],*b=&vertices[ib],*c=&vertices[ic];
        if(part==3){
            float cx=(a->x+b->x+c->x)*.000333333f;
            float cy=(a->y+b->y+c->y)*.000333333f;
            float cz=(a->z+b->z+c->z)*.000333333f;
            float facing=faces[i].nx*(camera->x-cx)+faces[i].ny*(camera->y-cy)+
                         faces[i].nz*(camera->z-cz);
            if(facing<=0)continue;
        }
        Vector2 pa=sunrise_volume_screen[ia],pb=sunrise_volume_screen[ib],
            pc=sunrise_volume_screen[ic];
        float area=(pb.x-pa.x)*(pc.y-pa.y)-(pb.y-pa.y)*(pc.x-pa.x);
        if(fabsf(area)<=.25f)continue;
        if(part==1&&area<0)continue;
        if(part==2&&area>0)continue;
        float min_x=fminf(pa.x,fminf(pb.x,pc.x));
        float max_x=fmaxf(pa.x,fmaxf(pb.x,pc.x));
        float min_y=fminf(pa.y,fminf(pb.y,pc.y));
        float max_y=fmaxf(pa.y,fmaxf(pb.y,pc.y));
        if(max_x<0||min_x>=480||max_y<0||min_y>=480)continue;
        int local_side_patch=part==2&&(i==110||i==111);
        if(cover_only){
            living_cover_add_triangle(pa,pb,pc);
            continue;
        }
        if(local_side_patch)continue;
        float au,av,bu,bv,cu,cv;
        if(part==2||part==3){
            /* HTML ClosedLandscape sides use world-space UVs, not the strip unwrap. */
            const float k=.056f;
            if(abs(faces[i].ny)>abs(faces[i].nx)){
                au=a->x*k;av=a->z*k;
                bu=b->x*k;bv=b->z*k;
                cu=c->x*k;cv=c->z*k;
            }else{
                au=a->z*k;av=a->y*k;
                bu=b->z*k;bv=b->y*k;
                cu=c->z*k;cv=c->y*k;
            }
        }else{
            au=a->u*scale_u;av=a->v*scale_v;bu=b->u*scale_u;bv=b->v*scale_v;
            cu=c->u*scale_u;cv=c->v*scale_v;
        }
        mosaico_textured_vertex_t va={pa.x,pa.y,au,av};
        mosaico_textured_vertex_t vb={pb.x,pb.y,bu,bv};
        mosaico_textured_vertex_t vc={pc.x,pc.y,cu,cv};
        Mosaico2DDrawTexturedTriangle(atlas.texture,va,vb,vc,faces[i].light);
    }
}

static void draw_sunrise_ridge_patch(const sunrise_camera_t *camera,
                                     MosaicoAtlas side)
{
    if(!side.texture.id)return;
    /* Faces 110/111 are the single marked fold between side rings 1 and 2.
       Replace only that quad, sampled from the neighboring grass face. */
    static const uint16_t rings[2][2]={{55,56},{91,92}};
    static const float source_v[2]={52.0f,214.0f};
    Vector2 screen[2][2];
    for(int ring=0;ring<2;++ring)
    for(int edge=0;edge<2;++edge){
        const sunrise_volume_vertex_t *v=&SUNRISE_SIDE_VERTICES[rings[ring][edge]];
        if(!sunrise_project_xyz(camera,v->x*.001f,v->y*.001f,v->z*.001f,
                                &screen[ring][edge]))return;
    }
    for(int ring=0;ring<1;++ring){
        mosaico_textured_vertex_t a={screen[ring][0].x,screen[ring][0].y,
                                      42.0f,source_v[ring]};
        mosaico_textured_vertex_t b={screen[ring][1].x,screen[ring][1].y,
                                      220.0f,source_v[ring]};
        mosaico_textured_vertex_t c={screen[ring+1][0].x,screen[ring+1][0].y,
                                      42.0f,source_v[ring+1]};
        mosaico_textured_vertex_t d={screen[ring+1][1].x,screen[ring+1][1].y,
                                      220.0f,source_v[ring+1]};
        Mosaico2DDrawTexturedQuad(side.texture,a,b,c,d,248U);
    }
}

static void draw_sunrise_cliff(const sunrise_camera_t *camera,
    MosaicoAtlas front,MosaicoAtlas side,MosaicoAtlas rear,int cover_only)
{
    draw_sunrise_volume_part(camera,SUNRISE_REAR_VERTICES,SUNRISE_REAR_VERTEX_COUNT,
        SUNRISE_REAR_FACES,SUNRISE_REAR_FACE_COUNT,rear,512,512,3,cover_only);
    draw_sunrise_volume_part(camera,SUNRISE_SIDE_VERTICES,SUNRISE_SIDE_VERTEX_COUNT,
        SUNRISE_SIDE_FACES,SUNRISE_SIDE_FACE_COUNT,side,
        (float)side.texture.width,(float)side.texture.height,2,cover_only);
    draw_sunrise_volume_part(camera,SUNRISE_FRONT_VERTICES,SUNRISE_FRONT_VERTEX_COUNT,
        SUNRISE_FRONT_FACES,SUNRISE_FRONT_FACE_COUNT,front,768,768,1,cover_only);
    if(!cover_only)draw_sunrise_ridge_patch(camera,side);
}

typedef struct {
    float longitude;
    float source_v;
} rainforest_flow_point_t;

static Vector2 rainforest_flow_project(const living_world_t *world,
                                       MosaicoAtlas rainforest,Vector2 source)
{
    float height=(float)rainforest.texture.height;
    float view_height=height*(VIEW_HEIGHT/PANORAMA_HEIGHT);
    float neutral_y=(height-view_height)*.5f;
    float pitch_pixels=world->pitch*(height/PANORAMA_HEIGHT)*4.0f;
    float denominator=view_height+pitch_pixels*.75f;
    float v=(source.y*height-neutral_y-pitch_pixels*.65f)/denominator;
    /* draw_panorama() treats yaw as the panorama crop's left edge, not its
       center.  Match that exact wrapped source interval so water never drifts
       onto land while the camera crosses the 360-degree seam. */
    float longitude=source.x;
    while(longitude<0.0f)longitude+=360.0f;
    while(longitude>=360.0f)longitude-=360.0f;
    float delta=longitude-world->yaw;
    while(delta<0.0f)delta+=360.0f;
    while(delta>=360.0f)delta-=360.0f;
    return (Vector2){delta*(480.0f/VIEW_FOV_DEG),v*480.0f};
}

static int rainforest_on_screen(Vector2 p)
{
    return p.x>-16.0f&&p.x<496.0f&&p.y>8.0f&&p.y<412.0f;
}

static Vector2 rainforest_unproject(const living_world_t *world,
                                    MosaicoAtlas rainforest,float x,float y)
{
    float height=(float)rainforest.texture.height;
    float view_height=height*(VIEW_HEIGHT/PANORAMA_HEIGHT);
    float neutral_y=(height-view_height)*.5f;
    float pitch_pixels=world->pitch*(height/PANORAMA_HEIGHT)*4.0f;
    float denominator=view_height+pitch_pixels*.75f;
    if(denominator<.001f)return (Vector2){0,0};
    float source_v=(y/480.0f*denominator+neutral_y+pitch_pixels*.65f)/height;
    float lon=world->yaw+(x/480.0f)*VIEW_FOV_DEG;
    while(lon<0.0f)lon+=360.0f;
    while(lon>=360.0f)lon-=360.0f;
    return (Vector2){lon,source_v};
}

static int rainforest_in_poly(float lon,float v,const rainforest_flow_point_t *p,
                              int count)
{
    int inside=0;
    for(int i=0,j=count-1;i<count;j=i++){
        float yi=p[i].source_v,yj=p[j].source_v;
        if((yi>v)==(yj>v))continue;
        float xi=p[i].longitude,xj=p[j].longitude;
        if(lon<(xj-xi)*(v-yi)/(yj-yi)+xi)inside=!inside;
    }
    return inside;
}

static int rainforest_in_water(float lon,float v)
{
    static const rainforest_flow_point_t left_stream[]={
        {116.0f,.720f},{140.0f,.705f},{158.0f,.698f},
        {159.0f,.725f},{140.0f,.738f},{118.0f,.748f}
    };
    static const rainforest_flow_point_t pool[]={
        {148.0f,.710f},{158.0f,.668f},{170.0f,.654f},{180.0f,.650f},
        {194.0f,.660f},{210.0f,.688f},{224.0f,.722f},{226.0f,.755f},
        {216.0f,.788f},{200.0f,.808f},{180.0f,.812f},{164.0f,.798f},
        {152.0f,.768f},{147.0f,.738f}
    };
    static const rainforest_flow_point_t right_stream[]={
        {286.0f,.685f},{300.0f,.662f},{314.0f,.668f},{328.0f,.690f},
        {342.0f,.718f},{348.0f,.742f},{340.0f,.768f},{322.0f,.762f},
        {304.0f,.740f},{290.0f,.722f}
    };
    static const rainforest_flow_point_t lower_stream_a[]={
        {340.0f,.718f},{349.0f,.736f},{359.5f,.760f},
        {359.5f,.838f},{350.0f,.814f},{340.0f,.770f}
    };
    static const rainforest_flow_point_t lower_stream_b[]={
        {.5f,.758f},{4.0f,.750f},{8.0f,.755f},
        {8.0f,.790f},{4.0f,.820f},{.5f,.838f}
    };
    return rainforest_in_poly(lon,v,left_stream,
               (int)(sizeof(left_stream)/sizeof(left_stream[0])))||
           rainforest_in_poly(lon,v,pool,
               (int)(sizeof(pool)/sizeof(pool[0])))||
           rainforest_in_poly(lon,v,right_stream,
               (int)(sizeof(right_stream)/sizeof(right_stream[0])))||
           rainforest_in_poly(lon,v,lower_stream_a,
               (int)(sizeof(lower_stream_a)/sizeof(lower_stream_a[0])))||
           rainforest_in_poly(lon,v,lower_stream_b,
               (int)(sizeof(lower_stream_b)/sizeof(lower_stream_b[0])));
}

/* DrawEllipseLines only stamps the left/right x of each scanline, so a
   ripple collapses into a C. Stroke a closed oval instead. */
static void draw_rainforest_ripple(int cx,int cy,float rh,float rv,Color color)
{
    const int steps=18;
    float prev_x=(float)cx+rh,prev_y=(float)cy;
    for(int i=1;i<=steps;++i){
        float a=(float)i*(6.2831853f/(float)steps);
        float x=(float)cx+cosf(a)*rh;
        float y=(float)cy+sinf(a)*rv;
        DrawLine((int)prev_x,(int)prev_y,(int)x,(int)y,color);
        prev_x=x;prev_y=y;
    }
}

typedef struct {
    rainforest_flow_point_t left;
    rainforest_flow_point_t right;
} rainforest_flow_slice_t;

static rainforest_flow_point_t rainforest_flow_sample_ribbon(
    const rainforest_flow_slice_t *slices,int count,float progress,float lane)
{
    float scaled=fminf(.9999f,fmaxf(0.0f,progress))*(float)(count-1);
    int segment=(int)scaled;
    float t=scaled-(float)segment;
    rainforest_flow_point_t a={
        slices[segment].left.longitude+
            (slices[segment].right.longitude-slices[segment].left.longitude)*lane,
        slices[segment].left.source_v+
            (slices[segment].right.source_v-slices[segment].left.source_v)*lane
    };
    rainforest_flow_point_t b={
        slices[segment+1].left.longitude+
            (slices[segment+1].right.longitude-slices[segment+1].left.longitude)*lane,
        slices[segment+1].left.source_v+
            (slices[segment+1].right.source_v-slices[segment+1].left.source_v)*lane
    };
    return (rainforest_flow_point_t){
        a.longitude+(b.longitude-a.longitude)*t,
        a.source_v+(b.source_v-a.source_v)*t
    };
}

static mosaico_textured_vertex_t rainforest_flow_vertex(
    const living_world_t *world,MosaicoAtlas rainforest,
    const rainforest_flow_slice_t *slices,int count,float progress,float lane,
    float phase,float inset,float speed_scale)
{
    float actual_lane=inset+lane*(1.0f-inset*2.0f);
    rainforest_flow_point_t destination=rainforest_flow_sample_ribbon(
        slices,count,progress,actual_lane);
    /* Two forward-travelling waves deform the photographed water itself.
       Their unequal wavelength and velocity prevent a synchronized push/pull.
       Fade deformation at the traced bank so rocks and plants stay locked. */
    float bank=4.0f*fminf(lane,1.0f-lane);
    float end=5.0f*fminf(progress,1.0f-progress);
    float envelope=fminf(1.0f,fminf(bank,end));
    float time=(float)world->tick*speed_scale;
    float flow=progress+phase;
    float longitudinal=(sinf(flow*31.0f-time*.105f)*.0080f+
                        sinf(flow*53.0f-time*.071f)*.0035f)*envelope;
    float lateral=(sinf(flow*39.0f-time*.083f+lane*5.0f)*.0070f)*envelope;
    rainforest_flow_point_t source=rainforest_flow_sample_ribbon(slices,count,
        fminf(.9999f,fmaxf(0.0f,progress+longitudinal)),
        fminf(.9999f,fmaxf(0.0f,actual_lane+lateral)));
    Vector2 p=rainforest_flow_project(world,rainforest,
        (Vector2){destination.longitude,destination.source_v});
    return (mosaico_textured_vertex_t){
        p.x,p.y,
        source.longitude*((float)rainforest.texture.width/360.0f),
        source.source_v*(float)rainforest.texture.height
    };
}

static void draw_rainforest_flow_mesh(const living_world_t *world,
                                      MosaicoAtlas rainforest,
                                      const rainforest_flow_slice_t *slices,
                                      int count,float phase,float inset,
                                      float speed_scale)
{
    int along=(count-1)*effect_count(world,3,4,5);
    int across=effect_count(world,2,3,4);
    for(int iy=0;iy<along;++iy){
        float p0=(float)iy/(float)along;
        float p1=(float)(iy+1)/(float)along;
        for(int ix=0;ix<across;++ix){
            float l0=(float)ix/(float)across;
            float l1=(float)(ix+1)/(float)across;
            mosaico_textured_vertex_t a=rainforest_flow_vertex(
                world,rainforest,slices,count,p0,l0,phase,inset,speed_scale);
            mosaico_textured_vertex_t b=rainforest_flow_vertex(
                world,rainforest,slices,count,p0,l1,phase,inset,speed_scale);
            mosaico_textured_vertex_t c=rainforest_flow_vertex(
                world,rainforest,slices,count,p1,l0,phase,inset,speed_scale);
            mosaico_textured_vertex_t d=rainforest_flow_vertex(
                world,rainforest,slices,count,p1,l1,phase,inset,speed_scale);
            float min_x=fminf(fminf(a.x,b.x),fminf(c.x,d.x));
            float max_x=fmaxf(fmaxf(a.x,b.x),fmaxf(c.x,d.x));
            float min_y=fminf(fminf(a.y,b.y),fminf(c.y,d.y));
            float max_y=fmaxf(fmaxf(a.y,b.y),fmaxf(c.y,d.y));
            if(max_x<0.0f||min_x>=480.0f||max_y<0.0f||min_y>=420.0f||
               max_x-min_x>120.0f)continue;
            Mosaico2DDrawTexturedQuad(rainforest.texture,a,b,c,d,256U);
        }
    }
}

static void draw_rainforest_texture_flow(const living_world_t *world,
                                         MosaicoAtlas rainforest)
{
    static const rainforest_flow_slice_t pool[]={
        {{170.0f,.654f},{194.0f,.660f}},
        {{158.0f,.680f},{210.0f,.688f}},
        {{150.0f,.716f},{224.0f,.726f}},
        {{152.0f,.764f},{216.0f,.788f}},
        {{164.0f,.798f},{200.0f,.808f}}
    };
    static const rainforest_flow_slice_t left_stream[]={
        {{154.0f,.700f},{159.0f,.724f}},
        {{138.0f,.708f},{146.0f,.736f}},
        {{118.0f,.722f},{124.0f,.746f}}
    };
    static const rainforest_flow_slice_t right_stream[]={
        {{286.0f,.686f},{290.0f,.722f}},
        {{300.0f,.664f},{306.0f,.740f}},
        {{316.0f,.672f},{324.0f,.760f}},
        {{336.0f,.704f},{342.0f,.764f}}
    };
    static const rainforest_flow_slice_t lower_stream_a[]={
        {{336.0f,.704f},{342.0f,.764f}},
        {{346.0f,.730f},{352.0f,.804f}},
        {{359.5f,.760f},{359.5f,.838f}}
    };
    static const rainforest_flow_slice_t lower_stream_b[]={
        {{.5f,.758f},{.5f,.838f}},
        {{4.0f,.750f},{4.0f,.820f}},
        {{8.0f,.755f},{8.0f,.790f}}
    };
    static const rainforest_flow_slice_t waterfall[]={
        {{264.0f,.308f},{270.0f,.308f}},
        {{265.0f,.356f},{272.0f,.356f}},
        {{266.0f,.407f},{274.0f,.407f}},
        {{267.0f,.458f},{276.0f,.458f}},
        {{268.0f,.500f},{277.0f,.500f}}
    };
    draw_rainforest_flow_mesh(world,rainforest,pool,
        (int)(sizeof(pool)/sizeof(pool[0])),0.0f,.28f,1.30f);
    draw_rainforest_flow_mesh(world,rainforest,left_stream,
        (int)(sizeof(left_stream)/sizeof(left_stream[0])),1.3f,.16f,1.45f);
    draw_rainforest_flow_mesh(world,rainforest,right_stream,
        (int)(sizeof(right_stream)/sizeof(right_stream[0])),2.0f,.16f,1.45f);
    draw_rainforest_flow_mesh(world,rainforest,lower_stream_a,
        (int)(sizeof(lower_stream_a)/sizeof(lower_stream_a[0])),3.0f,.08f,1.20f);
    draw_rainforest_flow_mesh(world,rainforest,lower_stream_b,
        (int)(sizeof(lower_stream_b)/sizeof(lower_stream_b[0])),4.0f,.08f,1.20f);
    draw_rainforest_flow_mesh(world,rainforest,waterfall,
        (int)(sizeof(waterfall)/sizeof(waterfall[0])),5.2f,.08f,3.20f);
}

/* One entry, then ripples. A polyline across the still pool reads as a worm. */
static void draw_rainforest_entry(const living_world_t *world,
                                  MosaicoAtlas rainforest,
                                  rainforest_flow_point_t site,float scale,
                                  unsigned phase_offset)
{
    Vector2 p=rainforest_flow_project(world,rainforest,
        (Vector2){site.longitude,site.source_v});
    if(!rainforest_on_screen(p))return;
    float cycle=fmodf((float)(world->tick+phase_offset),42.0f)/42.0f;
    if(cycle<.28f){
        float birth=cycle/.28f;
        int spray=effect_count(world,2,3,4);
        for(int i=0;i<spray;++i){
            float lift=sinf(birth*3.14159265f);
            float px=p.x+(-2.4f+(float)i*1.8f)*birth*scale;
            float py=p.y-(3.8f+(float)i)*lift*scale;
            unsigned char alpha=(unsigned char)(185.0f*(1.0f-birth));
            DrawCircle((int)px,(int)py,1.0f+(1.0f-birth)*scale,
                       (Color){246,250,248,alpha});
        }
        DrawEllipse((int)p.x,(int)p.y,2.4f+birth*5.2f*scale,1.0f+birth*1.7f*scale,
                    (Color){236,244,240,(unsigned char)(150.0f*(1.0f-birth))});
    }
    int rings=effect_count(world,2,3,4);
    for(int ring=0;ring<rings;++ring){
        float t=fmodf(cycle+(float)ring/(float)rings,1.0f);
        float fade=(1.0f-t)*(1.0f-t);
        unsigned char alpha=(unsigned char)(140.0f*fade);
        if(alpha<16)continue;
        float rh=(4.0f+t*22.0f)*scale;
        draw_rainforest_ripple((int)p.x,(int)(p.y+t*1.2f*scale),rh,rh/2.15f,
                               (Color){214,230,232,alpha});
    }
}

static void draw_rainforest_water(const living_world_t *world,
                                  MosaicoAtlas rainforest)
{
    if(!rainforest.texture.id||rainforest.texture.height<=0)return;
    draw_rainforest_texture_flow(world,rainforest);
    static const rainforest_flow_point_t entries[]={
        {174.0f,.676f},
        {322.2f,.703f},
        {265.5f,.456f}
    };
    static const float scales[]={1.0f,.82f,.40f};
    static const unsigned offsets[]={0U,19U,31U};
    for(int i=0;i<3;++i)
        draw_rainforest_entry(world,rainforest,entries[i],scales[i],offsets[i]);
}

static uint32_t rainforest_rain_hash(uint32_t value)
{
    value^=value>>16;value*=0x7feb352dU;value^=value>>15;
    value*=0x846ca68bU;value^=value>>16;return value;
}

static void draw_rainforest_rain(const living_world_t *world,
                                 MosaicoAtlas rainforest)
{
    /* Viewer-space weather, with continuous per-drop depth rather than five
       visible speed tiers.  Near drops are longer, brighter and occasionally
       thicker; distant rain dissolves into the photographed haze. */
    int n=effect_count(world,16,30,44);
    float wind=-1.3f+.4f*sinf((float)world->tick*.011f);
    int can_hit=rainforest.texture.id&&rainforest.texture.height>0;
    for(int i=0;i<n;++i){
        uint32_t seed=rainforest_rain_hash(0x9e3779b9u*(uint32_t)(i+1));
        float near=.06f+(float)(seed&1023U)*(1.0f/1100.0f);
        float x=(float)((seed>>10)%520)-20.0f+
                sinf((float)world->tick*.017f+(float)i)*near*2.2f;
        float speed=5.5f+near*15.5f;
        float length=8.0f+near*31.0f;
        float y=fmodf((float)((seed>>19)%460)+(float)world->tick*speed,470.0f)-24.0f;
        if(y>420.0f)continue;
        unsigned char alpha=(unsigned char)(34.0f+near*148.0f);
        float gust=((float)((seed>>5)&31U)-15.5f)*.018f;
        float tip_x=x+(wind+gust)*(.55f+near*.75f),tip_y=y+length;
        Color rain=(Color){231,242,245,alpha};
        if(y<400.0f){
            if(near>.78f)
                DrawLineEx((Vector2){x,y},(Vector2){tip_x,tip_y},1.45f,rain);
            else
                DrawLine((int)x,(int)y,(int)tip_x,(int)tip_y,rain);
        }
        if(!can_hit||tip_x<-8.0f||tip_x>488.0f)continue;
        /* Sparse leaf/rock impacts: a two-frame crown, only outside traced
           water polygons.  This makes rain contact the jungle without
           painting persistent decorations onto the panorama. */
        if((seed&3U)==0U){
            float impact_y=105.0f+(float)((seed>>12)%230U);
            float distance=tip_y-impact_y;
            Vector2 impact_src=rainforest_unproject(world,rainforest,tip_x,impact_y);
            if(distance>=0.0f&&distance<speed*1.35f&&
               !rainforest_in_water(impact_src.x,impact_src.y)){
                float burst=1.0f-distance/(speed*1.35f);
                Color splash=(Color){235,247,245,(unsigned char)(145.0f*burst)};
                float spread=2.0f+near*2.5f;
                DrawLine((int)tip_x,(int)impact_y,(int)(tip_x-spread),
                         (int)(impact_y-spread*.55f),splash);
                DrawLine((int)tip_x,(int)impact_y,(int)(tip_x+spread),
                         (int)(impact_y-spread*.45f),splash);
            }
        }
        /* A streak can lie on the pool after the tip has already reached
           rock. Find the water surface under this drop and keep a ripple
           there while the segment still crosses water. */
        Vector2 hit={0};
        float hit_y=0;
        int found=0;
        float from_y=y,to_y=tip_y+6.0f;
        for(int sample=0;sample<=10;++sample){
            float sy=from_y+(to_y-from_y)*((float)sample/10.0f);
            if(sy<8.0f||sy>412.0f)continue;
            Vector2 src=rainforest_unproject(world,rainforest,tip_x,sy);
            if(!rainforest_in_water(src.x,src.y))continue;
            hit=src;hit_y=sy;found=1;break;
        }
        if(!found)continue;
        for(int step=0;step<14;++step){
            float prev_y=hit_y-4.0f;
            Vector2 prev=rainforest_unproject(world,rainforest,tip_x,prev_y);
            if(prev_y<40.0f||!rainforest_in_water(prev.x,prev.y))break;
            hit=prev;hit_y=prev_y;
        }
        float age=fmodf((tip_y-hit_y)/fmaxf(1.0f,speed),18.0f);
        float t=age/18.0f;
        Vector2 p=rainforest_flow_project(world,rainforest,hit);
        if(!rainforest_on_screen(p))continue;
        unsigned char ring=(unsigned char)(175.0f*(1.0f-t*.65f));
        float rh=(3.4f+t*12.0f)*(1.12f-near*.34f);
        draw_rainforest_ripple((int)p.x,(int)p.y,rh,rh/2.15f,
                               (Color){226,240,242,ring});
        if(t<.28f){
            DrawCircle((int)p.x,(int)p.y,1.3f+(1.0f-t/.28f),
                       (Color){246,250,248,(unsigned char)(180.0f*(1.0f-t/.28f))});
        }
    }
}

static void draw_rainforest_fx(const living_world_t *world,
                               MosaicoAtlas rainforest)
{
    draw_rainforest_water(world,rainforest);
    /* Morpho butterflies stay sparse and move in world longitude. */
    static const float base_lon[]={42.0f,167.0f,284.0f};
    static const float base_y[]={176.0f,292.0f,116.0f};
    int butterflies=effect_count(world,1,2,3);
    for(int i=0;i<butterflies;++i){
        float phase=world->tick*(.018f+i*.003f)+i*2.3f;
        float lon=base_lon[i]+sinf(phase*.35f)*9.0f;
        int x=(int)world_to_screen_x(lon,world->yaw);
        int y=(int)(base_y[i]+sinf(phase)*15.0f-world->pitch*4.1f);
        if(x<-12||x>492||y<5||y>450)continue;
        int wing=1+(int)(fabsf(sinf(phase*4.2f))*2.0f);
        Color blue=(Color){22,107,187,205};
        DrawLine(x,y,x-wing-1,y-wing,blue);DrawLine(x,y,x+wing+1,y-wing,blue);
        DrawCircle(x-wing-1,y-wing,wing,blue);DrawCircle(x+wing+1,y-wing,wing,blue);
        DrawLine(x,y-2,x,y+5,(Color){24,34,28,240});
    }
    /* A distant toucan crossing is deliberately rare and understated. */
    float bird_phase=fmodf(world->tick*.055f,720.0f);
    float bird_lon=205.0f+bird_phase*.5f;
    int bx=(int)world_to_screen_x(fmodf(bird_lon,360.0f),world->yaw);
    int by=92+(int)(sinf(world->tick*.08f)*8.0f)-(int)(world->pitch*3.7f);
    if(bx>-24&&bx<504){
        DrawCircle(bx,by,5,(Color){20,27,21,235});
        DrawLine(bx-3,by,bx-13,by-5,(Color){18,25,20,220});
        DrawLine(bx+4,by-1,bx+12,by-2,(Color){207,128,37,230});
    }
    /* Pollen can live in the photographed volume. Rain cannot: a 5px mint
       dash locked to longitude sits on a leaf and crawls when yaw changes. */
    int motes=effect_count(world,3,7,12);
    for(int i=0;i<motes;++i){
        float phase=world->tick*.014f+i*1.71f;
        float lon=fmodf(19.0f+i*79.0f+sinf(phase)*2.4f,360.0f);
        int x=(int)world_to_screen_x(lon,world->yaw);
        int y=88+(i*53)%270+(int)(sinf(phase*.7f)*10.0f)-(int)(world->pitch*3.9f);
        if(x>2&&x<478)DrawCircle(x,y,1,(Color){221,231,172,(unsigned char)(70+i%3*30)});
    }
    draw_rainforest_rain(world,rainforest);
}

#if 0
static void draw_aurora_fx(const living_world_t *world)
{
    /* Slow translucent curtains enlarge and animate the aurora without resampling it. */
    int curtains=effect_count(world,2,4,6);
    for(int i=0;i<curtains;++i){
        float phase=world->tick*.012f+i*1.31f;
        int x=(int)world_to_screen_x_layer(34.0f+i*61.0f,world->yaw,.93f);
        int sway=(int)(sinf(phase)*24.0f);
        Color glow=(i&1)?(Color){92,238,184,28}:(Color){99,174,255,24};
        int py=(int)(-world->pitch*.55f);
        DrawLine(x+sway,8+py,x-sway/2,190+py+(i%3)*34,glow);
        DrawLine(x+sway+5,8+py,x-sway/2+18,214+py+(i%2)*30,glow);
    }
    int stars=effect_count(world,16,34,52);
    for(int i=0;i<stars;++i){
        float lon=fmodf(7.0f+i*47.3f,360.0f);
        int x=(int)world_to_screen_x(lon,world->yaw);
        int y=(i*73+17)%208+10-(int)(world->pitch*.65f);
        if(x<2||x>478)continue;
        unsigned char a=(unsigned char)(75+55*sinf(world->tick*.045f+i*1.7f));
        DrawCircle(x,y,(i%5==0)?2:1,(Color){220,255,246,a});
    }
    for(int i=0;i<3;++i){
        float phase=world->tick*.035f+i*2.1f;
        float lon=fmodf(48.0f+i*119.0f+sinf(phase)*7.0f,360.0f);
        int x=(int)world_to_screen_x(lon,world->yaw);
        int y=54+i*37+(int)(cosf(phase*.7f)*18.0f)-(int)(world->pitch*.8f);
        if(x<-45||x>520)continue;
        DrawLine(x,y,x-42,y+24,(Color){210,238,255,(unsigned char)(155-i*25)});
        DrawLine(x-1,y,x-29,y+16,(Color){255,255,255,210});
    }
    int snow=effect_count(world,5,11,20);
    for(int i=0;i<snow;++i){
        int layer=i%3;
        float lon=fmodf(13.0f+i*41.0f+world->tick*.01f*(1+layer),360.0f);
        int x=(int)world_to_screen_x_layer(lon,world->yaw,1.0f+layer*.035f);
        int y=(i*59+(int)(world->tick*(1+layer)/2))%390+55-
              (int)(world->pitch*(2.0f+layer*1.4f));
        if(x<-4||x>484)continue;
        DrawCircle(x,y,1+layer,(Color){232,247,251,(unsigned char)(90+layer*45)});
    }
}
#endif

static uint32_t sunrise_particle_hash(uint32_t value)
{
    value^=value>>16;value*=0x7feb352dU;value^=value>>15;
    value*=0x846ca68bU;value^=value>>16;return value;
}

static void sunrise_seed_point(const living_sunrise_seed_t *seed,
                               float x,float y,float z,float *wx,float *wy,float *wz)
{
    float cz=cosf(seed->rz),sz=sinf(seed->rz);
    float x1=x*cz-y*sz,y1=x*sz+y*cz;
    float cy=cosf(seed->ry),sy=sinf(seed->ry);
    float x2=x1*cy+z*sy,z2=-x1*sy+z*cy;
    float cx=cosf(seed->rx),sx=sinf(seed->rx);
    float y2=y1*cx-z2*sx,z3=y1*sx+z2*cx;
    *wx=seed->x+x2;*wy=seed->y+y2;*wz=seed->z+z3;
}

static bool sunrise_seed_project(const sunrise_camera_t *camera,
                                 const living_sunrise_seed_t *seed,
                                 float x,float y,float z,Vector2 *screen)
{
    float wx,wy,wz;
    sunrise_seed_point(seed,x,y,z,&wx,&wy,&wz);
    return sunrise_project_xyz(camera,wx,wy,wz,screen);
}

static Color sunrise_seed_tint(unsigned char r,unsigned char g,unsigned char b,
                               float alpha,float fade)
{
    float value=alpha*fade*255.0f;
    if(value<0)value=0;
    if(value>255.0f)value=255.0f;
    return (Color){r,g,b,(unsigned char)value};
}

static void draw_dandelion_seed_3d(const sunrise_camera_t *camera,
                                   const living_sunrise_seed_t *seed,
                                   int spokes)
{
    const float rad=seed->radius;
    Vector2 root,base,husk;
    if(!sunrise_seed_project(camera,seed,0,0,0,&root)||
       !sunrise_seed_project(camera,seed,.005f,-rad*1.5f,0,&base))return;
    if(root.x<-40||root.x>520||root.y<18||root.y>508)return;
    float dx=root.x-base.x,dy=root.y-base.y;
    float apparent=sqrtf(dx*dx+dy*dy);
    if(apparent<2.2f)return;
    float fade=fminf(1.0f,.42f+apparent*.055f);
    float stem_w=fmaxf(1.15f,fminf(2.6f,1.05f+apparent*.085f));
    float silk_w=fmaxf(1.08f,fminf(2.05f,1.02f+apparent*.055f));
    if(spokes>16&&apparent<9.0f)spokes=14;
    DrawLineEx(root,base,stem_w,sunrise_seed_tint(111,70,24,.62f,fade));
    if(sunrise_seed_project(camera,seed,0,-rad*1.7f,0,&husk))
        DrawCircle((int)husk.x,(int)husk.y,apparent>8.0f?2:1,
                   sunrise_seed_tint(139,78,22,.55f,fade));
    for(int i=0;i<spokes;++i){
        float angle=6.2831853f*i/(float)spokes;
        float ca=cosf(angle),sa=sinf(angle);
        Vector2 bend,tip;
        if(!sunrise_seed_project(camera,seed,ca*rad*.45f,rad*.31f,sa*rad*.45f,&bend)||
           !sunrise_seed_project(camera,seed,ca*rad,rad*(.49f+.06f*sinf(i*2.0f)),sa*rad,&tip))
            continue;
        DrawLineEx(root,bend,silk_w,sunrise_seed_tint(203,170,121,.48f,fade));
        DrawLineEx(bend,tip,silk_w,sunrise_seed_tint(239,218,165,.58f,fade));
        if(apparent>7.0f&&(i%3)==0)
            DrawCircle((int)tip.x,(int)tip.y,1,sunrise_seed_tint(255,211,135,.42f,fade));
    }
}

static void draw_sunrise_seeds(const living_world_t *world,bool foreground)
{
    sunrise_camera_t camera=sunrise_camera(world);
    uint8_t order[LIVING_SUNRISE_SEED_CAP];
    int count=world->sunrise_seed_count;
    if(count>LIVING_SUNRISE_SEED_CAP)count=LIVING_SUNRISE_SEED_CAP;
    for(int i=0;i<count;++i){
        order[i]=(uint8_t)i;
        int j=i;
        while(j>0&&world->sunrise_seeds[order[j-1]].z<
                         world->sunrise_seeds[order[j]].z){
            uint8_t swap=order[j-1];order[j-1]=order[j];order[j]=swap;--j;
        }
    }
    int spokes=world->effects_level==0?14:23;
    for(int i=0;i<count;++i){
        const living_sunrise_seed_t *seed=&world->sunrise_seeds[order[i]];
        bool is_foreground=seed->z<=6.2f;
        if(seed->active&&is_foreground==foreground)
            draw_dandelion_seed_3d(&camera,seed,spokes);
    }
}

static void draw_sunrise_fx(const living_world_t *world)
{
    float t=world->tick*.028f;
    float orbit_x=world->yaw/14.0f;
    float orbit_y=world->pitch/8.0f;
    for(int i=0;i<5;++i){
        int x=45+i*96+(int)(sinf(t*.35f+i)*24.0f-orbit_x*13.0f);
        int y=82+i*13+(int)(sinf(t+i)*7.0f-orbit_y*4.0f);
        if(x<-10||x>490)continue;
        DrawLine(x-8,y+3,x,y,(Color){51,49,44,180});DrawLine(x,y,x+8,y+3,(Color){51,49,44,180});
    }
    int pollen=effect_count(world,14,30,48);
    for(int i=0;i<pollen;++i){
        uint32_t h=sunrise_particle_hash((uint32_t)i+0xa3419U);
        int x=(int)(h&127U)-32+(int)fmodf(world->tick*(.12f+(h%7U)*.018f),560.0f)-
              (int)(orbit_x*(10.0f+(h&7U)));
        int y=128+(int)((h>>9)%290U)+(int)(sinf(t*.45f+(h&255U))*(5.0f+(h&3U)))-
              (int)(orbit_y*4.0f);
        if(x>1&&x<479)DrawCircle(x,y,1,(Color){255,219,157,(unsigned char)(45+(h&31U))});
    }
    int grass=effect_count(world,5,9,14);
    for(int i=0;i<grass;++i){
        int x=18+i*52-(int)(orbit_x*31.0f);
        int py=(int)(-orbit_y*14.0f);
        int bend=(int)(sinf(t*.7f+i*.8f)*5.0f);
        if(x>-8&&x<488)DrawLine(x,461+py,x+bend,438+py-(i%3)*5,(Color){77,83,48,120});
    }
    /* Rooted flower heads already live in the photographed cliff texture.
       Keeping them there avoids the synthetic wire-wheel silhouettes. */
}

static void draw_scene_button(int x,const char *label,bool selected)
{
    Color fill=selected?(Color){224,248,239,220}:(Color){3,21,31,155};
    Color text=selected?(Color){8,43,49,255}:(Color){218,242,237,230};
    DrawCircle(x,441,18,fill);DrawRectangle(x,423,54,36,fill);DrawCircle(x+54,441,18,fill);
    DrawText(label,x+8,435,10,text);
}

static void draw_overlay(const living_world_t *world)
{
    DrawRectangle(16,16,448,39,(Color){0,22,36,145});
    const char *title=world->scene==LIVING_SCENE_AURORA?"AURORA":
                      world->scene==LIVING_SCENE_SUNRISE?"SUNRISE":
                      world->scene==LIVING_SCENE_RAINFOREST?"RAINFOREST":"OCEAN";
    DrawText(title,29,26,16,(Color){218,250,242,255});
    const char *fx=world->effects_level==0?"FX CALM":
                   (world->effects_level==2?"FX VIVID":"FX LIVING");
    DrawText(fx,260,31,8,(Color){156,220,211,230});
    DrawText(TextFormat("%03d DEG",(int)world->yaw),376,28,12,(Color){131,226,221,255});
    draw_scene_button(25,"AURORA",world->scene==LIVING_SCENE_AURORA);
    draw_scene_button(137,"OCEAN",world->scene==LIVING_SCENE_OCEAN);
    draw_scene_button(249,"SUNRISE",world->scene==LIVING_SCENE_SUNRISE);
    draw_scene_button(361,"JUNGLE",world->scene==LIVING_SCENE_RAINFOREST);
}

void living_worlds_view_render(const living_world_t *world,
                            const living_worlds_atlases_t *atlases)
{
    if(!world||!atlases)return;
    BeginDrawing();
    ClearBackground((Color){1,28,44,255});
    if(world->scene==LIVING_SCENE_AURORA){
        living_aurora_draw(&world->aurora,world->yaw,world->pitch,world->effects_level,
            atlases->aurora,atlases->aurora_ice_front,atlases->aurora_ice_side,
            atlases->aurora_ice_rear);
    }else if(world->scene==LIVING_SCENE_SUNRISE){
        sunrise_camera_t camera=sunrise_camera(world);
        living_cover_reset();
        draw_sunrise_cliff(&camera,atlases->sunrise_cliff_front,atlases->sunrise_cliff_side,
                           atlases->sunrise_cliff_rear,1);
        living_cover_seal();
        draw_sunrise_orbit(world,atlases->sunrise);
        draw_sunrise_seeds(world,false);
        draw_sunrise_cliff(&camera,atlases->sunrise_cliff_front,atlases->sunrise_cliff_side,
                           atlases->sunrise_cliff_rear,0);
        draw_sunrise_seeds(world,true);
        draw_sunrise_fx(world);
    }else if(world->scene==LIVING_SCENE_RAINFOREST){
        draw_panorama(world,atlases->rainforest);
        draw_rainforest_fx(world,atlases->rainforest);
    }else{
        living_ocean_draw(&world->ocean,world->yaw,world->pitch,world->effects_level,
            atlases->ocean,atlases->ocean_left_front,atlases->ocean_left_side,
            atlases->ocean_left_rear,atlases->ocean_right_front,
            atlases->ocean_right_side,atlases->ocean_right_rear);
    }
    draw_overlay(world);
    EndDrawing();
}
