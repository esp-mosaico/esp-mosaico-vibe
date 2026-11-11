// SPDX-License-Identifier: Apache-2.0
#include "app_imu.h"

#include <math.h>
#include <string.h>

#include "bsp/esp_mosaico.h"
#include "bsp/imu.h"
#include "cJSON.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "minihub_core.h"

static const char *TAG = "app_imu";

#define APP_IMU_SAMPLE_MS 20
#define APP_IMU_STARTUP_MS 80
#define APP_IMU_REST_SAMPLES 40
#define APP_IMU_SHAKE_GYRO_DPS 140.0f
#define APP_IMU_SHAKE_JERK_G 0.45f
#define APP_IMU_SHAKE_HITS 2
#define APP_IMU_FLIP_DOT (-0.35f)
#define APP_IMU_TILT_DOT 0.75f
#define APP_IMU_POSE_HITS 8
#define APP_IMU_RELOCK_HITS 75
#define APP_IMU_STILL_GYRO_DPS 35.0f
#define APP_IMU_STILL_DOT 0.92f
#define APP_IMU_COOL_SHAKE_MS 350
#define APP_IMU_COOL_POSE_MS 500
#define APP_IMU_TASK_STACK 4096
#define APP_IMU_TASK_PRIORITY 3

typedef enum {
    APP_IMU_GESTURE_NONE = 0,
    APP_IMU_GESTURE_SHAKE,
    APP_IMU_GESTURE_TILT,
    APP_IMU_GESTURE_FLIP,
} app_imu_gesture_t;

typedef struct {
    float rest_ax;
    float rest_ay;
    float rest_az;
    float rest_mag;
    uint8_t rest_n;
    bool rest_ready;
    float ema_mag;
    uint8_t shake_hits;
    uint8_t pose_hits;
    uint8_t relock_hits;
    app_imu_gesture_t pose_kind;
    app_imu_gesture_t last;
    TickType_t cool_until;
    bool published_idle;
} app_imu_ctx_t;

static app_imu_ctx_t s_ctx;
static bool s_started;

static float vec_mag(float x, float y, float z)
{
    return sqrtf(x * x + y * y + z * z);
}

static const char *gesture_value(app_imu_gesture_t gesture)
{
    switch (gesture) {
    case APP_IMU_GESTURE_SHAKE:
        return "shake";
    case APP_IMU_GESTURE_TILT:
        return "tilt";
    case APP_IMU_GESTURE_FLIP:
        return "flip";
    default:
        return "still";
    }
}

static void rest_accumulate(float ax, float ay, float az, float mag)
{
    if (s_ctx.rest_ready || mag < 0.70f || mag > 1.30f) {
        return;
    }
    s_ctx.rest_ax += ax;
    s_ctx.rest_ay += ay;
    s_ctx.rest_az += az;
    ++s_ctx.rest_n;
    if (s_ctx.rest_n < APP_IMU_REST_SAMPLES) {
        return;
    }
    const float inv = 1.0f / (float)s_ctx.rest_n;
    s_ctx.rest_ax *= inv;
    s_ctx.rest_ay *= inv;
    s_ctx.rest_az *= inv;
    s_ctx.rest_mag = vec_mag(s_ctx.rest_ax, s_ctx.rest_ay, s_ctx.rest_az);
    if (s_ctx.rest_mag < 0.50f) {
        s_ctx.rest_ax = 0.0f;
        s_ctx.rest_ay = 0.0f;
        s_ctx.rest_az = 1.0f;
        s_ctx.rest_mag = 1.0f;
    }
    s_ctx.rest_ready = true;
    ESP_LOGI(TAG, "rest gravity[g]=%+.2f %+.2f %+.2f",
             s_ctx.rest_ax, s_ctx.rest_ay, s_ctx.rest_az);
}

static float gravity_dot(float ax, float ay, float az, float mag)
{
    if (!s_ctx.rest_ready || mag < 0.20f || s_ctx.rest_mag < 0.20f) {
        return 1.0f;
    }
    return (ax * s_ctx.rest_ax + ay * s_ctx.rest_ay + az * s_ctx.rest_az) /
           (mag * s_ctx.rest_mag);
}

static app_imu_gesture_t classify(float ax, float ay, float az,
                                  float gx, float gy, float gz,
                                  float mag, float gyro)
{
    const float jerk = fabsf(mag - s_ctx.ema_mag);
    s_ctx.ema_mag = 0.80f * s_ctx.ema_mag + 0.20f * mag;
    if (gyro >= APP_IMU_SHAKE_GYRO_DPS || jerk >= APP_IMU_SHAKE_JERK_G) {
        if (s_ctx.shake_hits < 255) {
            ++s_ctx.shake_hits;
        }
        if (s_ctx.shake_hits >= APP_IMU_SHAKE_HITS) {
            s_ctx.shake_hits = 0;
            s_ctx.pose_hits = 0;
            s_ctx.pose_kind = APP_IMU_GESTURE_NONE;
            return APP_IMU_GESTURE_SHAKE;
        }
    } else if (s_ctx.shake_hits > 0) {
        --s_ctx.shake_hits;
    }

    if (!s_ctx.rest_ready || gyro > 80.0f || fabsf(mag - 1.0f) > 0.35f) {
        s_ctx.pose_hits = 0;
        s_ctx.pose_kind = APP_IMU_GESTURE_NONE;
        return APP_IMU_GESTURE_NONE;
    }

    const float d = gravity_dot(ax, ay, az, mag);
    app_imu_gesture_t pose = APP_IMU_GESTURE_NONE;
    if (d <= APP_IMU_FLIP_DOT) {
        pose = APP_IMU_GESTURE_FLIP;
    } else if (d < APP_IMU_TILT_DOT) {
        pose = APP_IMU_GESTURE_TILT;
    }
    if (pose == APP_IMU_GESTURE_NONE) {
        s_ctx.pose_hits = 0;
        s_ctx.pose_kind = APP_IMU_GESTURE_NONE;
        return APP_IMU_GESTURE_NONE;
    }
    if (pose != s_ctx.pose_kind) {
        s_ctx.pose_kind = pose;
        s_ctx.pose_hits = 1;
        return APP_IMU_GESTURE_NONE;
    }
    if (s_ctx.pose_hits < 255) {
        ++s_ctx.pose_hits;
    }
    return s_ctx.pose_hits >= APP_IMU_POSE_HITS ? pose : APP_IMU_GESTURE_NONE;
}

static cJSON *make_param(const char *value)
{
    cJSON *entry = cJSON_CreateObject();
    if (!entry) {
        return NULL;
    }
    if (!cJSON_AddStringToObject(entry, "value", value) ||
            !cJSON_AddStringToObject(entry, "access", "R")) {
        cJSON_Delete(entry);
        return NULL;
    }
    return entry;
}

static void publish_gesture(app_imu_gesture_t gesture)
{
    const char *name = gesture_value(gesture);
    const bool moving = gesture != APP_IMU_GESTURE_NONE;
    cJSON *params = cJSON_CreateObject();
    cJSON *motion = make_param(moving ? "on" : "off");
    cJSON *g = make_param(name);
    if (!params || !motion || !g) {
        cJSON_Delete(params);
        cJSON_Delete(motion);
        cJSON_Delete(g);
        return;
    }
    cJSON_AddItemToObject(params, "motion", motion);
    cJSON_AddItemToObject(params, "gesture", g);
    const esp_err_t err =
        minihub_point_merge_params(APP_IMU_POINT_ID, name, params);
    cJSON_Delete(params);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "publish %s failed: %s", name, esp_err_to_name(err));
    }
}

static esp_err_t register_point(void)
{
    minihub_point_t point = {0};
    strlcpy(point.point_id, APP_IMU_POINT_ID, sizeof(point.point_id));
    strlcpy(point.type, "sensor", sizeof(point.type));
    strlcpy(point.adapter, "mosaico", sizeof(point.adapter));
    strlcpy(point.value, "still", sizeof(point.value));
    point.online = true;
    point.props = cJSON_CreateObject();
    if (!point.props) {
        return ESP_ERR_NO_MEM;
    }
    if (!cJSON_AddStringToObject(point.props, "name", "体感") ||
            !cJSON_AddTrueToObject(point.props, "no_control")) {
        cJSON_Delete(point.props);
        return ESP_ERR_NO_MEM;
    }
    cJSON *params = cJSON_CreateObject();
    cJSON *motion = make_param("off");
    cJSON *gesture = make_param("still");
    if (!params || !motion || !gesture) {
        cJSON_Delete(point.props);
        cJSON_Delete(params);
        cJSON_Delete(motion);
        cJSON_Delete(gesture);
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddItemToObject(params, "motion", motion);
    cJSON_AddItemToObject(params, "gesture", gesture);
    cJSON_AddItemToObject(point.props, "params", params);
    const esp_err_t err = minihub_point_register(&point);
    cJSON_Delete(point.props);
    return err;
}

static void imu_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(APP_IMU_STARTUP_MS));
    s_ctx.ema_mag = 1.0f;
    s_ctx.last = APP_IMU_GESTURE_NONE;
    s_ctx.published_idle = true;
    TickType_t last_wake = xTaskGetTickCount();

    while (true) {
        float ax = 0.0f;
        float ay = 0.0f;
        float az = 0.0f;
        float gx = 0.0f;
        float gy = 0.0f;
        float gz = 0.0f;
        if (bsp_imu_get_accel(&ax, &ay, &az) != ESP_OK) {
            xTaskDelayUntil(&last_wake, pdMS_TO_TICKS(APP_IMU_SAMPLE_MS));
            continue;
        }
        (void)bsp_imu_get_gyro(&gx, &gy, &gz);
        const float mag = vec_mag(ax, ay, az);
        const float gyro = vec_mag(gx, gy, gz);
        rest_accumulate(ax, ay, az, mag);
        const app_imu_gesture_t gesture =
            classify(ax, ay, az, gx, gy, gz, mag, gyro);
        const TickType_t now = xTaskGetTickCount();
        const bool quasi_static =
            gyro < APP_IMU_STILL_GYRO_DPS && fabsf(mag - 1.0f) < 0.15f;
        if (quasi_static) {
            if (s_ctx.relock_hits < 255) {
                ++s_ctx.relock_hits;
            }
        } else {
            s_ctx.relock_hits = 0;
        }
        const float d = gravity_dot(ax, ay, az, mag);
        bool relocked = false;
        if (s_ctx.rest_ready && quasi_static && d <= APP_IMU_STILL_DOT &&
                s_ctx.relock_hits >= APP_IMU_RELOCK_HITS) {
            s_ctx.rest_ax = ax;
            s_ctx.rest_ay = ay;
            s_ctx.rest_az = az;
            s_ctx.rest_mag = mag > 0.50f ? mag : 1.0f;
            s_ctx.last = APP_IMU_GESTURE_NONE;
            s_ctx.pose_hits = 0;
            s_ctx.pose_kind = APP_IMU_GESTURE_NONE;
            s_ctx.relock_hits = 0;
            relocked = true;
            if (!s_ctx.published_idle) {
                publish_gesture(APP_IMU_GESTURE_NONE);
                s_ctx.published_idle = true;
            }
            ESP_LOGI(TAG, "rest relock[g]=%+.2f %+.2f %+.2f", ax, ay, az);
        }
        const bool still = s_ctx.rest_ready && quasi_static && d > APP_IMU_STILL_DOT;

        if (relocked) {
            /* New viewing pose is rest; do not keep the old tilt/flip. */
        } else if (gesture != APP_IMU_GESTURE_NONE) {
            const uint32_t cool_ms = gesture == APP_IMU_GESTURE_SHAKE ?
                                     APP_IMU_COOL_SHAKE_MS : APP_IMU_COOL_POSE_MS;
            if (gesture != s_ctx.last && now >= s_ctx.cool_until) {
                publish_gesture(gesture);
                s_ctx.cool_until = now + pdMS_TO_TICKS(cool_ms);
                s_ctx.published_idle = false;
                ESP_LOGI(TAG, "gesture=%s accel[g]=%+.2f %+.2f %+.2f gyro=%.0f",
                         gesture_value(gesture), ax, ay, az, gyro);
            }
            s_ctx.last = gesture;
        } else if (still) {
            s_ctx.last = APP_IMU_GESTURE_NONE;
            if (!s_ctx.published_idle && now >= s_ctx.cool_until) {
                publish_gesture(APP_IMU_GESTURE_NONE);
                s_ctx.published_idle = true;
            }
        }
        xTaskDelayUntil(&last_wake, pdMS_TO_TICKS(APP_IMU_SAMPLE_MS));
    }
}

esp_err_t app_imu_start(void)
{
    if (s_started) {
        return ESP_OK;
    }

    const bsp_imu_config_t config = BSP_IMU_CONFIG_DEFAULT();
    esp_err_t err = bsp_imu_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "BMI270 init failed: %s", esp_err_to_name(err));
        return err;
    }
    err = bsp_imu_start(&config);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "BMI270 start failed: %s", esp_err_to_name(err));
        (void)bsp_imu_deinit();
        return err;
    }
    err = register_point();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "point register failed: %s", esp_err_to_name(err));
        (void)bsp_imu_deinit();
        return err;
    }
    BaseType_t created = xTaskCreate(imu_task, "app_imu", APP_IMU_TASK_STACK,
                                     NULL, APP_IMU_TASK_PRIORITY, NULL);
    if (created != pdPASS) {
        (void)minihub_point_unregister(APP_IMU_POINT_ID);
        (void)bsp_imu_deinit();
        return ESP_ERR_NO_MEM;
    }
    s_started = true;
    ESP_LOGI(TAG, "onboard IMU published as %s", APP_IMU_POINT_ID);
    return ESP_OK;
}
