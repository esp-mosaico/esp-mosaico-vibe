// SPDX-License-Identifier: Apache-2.0
#include "raylib_lite_game_audio.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "raylib_lite_assets.h"
#include "platform_esp_audio.h"
#include "raylib_lite_audio_mixer.h"
#include "sdkconfig.h"

static raylib_lite_audio_mixer_t *s_mixer;
static platform_esp_audio_service_t *s_service;
static float s_master_volume = 1.0f;

#define AUDIO_STOP_TIMEOUT_MS 100U

typedef enum { AUDIO_TICKET_SOUND = 1, AUDIO_TICKET_MUSIC = 2 } ticket_kind_t;
typedef struct audio_ticket {
    struct audio_ticket *next;
    raylib_lite_audio_mixer_t *owner;
    raylib_lite_audio_clip_t clip;
    ticket_kind_t kind;
    bool active;
} audio_ticket_t;

static audio_ticket_t *s_tickets;

static raylib_lite_result_t asset_open(
    void *context, const char *path, raylib_lite_audio_asset_t *out_asset)
{
    (void)context;
    if (!out_asset) return RAYLIB_LITE_INVALID_ARGUMENT;
    raylib_lite_asset_view_t *view = calloc(1, sizeof(*view));
    if (!view) return RAYLIB_LITE_NO_MEMORY;
    if (raylib_lite_asset_open(path, view) != RAYLIB_LITE_OK) {
        free(view);
        return RAYLIB_LITE_IO_ERROR;
    }
    *out_asset = (raylib_lite_audio_asset_t) {
        .data = view->data,
        .size = view->size,
        .lease = view,
    };
    return RAYLIB_LITE_OK;
}

static void asset_close(void *context, raylib_lite_audio_asset_t *asset)
{
    (void)context;
    if (!asset || !asset->lease) return;
    raylib_lite_asset_view_t *view = asset->lease;
    raylib_lite_asset_release(view);
    free(view);
    memset(asset, 0, sizeof(*asset));
}

static raylib_lite_result_t pull_audio(
    void *context, int16_t *out_frames, size_t frame_count)
{
    return raylib_lite_audio_mixer_mix(context, out_frames, frame_count);
}

static audio_ticket_t *ticket_create(
    raylib_lite_audio_clip_t clip, ticket_kind_t kind)
{
    audio_ticket_t *ticket = calloc(1, sizeof(*ticket));
    if (!ticket) return NULL;
    ticket->owner = s_mixer;
    ticket->clip = clip;
    ticket->kind = kind;
    ticket->active = true;
    ticket->next = s_tickets;
    s_tickets = ticket;
    return ticket;
}

/* Never dereference an untrusted/stale Raylib pointer before finding the same
 * address in our retained ticket registry. Retired tickets are reclaimed only
 * by CloseAudioDevice because Sound/Music values may have been copied. */
static audio_ticket_t *ticket_find(const void *handle, ticket_kind_t kind)
{
    for (audio_ticket_t *ticket = s_tickets; ticket; ticket = ticket->next) {
        if ((const void *)ticket == handle) {
            return ticket->active && ticket->owner == s_mixer &&
                   ticket->kind == kind ? ticket : NULL;
        }
    }
    return NULL;
}

static audio_ticket_t *sound_ticket(Sound sound)
{
    return ticket_find(sound.stream.buffer, AUDIO_TICKET_SOUND);
}

static audio_ticket_t *music_ticket(Music music)
{
    return ticket_find(music.ctxData, AUDIO_TICKET_MUSIC);
}

static void tickets_destroy_all(void)
{
    while (s_tickets) {
        audio_ticket_t *ticket = s_tickets;
        s_tickets = ticket->next;
        free(ticket);
    }
}

raylib_lite_result_t raylib_lite_game_audio_shutdown(uint32_t timeout_ms)
{
    if (!s_service) return RAYLIB_LITE_OK;
    raylib_lite_result_t result = platform_esp_audio_service_stop(s_service, timeout_ms);
    if (result != RAYLIB_LITE_OK) return result;

    /* The Worker is joined. Release the Mixer (which still uses the service
     * synchronization callbacks) before destroying that service. */
    raylib_lite_audio_mixer_destroy(s_mixer);
    s_mixer = NULL;
    tickets_destroy_all();
    result = platform_esp_audio_service_destroy(s_service, 0);
    if (result != RAYLIB_LITE_OK) return result;
    s_service = NULL;
    return RAYLIB_LITE_OK;
}

void raylib_lite_game_audio_init(void)
{
    if (s_service) return;
    if (platform_esp_audio_service_create(&s_service) != RAYLIB_LITE_OK) return;
    raylib_lite_audio_mixer_config_t config = {
        .max_clips = CONFIG_ESP_MOSAICO_AUDIO_CLIPS,
        .max_sfx_voices = CONFIG_ESP_MOSAICO_AUDIO_VOICES,
        .assets = {.open = asset_open, .close = asset_close},
        .sync = {
            .context = s_service,
            .lock = platform_esp_audio_service_lock,
            .unlock = platform_esp_audio_service_unlock,
        },
    };
    if (raylib_lite_audio_mixer_create(&config, &s_mixer) != RAYLIB_LITE_OK) {
        (void)raylib_lite_game_audio_shutdown(0);
        return;
    }
    if (platform_esp_audio_service_start(s_service, pull_audio, s_mixer) !=
            RAYLIB_LITE_OK) {
        /* A codec start that timed out may still be executing in the Worker.
         * Never free its pull context until a successful stop/join; a failed
         * bounded stop retains everything for the Native cleanup retry. */
        (void)raylib_lite_game_audio_shutdown(AUDIO_STOP_TIMEOUT_MS);
        return;
    }
    /* Keep Raylib Music gain per handle. The mixer bus remains at unity. */
    raylib_lite_audio_mixer_set_music_volume(s_mixer, 1.0f);
    raylib_lite_audio_mixer_set_master_volume(s_mixer, s_master_volume);
}

void raylib_lite_game_audio_close(void)
{
    (void)raylib_lite_game_audio_shutdown(AUDIO_STOP_TIMEOUT_MS);
}

bool raylib_lite_game_audio_ready(void)
{
    return platform_esp_audio_service_ready(s_service);
}

Sound raylib_lite_game_audio_load_sound(const char *path)
{
    raylib_lite_audio_clip_t clip = RAYLIB_LITE_AUDIO_CLIP_INVALID;
    if (!s_mixer || raylib_lite_audio_mixer_load(s_mixer, path, &clip) !=
                        RAYLIB_LITE_OK) return (Sound){0};
    audio_ticket_t *ticket = ticket_create(clip, AUDIO_TICKET_SOUND);
    if (!ticket) {
        (void)raylib_lite_audio_mixer_unload(s_mixer, clip);
        return (Sound){0};
    }
    return (Sound) {
        .stream = {
            .buffer = (rAudioBuffer *)ticket,
            .sampleRate = RAYLIB_LITE_MIXER_SAMPLE_RATE,
            .sampleSize = 16,
            .channels = RAYLIB_LITE_MIXER_CHANNELS,
        },
        .frameCount = raylib_lite_audio_mixer_clip_frames(s_mixer, clip),
    };
}

void raylib_lite_game_audio_unload_sound(Sound sound)
{
    audio_ticket_t *ticket = sound_ticket(sound);
    if (!ticket) return;
    (void)raylib_lite_audio_mixer_unload(s_mixer, ticket->clip);
    ticket->active = false;
}

void raylib_lite_game_audio_play_sound(Sound sound)
{
    audio_ticket_t *ticket = sound_ticket(sound);
    if (ticket) (void)raylib_lite_audio_mixer_play_sound(s_mixer, ticket->clip);
}

void raylib_lite_game_audio_stop_sound(Sound sound)
{
    audio_ticket_t *ticket = sound_ticket(sound);
    if (ticket) (void)raylib_lite_audio_mixer_stop_sound(s_mixer, ticket->clip);
}

bool raylib_lite_game_audio_is_sound_playing(Sound sound)
{
    audio_ticket_t *ticket = sound_ticket(sound);
    return ticket && raylib_lite_audio_mixer_is_sound_playing(
                         s_mixer, ticket->clip);
}

void raylib_lite_game_audio_set_sound_volume(Sound sound, float volume)
{
    audio_ticket_t *ticket = sound_ticket(sound);
    if (ticket) (void)raylib_lite_audio_mixer_set_sound_volume(
        s_mixer, ticket->clip, volume);
}

Music raylib_lite_game_audio_load_music(const char *path)
{
    raylib_lite_audio_clip_t clip = RAYLIB_LITE_AUDIO_CLIP_INVALID;
    if (!s_mixer || raylib_lite_audio_mixer_load(s_mixer, path, &clip) !=
                        RAYLIB_LITE_OK) return (Music){0};
    /* Match the previous default music loudness without a global voice gain. */
    if (raylib_lite_audio_mixer_set_music_clip_volume(s_mixer, clip, 0.28f) !=
            RAYLIB_LITE_OK) {
        (void)raylib_lite_audio_mixer_unload(s_mixer, clip);
        return (Music){0};
    }
    audio_ticket_t *ticket = ticket_create(clip, AUDIO_TICKET_MUSIC);
    if (!ticket) {
        (void)raylib_lite_audio_mixer_unload(s_mixer, clip);
        return (Music){0};
    }
    return (Music) {
        .stream = {
            .buffer = (rAudioBuffer *)ticket,
            .sampleRate = RAYLIB_LITE_MIXER_SAMPLE_RATE,
            .sampleSize = 16,
            .channels = RAYLIB_LITE_MIXER_CHANNELS,
        },
        .frameCount = raylib_lite_audio_mixer_clip_frames(s_mixer, clip),
        .looping = true,
        .ctxData = ticket,
    };
}

void raylib_lite_game_audio_unload_music(Music music)
{
    audio_ticket_t *ticket = music_ticket(music);
    if (!ticket) return;
    (void)raylib_lite_audio_mixer_unload(s_mixer, ticket->clip);
    ticket->active = false;
}

void raylib_lite_game_audio_play_music(Music music)
{
    audio_ticket_t *ticket = music_ticket(music);
    if (ticket) (void)raylib_lite_audio_mixer_play_music(s_mixer, ticket->clip);
}

void raylib_lite_game_audio_update_music(Music music)
{
    (void)music;
}

void raylib_lite_game_audio_stop_music(Music music)
{
    audio_ticket_t *ticket = music_ticket(music);
    if (ticket) (void)raylib_lite_audio_mixer_stop_music_clip(
        s_mixer, ticket->clip);
}

void raylib_lite_game_audio_set_music_volume(Music music, float volume)
{
    audio_ticket_t *ticket = music_ticket(music);
    if (ticket) (void)raylib_lite_audio_mixer_set_music_clip_volume(
        s_mixer, ticket->clip, volume);
}

void raylib_lite_game_audio_set_master_volume(float volume)
{
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    s_master_volume = volume;
    if (s_mixer) raylib_lite_audio_mixer_set_master_volume(s_mixer, volume);
}

void raylib_lite_game_audio_get_stats(raylib_lite_game_audio_stats_t *stats)
{
    if (!stats) return;
    memset(stats, 0, sizeof(*stats));
    if (!s_mixer) return;
    raylib_lite_audio_mixer_stats_t mixer_stats;
    platform_esp_audio_stats_t platform_stats = {0};
    raylib_lite_audio_mixer_get_stats(s_mixer, &mixer_stats);
    platform_esp_audio_service_get_stats(s_service, &platform_stats);
    stats->mixed_chunks = (uint32_t)(mixer_stats.mixed_frames / 240U);
    stats->write_errors = platform_stats.write_errors;
    stats->underruns = platform_stats.underruns;
    stats->voice_steals = mixer_stats.voice_steals;
    stats->active_sfx_voices = (uint8_t)mixer_stats.active_sfx_voices;
    stats->music_playing = mixer_stats.music_playing;
}
