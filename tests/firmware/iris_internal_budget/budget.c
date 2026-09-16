// SPDX-License-Identifier: Apache-2.0
/* Test-only hooks: no allocations, logging or task snapshots in a heap hook.
 * The record table is diagnostic overhead, excluded explicitly by the map
 * report. Actual allocation addresses, not requested caps, select DRAM. */
#include <string.h>
#include <inttypes.h>

#include "esp_attr.h"
#include "esp_heap_caps.h"
#include "esp_iris.h"
#include "esp_log.h"
#include "esp_memory_utils.h"
#include "esp_ota_ops.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define BUDGET_SERVICE 0x6A03U
#define RECORDS 1024U
#define SCOPES 8U

typedef struct { void *ptr; uint32_t bytes; } record_t;
typedef struct { TaskHandle_t task; uint32_t depth; } scope_t;
static record_t s_records[RECORDS];
static scope_t s_scopes[SCOPES];
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static bool s_ready;
static uint32_t s_current, s_peak, s_live, s_allocations, s_errors;
static esp_timer_handle_t s_exit_timer;

static void exit_snapshot(void *arg)
{
    (void)arg;
    uint32_t current, peak, errors;
    taskENTER_CRITICAL(&s_lock);
    current = s_current;
    peak = s_peak;
    errors = s_errors;
    taskEXIT_CRITICAL(&s_lock);
    ESP_LOGI("iris_budget", "EXIT current=%" PRIu32 " peak=%" PRIu32 " errors=%" PRIu32,
             current, peak, errors);
}

esp_err_t __real_esp_ota_set_boot_partition(const esp_partition_t *partition);
esp_err_t __wrap_esp_ota_set_boot_partition(const esp_partition_t *partition)
{
    const esp_err_t err = __real_esp_ota_set_boot_partition(partition);
    if (err == ESP_OK && partition->subtype == ESP_PARTITION_SUBTYPE_APP_FACTORY) {
        /* Existing timer task, test-only timer allocated before attribution:
         * sample after the normal enter-recovery task is created, before reset. */
        if (esp_timer_start_once(s_exit_timer, 250000) != ESP_OK) {
            taskENTER_CRITICAL(&s_lock);
            ++s_errors;
            taskEXIT_CRITICAL(&s_lock);
        }
    }
    return err;
}

static void scope_enter(void)
{
    const TaskHandle_t task = xTaskGetCurrentTaskHandle();
    taskENTER_CRITICAL(&s_lock);
    for (size_t i = 0; i < SCOPES; ++i) {
        if (s_scopes[i].task == task || s_scopes[i].depth == 0) {
            s_scopes[i].task = task;
            ++s_scopes[i].depth;
            taskEXIT_CRITICAL(&s_lock);
            return;
        }
    }
    ++s_errors;
    taskEXIT_CRITICAL(&s_lock);
}

static void scope_exit(void)
{
    const TaskHandle_t task = xTaskGetCurrentTaskHandle();
    taskENTER_CRITICAL(&s_lock);
    for (size_t i = 0; i < SCOPES; ++i) {
        if (s_scopes[i].task == task && s_scopes[i].depth != 0) {
            --s_scopes[i].depth;
            taskEXIT_CRITICAL(&s_lock);
            return;
        }
    }
    ++s_errors;
    taskEXIT_CRITICAL(&s_lock);
}

void esp_heap_trace_alloc_hook(void *ptr, size_t size, uint32_t caps)
{
    (void)size;
    (void)caps;
    if (!s_ready || ptr == NULL || !esp_ptr_internal(ptr)) return;
    const bool isr = xPortInIsrContext();
    const TaskHandle_t task = isr ? NULL : xTaskGetCurrentTaskHandle();
    const char *name = task == NULL ? "" : pcTaskGetName(task);
    /* Count ISR allocations conservatively. tcpip allocations count too if
     * the optional product network is later enabled during this test. */
    bool owned = isr || strcmp(name, "esp_iris") == 0 ||
        strcmp(name, "iris-service") == 0 || strcmp(name, "TinyUSB") == 0 ||
        strcmp(name, "tcpip_task") == 0;
    /* Include allocator rounding plus a conservative 16 B per-block charge
     * for TLSF/block-owner metadata. This fixture disables heap poisoning. */
    const uint32_t bytes = heap_caps_get_allocated_size(ptr) + 16U;
    taskENTER_CRITICAL(&s_lock);
    for (size_t i = 0; i < SCOPES; ++i) {
        owned |= s_scopes[i].task == task && s_scopes[i].depth != 0;
    }
    size_t vacant = RECORDS;
    for (size_t i = 0; i < RECORDS; ++i) {
        if (s_records[i].ptr == ptr) {
            /* In-place realloc emits alloc, not free. Refresh its charge.
             * A relocating realloc without a free hook overcounts the old
             * block, conservatively, rather than hiding a peak. */
            s_current -= s_records[i].bytes;
            s_records[i].bytes = bytes;
            s_current += bytes;
            if (s_current > s_peak) s_peak = s_current;
            taskEXIT_CRITICAL(&s_lock);
            return;
        }
        if (s_records[i].ptr == NULL && vacant == RECORDS) vacant = i;
    }
    if (owned) {
        if (vacant == RECORDS) {
            ++s_errors;
        } else {
            s_records[vacant] = (record_t){ptr, bytes};
            s_current += bytes;
            ++s_live;
            ++s_allocations;
            if (s_current > s_peak) s_peak = s_current;
        }
    }
    taskEXIT_CRITICAL(&s_lock);
}

void esp_heap_trace_free_hook(void *ptr)
{
    if (!s_ready || ptr == NULL) return;
    taskENTER_CRITICAL(&s_lock);
    for (size_t i = 0; i < RECORDS; ++i) {
        if (s_records[i].ptr == ptr) {
            s_current -= s_records[i].bytes;
            s_records[i] = (record_t){0};
            --s_live;
            break;
        }
    }
    taskEXIT_CRITICAL(&s_lock);
}

#define WRAP_NO_ARGS(function) \
    esp_err_t __real_##function(void); \
    esp_err_t __wrap_##function(void) { \
        scope_enter(); \
        const esp_err_t err = __real_##function(); \
        scope_exit(); \
        return err; \
    }
WRAP_NO_ARGS(esp_iris_start)
WRAP_NO_ARGS(esp_iris_boot_probe)
WRAP_NO_ARGS(esp_iris_mark_healthy)

void __real_iris_ota_support_start(void);
void __wrap_iris_ota_support_start(void)
{
    scope_enter();
    __real_iris_ota_support_start();
    scope_exit();
}

esp_err_t __real_esp_iris_rpc_register(uint16_t, uint16_t,
                                     esp_iris_rpc_handler_t, void *);
esp_err_t __wrap_esp_iris_rpc_register(uint16_t service, uint16_t method,
                                      esp_iris_rpc_handler_t handler, void *ctx)
{
    scope_enter();
    const esp_err_t err = __real_esp_iris_rpc_register(service, method, handler, ctx);
    scope_exit();
    return err;
}
esp_err_t __real_esp_iris_screen_register(const esp_iris_screen_backend_t *);
esp_err_t __wrap_esp_iris_screen_register(const esp_iris_screen_backend_t *backend)
{
    scope_enter();
    const esp_err_t err = __real_esp_iris_screen_register(backend);
    scope_exit();
    return err;
}

static esp_err_t echo_rpc(const esp_iris_rpc_request_t *request,
                          uint8_t *out, size_t capacity, size_t *length, void *ctx)
{
    (void)ctx;
    *length = 0;
    if (request->payload_size > capacity) return ESP_ERR_INVALID_SIZE;
    memcpy(out, request->payload, request->payload_size);
    *length = request->payload_size;
    return ESP_OK;
}

static void put_u32(uint8_t *out, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i) out[i] = value >> (8U * i);
}
static esp_err_t budget_rpc(const esp_iris_rpc_request_t *request,
                            uint8_t *out, size_t capacity, size_t *length, void *ctx)
{
    (void)ctx;
    *length = 0;
    if (request->payload_size != 0 || capacity < 28) return ESP_ERR_INVALID_SIZE;
    const uint32_t stack_free = uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t);
    const TaskHandle_t usb = xTaskGetHandle("TinyUSB");
    const uint32_t usb_free = usb == NULL ? 0 :
        uxTaskGetStackHighWaterMark(usb) * sizeof(StackType_t);
    taskENTER_CRITICAL(&s_lock);
    put_u32(out, s_current);
    put_u32(out + 4, s_peak);
    put_u32(out + 8, s_live);
    put_u32(out + 12, s_allocations);
    put_u32(out + 16, s_errors);
    put_u32(out + 20, stack_free);
    put_u32(out + 24, usb_free);
    taskEXIT_CRITICAL(&s_lock);
    *length = 28;
    return ESP_OK;
}

void __real_app_main(void);
void __wrap_app_main(void)
{
    const esp_timer_create_args_t timer = {
        .callback = exit_snapshot, .name = "iris-budget-exit",
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer, &s_exit_timer));
    s_ready = true;
    ESP_ERROR_CHECK(esp_iris_rpc_register(BUDGET_SERVICE, 1, echo_rpc, NULL));
    ESP_ERROR_CHECK(esp_iris_rpc_register(BUDGET_SERVICE, 2, budget_rpc, NULL));
    __real_app_main();
    ESP_LOGI("iris_budget", "READY echo=0x6a03/1 metrics=0x6a03/2; heap peak needs static map sum");
}
