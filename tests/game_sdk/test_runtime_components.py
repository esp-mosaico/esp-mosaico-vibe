import pathlib
import subprocess
import tempfile
import textwrap
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]


class RuntimeComponentsTest(unittest.TestCase):
    def test_scene_ui_fx_and_save_contracts(self):
        with tempfile.TemporaryDirectory() as temporary:
            temp = pathlib.Path(temporary)
            (temp / "esp_err.h").write_text(textwrap.dedent("""
                #pragma once
                typedef int esp_err_t;
                #define ESP_OK 0
                #define ESP_ERR_INVALID_ARG 1
                #define ESP_ERR_INVALID_STATE 2
                #define ESP_ERR_INVALID_CRC 3
                #define ESP_ERR_INVALID_VERSION 4
                #define ESP_ERR_NVS_NOT_FOUND 5
            """))
            (temp / "nvs.h").write_text(textwrap.dedent("""
                #pragma once
                #include <stddef.h>
                #include <stdint.h>
                #include "esp_err.h"
                typedef int nvs_handle_t;
                #define NVS_READONLY 0
                #define NVS_READWRITE 1
                esp_err_t nvs_open(const char *, int, nvs_handle_t *);
                esp_err_t nvs_get_blob(nvs_handle_t, const char *, void *, size_t *);
                esp_err_t nvs_set_blob(nvs_handle_t, const char *, const void *, size_t);
                esp_err_t nvs_commit(nvs_handle_t);
                void nvs_close(nvs_handle_t);
            """))
            (temp / "raylib.h").write_text(textwrap.dedent("""
                #pragma once
                typedef struct { float x,y,width,height; } Rectangle;
                typedef struct { unsigned char r,g,b,a; } Color;
                void DrawRectangle(int,int,int,int,Color);
                int MeasureText(const char *,int);
                void DrawText(const char *,int,int,int,Color);
            """))
            (temp / "mosaico_raylib_fast.h").write_text("#pragma once\n")
            harness = temp / "runtime_test.c"
            harness.write_text(textwrap.dedent("""
                #include <assert.h>
                #include <stdint.h>
                #include <string.h>
                #include "mosaico_game_scene.h"
                #include "mosaico_game_ui.h"
                #include "mosaico_game_fx.h"
                #include "mosaico_game_save.h"
                #include "nvs.h"

                static uint8_t blob[512]; static size_t blob_size; static int writes;
                esp_err_t nvs_open(const char *n,int m,nvs_handle_t *h){(void)n;(void)m;*h=1;return ESP_OK;}
                esp_err_t nvs_get_blob(nvs_handle_t h,const char *k,void *p,size_t *n){(void)h;(void)k;if(!blob_size)return ESP_ERR_NVS_NOT_FOUND;if(*n<blob_size)return ESP_ERR_INVALID_ARG;memcpy(p,blob,blob_size);*n=blob_size;return ESP_OK;}
                esp_err_t nvs_set_blob(nvs_handle_t h,const char *k,const void *p,size_t n){(void)h;(void)k;memcpy(blob,p,n);blob_size=n;++writes;return ESP_OK;}
                esp_err_t nvs_commit(nvs_handle_t h){(void)h;return ESP_OK;} void nvs_close(nvs_handle_t h){(void)h;}
                void DrawRectangle(int a,int b,int c,int d,Color e){(void)a;(void)b;(void)c;(void)d;(void)e;}
                int MeasureText(const char *s,int n){(void)s;return n;} void DrawText(const char *s,int a,int b,int c,Color d){(void)s;(void)a;(void)b;(void)c;(void)d;}

                static int entered,paused,resumed,exited,clicked;
                static void enter(void *p){(void)p;++entered;} static void pause_scene(void *p){(void)p;++paused;}
                static void resume_scene(void *p){(void)p;++resumed;} static void exit_scene(void *p){(void)p;++exited;}
                static void click(uint16_t id,void *p){(void)p;clicked+=(int)id;}
                static esp_err_t migrate(uint16_t old,const void *src,size_t size,void *dst,size_t target){assert(old==1&&size==2&&target==4);uint16_t value;memcpy(&value,src,2);*(uint32_t *)dst=value;return ESP_OK;}

                int main(void){
                    mosaico_scene_t scene={.enter=enter,.exit=exit_scene,.pause=pause_scene,.resume=resume_scene};
                    mosaico_scene_stack_t stack;mosaico_scene_stack_init(&stack);
                    assert(mosaico_scene_push(&stack,&scene,0));assert(mosaico_scene_push(&stack,&scene,0));
                    assert(entered==2&&paused==1);assert(mosaico_scene_pop(&stack));assert(exited==1&&resumed==1);
                    assert(mosaico_scene_replace(&stack,&scene,0));assert(entered==3&&paused==1&&exited==2);

                    mosaico_tween_t tween;mosaico_tween_start(&tween,0,10,2,MOSAICO_EASE_LINEAR);
                    assert(mosaico_tween_tick(&tween)==5);assert(mosaico_tween_tick(&tween)==10&&!tween.active);
                    mosaico_particle_t item[1];mosaico_particle_pool_t pool;mosaico_particle_pool_init(&pool,item,1);
                    assert(mosaico_particle_spawn(&pool,(mosaico_particle_t){.life=1}));mosaico_particle_pool_update(&pool);assert(!item[0].active);

                    mosaico_ui_t ui;mosaico_ui_init(&ui);Color white={255,255,255,255};
                    assert(mosaico_ui_add(&ui,(mosaico_ui_node_t){.id=1,.type=MOSAICO_UI_BUTTON,.bounds={0,0,10,10},.text_color=white,.click=click}));
                    assert(mosaico_ui_add(&ui,(mosaico_ui_node_t){.id=2,.type=MOSAICO_UI_BUTTON,.bounds={10,0,10,10},.text_color=white,.click=click}));
                    assert(mosaico_ui_pointer(&ui,10,5,5,1));assert(mosaico_ui_pointer(&ui,11,15,5,1));assert(ui.nodes[0].pressed&&ui.nodes[1].pressed);
                    assert(mosaico_ui_pointer(&ui,10,5,5,0));assert(clicked==1&&!ui.nodes[0].pressed&&ui.nodes[1].pressed);

                    uint16_t defaults=7,value=0; mosaico_save_t save;
                    mosaico_save_config_t config={"test","state",1,sizeof(value),750,0};assert(mosaico_save_init(&save,&config)==ESP_OK);
                    assert(mosaico_save_load(&save,&value,&defaults)==ESP_OK&&value==7);
                    value=42;assert(mosaico_save_request(&save,&value,100)==ESP_OK);assert(mosaico_save_flush(&save,500,0)==ESP_OK&&writes==0);
                    assert(mosaico_save_flush(&save,850,0)==ESP_OK&&writes==1);
                    value=0;assert(mosaico_save_load(&save,&value,&defaults)==ESP_OK&&value==42);
                    mosaico_save_t upgraded;uint32_t wide=0;mosaico_save_config_t next={"test","state",2,sizeof(wide),0,migrate};
                    assert(mosaico_save_init(&upgraded,&next)==ESP_OK);assert(mosaico_save_load(&upgraded,&wide,0)==ESP_OK&&wide==42);
                    blob[blob_size-1]^=1;value=0;assert(mosaico_save_load(&save,&value,&defaults)==ESP_ERR_INVALID_CRC&&value==7);
                    return 0;
                }
            """))
            include_dirs = [
                temp,
                ROOT / "submodule/raylib-lite-engine/components/mosaico_game_scene/include",
                ROOT / "submodule/raylib-lite-engine/components/mosaico_game_ui/include",
                ROOT / "submodule/raylib-lite-engine/components/mosaico_game_fx/include",
                ROOT / "submodule/raylib-lite-engine/components/mosaico_game_save/include",
            ]
            command = ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror"]
            command += [f"-I{path}" for path in include_dirs]
            command += [str(harness),
                        str(ROOT / "submodule/raylib-lite-engine/components/mosaico_game_scene/mosaico_game_scene.c"),
                        str(ROOT / "submodule/raylib-lite-engine/components/mosaico_game_ui/mosaico_game_ui.c"),
                        str(ROOT / "submodule/raylib-lite-engine/components/mosaico_game_fx/mosaico_game_fx.c"),
                        str(ROOT / "submodule/raylib-lite-engine/components/mosaico_game_save/mosaico_game_save.c"),
                        "-o", str(temp / "runtime_test")]
            subprocess.run(command, check=True)
            subprocess.run([str(temp / "runtime_test")], check=True)

    def test_action_mapper_zones_and_restart_pulse(self):
        with tempfile.TemporaryDirectory() as temporary:
            temp = pathlib.Path(temporary)
            (temp / "esp_err.h").write_text("#pragma once\ntypedef int esp_err_t;\n#define ESP_OK 0\n")
            harness = temp / "action_test.c"
            harness.write_text(textwrap.dedent("""
                #include <assert.h>
                #include "mosaico_game_action.h"

                int main(void){
                    mosaico_action_reset();
                    mosaico_action_zone_t zones[]={
                        {0,360,150,480,MOSAICO_ACTION_LEFT},
                        {150,360,300,480,MOSAICO_ACTION_RIGHT},
                        {300,360,480,480,MOSAICO_ACTION_JUMP},
                    };
                    mosaico_action_set_zones(zones,3);
                    mosaico_device_event_t down={.type=MOSAICO_DEVICE_EVENT_TOUCH,
                        .x=40,.y=400,.value=1,.pressed=true};
                    mosaico_action_apply_event(&down);
                    mosaico_action_begin_frame();
                    assert(mosaico_action_down(MOSAICO_ACTION_LEFT));
                    assert(mosaico_action_pressed(MOSAICO_ACTION_LEFT));
                    assert(mosaico_action_pressed(MOSAICO_ACTION_RESTART));
                    mosaico_action_begin_frame();
                    assert(mosaico_action_down(MOSAICO_ACTION_LEFT));
                    assert(!mosaico_action_pressed(MOSAICO_ACTION_RESTART));
                    mosaico_device_event_t jump={.type=MOSAICO_DEVICE_EVENT_TOUCH,
                        .x=350,.y=400,.value=2,.pressed=true};
                    mosaico_action_apply_event(&jump);
                    mosaico_action_begin_frame();
                    assert(mosaico_action_down(MOSAICO_ACTION_LEFT));
                    assert(mosaico_action_down(MOSAICO_ACTION_JUMP));
                    mosaico_device_event_t axis={.type=MOSAICO_DEVICE_EVENT_JOYSTICK,.x=400,.y=0};
                    mosaico_action_apply_event(&axis);
                    mosaico_action_begin_frame();
                    assert(mosaico_action_down(MOSAICO_ACTION_RIGHT));
                    return 0;
                }
            """))
            command = ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                       f"-I{temp}",
                       f"-I{ROOT / 'submodule/raylib-lite-engine/components/mosaico_game/include'}",
                       f"-I{ROOT / 'submodule/raylib-lite-engine/components/mosaico_game_input/include'}",
                       str(harness),
                       str(ROOT / "submodule/raylib-lite-engine/components/mosaico_game_input/mosaico_game_action.c"),
                       "-o", str(temp / "action_test")]
            subprocess.run(command, check=True)
            subprocess.run([str(temp / "action_test")], check=True)


if __name__ == "__main__":
    unittest.main()
