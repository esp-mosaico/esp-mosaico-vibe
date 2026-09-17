// SPDX-License-Identifier: Apache-2.0
#include "underwater_view.h"
#include <math.h>
#include "mosaico_raylib_fast.h"

#define PANORAMA_WIDTH 1600.0f
#define PANORAMA_HEIGHT 800.0f
#define VIEW_WIDTH 500.0f
#define VIEW_HEIGHT 666.6667f
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

static int effect_count(const underwater_world_t *world,int calm,int living,int vivid)
{
    return world->effects_level==0?calm:(world->effects_level==2?vivid:living);
}

static void draw_panorama(const underwater_world_t *world,MosaicoAtlas panorama)
{
    float panorama_width=(float)panorama.texture.width;
    float panorama_height=(float)panorama.texture.height;
    float view_width=panorama_width*(VIEW_WIDTH/PANORAMA_WIDTH);
    float view_height=panorama_height*(VIEW_HEIGHT/PANORAMA_HEIGHT);
    float source_x=world->yaw*(panorama_width/360.0f);
    float source_y=panorama_height/12.0f+world->pitch*(panorama_height/PANORAMA_HEIGHT)*2.25f;
    if(source_y<0)source_y=0;
    if(source_y>panorama_height-view_height)
        source_y=panorama_height-view_height;
    float first=panorama_width-source_x;
    if(first>view_width)first=view_width;
    float first_dest=first*(480.0f/view_width);
    DrawTexturePro(panorama.texture,(Rectangle){source_x,source_y,first,view_height},
                   (Rectangle){0,0,first_dest,480},(Vector2){0,0},0,WHITE);
    if(first<view_width){
        float second=view_width-first;
        DrawTexturePro(panorama.texture,(Rectangle){0,source_y,second,view_height},
                       (Rectangle){first_dest,0,480-first_dest,480},
                       (Vector2){0,0},0,WHITE);
    }
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
        int y=(int)(base_y[i]+sinf(phase)*15.0f-world->pitch*1.62f);
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
    int by=92+(int)(sinf(world->tick*.08f)*8.0f)-world->pitch;
    if(bx>-24&&bx<504){
        DrawCircle(bx,by,5,(Color){20,27,21,235});
        DrawLine(bx-3,by,bx-13,by-5,(Color){18,25,20,220});
        DrawLine(bx+4,by-1,bx+12,by-2,(Color){207,128,37,230});
    }
    /* Small stream glints imply moving water without repainting the photograph. */
    int glints=effect_count(world,2,5,8);
    for(int i=0;i<glints;++i){
        int x=(i*101+(int)(world->tick*.7f))%540-30;
        int y=348+i*17;
        DrawLine(x,y,x+18,y,(Color){207,229,214,(unsigned char)(48+i*7)});
    }
    int drops=effect_count(world,3,7,13);
    for(int i=0;i<drops;++i){
        int x=(i*109+31)%476;
        int y=(i*71+(int)world->tick*(2+i%2))%420;
        DrawLine(x,y,x-1,y+5,(Color){202,225,218,105});
    }
}

static void draw_aurora_fx(const underwater_world_t *world)
{
    int stars=effect_count(world,16,34,52);
    for(int i=0;i<stars;++i){
        int x=(i*137+23)%470+5,y=(i*73+17)%208+10;
        unsigned char a=(unsigned char)(75+55*sinf(world->tick*.045f+i*1.7f));
        DrawCircle(x,y,(i%5==0)?2:1,(Color){220,255,246,a});
    }
    for(int i=0;i<3;++i){
        float phase=world->tick*.035f+i*2.1f;
        int x=80+i*137+(int)(sinf(phase)*38.0f),y=54+i*37+(int)(cosf(phase*.7f)*18.0f);
        DrawLine(x,y,x-42,y+24,(Color){210,238,255,(unsigned char)(155-i*25)});
        DrawLine(x-1,y,x-29,y+16,(Color){255,255,255,210});
    }
    int snow=effect_count(world,5,11,20);
    for(int i=0;i<snow;++i){
        int x=(i*83+(int)(world->tick*(1+i%2)/3))%500-10;
        int y=(i*59+(int)(world->tick*(1+i%3)/2))%390+55;
        DrawCircle(x,y,(i%4==0)?2:1,(Color){232,247,251,150});
    }
}

static void draw_sunrise_fx(const underwater_world_t *world)
{
    float t=world->tick*.028f;
    for(int i=0;i<5;++i){
        int x=(int)fmodf(70.0f+i*113.0f+t*(13+i*2),560.0f)-40;
        int y=82+i*31+(int)(sinf(t+i)*7.0f);
        DrawLine(x-8,y+3,x,y,(Color){51,49,44,180});DrawLine(x,y,x+8,y+3,(Color){51,49,44,180});
    }
    int seeds=effect_count(world,6,12,20);
    for(int i=0;i<seeds;++i){
        int x=(i*91+(int)(world->tick*(1+i%3)/3))%520-20;
        int y=270+(i*47+(int)(sinf(t+i)*18))%145;
        Color seed=(Color){255,241,205,170};
        DrawLine(x,y,x-3,y+7,(Color){116,91,58,135});
        DrawLine(x,y,x-4,y-2,seed);DrawLine(x,y,x,y-4,seed);DrawLine(x,y,x+4,y-2,seed);
        DrawCircle(x,y,1,(Color){255,250,226,205});
    }
    int grass=effect_count(world,5,9,14);
    for(int i=0;i<grass;++i){
        int x=18+i*37,bend=(int)(sinf(t*.7f+i*.8f)*5.0f);
        DrawLine(x,461,x+bend,438-(i%3)*5,(Color){77,83,48,120});
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
    y-=world->pitch*1.62f;
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
        y-=(int)(world->pitch*1.62f);
        int r=3+(i*5)%7;
        if(x < -r || x >= 480+r || y < -r || y >= 480+r)continue;
        DrawCircleLines(x,y,(float)r,(Color){188,242,244,(unsigned char)(72+i%4*18)});
        if(r>=7)DrawCircle(x-r/3,y-r/3,1,(Color){232,255,252,150});
    }
    int rays=effect_count(world,2,4,7);
    for(int i=0;i<rays;++i){
        int x=(i*97+(int)(world->tick*.35f))%560-40;
        DrawLine(x,42,x+54,178,(Color){155,226,231,(unsigned char)(18+i*3)});
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
                -world->pitch*1.62f;
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
                            MosaicoAtlas reindeer,MosaicoAtlas rainforest)
{
    if(!world)return;
    BeginDrawing();
    ClearBackground((Color){1,28,44,255});
    if(world->scene==UNDERWATER_SCENE_AURORA){draw_panorama(world,aurora);draw_reindeer(world,reindeer);draw_aurora_fx(world);}
    else if(world->scene==UNDERWATER_SCENE_SUNRISE){draw_panorama(world,sunrise);draw_sunrise_fx(world);}
    else if(world->scene==UNDERWATER_SCENE_RAINFOREST){draw_panorama(world,rainforest);draw_rainforest_fx(world);}
    else {draw_panorama(world,panorama);draw_marine_creatures(world,creatures);draw_swimming_fish(world,fish);draw_lens_particles(world);}
    draw_overlay(world);
    EndDrawing();
}
