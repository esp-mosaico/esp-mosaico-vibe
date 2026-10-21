// SPDX-License-Identifier: Apache-2.0

#include "chess_peer_protocol.h"

#include <string.h>

#define HEADER_SIZE 13U

static void put_u16(uint8_t *out, uint16_t value)
{
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8);
}

static void put_u32(uint8_t *out, uint32_t value)
{
    for (size_t i = 0; i < 4; ++i) {
        out[i] = (uint8_t)(value >> (i * 8));
    }
}

static void put_u64(uint8_t *out, uint64_t value)
{
    for (size_t i = 0; i < 8; ++i) {
        out[i] = (uint8_t)(value >> (i * 8));
    }
}

static uint16_t get_u16(const uint8_t *in)
{
    return (uint16_t)in[0] | ((uint16_t)in[1] << 8);
}

static uint32_t get_u32(const uint8_t *in)
{
    uint32_t value = 0;
    for (size_t i = 0; i < 4; ++i) {
        value |= (uint32_t)in[i] << (i * 8);
    }
    return value;
}

static uint64_t get_u64(const uint8_t *in)
{
    uint64_t value = 0;
    for (size_t i = 0; i < 8; ++i) {
        value |= (uint64_t)in[i] << (i * 8);
    }
    return value;
}

static bool valid_move(const chess_move_t *move)
{
    return move->from < 64 && move->to < 64 &&
           move->promotion >= CHESS_PIECE_NONE &&
           move->promotion <= CHESS_PIECE_QUEEN;
}

static void encode_move(uint8_t *out, const chess_move_t *move)
{
    out[0] = move->from;
    out[1] = move->to;
    out[2] = (uint8_t)move->promotion;
}

static bool decode_move(const uint8_t *in, chess_move_t *move)
{
    move->from = in[0];
    move->to = in[1];
    move->promotion = (chess_piece_type_t)in[2];
    return valid_move(move);
}

bool chess_peer_protocol_encode(const chess_peer_message_t *message,
                                uint8_t *wire,
                                size_t capacity,
                                size_t *wire_size)
{
    if (message == NULL || wire == NULL || wire_size == NULL ||
        message->session_id == 0 || message->sequence == 0 ||
        capacity < HEADER_SIZE) {
        return false;
    }

    uint8_t payload[CHESS_PEER_WIRE_MAX - HEADER_SIZE] = {0};
    size_t payload_size = 0;
    switch (message->kind) {
    case CHESS_PEER_MSG_READY:
        payload[0] = message->payload.ready.ready ? 1U : 0U;
        payload_size = 1;
        break;
    case CHESS_PEER_MSG_START:
        if (message->payload.start.white_id == 0) {
            return false;
        }
        put_u64(payload, message->payload.start.white_id);
        payload_size = 8;
        break;
    case CHESS_PEER_MSG_MOVE:
        if (!valid_move(&message->payload.move.move) ||
            message->payload.move.ply == 0) {
            return false;
        }
        put_u16(payload, message->payload.move.ply);
        encode_move(payload + 2, &message->payload.move.move);
        put_u64(payload + 5, message->payload.move.before_hash);
        put_u64(payload + 13, message->payload.move.after_hash);
        payload_size = 21;
        break;
    case CHESS_PEER_MSG_ACK:
        put_u16(payload, message->payload.ack.ply);
        put_u64(payload + 2, message->payload.ack.position_hash);
        payload_size = 10;
        break;
    case CHESS_PEER_MSG_SYNC_REQUEST:
        put_u16(payload, message->payload.sync_request.from_ply);
        put_u64(payload + 2, message->payload.sync_request.position_hash);
        payload_size = 10;
        break;
    case CHESS_PEER_MSG_SYNC_CHUNK:
        if (message->payload.sync_chunk.count > CHESS_PEER_SYNC_MOVES_MAX ||
            message->payload.sync_chunk.from_ply >
                message->payload.sync_chunk.total_ply) {
            return false;
        }
        put_u16(payload, message->payload.sync_chunk.from_ply);
        put_u16(payload + 2, message->payload.sync_chunk.total_ply);
        payload[4] = message->payload.sync_chunk.count;
        payload_size = 5;
        for (uint8_t i = 0; i < message->payload.sync_chunk.count; ++i) {
            if (!valid_move(&message->payload.sync_chunk.moves[i])) {
                return false;
            }
            encode_move(payload + payload_size,
                        &message->payload.sync_chunk.moves[i]);
            payload_size += 3;
        }
        break;
    case CHESS_PEER_MSG_RESIGN:
        if (message->payload.resign.color != CHESS_WHITE &&
            message->payload.resign.color != CHESS_BLACK) {
            return false;
        }
        payload[0] = (uint8_t)message->payload.resign.color;
        payload_size = 1;
        break;
    case CHESS_PEER_MSG_PING:
        put_u32(payload, message->payload.ping.timestamp_ms);
        put_u32(payload + 4, message->payload.ping.echoed_timestamp_ms);
        payload_size = 8;
        break;
    default:
        return false;
    }

    if (HEADER_SIZE + payload_size > capacity ||
        HEADER_SIZE + payload_size > CHESS_PEER_WIRE_MAX) {
        return false;
    }
    put_u16(wire, CHESS_PEER_PROTOCOL_MAGIC);
    wire[2] = CHESS_PEER_PROTOCOL_VERSION;
    wire[3] = (uint8_t)message->kind;
    put_u32(wire + 4, message->session_id);
    put_u32(wire + 8, message->sequence);
    wire[12] = (uint8_t)payload_size;
    memcpy(wire + HEADER_SIZE, payload, payload_size);
    *wire_size = HEADER_SIZE + payload_size;
    return true;
}

bool chess_peer_protocol_decode(const uint8_t *wire,
                                size_t wire_size,
                                chess_peer_message_t *message)
{
    if (wire == NULL || message == NULL || wire_size < HEADER_SIZE ||
        wire_size > CHESS_PEER_WIRE_MAX ||
        get_u16(wire) != CHESS_PEER_PROTOCOL_MAGIC ||
        wire[2] != CHESS_PEER_PROTOCOL_VERSION ||
        wire[12] != wire_size - HEADER_SIZE) {
        return false;
    }
    memset(message, 0, sizeof(*message));
    message->kind = (chess_peer_message_kind_t)wire[3];
    message->session_id = get_u32(wire + 4);
    message->sequence = get_u32(wire + 8);
    const uint8_t *payload = wire + HEADER_SIZE;
    const size_t size = wire[12];
    if (message->session_id == 0 || message->sequence == 0) {
        return false;
    }

    switch (message->kind) {
    case CHESS_PEER_MSG_READY:
        if (size != 1 || payload[0] > 1) {
            return false;
        }
        message->payload.ready.ready = payload[0] != 0;
        return true;
    case CHESS_PEER_MSG_START:
        if (size != 8) {
            return false;
        }
        message->payload.start.white_id = get_u64(payload);
        return message->payload.start.white_id != 0;
    case CHESS_PEER_MSG_MOVE:
        if (size != 21) {
            return false;
        }
        message->payload.move.ply = get_u16(payload);
        message->payload.move.before_hash = get_u64(payload + 5);
        message->payload.move.after_hash = get_u64(payload + 13);
        return message->payload.move.ply != 0 &&
               decode_move(payload + 2, &message->payload.move.move);
    case CHESS_PEER_MSG_ACK:
        if (size != 10) {
            return false;
        }
        message->payload.ack.ply = get_u16(payload);
        message->payload.ack.position_hash = get_u64(payload + 2);
        return true;
    case CHESS_PEER_MSG_SYNC_REQUEST:
        if (size != 10) {
            return false;
        }
        message->payload.sync_request.from_ply = get_u16(payload);
        message->payload.sync_request.position_hash = get_u64(payload + 2);
        return true;
    case CHESS_PEER_MSG_SYNC_CHUNK: {
        if (size < 5 || (size - 5) % 3 != 0) {
            return false;
        }
        const uint8_t count = payload[4];
        if (count > CHESS_PEER_SYNC_MOVES_MAX || size != 5U + count * 3U) {
            return false;
        }
        message->payload.sync_chunk.from_ply = get_u16(payload);
        message->payload.sync_chunk.total_ply = get_u16(payload + 2);
        message->payload.sync_chunk.count = count;
        if (message->payload.sync_chunk.from_ply >
            message->payload.sync_chunk.total_ply) {
            return false;
        }
        for (uint8_t i = 0; i < count; ++i) {
            if (!decode_move(payload + 5 + i * 3,
                             &message->payload.sync_chunk.moves[i])) {
                return false;
            }
        }
        return true;
    }
    case CHESS_PEER_MSG_RESIGN:
        if (size != 1 || payload[0] > CHESS_WHITE) {
            return false;
        }
        message->payload.resign.color = (chess_color_t)payload[0];
        return true;
    case CHESS_PEER_MSG_PING:
        if (size != 8) {
            return false;
        }
        message->payload.ping.timestamp_ms = get_u32(payload);
        message->payload.ping.echoed_timestamp_ms = get_u32(payload + 4);
        return true;
    default:
        return false;
    }
}

bool chess_peer_sequence_is_newer(uint32_t candidate, uint32_t reference)
{
    return candidate != reference &&
           (uint32_t)(candidate - reference) < UINT32_MAX / 2U;
}
