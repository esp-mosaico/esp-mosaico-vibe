// SPDX-License-Identifier: Apache-2.0
/* ESP32-S3-BOX-3 application-side adapter using official ESP Board Manager.
 * All pins, panel/touch chip selection and backlight initialization come from
 * espressif/esp_boards, selected by idf.py bmgr with our small amend profile. */
#include "raylib_lite_example_board.h"

#include <stdatomic.h>
#include <string.h>
#include "esp_board_manager_includes.h"
#include "esp_lcd_touch.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "raylib_lite_action.h"
#include "box3_video.h"
#include "platform_esp_audio.h"

#define INPUT_CAPACITY 32U
#define POLL_MS 12U
#define BOARD_TAG "box3_board"

struct raylib_lite_example_board {
    raylib_lite_example_board_config_t config;
    raylib_lite_platform_t services;
    raylib_lite_input_queue_t input;
    raylib_lite_input_event_t events[INPUT_CAPACITY];
    box3_video_t *video;
    esp_lcd_touch_handle_t touch;
    SemaphoreHandle_t input_mutex;
    SemaphoreHandle_t touch_done;
    TaskHandle_t touch_task;
    atomic_bool sampling;
    bool bmgr_initialized;
};

static struct raylib_lite_example_board s_board;
static bool s_claimed;

static esp_err_t to_esp(raylib_lite_result_t result)
{
    switch (result) {
    case RAYLIB_LITE_OK: return ESP_OK;
    case RAYLIB_LITE_INVALID_ARGUMENT: return ESP_ERR_INVALID_ARG;
    case RAYLIB_LITE_INVALID_STATE: return ESP_ERR_INVALID_STATE;
    case RAYLIB_LITE_NO_MEMORY: return ESP_ERR_NO_MEM;
    case RAYLIB_LITE_NOT_SUPPORTED: return ESP_ERR_NOT_SUPPORTED;
    case RAYLIB_LITE_BUSY: return ESP_ERR_INVALID_STATE;
    case RAYLIB_LITE_TIMEOUT: return ESP_ERR_TIMEOUT;
    default: return ESP_FAIL;
    }
}

esp_err_t raylib_lite_example_board_haptic_init(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t raylib_lite_example_board_haptic_set(uint8_t strength)
{
    (void)strength;
    return ESP_ERR_NOT_SUPPORTED;
}

static void lock_input(void *ctx)
{
    (void)xSemaphoreTake((SemaphoreHandle_t)ctx, portMAX_DELAY);
}

static void unlock_input(void *ctx)
{
    (void)xSemaphoreGive((SemaphoreHandle_t)ctx);
}

static uint64_t monotonic_us(void *ctx)
{
    (void)ctx;
    return (uint64_t)esp_timer_get_time();
}

static void sleep_for_us(void *ctx, uint64_t us)
{
    (void)ctx;
    if (us == 0) {
        taskYIELD();
        return;
    }
    TickType_t ticks = pdMS_TO_TICKS((us + 999U) / 1000U);
    vTaskDelay(ticks ? ticks : 1);
}

static bool push_event(struct raylib_lite_example_board *board,
                       const raylib_lite_input_event_t *event)
{
    return raylib_lite_input_push(&board->input, event) == RAYLIB_LITE_OK;
}

/* Retry unqueued release events on the next poll rather than losing them
 * permanently when the bounded Game input queue fills. */
static void touch_worker(void *arg)
{
    struct raylib_lite_example_board *board = arg;
    uint8_t max = board->config.touch_points ? board->config.touch_points : 1;
    if (max > RAYLIB_LITE_ACTION_CONTACT_CAPACITY)
        max = RAYLIB_LITE_ACTION_CONTACT_CAPACITY;

    bool was_pressed = false;
    int32_t last_x = 0, last_y = 0;
    raylib_lite_input_contact_t old[RAYLIB_LITE_ACTION_CONTACT_CAPACITY] = {0};

    while (atomic_load_explicit(&board->sampling, memory_order_acquire)) {
        esp_lcd_touch_point_data_t points[RAYLIB_LITE_ACTION_CONTACT_CAPACITY] = {0};
        uint8_t count = 0;
        if (esp_lcd_touch_read_data(board->touch) == ESP_OK &&
                esp_lcd_touch_get_data(board->touch, points, &count, max) == ESP_OK) {
            if (count > max) count = max;
            uint64_t stamp = monotonic_us(NULL);
            if (max == 1) {
                bool pressed = count > 0;
                int32_t x = last_x, y = last_y;
                if (pressed) {
                    int32_t px = points[0].x, py = points[0].y;
                    if (box3_video_map_touch(board->video, &px, &py)) {
                        x = px;
                        y = py;
                    } else {
                        pressed = false;
                    }
                }
                if ((pressed && (!was_pressed || x != last_x || y != last_y)) ||
                        (!pressed && was_pressed)) {
                    raylib_lite_input_event_t event = {
                        .type = RAYLIB_LITE_INPUT_POINTER,
                        .x = x, .y = y, .pressed = pressed,
                        .timestamp_us = stamp,
                    };
                    if (push_event(board, &event)) {
                        was_pressed = pressed;
                        last_x = x;
                        last_y = y;
                    }
                }
            } else {
                /* Release absent contacts first; retained old[] entries retry
                 * when the queue cannot accept an important release edge. */
                for (size_t i = 0; i < RAYLIB_LITE_ACTION_CONTACT_CAPACITY; i++) {
                    if (!old[i].active) continue;
                    bool found = false;
                    for (uint8_t j = 0; j < count; j++) {
                        int32_t px = points[j].x, py = points[j].y;
                        if (points[j].track_id == old[i].track_id &&
                                box3_video_map_touch(board->video, &px, &py)) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        raylib_lite_input_event_t event = {
                            .type = RAYLIB_LITE_INPUT_TOUCH,
                            .x = old[i].x, .y = old[i].y,
                            .value = old[i].track_id, .pressed = false,
                            .timestamp_us = stamp,
                        };
                        if (push_event(board, &event)) old[i].active = false;
                    }
                }
                for (uint8_t j = 0; j < count; j++) {
                    int32_t x = points[j].x, y = points[j].y;
                    if (!box3_video_map_touch(board->video, &x, &y)) continue;
                    int slot = -1;
                    for (size_t i = 0; i < RAYLIB_LITE_ACTION_CONTACT_CAPACITY; i++) {
                        if (old[i].active && old[i].track_id == points[j].track_id) {
                            slot = (int)i;
                            break;
                        }
                    }
                    if (slot >= 0 && old[slot].x == x && old[slot].y == y) continue;
                    if (slot < 0) {
                        for (size_t i = 0; i < RAYLIB_LITE_ACTION_CONTACT_CAPACITY; i++) {
                            if (!old[i].active) {
                                slot = (int)i;
                                break;
                            }
                        }
                    }
                    if (slot < 0) continue;
                    raylib_lite_input_event_t event = {
                        .type = RAYLIB_LITE_INPUT_TOUCH,
                        .x = x, .y = y, .value = points[j].track_id,
                        .pressed = true, .timestamp_us = stamp,
                    };
                    if (push_event(board, &event)) {
                        old[slot] = (raylib_lite_input_contact_t) {
                            .track_id = points[j].track_id,
                            .x = x, .y = y, .active = true,
                        };
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
    }
    xSemaphoreGive(board->touch_done);
    vTaskDelete(NULL);
}

esp_err_t raylib_lite_example_board_create(
    const raylib_lite_example_board_config_t *config,
    raylib_lite_example_board_t **out_board)
{
    if (!config || !out_board || !config->logical_width || !config->logical_height)
        return ESP_ERR_INVALID_ARG;
    *out_board = NULL;
    if (s_claimed) return ESP_ERR_INVALID_STATE;
    s_claimed = true;
    struct raylib_lite_example_board *board = &s_board;
    memset(board, 0, sizeof(*board));
    atomic_init(&board->sampling, false);
    board->config = *config;
    *out_board = board;

    esp_err_t err = esp_board_manager_init();
    if (err != ESP_OK) goto failed;
    board->bmgr_initialized = true;

    dev_display_lcd_handles_t *lcd = NULL;
    dev_display_lcd_config_t *lcd_config = NULL;
    dev_lcd_touch_handles_t *touch = NULL;
    if ((err = esp_board_manager_get_device_handle(
            "display_lcd", (void **)&lcd)) != ESP_OK ||
            (err = esp_board_manager_get_device_config(
            "display_lcd", (void **)&lcd_config)) != ESP_OK ||
            (err = esp_board_manager_get_device_handle(
            "lcd_touch", (void **)&touch)) != ESP_OK)
        goto failed;
    if (!lcd || !lcd->panel_handle || !lcd->io_handle ||
            !touch || !touch->touch_handle || !lcd_config) {
        err = ESP_ERR_INVALID_STATE;
        goto failed;
    }
    board->touch = touch->touch_handle;

    /* BOX-3 TT21100 has a reversed physical X axis. Board Manager publishes
     * the detected 8-bit I2C address; 0x48 identifies TT21100. Do not mirror
     * GT911, which uses a different touch orientation. */
    uint16_t touch_addr = 0;
    err = esp_board_device_get_i2c_effective_addr("lcd_touch", &touch_addr);
    if (err != ESP_OK) goto failed;
    if (touch_addr == 0x48) {
        err = esp_lcd_touch_set_mirror_x(board->touch, true);
        if (err != ESP_OK) goto failed;
        ESP_LOGI(BOARD_TAG, "TT21100 touch X mirroring enabled");
    }

    bool swap_bytes = true;
    if (lcd_config->frame_format == DEV_DISPLAY_LCD_FRAME_FORMAT_RGB565_LE)
        swap_bytes = false;
    else if (lcd_config->frame_format != DEV_DISPLAY_LCD_FRAME_FORMAT_RGB565_BE)
        ESP_LOGW(BOARD_TAG, "LCD frame format unspecified, assuming RGB565 BE");

    raylib_lite_result_t result = box3_video_open(
        lcd->panel_handle, lcd->io_handle,
        config->logical_width, config->logical_height, swap_bytes,
        &board->video);
    if (result != RAYLIB_LITE_OK) {
        err = to_esp(result);
        goto failed;
    }

    board->input_mutex = xSemaphoreCreateMutex();
    board->touch_done = xSemaphoreCreateBinary();
    if (!board->input_mutex || !board->touch_done) {
        err = ESP_ERR_NO_MEM;
        goto failed;
    }
    const raylib_lite_input_sync_t sync = {
        .context = board->input_mutex,
        .lock = lock_input,
        .unlock = unlock_input,
    };
    result = raylib_lite_input_queue_init(
        &board->input, board->events, INPUT_CAPACITY, &sync);
    if (result != RAYLIB_LITE_OK) {
        err = to_esp(result);
        goto failed;
    }
    board->services = (raylib_lite_platform_t) {
        .video = box3_video_backend(board->video),
        .clock = {
            .monotonic_us = monotonic_us,
            .sleep_for_us = sleep_for_us,
        },
        .audio = NULL,
    };
    if (config->enable_imu)
        ESP_LOGW(BOARD_TAG, "IMU unavailable in current Board Manager profile");
    return ESP_OK;

failed:
    {
        esp_err_t cleanup = raylib_lite_example_board_retry_cleanup(board, 3000);
        if (cleanup == ESP_OK) *out_board = NULL;
        return cleanup == ESP_OK ? err : cleanup;
    }
}

const raylib_lite_platform_t *raylib_lite_example_board_services(
    raylib_lite_example_board_t *board)
{
    return board ? &board->services : NULL;
}

raylib_lite_input_queue_t *raylib_lite_example_board_input(
    raylib_lite_example_board_t *board)
{
    return board ? &board->input : NULL;
}

esp_err_t raylib_lite_example_board_start_input(raylib_lite_example_board_t *board)
{
    if (!board || atomic_load(&board->sampling))
        return ESP_ERR_INVALID_STATE;
    atomic_store(&board->sampling, true);
    if (xTaskCreate(touch_worker, "box3_touch", 4096, board, 5,
                    &board->touch_task) != pdPASS) {
        atomic_store(&board->sampling, false);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t raylib_lite_example_board_stop(
    raylib_lite_example_board_t *board, uint32_t timeout_ms)
{
    if (!board) return ESP_ERR_INVALID_ARG;
    atomic_store(&board->sampling, false);
    if (board->touch_task) {
        if (xSemaphoreTake(board->touch_done, pdMS_TO_TICKS(timeout_ms)) != pdTRUE)
            return ESP_ERR_TIMEOUT;
        board->touch_task = NULL;
    }
    return ESP_OK;
}

esp_err_t raylib_lite_example_board_retry_cleanup(
    raylib_lite_example_board_t *board, uint32_t timeout_ms)
{
    if (!board || board != &s_board) return ESP_ERR_INVALID_ARG;
    /* Also fence direct Board cleanup, not just the Native Game launcher. */
    raylib_lite_result_t audio_result = raylib_lite_game_audio_shutdown(timeout_ms);
    if (audio_result != RAYLIB_LITE_OK) return to_esp(audio_result);
    esp_err_t err = raylib_lite_example_board_stop(board, timeout_ms);
    if (err != ESP_OK) return err;
    if (board->video) {
        raylib_lite_result_t result = box3_video_close(board->video, timeout_ms);
        if (result != RAYLIB_LITE_OK) return to_esp(result);
        board->video = NULL;
    }
    if (board->bmgr_initialized) {
        err = esp_board_manager_deinit();
        if (err != ESP_OK) return err;
        board->bmgr_initialized = false;
    }
    if (board->input.initialized) raylib_lite_input_queue_deinit(&board->input);
    if (board->touch_done) {
        vSemaphoreDelete(board->touch_done);
        board->touch_done = NULL;
    }
    if (board->input_mutex) {
        vSemaphoreDelete(board->input_mutex);
        board->input_mutex = NULL;
    }
    s_claimed = false;
    return ESP_OK;
}

esp_err_t raylib_lite_example_board_destroy(
    raylib_lite_example_board_t *board, uint32_t timeout_ms)
{
    return raylib_lite_example_board_retry_cleanup(board, timeout_ms);
}
