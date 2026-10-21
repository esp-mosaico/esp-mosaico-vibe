// SPDX-License-Identifier: Apache-2.0

#include "chess_peer_link.h"

#ifndef ESP_PLATFORM

#include <string.h>

bool chess_peer_link_init(void) { return true; }
void chess_peer_link_deinit(void) {}
bool chess_peer_link_begin_pairing(void) { return true; }
void chess_peer_link_leave(void) {}
bool chess_peer_link_set_ready(void) { return true; }
bool chess_peer_link_send_move(const chess_move_t *move,
                               uint64_t before_hash,
                               uint64_t after_hash)
{
    return move != NULL && before_hash != after_hash;
}
bool chess_peer_link_accept_remote_move(uint16_t ply,
                                        const chess_move_t *move,
                                        uint64_t after_hash)
{
    return ply > 0 && move != NULL && after_hash != 0;
}
bool chess_peer_link_set_position(uint16_t ply, uint64_t position_hash)
{
    (void)ply;
    (void)position_hash;
    return true;
}
bool chess_peer_link_send_resign(chess_color_t color)
{
    return color == CHESS_WHITE || color == CHESS_BLACK;
}
bool chess_peer_link_request_sync(uint16_t from_ply, uint64_t position_hash)
{
    (void)from_ply;
    (void)position_hash;
    return true;
}
bool chess_peer_link_complete_sync(uint16_t total_ply, uint64_t position_hash)
{
    (void)total_ply;
    (void)position_hash;
    return true;
}
bool chess_peer_link_poll_event(chess_peer_event_t *event)
{
    (void)event;
    return false;
}
bool chess_peer_link_get_status(chess_peer_status_t *status)
{
    if (status == NULL) {
        return false;
    }
    memset(status, 0, sizeof(*status));
    status->initialized = true;
    return true;
}

#else

#include <inttypes.h>
#include <string.h>

#include "bsp/esp_mosaico.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "mosaico_mag_classifier.h"
#include "mosaico_peer_link.h"
#include "mosaico_peer_session.h"

#define CHESS_NETWORK_ID 0x43485353U
#define CHESS_ESPNOW_CHANNEL 1U
#define CHESS_RX_DEPTH 32U
#define CHESS_EVENT_DEPTH 24U
#define CHESS_MOVE_LOG_MAX 1024U
#define CHESS_TASK_PERIOD_MS 50U
#define CHESS_MAG_SAMPLE_MS 50U
#define CHESS_PING_PERIOD_MS 500U
#define CHESS_DISCONNECT_MS 1800U
#define CHESS_RETRY_MS 250U

static const char *TAG = "chess_peer";

typedef struct {
    int8_t rssi;
    uint32_t received_ms;
    mosaico_peer_message_t message;
} rx_item_t;

typedef struct {
    volatile bool running;
    bool pairing;
    bool paired;
    bool local_ready;
    bool peer_ready;
    bool started;
    bool disconnected;
    bool pending_move;
    bool remote_move_pending;
    bool sync_receiving;
    uint64_t local_id;
    uint64_t peer_id;
    uint32_t local_boot_id;
    uint32_t peer_boot_id;
    uint32_t session_id;
    uint32_t tx_sequence;
    uint32_t last_receive_ms;
    uint32_t last_ping_ms;
    uint32_t last_retry_ms;
    uint32_t sync_last_ms;
    uint16_t ply;
    uint16_t remote_pending_ply;
    uint16_t sync_expected_ply;
    uint16_t sync_total_ply;
    chess_color_t local_color;
    mosaico_edge_t local_edge;
    mosaico_edge_t peer_edge;
    int8_t rssi;
    uint64_t position_hash;
    chess_peer_message_t pending_message;
    chess_move_t move_log[CHESS_MOVE_LOG_MAX];
    mosaico_edge_mask_t detected_edges;
    mosaico_mag_classifier_t classifier;
    mosaico_mag_presence_tracker_t presence_tracker;
    mosaico_peer_session_manager_t peer_sessions;
    QueueHandle_t rx_queue;
    QueueHandle_t event_queue;
    SemaphoreHandle_t lock;
    SemaphoreHandle_t session_lock;
    SemaphoreHandle_t send_lock;
    TaskHandle_t network_task;
    TaskHandle_t magnetic_task;
} chess_peer_context_t;

static chess_peer_context_t s_peer;

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static bool elapsed(uint32_t now, uint32_t then, uint32_t period)
{
    return (uint32_t)(now - then) >= period;
}

static void post_event(chess_peer_event_t *event)
{
    event->local_color = s_peer.local_color;
    event->peer_id = s_peer.peer_id;
    event->session_id = s_peer.session_id;
    if (xQueueSend(s_peer.event_queue, event, 0) != pdTRUE) {
        ESP_LOGW(TAG, "event queue full: kind=%u", (unsigned)event->kind);
    }
}

static bool send_message(const chess_peer_message_t *source)
{
    uint8_t wire[CHESS_PEER_WIRE_MAX];
    size_t wire_size = 0;
    chess_peer_message_t message = *source;
    xSemaphoreTake(s_peer.send_lock, portMAX_DELAY);
    message.sequence = ++s_peer.tx_sequence;
    if (message.sequence == 0) {
        message.sequence = ++s_peer.tx_sequence;
    }
    const bool encoded = chess_peer_protocol_encode(
        &message, wire, sizeof(wire), &wire_size);
    const esp_err_t result = encoded ?
        mosaico_peer_link_send_broadcast_to(
            MOSAICO_PEER_MSG_GAME_EVENT, s_peer.peer_id, s_peer.session_id,
            s_peer.local_edge, s_peer.peer_edge, 0, wire, wire_size) :
        ESP_ERR_INVALID_ARG;
    xSemaphoreGive(s_peer.send_lock);
    return result == ESP_OK;
}

static void send_ack(uint16_t ply, uint64_t position_hash)
{
    const chess_peer_message_t ack = {
        .kind = CHESS_PEER_MSG_ACK,
        .session_id = s_peer.session_id,
        .payload.ack = {
            .ply = ply,
            .position_hash = position_hash,
        },
    };
    (void)send_message(&ack);
}

static void send_start(void)
{
    const chess_peer_message_t start = {
        .kind = CHESS_PEER_MSG_START,
        .session_id = s_peer.session_id,
        .payload.start.white_id =
            s_peer.local_color == CHESS_WHITE ? s_peer.local_id : s_peer.peer_id,
    };
    (void)send_message(&start);
}

static void send_sync_chunks(uint16_t from_ply)
{
    if (s_peer.local_color != CHESS_WHITE || from_ply > s_peer.ply) {
        return;
    }
    uint16_t cursor = from_ply;
    if (cursor == s_peer.ply) {
        chess_peer_message_t empty = {
            .kind = CHESS_PEER_MSG_SYNC_CHUNK,
            .session_id = s_peer.session_id,
            .payload.sync_chunk = {
                .from_ply = cursor,
                .total_ply = s_peer.ply,
                .count = 0,
            },
        };
        (void)send_message(&empty);
        return;
    }
    while (cursor < s_peer.ply) {
        chess_peer_message_t chunk = {
            .kind = CHESS_PEER_MSG_SYNC_CHUNK,
            .session_id = s_peer.session_id,
        };
        chunk.payload.sync_chunk.from_ply = cursor;
        chunk.payload.sync_chunk.total_ply = s_peer.ply;
        const uint16_t remaining = s_peer.ply - cursor;
        chunk.payload.sync_chunk.count =
            remaining > CHESS_PEER_SYNC_MOVES_MAX ?
                CHESS_PEER_SYNC_MOVES_MAX : (uint8_t)remaining;
        memcpy(chunk.payload.sync_chunk.moves, s_peer.move_log + cursor,
               chunk.payload.sync_chunk.count * sizeof(chess_move_t));
        if (!send_message(&chunk)) {
            break;
        }
        cursor += chunk.payload.sync_chunk.count;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void handle_ready(const chess_peer_message_t *message)
{
    s_peer.peer_ready = message->payload.ready.ready;
    if (s_peer.local_color == CHESS_WHITE && s_peer.local_ready &&
        s_peer.peer_ready && !s_peer.started) {
        s_peer.started = true;
        s_peer.ply = 0;
        send_start();
        chess_peer_event_t event = {.kind = CHESS_PEER_EVENT_STARTED};
        post_event(&event);
    }
}

static void handle_start(const chess_peer_message_t *message)
{
    const uint64_t expected_white =
        s_peer.local_id < s_peer.peer_id ? s_peer.local_id : s_peer.peer_id;
    if (message->payload.start.white_id != expected_white) {
        chess_peer_event_t event = {.kind = CHESS_PEER_EVENT_SYNC_ERROR};
        post_event(&event);
        return;
    }
    if (!s_peer.started) {
        s_peer.started = true;
        s_peer.ply = 0;
        chess_peer_event_t event = {.kind = CHESS_PEER_EVENT_STARTED};
        post_event(&event);
    }
}

static void handle_move(const chess_peer_message_t *message)
{
    if (s_peer.sync_receiving) {
        return;
    }
    const uint16_t ply = message->payload.move.ply;
    if (ply <= s_peer.ply) {
        send_ack(s_peer.ply, s_peer.position_hash);
        return;
    }
    if (s_peer.remote_move_pending) {
        if (ply != s_peer.remote_pending_ply) {
            chess_peer_event_t error = {
                .kind = CHESS_PEER_EVENT_SYNC_ERROR,
            };
            post_event(&error);
        }
        return;
    }
    if (ply != (uint16_t)(s_peer.ply + 1) ||
        message->payload.move.before_hash != s_peer.position_hash) {
        if (s_peer.local_color == CHESS_WHITE) {
            send_sync_chunks(0);
        } else {
            (void)chess_peer_link_request_sync(0, s_peer.position_hash);
        }
        return;
    }
    s_peer.pending_move = false;
    s_peer.remote_move_pending = true;
    s_peer.remote_pending_ply = ply;
    chess_peer_event_t event = {
        .kind = CHESS_PEER_EVENT_REMOTE_MOVE,
        .data.remote_move = {
            .ply = ply,
            .move = message->payload.move.move,
            .before_hash = message->payload.move.before_hash,
            .after_hash = message->payload.move.after_hash,
        },
    };
    post_event(&event);
}

static void handle_sync_chunk(const chess_peer_message_t *message)
{
    const uint16_t from = message->payload.sync_chunk.from_ply;
    if (from == 0 && !s_peer.sync_receiving) {
        s_peer.sync_receiving = true;
        s_peer.sync_expected_ply = 0;
        s_peer.sync_total_ply = message->payload.sync_chunk.total_ply;
        s_peer.sync_last_ms = now_ms();
        chess_peer_event_t begin = {.kind = CHESS_PEER_EVENT_SYNC_BEGIN};
        post_event(&begin);
    }
    if (!s_peer.sync_receiving || from != s_peer.sync_expected_ply ||
        message->payload.sync_chunk.total_ply != s_peer.sync_total_ply) {
        s_peer.sync_receiving = false;
        chess_peer_event_t error = {.kind = CHESS_PEER_EVENT_SYNC_ERROR};
        post_event(&error);
        (void)chess_peer_link_request_sync(0, s_peer.position_hash);
        return;
    }
    s_peer.sync_last_ms = now_ms();
    chess_peer_event_t event = {
        .kind = CHESS_PEER_EVENT_SYNC_CHUNK,
        .data.sync_chunk = {
            .from_ply = from,
            .total_ply = s_peer.sync_total_ply,
            .count = message->payload.sync_chunk.count,
        },
    };
    memcpy(event.data.sync_chunk.moves,
           message->payload.sync_chunk.moves,
           message->payload.sync_chunk.count * sizeof(chess_move_t));
    if ((size_t)from + message->payload.sync_chunk.count <=
        CHESS_MOVE_LOG_MAX) {
        memcpy(s_peer.move_log + from,
               message->payload.sync_chunk.moves,
               message->payload.sync_chunk.count * sizeof(chess_move_t));
    }
    post_event(&event);
    s_peer.sync_expected_ply += message->payload.sync_chunk.count;
    if (s_peer.sync_expected_ply == s_peer.sync_total_ply) {
        chess_peer_event_t complete = {.kind = CHESS_PEER_EVENT_SYNC_COMPLETE};
        post_event(&complete);
    }
}

static void handle_protocol_message(const rx_item_t *item)
{
    const mosaico_peer_message_t *outer = &item->message;
    if (!s_peer.paired || outer->source_id != s_peer.peer_id ||
        outer->source_boot_id != s_peer.peer_boot_id ||
        outer->target_id != s_peer.local_id ||
        outer->session_id != s_peer.session_id) {
        return;
    }
    chess_peer_message_t message;
    if (!chess_peer_protocol_decode(
            outer->payload, outer->payload_len, &message) ||
        message.session_id != s_peer.session_id) {
        return;
    }
    const bool was_disconnected = s_peer.disconnected;
    s_peer.last_receive_ms = item->received_ms;
    s_peer.rssi = item->rssi;
    s_peer.disconnected = false;
    if (was_disconnected) {
        chess_peer_event_t event = {.kind = CHESS_PEER_EVENT_RECONNECTED};
        post_event(&event);
    }
    switch (message.kind) {
    case CHESS_PEER_MSG_READY:
        handle_ready(&message);
        break;
    case CHESS_PEER_MSG_START:
        handle_start(&message);
        break;
    case CHESS_PEER_MSG_MOVE:
        handle_move(&message);
        break;
    case CHESS_PEER_MSG_ACK:
        if (s_peer.pending_move &&
            message.payload.ack.ply == s_peer.ply &&
            message.payload.ack.position_hash == s_peer.position_hash) {
            s_peer.pending_move = false;
        }
        break;
    case CHESS_PEER_MSG_SYNC_REQUEST:
        if (s_peer.local_color == CHESS_WHITE) {
            send_sync_chunks(message.payload.sync_request.from_ply);
        }
        break;
    case CHESS_PEER_MSG_SYNC_CHUNK:
        if (s_peer.local_color == CHESS_BLACK) {
            handle_sync_chunk(&message);
        }
        break;
    case CHESS_PEER_MSG_RESIGN: {
        if (message.payload.resign.color == s_peer.local_color) {
            break;
        }
        chess_peer_event_t event = {
            .kind = CHESS_PEER_EVENT_REMOTE_RESIGNED,
            .data.resign.color = message.payload.resign.color,
        };
        post_event(&event);
        break;
    }
    case CHESS_PEER_MSG_PING:
        if (message.payload.ping.echoed_timestamp_ms == 0) {
            chess_peer_message_t pong = {
                .kind = CHESS_PEER_MSG_PING,
                .session_id = s_peer.session_id,
                .payload.ping = {
                    .timestamp_ms = item->received_ms,
                    .echoed_timestamp_ms =
                        message.payload.ping.timestamp_ms,
                },
            };
            (void)send_message(&pong);
        }
        break;
    default:
        break;
    }
}

static void peer_session_event(
    const mosaico_peer_session_event_t *event, void *user_ctx)
{
    (void)user_ctx;
    if (event->type == MOSAICO_PEER_SESSION_EVENT_SEND) {
        (void)mosaico_peer_link_send_broadcast_to(
            event->message_type, event->target_id, event->session_id,
            event->local_edge, event->peer_edge, event->relative_rotation,
            NULL, 0);
        return;
    }
    if (event->type != MOSAICO_PEER_SESSION_EVENT_ATTACHED ||
        s_peer.paired || !s_peer.pairing) {
        return;
    }
    s_peer.paired = true;
    s_peer.pairing = false;
    s_peer.peer_id = event->peer_id;
    s_peer.session_id = event->session_id;
    s_peer.local_edge = event->local_edge;
    s_peer.peer_edge = event->peer_edge;
    s_peer.local_color =
        s_peer.local_id < s_peer.peer_id ? CHESS_WHITE : CHESS_BLACK;
    s_peer.last_receive_ms = now_ms();
    chess_peer_event_t paired = {.kind = CHESS_PEER_EVENT_PAIRED};
    post_event(&paired);
    ESP_LOGI(TAG, "magnetic peer bound: peer=%012" PRIx64
             " session=%08" PRIx32 " color=%s",
             s_peer.peer_id, s_peer.session_id,
             s_peer.local_color == CHESS_WHITE ? "white" : "black");
}

static void peer_receive(const uint8_t source_mac[6], int8_t rssi,
                         const mosaico_peer_message_t *message, void *user_ctx)
{
    (void)source_mac;
    (void)user_ctx;
    if (!s_peer.running || message == NULL ||
        message->source_id == s_peer.local_id) {
        return;
    }
    const rx_item_t item = {
        .rssi = rssi,
        .received_ms = now_ms(),
        .message = *message,
    };
    (void)xQueueSend(s_peer.rx_queue, &item, 0);
}

static void presence_event(const mosaico_mag_presence_event_t *event,
                           void *user_ctx)
{
    (void)user_ctx;
    xSemaphoreTake(s_peer.lock, portMAX_DELAY);
    s_peer.detected_edges = event->attached_mask;
    /*
     * Magnetism is an admission gesture, not the game transport.  Once the
     * CLAIM/ACK/COMMIT handshake binds a peer, deliberately do not forward a
     * physical release to mosaico_peer_session; ESP-NOW owns the remaining
     * lifetime and the boards may be separated for play.
     */
    const bool should_negotiate = s_peer.pairing && !s_peer.paired;
    xSemaphoreGive(s_peer.lock);
    if (!should_negotiate) {
        return;
    }
    xSemaphoreTake(s_peer.session_lock, portMAX_DELAY);
    if (event->type == MOSAICO_MAG_PRESENCE_CONTACT) {
        (void)mosaico_peer_session_local_contact(
            &s_peer.peer_sessions, event->edge, 0, event->timestamp_ms);
    } else {
        (void)mosaico_peer_session_local_release(
            &s_peer.peer_sessions, event->edge, event->timestamp_ms);
    }
    xSemaphoreGive(s_peer.session_lock);
}

static void magnetic_task(void *arg)
{
    (void)arg;
    TickType_t wake = xTaskGetTickCount();
    while (s_peer.running) {
        struct bmm150_mag_data right = {0};
        struct bmm150_mag_data left = {0};
        const esp_err_t right_result =
            bsp_magnetometer_read(BSP_MAGNETOMETER_0, &right);
        const esp_err_t left_result =
            bsp_magnetometer_read(BSP_MAGNETOMETER_1, &left);
        const mosaico_mag_sample_t sample = {
            .right = {
                .valid = right_result == ESP_OK,
                .x = right.x, .y = right.y, .z = right.z,
            },
            .left = {
                .valid = left_result == ESP_OK,
                .x = left.x, .y = left.y, .z = left.z,
            },
            .timestamp_ms = now_ms(),
        };
        mosaico_mag_classification_t classification = {0};
        if (mosaico_mag_classifier_process(
                &s_peer.classifier, &sample, &classification) == ESP_OK) {
            (void)mosaico_mag_presence_tracker_process(
                &s_peer.presence_tracker, &classification);
        }
        xTaskDelayUntil(&wake, pdMS_TO_TICKS(CHESS_MAG_SAMPLE_MS));
    }
    s_peer.magnetic_task = NULL;
    vTaskDelete(NULL);
}

static void process_rx(const rx_item_t *item)
{
    const mosaico_peer_message_t *message = &item->message;
    if (message->target_id != 0 && message->target_id != s_peer.local_id) {
        return;
    }
    if (s_peer.paired && message->source_id == s_peer.peer_id) {
        if (s_peer.peer_boot_id == 0) {
            s_peer.peer_boot_id = message->source_boot_id;
        } else if (s_peer.peer_boot_id != message->source_boot_id) {
            if (!s_peer.disconnected) {
                s_peer.disconnected = true;
                chess_peer_event_t event = {
                    .kind = CHESS_PEER_EVENT_DISCONNECTED,
                };
                post_event(&event);
            }
            return;
        }
    }
    if (message->type == MOSAICO_PEER_MSG_GAME_EVENT) {
        handle_protocol_message(item);
        return;
    }
    const bool bound_session =
        s_peer.paired && message->source_id == s_peer.peer_id &&
        message->session_id == s_peer.session_id;
    if ((!s_peer.pairing || s_peer.paired) && !bound_session) {
        return;
    }
    xSemaphoreTake(s_peer.session_lock, portMAX_DELAY);
    (void)mosaico_peer_session_receive(
        &s_peer.peer_sessions, message, item->received_ms);
    xSemaphoreGive(s_peer.session_lock);
}

static void network_task(void *arg)
{
    (void)arg;
    TickType_t wake = xTaskGetTickCount();
    while (s_peer.running) {
        rx_item_t item;
        while (xQueueReceive(s_peer.rx_queue, &item, 0) == pdTRUE) {
            xSemaphoreTake(s_peer.lock, portMAX_DELAY);
            process_rx(&item);
            xSemaphoreGive(s_peer.lock);
        }
        const uint32_t now = now_ms();
        xSemaphoreTake(s_peer.session_lock, portMAX_DELAY);
        if (s_peer.pairing || s_peer.paired) {
            mosaico_peer_session_tick(&s_peer.peer_sessions, now);
        }
        xSemaphoreGive(s_peer.session_lock);

        xSemaphoreTake(s_peer.lock, portMAX_DELAY);
        if (s_peer.paired && elapsed(now, s_peer.last_ping_ms,
                                    CHESS_PING_PERIOD_MS)) {
            s_peer.last_ping_ms = now;
            chess_peer_message_t ping = {
                .kind = CHESS_PEER_MSG_PING,
                .session_id = s_peer.session_id,
                .payload.ping.timestamp_ms = now,
            };
            (void)send_message(&ping);
            if (s_peer.local_ready && !s_peer.started) {
                chess_peer_message_t ready = {
                    .kind = CHESS_PEER_MSG_READY,
                    .session_id = s_peer.session_id,
                    .payload.ready.ready = true,
                };
                (void)send_message(&ready);
            }
            if (s_peer.local_color == CHESS_WHITE && s_peer.started) {
                send_start();
            }
        }
        if (s_peer.pending_move &&
            elapsed(now, s_peer.last_retry_ms, CHESS_RETRY_MS)) {
            s_peer.last_retry_ms = now;
            (void)send_message(&s_peer.pending_message);
        }
        if (s_peer.sync_receiving &&
            elapsed(now, s_peer.sync_last_ms, 1000U)) {
            s_peer.sync_receiving = false;
            (void)chess_peer_link_request_sync(0, s_peer.position_hash);
        }
        if (s_peer.paired && !s_peer.disconnected &&
            elapsed(now, s_peer.last_receive_ms, CHESS_DISCONNECT_MS)) {
            s_peer.disconnected = true;
            chess_peer_event_t event = {
                .kind = CHESS_PEER_EVENT_DISCONNECTED,
            };
            post_event(&event);
        }
        xSemaphoreGive(s_peer.lock);
        xTaskDelayUntil(&wake, pdMS_TO_TICKS(CHESS_TASK_PERIOD_MS));
    }
    s_peer.network_task = NULL;
    vTaskDelete(NULL);
}

bool chess_peer_link_init(void)
{
    if (s_peer.running) {
        return true;
    }
    memset(&s_peer, 0, sizeof(s_peer));
    s_peer.rx_queue = xQueueCreate(CHESS_RX_DEPTH, sizeof(rx_item_t));
    s_peer.event_queue =
        xQueueCreate(CHESS_EVENT_DEPTH, sizeof(chess_peer_event_t));
    s_peer.lock = xSemaphoreCreateMutex();
    s_peer.session_lock = xSemaphoreCreateMutex();
    s_peer.send_lock = xSemaphoreCreateMutex();
    if (s_peer.rx_queue == NULL || s_peer.event_queue == NULL ||
        s_peer.lock == NULL || s_peer.session_lock == NULL ||
        s_peer.send_lock == NULL) {
        chess_peer_link_deinit();
        return false;
    }
    mosaico_peer_link_config_t config = MOSAICO_PEER_LINK_CONFIG_DEFAULT();
    config.network_id = CHESS_NETWORK_ID;
    config.channel = CHESS_ESPNOW_CHANNEL;
    config.receive_queue_depth = CHESS_RX_DEPTH;
    if (mosaico_peer_link_init(&config, peer_receive, NULL) != ESP_OK) {
        chess_peer_link_deinit();
        return false;
    }
    s_peer.local_id = mosaico_peer_link_get_device_id();
    s_peer.local_boot_id = mosaico_peer_link_get_boot_id();
    if (mosaico_peer_session_init(
            &s_peer.peer_sessions, s_peer.local_id,
            NULL, peer_session_event, NULL) != ESP_OK ||
        mosaico_mag_classifier_init(
            &s_peer.classifier, &MOSAICO_MAG_CALIBRATION_S31_V1) != ESP_OK ||
        mosaico_mag_presence_tracker_init(
            &s_peer.presence_tracker, NULL, presence_event, NULL) != ESP_OK ||
        bsp_magnetometer_init_all() != ESP_OK) {
        chess_peer_link_deinit();
        return false;
    }
    s_peer.running = true;
    if (xTaskCreate(network_task, "chess_peer", 5120, NULL, 5,
                    &s_peer.network_task) != pdPASS ||
        xTaskCreate(magnetic_task, "chess_magnetic", 4096, NULL, 6,
                    &s_peer.magnetic_task) != pdPASS) {
        chess_peer_link_deinit();
        return false;
    }
    return true;
}

void chess_peer_link_deinit(void)
{
    s_peer.running = false;
    for (unsigned i = 0; i < 20 &&
         (s_peer.network_task != NULL || s_peer.magnetic_task != NULL); ++i) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    (void)mosaico_peer_link_deinit();
    if (s_peer.rx_queue != NULL) {
        vQueueDelete(s_peer.rx_queue);
    }
    if (s_peer.event_queue != NULL) {
        vQueueDelete(s_peer.event_queue);
    }
    if (s_peer.lock != NULL) {
        vSemaphoreDelete(s_peer.lock);
    }
    if (s_peer.session_lock != NULL) {
        vSemaphoreDelete(s_peer.session_lock);
    }
    if (s_peer.send_lock != NULL) {
        vSemaphoreDelete(s_peer.send_lock);
    }
    memset(&s_peer, 0, sizeof(s_peer));
}

bool chess_peer_link_begin_pairing(void)
{
    if (!s_peer.running) {
        return false;
    }
    xSemaphoreTake(s_peer.lock, portMAX_DELAY);
    s_peer.pairing = true;
    s_peer.paired = false;
    s_peer.local_ready = false;
    s_peer.peer_ready = false;
    s_peer.started = false;
    s_peer.disconnected = false;
    s_peer.peer_id = 0;
    s_peer.peer_boot_id = 0;
    s_peer.session_id = 0;
    s_peer.ply = 0;
    s_peer.position_hash = 0;
    s_peer.detected_edges = s_peer.presence_tracker.attached_mask;
    const mosaico_edge_mask_t edges = s_peer.detected_edges;
    xSemaphoreGive(s_peer.lock);
    xSemaphoreTake(s_peer.session_lock, portMAX_DELAY);
    mosaico_peer_session_reset(&s_peer.peer_sessions);
    for (mosaico_edge_t edge = MOSAICO_EDGE_TOP;
         edge <= MOSAICO_EDGE_LEFT; ++edge) {
        if ((edges & mosaico_edge_to_mask(edge)) != 0) {
            (void)mosaico_peer_session_local_contact(
                &s_peer.peer_sessions, edge, 0, now_ms());
        }
    }
    xSemaphoreGive(s_peer.session_lock);
    return true;
}

void chess_peer_link_leave(void)
{
    if (!s_peer.running) {
        return;
    }
    xSemaphoreTake(s_peer.lock, portMAX_DELAY);
    s_peer.pairing = false;
    s_peer.paired = false;
    s_peer.started = false;
    s_peer.pending_move = false;
    s_peer.peer_id = 0;
    s_peer.session_id = 0;
    xSemaphoreGive(s_peer.lock);
    xSemaphoreTake(s_peer.session_lock, portMAX_DELAY);
    mosaico_peer_session_reset(&s_peer.peer_sessions);
    xSemaphoreGive(s_peer.session_lock);
}

bool chess_peer_link_set_ready(void)
{
    if (!s_peer.paired) {
        return false;
    }
    xSemaphoreTake(s_peer.lock, portMAX_DELAY);
    s_peer.local_ready = true;
    chess_peer_message_t ready = {
        .kind = CHESS_PEER_MSG_READY,
        .session_id = s_peer.session_id,
        .payload.ready.ready = true,
    };
    const bool sent = send_message(&ready);
    if (s_peer.local_color == CHESS_WHITE && s_peer.peer_ready &&
        !s_peer.started) {
        s_peer.started = true;
        send_start();
        chess_peer_event_t event = {.kind = CHESS_PEER_EVENT_STARTED};
        post_event(&event);
    }
    xSemaphoreGive(s_peer.lock);
    return sent;
}

bool chess_peer_link_send_move(const chess_move_t *move,
                               uint64_t before_hash,
                               uint64_t after_hash)
{
    if (!s_peer.started || s_peer.disconnected || move == NULL ||
        s_peer.pending_move || s_peer.ply >= CHESS_MOVE_LOG_MAX) {
        return false;
    }
    xSemaphoreTake(s_peer.lock, portMAX_DELAY);
    s_peer.ply++;
    s_peer.move_log[s_peer.ply - 1] = *move;
    s_peer.position_hash = after_hash;
    s_peer.pending_message = (chess_peer_message_t) {
        .kind = CHESS_PEER_MSG_MOVE,
        .session_id = s_peer.session_id,
        .payload.move = {
            .ply = s_peer.ply,
            .move = *move,
            .before_hash = before_hash,
            .after_hash = after_hash,
        },
    };
    s_peer.pending_move = true;
    s_peer.last_retry_ms = now_ms();
    (void)send_message(&s_peer.pending_message);
    xSemaphoreGive(s_peer.lock);
    return true;
}

bool chess_peer_link_accept_remote_move(uint16_t ply,
                                        const chess_move_t *move,
                                        uint64_t after_hash)
{
    xSemaphoreTake(s_peer.lock, portMAX_DELAY);
    if (move == NULL || ply != (uint16_t)(s_peer.ply + 1) ||
        ply > CHESS_MOVE_LOG_MAX) {
        xSemaphoreGive(s_peer.lock);
        return false;
    }
    s_peer.ply = ply;
    s_peer.move_log[ply - 1] = *move;
    s_peer.position_hash = after_hash;
    s_peer.remote_move_pending = false;
    s_peer.remote_pending_ply = 0;
    send_ack(ply, after_hash);
    xSemaphoreGive(s_peer.lock);
    return true;
}

bool chess_peer_link_set_position(uint16_t ply, uint64_t position_hash)
{
    xSemaphoreTake(s_peer.lock, portMAX_DELAY);
    if (!s_peer.started || ply > CHESS_MOVE_LOG_MAX) {
        xSemaphoreGive(s_peer.lock);
        return false;
    }
    s_peer.ply = ply;
    s_peer.position_hash = position_hash;
    xSemaphoreGive(s_peer.lock);
    return true;
}

bool chess_peer_link_send_resign(chess_color_t color)
{
    if (!s_peer.started) {
        return false;
    }
    const chess_peer_message_t resign = {
        .kind = CHESS_PEER_MSG_RESIGN,
        .session_id = s_peer.session_id,
        .payload.resign.color = color,
    };
    bool queued = false;
    for (unsigned attempt = 0; attempt < 3; ++attempt) {
        queued = send_message(&resign) || queued;
    }
    return queued;
}

bool chess_peer_link_request_sync(uint16_t from_ply, uint64_t position_hash)
{
    if (!s_peer.paired) {
        return false;
    }
    s_peer.remote_move_pending = false;
    s_peer.remote_pending_ply = 0;
    const chess_peer_message_t request = {
        .kind = CHESS_PEER_MSG_SYNC_REQUEST,
        .session_id = s_peer.session_id,
        .payload.sync_request = {
            .from_ply = from_ply,
            .position_hash = position_hash,
        },
    };
    return send_message(&request);
}

bool chess_peer_link_complete_sync(uint16_t total_ply, uint64_t position_hash)
{
    if (!s_peer.sync_receiving ||
        total_ply != s_peer.sync_total_ply ||
        total_ply != s_peer.sync_expected_ply) {
        return false;
    }
    s_peer.ply = total_ply;
    s_peer.position_hash = position_hash;
    s_peer.sync_receiving = false;
    send_ack(total_ply, position_hash);
    return true;
}

bool chess_peer_link_poll_event(chess_peer_event_t *event)
{
    return event != NULL && s_peer.event_queue != NULL &&
           xQueueReceive(s_peer.event_queue, event, 0) == pdTRUE;
}

bool chess_peer_link_get_status(chess_peer_status_t *status)
{
    if (status == NULL || !s_peer.running) {
        return false;
    }
    xSemaphoreTake(s_peer.lock, portMAX_DELAY);
    *status = (chess_peer_status_t) {
        .initialized = true,
        .pairing = s_peer.pairing,
        .paired = s_peer.paired,
        .local_ready = s_peer.local_ready,
        .peer_ready = s_peer.peer_ready,
        .started = s_peer.started,
        .connected = s_peer.paired && !s_peer.disconnected,
        .local_color = s_peer.local_color,
        .local_id = s_peer.local_id,
        .peer_id = s_peer.peer_id,
        .session_id = s_peer.session_id,
        .ply = s_peer.ply,
        .rssi = s_peer.rssi,
    };
    xSemaphoreGive(s_peer.lock);
    return true;
}

#endif
