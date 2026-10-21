// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "chess.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CHESS_PEER_PROTOCOL_MAGIC 0x4348U
#define CHESS_PEER_PROTOCOL_VERSION 1U
#define CHESS_PEER_SYNC_MOVES_MAX 20U
#define CHESS_PEER_WIRE_MAX 100U

typedef enum {
    CHESS_PEER_MSG_READY = 1,
    CHESS_PEER_MSG_START,
    CHESS_PEER_MSG_MOVE,
    CHESS_PEER_MSG_ACK,
    CHESS_PEER_MSG_SYNC_REQUEST,
    CHESS_PEER_MSG_SYNC_CHUNK,
    CHESS_PEER_MSG_RESIGN,
    CHESS_PEER_MSG_PING,
} chess_peer_message_kind_t;

typedef struct {
    uint16_t ply;
    chess_move_t move;
} chess_peer_logged_move_t;

typedef struct {
    chess_peer_message_kind_t kind;
    uint32_t session_id;
    uint32_t sequence;
    union {
        struct {
            bool ready;
        } ready;
        struct {
            uint64_t white_id;
        } start;
        struct {
            uint16_t ply;
            chess_move_t move;
            uint64_t before_hash;
            uint64_t after_hash;
        } move;
        struct {
            uint16_t ply;
            uint64_t position_hash;
        } ack;
        struct {
            uint16_t from_ply;
            uint64_t position_hash;
        } sync_request;
        struct {
            uint16_t from_ply;
            uint16_t total_ply;
            uint8_t count;
            chess_move_t moves[CHESS_PEER_SYNC_MOVES_MAX];
        } sync_chunk;
        struct {
            chess_color_t color;
        } resign;
        struct {
            uint32_t timestamp_ms;
            uint32_t echoed_timestamp_ms;
        } ping;
    } payload;
} chess_peer_message_t;

bool chess_peer_protocol_encode(const chess_peer_message_t *message,
                                uint8_t *wire,
                                size_t capacity,
                                size_t *wire_size);
bool chess_peer_protocol_decode(const uint8_t *wire,
                                size_t wire_size,
                                chess_peer_message_t *message);
bool chess_peer_sequence_is_newer(uint32_t candidate, uint32_t reference);
