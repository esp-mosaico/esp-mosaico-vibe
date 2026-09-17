#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "bsp/esp_mosaico.h"
#include "esp_display_present.h"
#include "esp_gsp_esp_lcd.h"
#include "esp_heap_caps.h"
#include "esp_iris.h"
#include "freertos/semphr.h"
#include "iris_screen_mirror.h"

#define FRAME_BYTES ((size_t)BSP_LCD_H_RES * BSP_LCD_V_RES * 2U)
#define COVERAGE_BYTES ((((BSP_LCD_H_RES + 31U) / 32U) * BSP_LCD_V_RES) * 4U)
#define IDLE_BYTES (FRAME_BYTES + COVERAGE_BYTES)

typedef struct {
    size_t size;
} allocation_header_t;

struct esp_gsp_esp_lcd_pause {
    unsigned marker;
};

static esp_iris_screen_backend_t registered_backend;
static bool backend_registered;
static uint8_t repaint_frame[FRAME_BYTES];
static size_t active_bytes;
static size_t peak_bytes;
static unsigned allocation_calls;
static unsigned fail_allocation_call;
static unsigned present_calls;

static void *tracked_allocate(size_t size, bool clear)
{
    ++allocation_calls;
    if (allocation_calls == fail_allocation_call) {
        return NULL;
    }
    allocation_header_t *header = malloc(sizeof(*header) + size);
    assert(header != NULL);
    header->size = size;
    active_bytes += size;
    if (active_bytes > peak_bytes) {
        peak_bytes = active_bytes;
    }
    void *allocation = header + 1;
    if (clear) {
        memset(allocation, 0, size);
    }
    return allocation;
}

void *heap_caps_calloc(size_t count, size_t size, unsigned caps)
{
    assert(caps == (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    return tracked_allocate(count * size, true);
}

void *heap_caps_malloc(size_t size, unsigned caps)
{
    assert(caps == (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    return tracked_allocate(size, false);
}

void heap_caps_free(void *allocation)
{
    if (allocation == NULL) {
        return;
    }
    allocation_header_t *header = (allocation_header_t *)allocation - 1;
    assert(active_bytes >= header->size);
    active_bytes -= header->size;
    free(header);
}

SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t *storage)
{
    storage->count = 1;
    return storage;
}

SemaphoreHandle_t xSemaphoreCreateBinaryStatic(StaticSemaphore_t *storage)
{
    storage->count = 0;
    return storage;
}

int xSemaphoreTake(SemaphoreHandle_t semaphore, TickType_t ticks)
{
    (void)ticks;
    if (semaphore->count == 0) {
        return 0;
    }
    semaphore->count = 0;
    return pdTRUE;
}

int xSemaphoreGive(SemaphoreHandle_t semaphore)
{
    semaphore->count = 1;
    return pdTRUE;
}

void vSemaphoreDelete(SemaphoreHandle_t semaphore)
{
    (void)semaphore;
}

esp_err_t esp_iris_screen_register(const esp_iris_screen_backend_t *backend)
{
    assert(!backend_registered);
    registered_backend = *backend;
    backend_registered = true;
    return ESP_OK;
}

esp_err_t esp_gsp_esp_lcd_pause(esp_gsp_handle_t handle, uint32_t timeout_ms,
                                esp_gsp_esp_lcd_pause_t **pause)
{
    (void)handle; (void)timeout_ms; (void)pause;
    assert(!"capture must not pause GSP or allocate renderer control objects");
    return ESP_ERR_INVALID_STATE;
}

esp_err_t __wrap_esp_display_presenter_submit_buffer(
    esp_display_presenter_t *presenter,
    const esp_display_presenter_buffer_t *buffer,
    const esp_display_present_area_t *area,
    size_t stride_bytes);

esp_err_t esp_gsp_esp_lcd_resume_paused(esp_gsp_esp_lcd_pause_t *pause,
                                        esp_gsp_handle_t *handle)
{
    (void)pause; (void)handle;
    assert(!"capture must not resume/recreate GSP");
    return ESP_ERR_INVALID_STATE;
}

static esp_err_t present_rows(unsigned first, unsigned last)
{
    const esp_display_presenter_buffer_t buffer = {
        .surface = {
            .pixels = repaint_frame + first * BSP_LCD_H_RES * 2U,
            .pixel_format = ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB565,
        },
        .capacity_bytes = (last - first + 1U) * BSP_LCD_H_RES * 2U,
    };
    const esp_display_present_area_t area = {
        .x1 = 0,
        .y1 = first,
        .x2 = BSP_LCD_H_RES - 1,
        .y2 = last,
    };
    return __wrap_esp_display_presenter_submit_buffer(
        NULL, &buffer, &area, BSP_LCD_H_RES * 2U);
}

esp_err_t __real_esp_display_presenter_submit_buffer(
    esp_display_presenter_t *presenter,
    const esp_display_presenter_buffer_t *buffer,
    const esp_display_present_area_t *area,
    size_t stride_bytes)
{
    (void)presenter;
    (void)buffer;
    (void)area;
    (void)stride_bytes;
    ++present_calls;
    return ESP_OK;
}

static void exercise_one_stream(void)
{
    esp_iris_media_desc_t actual = {0};
    uint32_t total_size = 0;
    assert(registered_backend.begin(NULL, &actual, &total_size,
                                    registered_backend.user_ctx) == ESP_OK);
    assert(active_bytes == IDLE_BYTES + FRAME_BYTES);
    assert(actual.width == BSP_LCD_H_RES);
    assert(actual.height == BSP_LCD_V_RES);
    assert(actual.stride == BSP_LCD_H_RES * 2U);
    assert(actual.format == ESP_IRIS_PIXEL_FORMAT_RGB565);
    assert(total_size == FRAME_BYTES);
    assert(registered_backend.begin(NULL, &actual, &total_size,
                                    registered_backend.user_ctx) ==
           ESP_ERR_INVALID_STATE);
    assert(active_bytes == IDLE_BYTES + FRAME_BYTES);

    uint8_t sample[64] = {0};
    size_t sample_size = 0;
    assert(registered_backend.read(0, sample, sizeof(sample), &sample_size,
                                   registered_backend.user_ctx) == ESP_OK);
    assert(sample_size == sizeof(sample));
    assert(memcmp(sample, repaint_frame, sizeof(sample)) == 0);

    uint8_t retained[sizeof(sample)];
    memcpy(retained, sample, sizeof(retained));
    repaint_frame[0] ^= 0xff;
    assert(present_rows(0, 0) == ESP_OK);
    assert(registered_backend.read(0, sample, sizeof(sample), &sample_size,
                                   registered_backend.user_ctx) == ESP_OK);
    assert(memcmp(sample, retained, sizeof(sample)) == 0); /* stable snapshot */

    registered_backend.end(registered_backend.user_ctx);
    assert(active_bytes == IDLE_BYTES);
    registered_backend.end(registered_backend.user_ctx);
    assert(active_bytes == IDLE_BYTES);
}

int main(void)
{
    memset(repaint_frame, 0xa5, sizeof(repaint_frame));
    for (unsigned failed = 1; failed <= 2; ++failed) {
        fail_allocation_call = allocation_calls + failed;
        assert(iris_screen_mirror_init() == ESP_ERR_NO_MEM);
        assert(active_bytes == 0);
        assert(!backend_registered);
    }
    fail_allocation_call = 0;
    assert(iris_screen_mirror_init() == ESP_OK);
    assert(backend_registered);
    assert(active_bytes == IDLE_BYTES);
    assert(iris_screen_mirror_attach((esp_gsp_handle_t)(uintptr_t)1) == ESP_OK);
    assert(active_bytes == IDLE_BYTES);

    fail_allocation_call = allocation_calls + 1;
    esp_iris_media_desc_t actual = {0};
    uint32_t total_size = 0;
    assert(registered_backend.begin(NULL, &actual, &total_size,
                                    registered_backend.user_ctx) ==
           ESP_ERR_NO_MEM);
    assert(active_bytes == IDLE_BYTES);

    fail_allocation_call = 0;
    assert(registered_backend.begin(NULL, &actual, &total_size,
                                    registered_backend.user_ctx) == ESP_ERR_TIMEOUT);
    assert(active_bytes == IDLE_BYTES);
    assert(present_rows(0, BSP_LCD_V_RES / 2 - 1) == ESP_OK);
    assert(present_rows(0, BSP_LCD_V_RES / 2 - 1) == ESP_OK); /* no double count */
    assert(registered_backend.begin(NULL, &actual, &total_size,
                                    registered_backend.user_ctx) == ESP_ERR_TIMEOUT);
    assert(active_bytes == IDLE_BYTES);
    assert(present_rows(BSP_LCD_V_RES / 2, BSP_LCD_V_RES - 1) == ESP_OK);
    for (unsigned cycle = 0; cycle < 40; ++cycle) {
        exercise_one_stream();
    }
    assert(active_bytes == IDLE_BYTES);
    assert(peak_bytes == IDLE_BYTES + FRAME_BYTES);
    assert(present_calls == 43);
    return 0;
}
