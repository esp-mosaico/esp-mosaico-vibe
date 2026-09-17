// SPDX-License-Identifier: Apache-2.0
#define GSP_BUNDLE_ENABLE_RAW_IDS 1
#include "bundle_gsp.h"
#include "driver/jpeg_decode.h"
#include "esp_check.h"
#include "esp_log.h"
#include <stdlib.h>
#include "mosaico_game_assets.h"
#include "raylib_screen_mirror.h"
#include "underwater_app.h"
#include "underwater_view.h"
#include "underwater_world.h"

static underwater_world_t world;
static MosaicoAtlas background,fish,creatures,reindeer;
static void *background_pixels;
static jpeg_decoder_handle_t background_decoder;
static uint8_t loaded_scene=UINT8_MAX;
extern const uint8_t _binary_reef_fish_atlas_start[];
extern const uint8_t _binary_reef_fish_atlas_end[];
extern const uint8_t _binary_marine_creatures_atlas_start[];
extern const uint8_t _binary_marine_creatures_atlas_end[];
extern const uint8_t _binary_reindeer_atlas_start[],_binary_reindeer_atlas_end[];
extern const uint8_t _binary_ocean_jpg_start[],_binary_ocean_jpg_end[];
extern const uint8_t _binary_aurora_jpg_start[],_binary_aurora_jpg_end[];
extern const uint8_t _binary_sunrise_jpg_start[],_binary_sunrise_jpg_end[];
extern const uint8_t _binary_rainforest_jpg_start[],_binary_rainforest_jpg_end[];

static esp_err_t decode_background(jpeg_decoder_handle_t decoder,const char *name,
    const uint8_t *start,const uint8_t *end,MosaicoAtlas *out,void **out_pixels)
{
    jpeg_decode_picture_info_t info={0};
    size_t stream_size=(size_t)(end-start);
    ESP_RETURN_ON_ERROR(jpeg_decoder_get_info(start,stream_size,&info),"underwater_360","parse %s",name);
    size_t padded_width=(info.width+15U)&~15U,padded_height=(info.height+15U)&~15U;
    size_t required=padded_width*padded_height*2U,allocated=0;
    const jpeg_decode_memory_alloc_cfg_t memory={.buffer_direction=JPEG_DEC_ALLOC_OUTPUT_BUFFER};
    void *pixels=jpeg_alloc_decoder_mem(required,&memory,&allocated);
    ESP_RETURN_ON_FALSE(pixels&&allocated>=required,ESP_ERR_NO_MEM,"underwater_360","allocate %s RGB565",name);
    const jpeg_decode_cfg_t config={.output_format=JPEG_DECODE_OUT_FORMAT_RGB565,
        /* The software rasterizer and ESP-Iris mirror consume native
           little-endian RGB565. BGR selects little-endian output in the S31
           JPEG driver; RGB would require an explicit swap16 pass. */
        .rgb_order=JPEG_DEC_RGB_ELEMENT_ORDER_BGR,.conv_std=JPEG_YUV_RGB_CONV_STD_BT601};
    uint32_t output_size=0;
    esp_err_t err=jpeg_decoder_process(decoder,&config,start,stream_size,pixels,allocated,&output_size);
    if(err!=ESP_OK){free(pixels);return err;}
    Texture2D texture=Mosaico2DRegisterRGB565(pixels,(int)info.width,(int)info.height);
    if(!texture.id){free(pixels);return ESP_ERR_NO_MEM;}
    *out_pixels=pixels;*out=(MosaicoAtlas){.texture=texture};
    ESP_LOGI("underwater_360","JPEG %s: %lux%lu %u bytes -> %u-byte RGB565",
        name,(unsigned long)info.width,(unsigned long)info.height,(unsigned)stream_size,(unsigned)output_size);
    return ESP_OK;
}

static esp_err_t load_scene_background(uint8_t scene)
{
    const char *name;const uint8_t *start,*end;
    switch(scene){
    case UNDERWATER_SCENE_AURORA:name="aurora";start=_binary_aurora_jpg_start;end=_binary_aurora_jpg_end;break;
    case UNDERWATER_SCENE_SUNRISE:name="sunrise";start=_binary_sunrise_jpg_start;end=_binary_sunrise_jpg_end;break;
    case UNDERWATER_SCENE_RAINFOREST:name="rainforest";start=_binary_rainforest_jpg_start;end=_binary_rainforest_jpg_end;break;
    default:name="ocean";start=_binary_ocean_jpg_start;end=_binary_ocean_jpg_end;scene=UNDERWATER_SCENE_OCEAN;break;
    }
    MosaicoAtlas next={0};void *next_pixels=NULL;
    ESP_RETURN_ON_ERROR(decode_background(background_decoder,name,start,end,&next,&next_pixels),
                        "underwater_360","load scene background");
    if(background.texture.id)Mosaico2DUnloadTexture(background.texture);
    free(background_pixels);background=next;background_pixels=next_pixels;loaded_scene=scene;
    return ESP_OK;
}
static esp_err_t before_display(void)
{
    esp_err_t err;
    err=mosaico_game_asset_register_memory("marine_creatures.atlas",_binary_marine_creatures_atlas_start,
        (size_t)(_binary_marine_creatures_atlas_end-_binary_marine_creatures_atlas_start));
    if(err!=ESP_OK)return err;
    err=mosaico_game_asset_register_memory("reindeer.atlas",_binary_reindeer_atlas_start,(size_t)(_binary_reindeer_atlas_end-_binary_reindeer_atlas_start));if(err!=ESP_OK)return err;
    err=mosaico_game_asset_register_memory("reef_fish.atlas",_binary_reef_fish_atlas_start,
        (size_t)(_binary_reef_fish_atlas_end-_binary_reef_fish_atlas_start));
    if(err!=ESP_OK)return err;
    const jpeg_decode_engine_cfg_t engine_config={.intr_priority=0,.timeout_ms=250};
    ESP_RETURN_ON_ERROR(jpeg_new_decoder_engine(&engine_config,&background_decoder),"underwater_360","create JPEG decoder");
    ESP_RETURN_ON_ERROR(load_scene_background(UNDERWATER_SCENE_OCEAN),"underwater_360","load initial background");
    fish=LoadMosaicoAtlas("reef_fish.atlas");
    creatures=LoadMosaicoAtlas("marine_creatures.atlas");
    reindeer=LoadMosaicoAtlas("reindeer.atlas");
    return background.texture.id&&fish.texture.id&&creatures.texture.id&&reindeer.texture.id?ESP_OK:ESP_ERR_NOT_FOUND;
}
static esp_err_t on_start(void){underwater_world_reset(&world);return ESP_OK;}
static void on_event(const mosaico_device_event_t *event){
    if(event&&event->type==MOSAICO_DEVICE_EVENT_POINTER){
        uint8_t previous=world.scene;
        underwater_world_pointer(&world,(float)event->x,(float)event->y,event->pressed);
        if(world.scene!=previous&&world.scene!=loaded_scene){
            if(load_scene_background(world.scene)!=ESP_OK)world.scene=previous;
        }
    }
}
static void on_update(void){underwater_world_update(&world);}
static void on_render(void){underwater_view_render(&world,background,fish,creatures,background,background,reindeer,background);}
static void on_stats(void){ESP_LOGI("underwater_360","yaw=%.1f pitch=%.1f hash=%08lx",world.yaw,world.pitch,(unsigned long)underwater_world_hash(&world));}
static const mosaico_game_app_config_t CONFIG={.tag="underwater_360",.window_title="Living Worlds",.canvas_bind=GSP_UNDERWATER_360_BIND_GAME_CANVAS,.touch_points=1,.target_fps=30,.gsp_bundle=gsp_bundle_config,.register_mirror=raylib_screen_mirror_register,.before_display=before_display,.on_start=on_start,.on_event=on_event,.on_update=on_update,.on_render=on_render,.on_stats=on_stats};
const mosaico_game_app_config_t *underwater_app_config(void){return &CONFIG;}
