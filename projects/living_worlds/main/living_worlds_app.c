// SPDX-License-Identifier: Apache-2.0
#define GSP_BUNDLE_ENABLE_RAW_IDS 1
#include "bundle_gsp.h"
#include "driver/jpeg_decode.h"
#include "esp_check.h"
#include "esp_log.h"
#include <stdlib.h>
#include "mosaico_game_assets.h"
#include "raylib_screen_mirror.h"
#include "living_worlds_app.h"
#include "living_worlds_view.h"
#include "living_worlds_world.h"

static living_world_t world;
static living_worlds_atlases_t atlases;
static void *background_pixels;
static jpeg_decoder_handle_t background_decoder;
static uint8_t loaded_scene=UINT8_MAX;
static uint8_t loaded_volumes=UINT8_MAX;

#define ATLAS_SYMBOLS(name) \
    extern const uint8_t _binary_##name##_atlas_start[]; \
    extern const uint8_t _binary_##name##_atlas_end[]
ATLAS_SYMBOLS(sunrise_cliff_front);
ATLAS_SYMBOLS(sunrise_cliff_side);
ATLAS_SYMBOLS(sunrise_cliff_rear);
ATLAS_SYMBOLS(aurora_ice_front);
ATLAS_SYMBOLS(aurora_ice_side);
ATLAS_SYMBOLS(aurora_ice_rear);
ATLAS_SYMBOLS(ocean_reef_left_front);
ATLAS_SYMBOLS(ocean_reef_left_side);
ATLAS_SYMBOLS(ocean_reef_left_rear);
ATLAS_SYMBOLS(ocean_reef_right_front);
ATLAS_SYMBOLS(ocean_reef_right_side);
ATLAS_SYMBOLS(ocean_reef_right_rear);
extern const uint8_t _binary_ocean_jpg_start[],_binary_ocean_jpg_end[];
extern const uint8_t _binary_aurora_jpg_start[],_binary_aurora_jpg_end[];
extern const uint8_t _binary_sunrise_jpg_start[],_binary_sunrise_jpg_end[];
extern const uint8_t _binary_rainforest_jpg_start[],_binary_rainforest_jpg_end[];

static esp_err_t decode_background(jpeg_decoder_handle_t decoder,const char *name,
    const uint8_t *start,const uint8_t *end,MosaicoAtlas *out,void **out_pixels)
{
    jpeg_decode_picture_info_t info={0};
    size_t stream_size=(size_t)(end-start);
    ESP_RETURN_ON_ERROR(jpeg_decoder_get_info(start,stream_size,&info),"living_worlds","parse %s",name);
    size_t padded_width=(info.width+15U)&~15U,padded_height=(info.height+15U)&~15U;
    size_t required=padded_width*padded_height*2U,allocated=0;
    const jpeg_decode_memory_alloc_cfg_t memory={.buffer_direction=JPEG_DEC_ALLOC_OUTPUT_BUFFER};
    void *pixels=jpeg_alloc_decoder_mem(required,&memory,&allocated);
    ESP_RETURN_ON_FALSE(pixels&&allocated>=required,ESP_ERR_NO_MEM,"living_worlds","allocate %s RGB565",name);
    const jpeg_decode_cfg_t config={.output_format=JPEG_DECODE_OUT_FORMAT_RGB565,
        .rgb_order=JPEG_DEC_RGB_ELEMENT_ORDER_BGR,.conv_std=JPEG_YUV_RGB_CONV_STD_BT601};
    uint32_t output_size=0;
    esp_err_t err=jpeg_decoder_process(decoder,&config,start,stream_size,pixels,allocated,&output_size);
    if(err!=ESP_OK){free(pixels);return err;}
    Texture2D texture=Mosaico2DRegisterRGB565(pixels,(int)info.width,(int)info.height);
    if(!texture.id){free(pixels);return ESP_ERR_NO_MEM;}
    *out_pixels=pixels;*out=(MosaicoAtlas){.texture=texture};
    ESP_LOGI("living_worlds","JPEG %s: %lux%lu %u bytes -> %u-byte RGB565",
        name,(unsigned long)info.width,(unsigned long)info.height,(unsigned)stream_size,(unsigned)output_size);
    return ESP_OK;
}

static esp_err_t load_scene_background(uint8_t scene)
{
    const char *name;const uint8_t *start,*end;
    switch(scene){
    case LIVING_SCENE_AURORA:name="aurora";start=_binary_aurora_jpg_start;end=_binary_aurora_jpg_end;break;
    case LIVING_SCENE_SUNRISE:name="sunrise";start=_binary_sunrise_jpg_start;end=_binary_sunrise_jpg_end;break;
    case LIVING_SCENE_RAINFOREST:name="rainforest";start=_binary_rainforest_jpg_start;end=_binary_rainforest_jpg_end;break;
    default:name="ocean";start=_binary_ocean_jpg_start;end=_binary_ocean_jpg_end;scene=LIVING_SCENE_OCEAN;break;
    }
    MosaicoAtlas next={0};void *next_pixels=NULL;
    ESP_RETURN_ON_ERROR(decode_background(background_decoder,name,start,end,&next,&next_pixels),
                        "living_worlds","load scene background");
    if(atlases.aurora.texture.id)Mosaico2DUnloadTexture(atlases.aurora.texture);
    free(background_pixels);
    background_pixels=next_pixels;
    atlases.aurora=atlases.ocean=atlases.sunrise=atlases.rainforest=next;
    loaded_scene=scene;
    return ESP_OK;
}

static void unload_atlas(MosaicoAtlas *atlas)
{
    if(!atlas||!atlas->texture.id)return;
    UnloadMosaicoAtlas(*atlas);
    *atlas=(MosaicoAtlas){0};
}

static void unload_volumes(void)
{
    unload_atlas(&atlases.sunrise_cliff_front);
    unload_atlas(&atlases.sunrise_cliff_side);
    unload_atlas(&atlases.sunrise_cliff_rear);
    unload_atlas(&atlases.aurora_ice_front);
    unload_atlas(&atlases.aurora_ice_side);
    unload_atlas(&atlases.aurora_ice_rear);
    unload_atlas(&atlases.ocean_left_front);
    unload_atlas(&atlases.ocean_left_side);
    unload_atlas(&atlases.ocean_left_rear);
    unload_atlas(&atlases.ocean_right_front);
    unload_atlas(&atlases.ocean_right_side);
    unload_atlas(&atlases.ocean_right_rear);
    loaded_volumes=UINT8_MAX;
}

static bool atlas_ready(MosaicoAtlas atlas)
{
    return atlas.texture.id!=0;
}

static esp_err_t load_scene_volumes(uint8_t scene)
{
    if(loaded_volumes==scene)return ESP_OK;
    unload_volumes();
    if(scene==LIVING_SCENE_AURORA){
        atlases.aurora_ice_front=LoadMosaicoAtlas("aurora_ice_front.atlas");
        atlases.aurora_ice_side=LoadMosaicoAtlas("aurora_ice_side.atlas");
        atlases.aurora_ice_rear=LoadMosaicoAtlas("aurora_ice_rear.atlas");
        if(!atlas_ready(atlases.aurora_ice_front)||!atlas_ready(atlases.aurora_ice_side)||
           !atlas_ready(atlases.aurora_ice_rear)){
            unload_volumes();
            return ESP_ERR_NOT_FOUND;
        }
    }else if(scene==LIVING_SCENE_SUNRISE){
        atlases.sunrise_cliff_front=LoadMosaicoAtlas("sunrise_cliff_front.atlas");
        atlases.sunrise_cliff_side=LoadMosaicoAtlas("sunrise_cliff_side.atlas");
        atlases.sunrise_cliff_rear=LoadMosaicoAtlas("sunrise_cliff_rear.atlas");
        if(!atlas_ready(atlases.sunrise_cliff_front)||!atlas_ready(atlases.sunrise_cliff_side)||
           !atlas_ready(atlases.sunrise_cliff_rear)){
            unload_volumes();
            return ESP_ERR_NOT_FOUND;
        }
    }else if(scene==LIVING_SCENE_OCEAN){
        atlases.ocean_left_front=LoadMosaicoAtlas("ocean_reef_left_front.atlas");
        atlases.ocean_left_side=LoadMosaicoAtlas("ocean_reef_left_side.atlas");
        atlases.ocean_left_rear=LoadMosaicoAtlas("ocean_reef_left_rear.atlas");
        atlases.ocean_right_front=LoadMosaicoAtlas("ocean_reef_right_front.atlas");
        atlases.ocean_right_side=LoadMosaicoAtlas("ocean_reef_right_side.atlas");
        atlases.ocean_right_rear=LoadMosaicoAtlas("ocean_reef_right_rear.atlas");
        if(!atlas_ready(atlases.ocean_left_front)||!atlas_ready(atlases.ocean_left_side)||
           !atlas_ready(atlases.ocean_left_rear)||!atlas_ready(atlases.ocean_right_front)||
           !atlas_ready(atlases.ocean_right_side)||!atlas_ready(atlases.ocean_right_rear)){
            unload_volumes();
            return ESP_ERR_NOT_FOUND;
        }
    }
    loaded_volumes=scene;
    return ESP_OK;
}

static esp_err_t register_atlas(const char *name,const uint8_t *start,const uint8_t *end)
{
    return mosaico_game_asset_register_memory(name,start,(size_t)(end-start));
}

static esp_err_t before_display(void)
{
    esp_err_t err;
    err=register_atlas("sunrise_cliff_front.atlas",_binary_sunrise_cliff_front_atlas_start,_binary_sunrise_cliff_front_atlas_end);
    if(err!=ESP_OK)return err;
    err=register_atlas("sunrise_cliff_side.atlas",_binary_sunrise_cliff_side_atlas_start,_binary_sunrise_cliff_side_atlas_end);
    if(err!=ESP_OK)return err;
    err=register_atlas("sunrise_cliff_rear.atlas",_binary_sunrise_cliff_rear_atlas_start,_binary_sunrise_cliff_rear_atlas_end);
    if(err!=ESP_OK)return err;
    err=register_atlas("aurora_ice_front.atlas",_binary_aurora_ice_front_atlas_start,_binary_aurora_ice_front_atlas_end);
    if(err!=ESP_OK)return err;
    err=register_atlas("aurora_ice_side.atlas",_binary_aurora_ice_side_atlas_start,_binary_aurora_ice_side_atlas_end);
    if(err!=ESP_OK)return err;
    err=register_atlas("aurora_ice_rear.atlas",_binary_aurora_ice_rear_atlas_start,_binary_aurora_ice_rear_atlas_end);
    if(err!=ESP_OK)return err;
    err=register_atlas("ocean_reef_left_front.atlas",_binary_ocean_reef_left_front_atlas_start,_binary_ocean_reef_left_front_atlas_end);
    if(err!=ESP_OK)return err;
    err=register_atlas("ocean_reef_left_side.atlas",_binary_ocean_reef_left_side_atlas_start,_binary_ocean_reef_left_side_atlas_end);
    if(err!=ESP_OK)return err;
    err=register_atlas("ocean_reef_left_rear.atlas",_binary_ocean_reef_left_rear_atlas_start,_binary_ocean_reef_left_rear_atlas_end);
    if(err!=ESP_OK)return err;
    err=register_atlas("ocean_reef_right_front.atlas",_binary_ocean_reef_right_front_atlas_start,_binary_ocean_reef_right_front_atlas_end);
    if(err!=ESP_OK)return err;
    err=register_atlas("ocean_reef_right_side.atlas",_binary_ocean_reef_right_side_atlas_start,_binary_ocean_reef_right_side_atlas_end);
    if(err!=ESP_OK)return err;
    err=register_atlas("ocean_reef_right_rear.atlas",_binary_ocean_reef_right_rear_atlas_start,_binary_ocean_reef_right_rear_atlas_end);
    if(err!=ESP_OK)return err;
    const jpeg_decode_engine_cfg_t engine_config={.intr_priority=0,.timeout_ms=250};
    ESP_RETURN_ON_ERROR(jpeg_new_decoder_engine(&engine_config,&background_decoder),"living_worlds","create JPEG decoder");
    ESP_RETURN_ON_ERROR(load_scene_background(LIVING_SCENE_OCEAN),"living_worlds","load initial background");
    ESP_RETURN_ON_ERROR(load_scene_volumes(LIVING_SCENE_OCEAN),"living_worlds","load ocean volumes");
    return atlases.aurora.texture.id?ESP_OK:ESP_ERR_NOT_FOUND;
}
static esp_err_t on_start(void){living_world_reset(&world);return ESP_OK;}
static void on_event(const mosaico_device_event_t *event){
    if(event&&(event->type==MOSAICO_DEVICE_EVENT_POINTER||
               event->type==MOSAICO_DEVICE_EVENT_TOUCH)){
        uint8_t previous=world.scene;
        living_world_pointer(&world,(float)event->x,(float)event->y,event->pressed);
        if(world.scene!=previous){
            if(load_scene_background(world.scene)!=ESP_OK||
               load_scene_volumes(world.scene)!=ESP_OK){
                load_scene_background(previous);
                load_scene_volumes(previous);
                world.scene=previous;
            }
        }
    }
}
static void on_update(void){living_world_update(&world);}
static void on_render(void){living_worlds_view_render(&world,&atlases);}
static void on_stats(void){ESP_LOGI("living_worlds","yaw=%.1f pitch=%.1f hash=%08lx",world.yaw,world.pitch,(unsigned long)living_world_hash(&world));}
static const mosaico_game_app_config_t CONFIG={.tag="living_worlds",.window_title="Living Worlds",.canvas_bind=GSP_LIVING_WORLDS_BIND_GAME_CANVAS,.touch_points=1,.target_fps=30,.gsp_bundle=gsp_bundle_config,.register_mirror=raylib_screen_mirror_register,.before_display=before_display,.on_start=on_start,.on_event=on_event,.on_update=on_update,.on_render=on_render,.on_stats=on_stats};
const mosaico_game_app_config_t *living_worlds_app_config(void){return &CONFIG;}
