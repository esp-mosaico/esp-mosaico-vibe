// SPDX-License-Identifier: Apache-2.0
#include "underwater_view.h"
#include <math.h>
#include "mosaico_raylib_fast.h"
#include "sunrise_depth.h"
#include "sunrise_volume.h"

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

static float world_to_screen_x_layer(float longitude,float camera_yaw,float parallax)
{
    return 240.0f+signed_angle(longitude-camera_yaw)*parallax*(480.0f/VIEW_FOV_DEG);
}

static int effect_count(const underwater_world_t *world,int calm,int living,int vivid)
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

static void draw_panorama(const underwater_world_t *world,MosaicoAtlas panorama)
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

static sunrise_camera_t sunrise_camera(const underwater_world_t *world)
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
    float inverse_depth=(float)raw_depth/65535.0f*.3f;
    float z=1.0f/fmaxf(.007f,inverse_depth);
    float x=(u-.5f)*480.0f*z/focal*1.14f;
    float y=(.5f-v)*480.0f*z/focal*1.14f;
    float dx=x-camera->x,dy=y-camera->y,dz=z-camera->z;
    float view_z=dx*camera->m[6]+dy*camera->m[7]+dz*camera->m[8];
    if(view_z<.2f)return false;
    out->x=240.0f+focal*(dx*camera->m[0]+dy*camera->m[1]+dz*camera->m[2])/view_z;
    out->y=240.0f-focal*(dx*camera->m[3]+dy*camera->m[4]+dz*camera->m[5])/view_z;
    return true;
}

static bool sunrise_project_xyz(const sunrise_camera_t *camera,float x,float y,
                                float z,Vector2 *out)
{
    const float focal=480.0f*1.055f;
    float dx=x-camera->x,dy=y-camera->y,dz=z-camera->z;
    float view_z=dx*camera->m[6]+dy*camera->m[7]+dz*camera->m[8];
    if(view_z<.2f)return false;
    out->x=240.0f+focal*(dx*camera->m[0]+dy*camera->m[1]+dz*camera->m[2])/view_z;
    out->y=240.0f-focal*(dx*camera->m[3]+dy*camera->m[4]+dz*camera->m[5])/view_z;
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

static void draw_sunrise_orbit(const underwater_world_t *world,MosaicoAtlas sunrise)
{
    sunrise_camera_t camera=sunrise_camera(world);
    const int n=SUNRISE_DEPTH_GRID;
    enum { HORIZONTAL_EDGE_TILES=6, VERTICAL_EDGE_TILES=2 };
    /* Render the prototype's depth surface far-to-near. Small texture patches
       approximate its perspective triangles while staying on the RGB565 fast
       path shared by Host and device. */
    for(int depth_band=0;depth_band<8;++depth_band){
        for(int y=-VERTICAL_EDGE_TILES;y<n+VERTICAL_EDGE_TILES;++y)
        for(int x=-HORIZONTAL_EDGE_TILES;x<n+HORIZONTAL_EDGE_TILES;++x){
            int x0=sunrise_depth_index(x),x1=sunrise_depth_index(x+1);
            int y0=sunrise_depth_index(y),y1=sunrise_depth_index(y+1);
            int a=y0*(n+1)+x0,b=y0*(n+1)+x1;
            int c=y1*(n+1)+x0,d=y1*(n+1)+x1;
            unsigned average=((unsigned)SUNRISE_DEPTH[a]+SUNRISE_DEPTH[b]+
                              SUNRISE_DEPTH[c]+SUNRISE_DEPTH[d])/4U;
            if((int)(average*8U/65536U)!=depth_band)continue;
            float u0=(float)x/n,u1=(float)(x+1)/n;
            float v0=(float)y/n,v1=(float)(y+1)/n;
            Vector2 p[4];
            if(!sunrise_project(&camera,u0,v0,SUNRISE_DEPTH[a],&p[0])||
               !sunrise_project(&camera,u1,v0,SUNRISE_DEPTH[b],&p[1])||
               !sunrise_project(&camera,u0,v1,SUNRISE_DEPTH[c],&p[2])||
               !sunrise_project(&camera,u1,v1,SUNRISE_DEPTH[d],&p[3]))continue;
            float left=fminf(fminf(p[0].x,p[1].x),fminf(p[2].x,p[3].x));
            float right=fmaxf(fmaxf(p[0].x,p[1].x),fmaxf(p[2].x,p[3].x));
            float top=fminf(fminf(p[0].y,p[1].y),fminf(p[2].y,p[3].y));
            float bottom=fmaxf(fmaxf(p[0].y,p[1].y),fmaxf(p[2].y,p[3].y));
            if(right<0||left>480||bottom<0||top>480)continue;
            float source_u0=mirror_unit(u0),source_u1=mirror_unit(u1);
            float source_v0=mirror_unit(v0),source_v1=mirror_unit(v1);
            float tx0=source_u0*(sunrise.texture.width-1);
            float tx1=source_u1*(sunrise.texture.width-1);
            float ty0=source_v0*(sunrise.texture.height-1);
            float ty1=source_v1*(sunrise.texture.height-1);
            mosaico_textured_vertex_t va={p[0].x,p[0].y,tx0,ty0};
            mosaico_textured_vertex_t vb={p[1].x,p[1].y,tx1,ty0};
            mosaico_textured_vertex_t vc={p[2].x,p[2].y,tx0,ty1};
            mosaico_textured_vertex_t vd={p[3].x,p[3].y,tx1,ty1};
            Mosaico2DDrawTexturedTriangle(sunrise.texture,va,vc,vb,256);
            Mosaico2DDrawTexturedTriangle(sunrise.texture,vb,vc,vd,256);
        }
    }
}

static Vector2 sunrise_volume_screen[SUNRISE_SIDE_VERTEX_COUNT];
static uint8_t sunrise_volume_valid[SUNRISE_SIDE_VERTEX_COUNT];

static void draw_sunrise_volume_part(const sunrise_camera_t *camera,
    const sunrise_volume_vertex_t *vertices,int vertex_count,
    const sunrise_volume_face_t *faces,int face_count,MosaicoAtlas atlas,
    float authored_width,float authored_height,unsigned light)
{
    if(!atlas.texture.id||vertex_count>SUNRISE_SIDE_VERTEX_COUNT)return;
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
        Vector2 pa=sunrise_volume_screen[ia],pb=sunrise_volume_screen[ib],
            pc=sunrise_volume_screen[ic];
        float area=(pb.x-pa.x)*(pc.y-pa.y)-(pb.y-pa.y)*(pc.x-pa.x);
        if(area<=.25f)continue;
        float min_x=fminf(pa.x,fminf(pb.x,pc.x));
        float max_x=fmaxf(pa.x,fmaxf(pb.x,pc.x));
        float min_y=fminf(pa.y,fminf(pb.y,pc.y));
        float max_y=fmaxf(pa.y,fmaxf(pb.y,pc.y));
        if(max_x<0||min_x>=480||max_y<0||min_y>=480)continue;
        mosaico_textured_vertex_t va={sunrise_volume_screen[ia].x,
            sunrise_volume_screen[ia].y,a->u*scale_u,a->v*scale_v};
        mosaico_textured_vertex_t vb={sunrise_volume_screen[ib].x,
            sunrise_volume_screen[ib].y,b->u*scale_u,b->v*scale_v};
        mosaico_textured_vertex_t vc={sunrise_volume_screen[ic].x,
            sunrise_volume_screen[ic].y,c->u*scale_u,c->v*scale_v};
        Mosaico2DDrawTexturedTriangle(atlas.texture,va,vb,vc,light);
    }
}

static void draw_sunrise_cliff(const underwater_world_t *world,
    MosaicoAtlas front,MosaicoAtlas side,MosaicoAtlas rear)
{
    sunrise_camera_t camera=sunrise_camera(world);
    draw_sunrise_volume_part(&camera,SUNRISE_REAR_VERTICES,SUNRISE_REAR_VERTEX_COUNT,
        SUNRISE_REAR_FACES,SUNRISE_REAR_FACE_COUNT,rear,512,512,184);
    draw_sunrise_volume_part(&camera,SUNRISE_SIDE_VERTICES,SUNRISE_SIDE_VERTEX_COUNT,
        SUNRISE_SIDE_FACES,SUNRISE_SIDE_FACE_COUNT,side,1024,256,225);
    draw_sunrise_volume_part(&camera,SUNRISE_FRONT_VERTICES,SUNRISE_FRONT_VERTEX_COUNT,
        SUNRISE_FRONT_FACES,SUNRISE_FRONT_FACE_COUNT,front,768,768,256);
}

static void draw_rainforest_fx(const underwater_world_t *world)
{
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
    /* Stream glints, rain and motes occupy world longitudes, never screen slots. */
    int glints=effect_count(world,2,5,8);
    for(int i=0;i<glints;++i){
        float lon=fmodf(26.0f+i*67.0f+world->tick*.008f*(1+i%2),360.0f);
        int x=(int)world_to_screen_x(lon,world->yaw);
        int y=338+(i%4)*21-(int)(world->pitch*5.2f);
        if(x<-20||x>500)continue;
        DrawLine(x,y,x+18,y,(Color){207,229,214,(unsigned char)(48+i*7)});
    }
    int drops=effect_count(world,3,7,13);
    for(int i=0;i<drops;++i){
        float lon=fmodf(11.0f+i*47.0f+world->tick*.012f*(1+i%3),360.0f);
        int x=(int)world_to_screen_x(lon,world->yaw);
        int y=(i*71+(int)world->tick*(2+i%2))%420-(int)(world->pitch*4.4f);
        if(x<0||x>=480)continue;
        DrawLine(x,y,x-1,y+5,(Color){202,225,218,105});
    }
    int motes=effect_count(world,3,7,12);
    for(int i=0;i<motes;++i){
        float phase=world->tick*.014f+i*1.71f;
        float lon=fmodf(19.0f+i*79.0f+sinf(phase)*2.4f,360.0f);
        int x=(int)world_to_screen_x(lon,world->yaw);
        int y=88+(i*53)%270+(int)(sinf(phase*.7f)*10.0f)-(int)(world->pitch*3.9f);
        if(x>2&&x<478)DrawCircle(x,y,1,(Color){221,231,172,(unsigned char)(70+i%3*30)});
    }
    /* Close vines and leaves travel faster than the photographic background. */
    static const float vine_lon[]={18.0f,104.0f,221.0f,319.0f};
    for(int i=0;i<4;++i){
        int x=(int)world_to_screen_x_layer(vine_lon[i],world->yaw,1.12f);
        if(x<-28||x>508)continue;
        int sway=(int)(sinf(world->tick*.018f+i*1.4f)*7.0f);
        int py=(int)(-world->pitch*6.0f);
        DrawLine(x,py,x+sway,py+104+i*17,(Color){20,55,27,210});
        for(int j=1;j<4;++j){
            int y=py+20+j*23+i*5,side=((i+j)&1)?1:-1;
            int stem=x+sway*j/4;
            DrawLine(stem,y,stem+side*13,y-7,(Color){23,68,31,185});
            DrawLine(stem+side*4,y-2,stem+side*17,y+2,(Color){42,91,45,150});
        }
    }
}

static void draw_aurora_fx(const underwater_world_t *world)
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

static void draw_sunrise_fx(const underwater_world_t *world)
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
    int seeds=effect_count(world,6,12,20);
    for(int i=0;i<seeds;++i){
        int layer=i%3;
        int x=(i*43+(int)(world->tick*.008f*(1+layer)))%520-20-
              (int)(orbit_x*(18.0f+layer*6.0f));
        int y=270+(i*47+(int)(sinf(t+i)*18))%145-
              (int)(orbit_y*(6.0f+layer*2.0f));
        if(x<-6||x>486)continue;
        Color seed=(Color){255,241,205,170};
        DrawLine(x,y,x-3,y+7,(Color){116,91,58,135});
        DrawLine(x,y,x-4,y-2,seed);DrawLine(x,y,x,y-4,seed);DrawLine(x,y,x+4,y-2,seed);
        DrawCircle(x,y,1,(Color){255,250,226,205});
    }
    int grass=effect_count(world,5,9,14);
    for(int i=0;i<grass;++i){
        int x=18+i*52-(int)(orbit_x*31.0f);
        int py=(int)(-orbit_y*14.0f);
        int bend=(int)(sinf(t*.7f+i*.8f)*5.0f);
        if(x>-8&&x<488)DrawLine(x,461+py,x+bend,438+py-(i%3)*5,(Color){77,83,48,120});
    }
    /* Large foreground dandelions keep the original scene idea and add depth. */
    static const float flower_x[]={42.0f,166.0f,304.0f,438.0f};
    int flowers=effect_count(world,2,3,4);
    for(int i=0;i<flowers;++i){
        int x=(int)(flower_x[i]-orbit_x*38.0f);
        if(x<-32||x>512)continue;
        int bend=(int)(sinf(t*.55f+i*1.9f)*8.0f);
        int y=391+(i%2)*24-(int)(orbit_y*18.0f);
        int r=13+(i%3)*3;
        DrawLine(x,y+80,x+bend,y,(Color){74,83,43,210});
        DrawCircleLines(x+bend,y,(float)r,(Color){244,230,190,205});
        for(int j=0;j<8;++j){
            float a=(float)j*.7854f+t*.05f;
            int px=x+bend+(int)(cosf(a)*r),py=y+(int)(sinf(a)*r);
            DrawLine(x+bend,y,px,py,(Color){255,242,210,155});
            DrawCircle(px,py,1,(Color){255,250,224,220});
        }
        DrawCircle(x+bend,y,2,(Color){154,120,65,240});
    }
}

static const MosaicoSpriteFrame *animation_frame(MosaicoAtlas atlas,
                                                  const char *const names[3],
                                                  uint32_t tick,uint32_t speed,
                                                  uint32_t phase_offset)
{
    int phase=(int)(((tick+phase_offset)/speed)%4U);
    if(phase==3)phase=1;
    return MosaicoAtlasGetFrame(atlas,mosaico_game_asset_id(names[phase]));
}

static void draw_world_sprite(const underwater_world_t *world,MosaicoAtlas atlas,
                              const MosaicoSpriteFrame *frame,float longitude,float y,float size,
                              bool face_left)
{
    if(!frame)return;
    float x=world_to_screen_x(longitude,world->yaw);
    y-=world->pitch*4.4f;
    if(x < -size*.6f || x > 480.0f+size*.6f)return;
    Rectangle source=frame->source;
    if(face_left)source.width=-source.width;
    DrawTexturePro(atlas.texture,source,
                   (Rectangle){x-size*.5f,y-size*.5f,size,size},
                   (Vector2){0,0},0,WHITE);
}

static void draw_reindeer(const underwater_world_t *world,MosaicoAtlas atlas)
{
    static const char *const names[]={"reindeer_walk_0","reindeer_walk_1","reindeer_walk_2"};
    static const float spacing[]={0.0f,-10.0f,-19.0f};
    static const float y[]={330.0f,347.0f,354.0f};
    static const float size[]={104.0f,82.0f,68.0f};
    float lead=fmodf(292.0f+world->tick*.035f,360.0f);
    for(int i=2;i>=0;--i){
        const MosaicoSpriteFrame *frame=animation_frame(
            atlas,names,world->tick,7U,(uint32_t)i*5U);
        draw_world_sprite(world,atlas,frame,lead+spacing[i],y[i],size[i],false);
    }
}

static void draw_marine_creatures(const underwater_world_t *world,MosaicoAtlas atlas)
{
    static const char *const turtle_names[]={"turtle_swim_0","turtle_swim_1","turtle_swim_2"};
    static const char *const whale_names[]={"whale_swim_0","whale_swim_1","whale_swim_2"};
    static const char *const jelly_names[]={"jelly_pulse_0","jelly_pulse_1","jelly_pulse_2"};
    /* The panorama is staged as four discoveries rather than one crowded overlay:
       reef fish around 45 deg, jelly garden around 125, distant whale around 210,
       and a close turtle around 300. */
    const MosaicoSpriteFrame *turtle=animation_frame(atlas,turtle_names,world->tick,9,0);
    float turtle_phase=world->tick*.0038f;
    float turtle_y=208.0f+sinf(world->tick*.018f)*9.0f;
    draw_world_sprite(world,atlas,turtle,
                      300.0f+sinf(turtle_phase)*10.0f,turtle_y,118.0f,
                      cosf(turtle_phase)<0.0f);

    const MosaicoSpriteFrame *whale=animation_frame(atlas,whale_names,world->tick,14,8);
    float whale_phase=world->tick*.0018f+.6f;
    float whale_y=112.0f+sinf(world->tick*.009f+1.2f)*5.0f;
    draw_world_sprite(world,atlas,whale,
                      210.0f+sinf(whale_phase)*7.0f,whale_y,148.0f,
                      cosf(whale_phase)<0.0f);

    static const float longitude[]={116.0f,128.0f,140.0f,124.0f};
    static const float y[]={232.0f,315.0f,257.0f,372.0f};
    static const float size[]={66.0f,46.0f,54.0f,38.0f};
    for(int i=0;i<4;++i){
        const MosaicoSpriteFrame *jelly=animation_frame(
            atlas,jelly_names,world->tick,8U+(uint32_t)(i&1)*2U,(uint32_t)i*9U);
        float drift=sinf(world->tick*(.012f+i*.0015f)+i*1.7f)*7.0f;
        float lon=longitude[i]+sinf(world->tick*.004f+i)*1.8f;
        draw_world_sprite(world,atlas,jelly,lon,y[i]+drift,size[i],false);
    }
}

static void draw_lens_particles(const underwater_world_t *world)
{
    int particles=effect_count(world,7,14,22);
    for(int i=0;i<particles;++i){
        float longitude=fmodf(17.0f+(float)i*61.7f,360.0f);
        int x=(int)world_to_screen_x(longitude,world->yaw);
        int y=(i*71-(int)world->tick*(i%2+1)/2)%500;
        if(y<0)y+=500;
        y-=(int)(world->pitch*5.2f);
        int r=3+(i*5)%7;
        if(x < -r || x >= 480+r || y < -r || y >= 480+r)continue;
        DrawCircleLines(x,y,(float)r,(Color){188,242,244,(unsigned char)(72+i%4*18)});
        if(r>=7)DrawCircle(x-r/3,y-r/3,1,(Color){232,255,252,150});
    }
    int rays=effect_count(world,2,4,7);
    for(int i=0;i<rays;++i){
        float lon=fmodf(22.0f+i*71.0f+sinf(world->tick*.006f+i)*2.0f,360.0f);
        int x=(int)world_to_screen_x_layer(lon,world->yaw,.94f);
        int py=(int)(-world->pitch*1.2f);
        if(x>-60&&x<500)DrawLine(x,42+py,x+54,178+py,(Color){155,226,231,(unsigned char)(18+i*3)});
    }
    /* Near sea grass has stronger yaw parallax than fish and the reef. */
    int grass=effect_count(world,4,7,11);
    for(int i=0;i<grass;++i){
        int x=(int)world_to_screen_x_layer(12.0f+i*47.0f,world->yaw,1.18f);
        if(x<-12||x>492)continue;
        int h=31+(i%4)*11;
        int bend=(int)(sinf(world->tick*.022f+i*.8f)*9.0f);
        int py=(int)(-world->pitch*6.0f);
        DrawLine(x,480+py,x+bend,480+py-h,(Color){21,91,86,195});
        DrawLine(x+4,480+py,x-bend/2+5,486+py-h,(Color){39,119,101,160});
    }
    DrawRectangle(0,0,480,18,(Color){0,23,38,90});
    DrawRectangle(0,462,480,18,(Color){0,14,25,120});
}

static void draw_swimming_fish(const underwater_world_t *world,MosaicoAtlas fish)
{
    static const char *const frame_names[]={
        "reef_fish_swim_0","reef_fish_swim_1","reef_fish_swim_2",
        "reef_fish_swim_1"
    };
    /* Fish live in panorama longitude, then move through that world. Turning the
       camera therefore moves them with the reef instead of pinning them to glass.
       A loose diamond formation reads as a school while leaving negative space. */
    static const float start_longitude[]={38.0f,47.0f,49.0f,58.0f};
    static const float base_y[]={138.0f,105.0f,174.0f,139.0f};
    static const float size[]={62.0f,82.0f,52.0f,66.0f};
    for(int i=0;i<4;++i){
        int phase=(int)(((world->tick+(uint32_t)i*5U)/5U)%4U);
        const MosaicoSpriteFrame *frame=MosaicoAtlasGetFrame(fish,
            mosaico_game_asset_id(frame_names[phase]));
        if(!frame)continue;
        float swim_phase=(float)world->tick*.0045f+i*.17f;
        float longitude=start_longitude[i]+sinf(swim_phase)*7.0f;
        float x=world_to_screen_x(longitude,world->yaw);
        float side=size[i];
        float y=base_y[i]+sinf((float)world->tick*.028f+i*1.3f)*4.0f
                -world->pitch*4.2f;
        if(x < -side*.6f || x > 480.0f+side*.6f)continue;
        Rectangle source=frame->source;
        if(cosf(swim_phase)<0.0f)source.width=-source.width;
        DrawTexturePro(fish.texture,source,
                       (Rectangle){x-side*.5f,y-side*.5f,side,side},
                       (Vector2){0,0},0,WHITE);
    }
}

static void draw_scene_button(int x,const char *label,bool selected)
{
    Color fill=selected?(Color){224,248,239,220}:(Color){3,21,31,155};
    Color text=selected?(Color){8,43,49,255}:(Color){218,242,237,230};
    DrawCircle(x,441,18,fill);DrawRectangle(x,423,54,36,fill);DrawCircle(x+54,441,18,fill);
    DrawText(label,x+8,435,10,text);
}

static void draw_overlay(const underwater_world_t *world)
{
    DrawRectangle(16,16,448,39,(Color){0,22,36,145});
    const char *title=world->scene==UNDERWATER_SCENE_AURORA?"AURORA":
                      world->scene==UNDERWATER_SCENE_SUNRISE?"SUNRISE":
                      world->scene==UNDERWATER_SCENE_RAINFOREST?"RAINFOREST":"ABYSS 360";
    DrawText(title,29,26,16,(Color){218,250,242,255});
    const char *fx=world->effects_level==0?"FX CALM":
                   (world->effects_level==2?"FX VIVID":"FX LIVING");
    DrawText(fx,260,31,8,(Color){156,220,211,230});
    DrawText(TextFormat("%03d DEG",(int)world->yaw),376,28,12,(Color){131,226,221,255});
    draw_scene_button(25,"AURORA",world->scene==UNDERWATER_SCENE_AURORA);
    draw_scene_button(137,"OCEAN",world->scene==UNDERWATER_SCENE_OCEAN);
    draw_scene_button(249,"SUNRISE",world->scene==UNDERWATER_SCENE_SUNRISE);
    draw_scene_button(361,"JUNGLE",world->scene==UNDERWATER_SCENE_RAINFOREST);
}

void underwater_view_render(const underwater_world_t *world,MosaicoAtlas panorama,
                            MosaicoAtlas fish,MosaicoAtlas creatures,
                            MosaicoAtlas aurora,MosaicoAtlas sunrise,
                            MosaicoAtlas sunrise_cliff_front,
                            MosaicoAtlas sunrise_cliff_side,
                            MosaicoAtlas sunrise_cliff_rear,
                            MosaicoAtlas reindeer,MosaicoAtlas rainforest)
{
    if(!world)return;
    BeginDrawing();
    ClearBackground((Color){1,28,44,255});
    if(world->scene==UNDERWATER_SCENE_AURORA){draw_panorama(world,aurora);draw_reindeer(world,reindeer);draw_aurora_fx(world);}
    else if(world->scene==UNDERWATER_SCENE_SUNRISE){draw_sunrise_orbit(world,sunrise);draw_sunrise_cliff(world,sunrise_cliff_front,sunrise_cliff_side,sunrise_cliff_rear);draw_sunrise_fx(world);}
    else if(world->scene==UNDERWATER_SCENE_RAINFOREST){draw_panorama(world,rainforest);draw_rainforest_fx(world);}
    else {draw_panorama(world,panorama);draw_marine_creatures(world,creatures);draw_swimming_fish(world,fish);draw_lens_particles(world);}
    draw_overlay(world);
    EndDrawing();
}
