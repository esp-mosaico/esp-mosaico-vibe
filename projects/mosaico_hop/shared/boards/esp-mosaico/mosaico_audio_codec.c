// SPDX-License-Identifier: Apache-2.0
/* ESP-Mosaico ES8311 DAC via the official ESP Board Manager device. */
#include "platform_esp_audio.h"

#include "esp_board_manager_includes.h"
#include "esp_codec_dev.h"
#include "esp_log.h"

#define MOSAICO_AUDIO_TAG "mosaico_audio"
#define MOSAICO_DAC_DEVICE "audio_dac"

static dev_audio_codec_handles_t *s_dac;
static bool s_dac_initialized;
static bool s_codec_open;
static bool s_nonzero_pcm_reported;

static raylib_lite_result_t release_dac(void)
{
    if (s_codec_open) {
        if (!s_dac || !s_dac->codec_dev ||
                esp_codec_dev_close(s_dac->codec_dev) != ESP_CODEC_DEV_OK) {
            ESP_LOGE(MOSAICO_AUDIO_TAG, "ES8311 close failed; retaining DAC handle");
            return RAYLIB_LITE_PLATFORM_ERROR;
        }
        s_codec_open = false;
    }
    /* The Board Manager device may free its codec handle during deinit.
     * Never dereference that handle on a subsequent failed-deinit retry. */
    s_dac = NULL;
    if (s_dac_initialized) {
        esp_err_t err = esp_board_manager_deinit_device_by_name(MOSAICO_DAC_DEVICE);
        if (err != ESP_OK) {
            ESP_LOGE(MOSAICO_AUDIO_TAG, "DAC deinit failed: %s", esp_err_to_name(err));
            return RAYLIB_LITE_PLATFORM_ERROR;
        }
        s_dac_initialized = false;
    }
    return RAYLIB_LITE_OK;
}

static raylib_lite_result_t codec_start(
    void *context, const raylib_lite_audio_format_t *format)
{
    (void)context;
    if (!format || format->sample_rate != RAYLIB_LITE_AUDIO_SAMPLE_RATE ||
            format->channels != RAYLIB_LITE_AUDIO_CHANNELS ||
            format->format != RAYLIB_LITE_PCM_S16_NATIVE)
        return RAYLIB_LITE_NOT_SUPPORTED;
    if (s_dac || s_dac_initialized) return RAYLIB_LITE_INVALID_STATE;

    /* The board profile keeps audio_dac init_skip=true so games without sound
     * do not require a codec. Initialize only on InitAudioDevice(). */
    esp_err_t err = esp_board_manager_init_device_by_name(MOSAICO_DAC_DEVICE);
    if (err != ESP_OK) {
        ESP_LOGE(MOSAICO_AUDIO_TAG, "DAC init failed: %s", esp_err_to_name(err));
        return RAYLIB_LITE_NOT_READY;
    }
    s_dac_initialized = true;
    err = esp_board_manager_get_device_handle(MOSAICO_DAC_DEVICE, (void **)&s_dac);
    if (err != ESP_OK || !s_dac || !s_dac->codec_dev) {
        ESP_LOGE(MOSAICO_AUDIO_TAG, "DAC handle unavailable");
        /* A failed start is finalized by backend.stop() after Worker join. */
        return RAYLIB_LITE_NOT_READY;
    }

    esp_codec_dev_sample_info_t info = {
        .sample_rate = RAYLIB_LITE_AUDIO_SAMPLE_RATE,
        .bits_per_sample = 16,
        .channel = 1,
        .channel_mask = 0,
    };
    /* Conservatively close a partially opened codec on startup failure. */
    s_codec_open = true;
    if (esp_codec_dev_open(s_dac->codec_dev, &info) != ESP_CODEC_DEV_OK) {
        ESP_LOGE(MOSAICO_AUDIO_TAG, "ES8311 open failed");
        return RAYLIB_LITE_NOT_READY;
    }
    if (esp_codec_dev_set_out_vol(s_dac->codec_dev, 72) != ESP_CODEC_DEV_OK) {
        ESP_LOGE(MOSAICO_AUDIO_TAG, "ES8311 output volume failed");
        return RAYLIB_LITE_PLATFORM_ERROR;
    }
    s_nonzero_pcm_reported = false;
    ESP_LOGI(MOSAICO_AUDIO_TAG, "ES8311 playback started: 24 kHz, mono, S16");
    return RAYLIB_LITE_OK;
}

static raylib_lite_result_t codec_write(
    void *context, const int16_t *frames, size_t frame_count,
    uint32_t timeout_ms, size_t *out_written)
{
    (void)context;
    (void)timeout_ms;
    if (!out_written || (!frames && frame_count)) return RAYLIB_LITE_INVALID_ARGUMENT;
    *out_written = 0;
    if (!s_dac || !s_dac->codec_dev) return RAYLIB_LITE_NOT_READY;
    if (esp_codec_dev_write(s_dac->codec_dev, (void *)frames,
                            frame_count * sizeof(*frames)) != ESP_CODEC_DEV_OK)
        return RAYLIB_LITE_IO_ERROR;
    *out_written = frame_count;
    /* One-shot diagnostic evidence of actual non-silent PCM writes. */
    if (!s_nonzero_pcm_reported) {
        for (size_t i = 0; i < frame_count; ++i) {
            if (frames[i] != 0) {
                s_nonzero_pcm_reported = true;
                ESP_LOGI(MOSAICO_AUDIO_TAG, "Non-silent PCM submitted to ES8311");
                break;
            }
        }
    }
    return RAYLIB_LITE_OK;
}

static raylib_lite_result_t codec_stop(void *context, uint32_t timeout_ms)
{
    (void)context;
    (void)timeout_ms;
    return release_dac();
}

raylib_lite_audio_backend_t platform_esp_audio_board_backend(void)
{
    return (raylib_lite_audio_backend_t) {
        .context = NULL,
        .start = codec_start,
        .write = codec_write,
        .stop = codec_stop,
    };
}
