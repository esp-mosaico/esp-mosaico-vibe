// SPDX-License-Identifier: Apache-2.0
#define GSP_BUNDLE_ENABLE_RAW_IDS 1
#include "bundle_gsp.h"
#include "esp_log.h"
#include "mosaico_game_assets.h"
#include "raylib_screen_mirror.h"
#include "underwater_app.h"
#include "underwater_view.h"
#include "underwater_world.h"

static underwater_world_t world;
static MosaicoAtlas panorama,fish,creatures,aurora,sunrise;
extern const uint8_t _binary_panorama_atlas_start[];
extern const uint8_t _binary_panorama_atlas_end[];
extern const uint8_t _binary_reef_fish_atlas_start[];
extern const uint8_t _binary_reef_fish_atlas_end[];
extern const uint8_t _binary_marine_creatures_atlas_start[];
extern const uint8_t _binary_marine_creatures_atlas_end[];
extern const uint8_t _binary_aurora_atlas_start[],_binary_aurora_atlas_end[];
extern const uint8_t _binary_sunrise_atlas_start[],_binary_sunrise_atlas_end[];
static esp_err_t before_display(void)
{
    esp_err_t err=mosaico_game_asset_register_memory("panorama.atlas",_binary_panorama_atlas_start,
        (size_t)(_binary_panorama_atlas_end-_binary_panorama_atlas_start));
    if(err!=ESP_OK)return err;
    err=mosaico_game_asset_register_memory("marine_creatures.atlas",_binary_marine_creatures_atlas_start,
        (size_t)(_binary_marine_creatures_atlas_end-_binary_marine_creatures_atlas_start));
    if(err!=ESP_OK)return err;
    err=mosaico_game_asset_register_memory("aurora.atlas",_binary_aurora_atlas_start,(size_t)(_binary_aurora_atlas_end-_binary_aurora_atlas_start));if(err!=ESP_OK)return err;
    err=mosaico_game_asset_register_memory("sunrise.atlas",_binary_sunrise_atlas_start,(size_t)(_binary_sunrise_atlas_end-_binary_sunrise_atlas_start));if(err!=ESP_OK)return err;
    err=mosaico_game_asset_register_memory("reef_fish.atlas",_binary_reef_fish_atlas_start,
        (size_t)(_binary_reef_fish_atlas_end-_binary_reef_fish_atlas_start));
    if(err!=ESP_OK)return err;
    panorama=LoadMosaicoAtlas("panorama.atlas");
    fish=LoadMosaicoAtlas("reef_fish.atlas");
    creatures=LoadMosaicoAtlas("marine_creatures.atlas");
    aurora=LoadMosaicoAtlas("aurora.atlas");sunrise=LoadMosaicoAtlas("sunrise.atlas");
    return panorama.texture.id&&fish.texture.id&&creatures.texture.id&&aurora.texture.id&&sunrise.texture.id?ESP_OK:ESP_ERR_NOT_FOUND;
}
static esp_err_t on_start(void){underwater_world_reset(&world);return ESP_OK;}
static void on_event(const mosaico_device_event_t *event){if(event&&event->type==MOSAICO_DEVICE_EVENT_POINTER)underwater_world_pointer(&world,(float)event->x,(float)event->y,event->pressed);}
static void on_update(void){underwater_world_update(&world);}
static void on_render(void){underwater_view_render(&world,panorama,fish,creatures,aurora,sunrise);}
static void on_stats(void){ESP_LOGI("underwater_360","yaw=%.1f pitch=%.1f hash=%08lx",world.yaw,world.pitch,(unsigned long)underwater_world_hash(&world));}
static const mosaico_game_app_config_t CONFIG={.tag="underwater_360",.window_title="Living Worlds",.canvas_bind=GSP_UNDERWATER_360_BIND_GAME_CANVAS,.touch_points=1,.target_fps=30,.gsp_bundle=gsp_bundle_config,.register_mirror=raylib_screen_mirror_register,.before_display=before_display,.on_start=on_start,.on_event=on_event,.on_update=on_update,.on_render=on_render,.on_stats=on_stats};
const mosaico_game_app_config_t *underwater_app_config(void){return &CONFIG;}
