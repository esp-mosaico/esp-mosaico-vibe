// SPDX-License-Identifier: Apache-2.0

#include "chess_peer_protocol.h"

#include <stdio.h>
#include <string.h>

static int s_failures;

#define EXPECT(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d expectation failed: %s\n", \
                __FILE__, __LINE__, #condition); \
        ++s_failures; \
    } \
} while (0)

static chess_peer_message_t round_trip(const chess_peer_message_t *source)
{
    uint8_t wire[CHESS_PEER_WIRE_MAX] = {0};
    size_t wire_size = 0;
    chess_peer_message_t decoded = {0};
    EXPECT(chess_peer_protocol_encode(
        source, wire, sizeof(wire), &wire_size));
    EXPECT(wire_size <= CHESS_PEER_WIRE_MAX);
    EXPECT(chess_peer_protocol_decode(wire, wire_size, &decoded));
    return decoded;
}

static void test_move_round_trip(void)
{
    const chess_peer_message_t source = {
        .kind = CHESS_PEER_MSG_MOVE,
        .session_id = 0x12345678,
        .sequence = 7,
        .payload.move = {
            .ply = 12,
            .move = {
                .from = 52,
                .to = 60,
                .promotion = CHESS_PIECE_KNIGHT,
            },
            .before_hash = UINT64_C(0x1020304050607080),
            .after_hash = UINT64_C(0x8877665544332211),
        },
    };
    const chess_peer_message_t decoded = round_trip(&source);
    EXPECT(decoded.kind == source.kind);
    EXPECT(decoded.session_id == source.session_id);
    EXPECT(decoded.sequence == source.sequence);
    EXPECT(decoded.payload.move.ply == source.payload.move.ply);
    EXPECT(decoded.payload.move.move.from == source.payload.move.move.from);
    EXPECT(decoded.payload.move.move.to == source.payload.move.move.to);
    EXPECT(decoded.payload.move.move.promotion ==
           source.payload.move.move.promotion);
    EXPECT(decoded.payload.move.before_hash ==
           source.payload.move.before_hash);
    EXPECT(decoded.payload.move.after_hash == source.payload.move.after_hash);
}

static void test_sync_chunk_capacity(void)
{
    chess_peer_message_t source = {
        .kind = CHESS_PEER_MSG_SYNC_CHUNK,
        .session_id = 9,
        .sequence = UINT32_MAX,
    };
    source.payload.sync_chunk.from_ply = 40;
    source.payload.sync_chunk.total_ply = 60;
    source.payload.sync_chunk.count = CHESS_PEER_SYNC_MOVES_MAX;
    for (uint8_t i = 0; i < CHESS_PEER_SYNC_MOVES_MAX; ++i) {
        source.payload.sync_chunk.moves[i] = (chess_move_t) {
            .from = (uint8_t)(i % 8),
            .to = (uint8_t)(8 + i),
            .promotion = CHESS_PIECE_NONE,
        };
    }
    const chess_peer_message_t decoded = round_trip(&source);
    EXPECT(decoded.payload.sync_chunk.from_ply == 40);
    EXPECT(decoded.payload.sync_chunk.total_ply == 60);
    EXPECT(decoded.payload.sync_chunk.count == CHESS_PEER_SYNC_MOVES_MAX);
    for (uint8_t i = 0; i < CHESS_PEER_SYNC_MOVES_MAX; ++i) {
        EXPECT(decoded.payload.sync_chunk.moves[i].from ==
               source.payload.sync_chunk.moves[i].from);
        EXPECT(decoded.payload.sync_chunk.moves[i].to ==
               source.payload.sync_chunk.moves[i].to);
        EXPECT(decoded.payload.sync_chunk.moves[i].promotion ==
               source.payload.sync_chunk.moves[i].promotion);
    }
}

static void test_rejects_corruption(void)
{
    const chess_peer_message_t source = {
        .kind = CHESS_PEER_MSG_READY,
        .session_id = 1,
        .sequence = 1,
        .payload.ready.ready = true,
    };
    uint8_t wire[CHESS_PEER_WIRE_MAX] = {0};
    size_t wire_size = 0;
    chess_peer_message_t decoded;
    EXPECT(chess_peer_protocol_encode(
        &source, wire, sizeof(wire), &wire_size));
    wire[2]++;
    EXPECT(!chess_peer_protocol_decode(wire, wire_size, &decoded));
    wire[2] = CHESS_PEER_PROTOCOL_VERSION;
    wire[12]++;
    EXPECT(!chess_peer_protocol_decode(wire, wire_size, &decoded));
}

static void test_control_messages(void)
{
    chess_peer_message_t message = {
        .session_id = 0x55aa,
        .sequence = 3,
    };
    message.kind = CHESS_PEER_MSG_READY;
    message.payload.ready.ready = true;
    EXPECT(round_trip(&message).payload.ready.ready);

    message.kind = CHESS_PEER_MSG_START;
    message.payload.start.white_id = UINT64_C(0x020000000001);
    EXPECT(round_trip(&message).payload.start.white_id ==
           message.payload.start.white_id);

    message.kind = CHESS_PEER_MSG_ACK;
    message.payload.ack.ply = 44;
    message.payload.ack.position_hash = UINT64_C(0x1234);
    EXPECT(round_trip(&message).payload.ack.ply == 44);

    message.kind = CHESS_PEER_MSG_SYNC_REQUEST;
    message.payload.sync_request.from_ply = 20;
    message.payload.sync_request.position_hash = UINT64_C(0x5678);
    EXPECT(round_trip(&message).payload.sync_request.from_ply == 20);

    message.kind = CHESS_PEER_MSG_RESIGN;
    message.payload.resign.color = CHESS_BLACK;
    EXPECT(round_trip(&message).payload.resign.color == CHESS_BLACK);

    message.kind = CHESS_PEER_MSG_PING;
    message.payload.ping.timestamp_ms = UINT32_MAX - 10;
    message.payload.ping.echoed_timestamp_ms = 123;
    EXPECT(round_trip(&message).payload.ping.echoed_timestamp_ms == 123);
}

static void test_sequence_wrap(void)
{
    EXPECT(chess_peer_sequence_is_newer(11, 10));
    EXPECT(!chess_peer_sequence_is_newer(10, 10));
    EXPECT(chess_peer_sequence_is_newer(1, UINT32_MAX));
    EXPECT(!chess_peer_sequence_is_newer(UINT32_MAX, 1));
}

static void test_special_move_sync(const char *fen, chess_move_t move)
{
    chess_game_t *source = NULL;
    chess_game_t *replica = NULL;
    EXPECT(chess_game_create(&source) == CHESS_OK);
    EXPECT(chess_game_create(&replica) == CHESS_OK);
    EXPECT(chess_game_load_fen(source, fen) == CHESS_OK);
    EXPECT(chess_game_load_fen(replica, fen) == CHESS_OK);
    const uint64_t before = chess_game_position_hash(source);
    EXPECT(chess_game_apply(source, &move) == CHESS_OK);

    const chess_peer_message_t frame = {
        .kind = CHESS_PEER_MSG_MOVE,
        .session_id = 17,
        .sequence = 22,
        .payload.move = {
            .ply = 1,
            .move = move,
            .before_hash = before,
            .after_hash = chess_game_position_hash(source),
        },
    };
    const chess_peer_message_t decoded = round_trip(&frame);
    EXPECT(decoded.payload.move.before_hash ==
           chess_game_position_hash(replica));
    EXPECT(chess_game_apply(
               replica, &decoded.payload.move.move) == CHESS_OK);
    EXPECT(decoded.payload.move.after_hash ==
           chess_game_position_hash(replica));
    EXPECT(chess_game_position_hash(source) ==
           chess_game_position_hash(replica));
    chess_game_destroy(source);
    chess_game_destroy(replica);
}

int main(void)
{
    test_move_round_trip();
    test_sync_chunk_capacity();
    test_rejects_corruption();
    test_control_messages();
    test_sequence_wrap();
    test_special_move_sync(
        "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1",
        (chess_move_t) {
            .from = 4, .to = 6, .promotion = CHESS_PIECE_NONE,
        });
    test_special_move_sync(
        "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1",
        (chess_move_t) {
            .from = 36, .to = 43, .promotion = CHESS_PIECE_NONE,
        });
    test_special_move_sync(
        "4k3/P7/8/8/8/8/8/4K3 w - - 0 1",
        (chess_move_t) {
            .from = 48, .to = 56, .promotion = CHESS_PIECE_KNIGHT,
        });
    if (s_failures != 0) {
        fprintf(stderr, "%d peer protocol expectation(s) failed\n", s_failures);
        return 1;
    }
    puts("chess peer protocol tests passed");
    return 0;
}
