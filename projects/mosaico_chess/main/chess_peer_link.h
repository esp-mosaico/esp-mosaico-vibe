// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "chess_peer_protocol.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    CHESS_PEER_EVENT_PAIRED = 0,
    CHESS_PEER_EVENT_STARTED,
    CHESS_PEER_EVENT_REMOTE_MOVE,
    CHESS_PEER_EVENT_REMOTE_RESIGNED,
    CHESS_PEER_EVENT_DISCONNECTED,
    CHESS_PEER_EVENT_RECONNECTED,
    CHESS_PEER_EVENT_SYNC_BEGIN,
    CHESS_PEER_EVENT_SYNC_CHUNK,
    CHESS_PEER_EVENT_SYNC_COMPLETE,
    CHESS_PEER_EVENT_SYNC_ERROR,
} chess_peer_event_kind_t;

typedef struct {
    chess_peer_event_kind_t kind;
    chess_color_t local_color;
    uint64_t peer_id;
    uint32_t session_id;
    union {
        struct {
            uint16_t ply;
            chess_move_t move;
            uint64_t before_hash;
            uint64_t after_hash;
        } remote_move;
        struct {
            uint16_t from_ply;
            uint16_t total_ply;
            uint8_t count;
            chess_move_t moves[CHESS_PEER_SYNC_MOVES_MAX];
        } sync_chunk;
        struct {
            chess_color_t color;
        } resign;
    } data;
} chess_peer_event_t;

typedef struct {
    bool initialized;
    bool pairing;
    bool paired;
    bool local_ready;
    bool peer_ready;
    bool started;
    bool connected;
    chess_color_t local_color;
    uint64_t local_id;
    uint64_t peer_id;
    uint32_t session_id;
    uint16_t ply;
    int8_t rssi;
} chess_peer_status_t;

bool chess_peer_link_init(void);
void chess_peer_link_deinit(void);
bool chess_peer_link_begin_pairing(void);
void chess_peer_link_leave(void);
bool chess_peer_link_set_ready(void);
bool chess_peer_link_send_move(const chess_move_t *move,
                               uint64_t before_hash,
                               uint64_t after_hash);
bool chess_peer_link_accept_remote_move(uint16_t ply,
                                        const chess_move_t *move,
                                        uint64_t after_hash);
bool chess_peer_link_set_position(uint16_t ply, uint64_t position_hash);
bool chess_peer_link_send_resign(chess_color_t color);
bool chess_peer_link_request_sync(uint16_t from_ply, uint64_t position_hash);
bool chess_peer_link_complete_sync(uint16_t total_ply, uint64_t position_hash);
bool chess_peer_link_poll_event(chess_peer_event_t *event);
bool chess_peer_link_get_status(chess_peer_status_t *status);
