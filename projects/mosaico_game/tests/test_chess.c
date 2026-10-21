// SPDX-License-Identifier: Apache-2.0

#include "chess.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int s_failures;

#define EXPECT(condition)                                                       \
    do {                                                                        \
        if (!(condition)) {                                                     \
            fprintf(stderr, "%s:%d: expectation failed: %s\n",                 \
                    __FILE__, __LINE__, #condition);                            \
            ++s_failures;                                                       \
        }                                                                       \
    } while (0)

static int square(const char *name)
{
    return name[0] - 'a' + (name[1] - '1') * 8;
}

static chess_piece_type_t promotion_type(char value)
{
    switch (value) {
    case 'q':
        return CHESS_PIECE_QUEEN;
    case 'r':
        return CHESS_PIECE_ROOK;
    case 'b':
        return CHESS_PIECE_BISHOP;
    case 'n':
        return CHESS_PIECE_KNIGHT;
    default:
        return CHESS_PIECE_NONE;
    }
}

static chess_move_t move_from_uci(const char *uci)
{
    return (chess_move_t) {
        .from = (uint8_t)square(uci),
        .to = (uint8_t)square(uci + 2),
        .promotion = strlen(uci) == 5 ? promotion_type(uci[4])
                                      : CHESS_PIECE_NONE,
    };
}

static void apply(chess_game_t *game, const char *uci)
{
    const chess_move_t move = move_from_uci(uci);
    EXPECT(chess_game_apply(game, &move) == CHESS_OK);
}

static bool has_move(chess_game_t *game, const char *uci)
{
    const chess_move_t move = move_from_uci(uci);
    return chess_game_is_legal(game, &move);
}

static chess_game_t *game_from_fen(const char *fen)
{
    chess_game_t *game = NULL;
    EXPECT(chess_game_create(&game) == CHESS_OK);
    EXPECT(chess_game_load_fen(game, fen) == CHESS_OK);
    return game;
}

static void test_initial_and_basic_moves(void)
{
    chess_game_t *game = NULL;
    EXPECT(chess_game_create(&game) == CHESS_OK);
    EXPECT(chess_game_generate_legal(game, NULL, 0) == 20);
    EXPECT(has_move(game, "e2e4"));
    EXPECT(has_move(game, "g1f3"));
    EXPECT(!has_move(game, "c1h6"));
    EXPECT(!has_move(game, "a1a3"));
    EXPECT(!has_move(game, "e2e2"));
    EXPECT(!has_move(game, "e2e5"));
    apply(game, "e2e4");
    EXPECT(chess_game_side_to_move(game) == CHESS_BLACK);
    EXPECT(!has_move(game, "d2d4"));
    apply(game, "d7d5");
    EXPECT(has_move(game, "e4d5"));
    apply(game, "e4d5");
    EXPECT(chess_piece_type(chess_game_piece_at(game, square("d5"))) ==
           CHESS_PIECE_PAWN);
    chess_game_destroy(game);
}

static void test_piece_movement_and_blocking(void)
{
    chess_game_t *game = game_from_fen(
        "4k3/8/3p4/8/3R4/2B1N3/8/4K3 w - - 0 1");
    EXPECT(has_move(game, "d4d6"));
    EXPECT(!has_move(game, "d4d7"));
    EXPECT(!has_move(game, "c3d4"));
    EXPECT(has_move(game, "c3b4"));
    EXPECT(has_move(game, "e3f5"));
    EXPECT(!has_move(game, "e3e5"));
    chess_game_destroy(game);

    game = game_from_fen("4k3/8/8/8/3Q4/8/8/4K3 w - - 0 1");
    EXPECT(has_move(game, "d4d8"));
    EXPECT(has_move(game, "d4h4"));
    EXPECT(has_move(game, "d4h8"));
    chess_game_destroy(game);
}

static void test_check_pin_block_and_king_safety(void)
{
    chess_game_t *game = game_from_fen(
        "4r1k1/8/8/8/8/8/4R3/4K3 w - - 0 1");
    EXPECT(!has_move(game, "e2d2"));
    EXPECT(has_move(game, "e2e8"));
    chess_game_destroy(game);

    game = game_from_fen("4r1k1/8/8/8/1b6/8/8/4K3 w - - 0 1");
    chess_move_t moves[32];
    const size_t count = chess_game_generate_legal(game, moves, 32);
    for (size_t index = 0; index < count; ++index) {
        EXPECT(moves[index].from == square("e1"));
    }
    chess_game_destroy(game);

    game = game_from_fen("4r1k1/8/8/8/8/8/2B5/4K3 w - - 0 1");
    EXPECT(has_move(game, "c2e4"));
    EXPECT(!has_move(game, "c2b3"));
    chess_game_destroy(game);

    game = game_from_fen("8/8/8/8/8/4k3/8/4K3 w - - 0 1");
    EXPECT(!has_move(game, "e1e2"));
    chess_game_destroy(game);
}

static void test_castling(void)
{
    chess_game_t *game = game_from_fen(
        "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
    EXPECT(has_move(game, "e1g1"));
    EXPECT(has_move(game, "e1c1"));
    apply(game, "e1g1");
    EXPECT(chess_piece_type(chess_game_piece_at(game, square("f1"))) ==
           CHESS_PIECE_ROOK);
    EXPECT(chess_piece_type(chess_game_piece_at(game, square("g1"))) ==
           CHESS_PIECE_KING);
    apply(game, "e8c8");
    EXPECT(chess_piece_type(chess_game_piece_at(game, square("d8"))) ==
           CHESS_PIECE_ROOK);
    chess_game_destroy(game);

    game = game_from_fen("r3k2r/8/8/8/2b5/8/8/R3K2R w KQkq - 0 1");
    EXPECT(!has_move(game, "e1g1"));
    EXPECT(has_move(game, "e1c1"));
    chess_game_destroy(game);

    game = game_from_fen("r3k2r/8/8/8/4r3/8/8/R3K2R w KQkq - 0 1");
    EXPECT(!has_move(game, "e1g1"));
    EXPECT(!has_move(game, "e1c1"));
    chess_game_destroy(game);

    game = game_from_fen("3rk3/8/8/8/8/8/8/R3K2R w KQ - 0 1");
    EXPECT(has_move(game, "e1g1"));
    EXPECT(!has_move(game, "e1c1"));
    chess_game_destroy(game);

    game = game_from_fen("4k3/8/7b/8/8/8/8/R3K2R w KQ - 0 1");
    EXPECT(has_move(game, "e1g1"));
    EXPECT(!has_move(game, "e1c1"));
    chess_game_destroy(game);

    game = game_from_fen("4k3/8/8/8/8/8/8/4K3 w KQ - 0 1");
    EXPECT(!has_move(game, "e1g1"));
    EXPECT(!has_move(game, "e1c1"));
    chess_game_destroy(game);

    game = game_from_fen(
        "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
    apply(game, "h1h2");
    apply(game, "h8h7");
    apply(game, "h2h1");
    apply(game, "h7h8");
    EXPECT(!has_move(game, "e1g1"));
    chess_game_destroy(game);
}

static void test_en_passant(void)
{
    chess_game_t *game = game_from_fen(
        "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1");
    EXPECT(has_move(game, "e5d6"));
    apply(game, "e5d6");
    EXPECT(chess_game_piece_at(game, square("d5")) == 0);
    EXPECT(chess_piece_type(chess_game_piece_at(game, square("d6"))) ==
           CHESS_PIECE_PAWN);
    EXPECT(chess_game_status(game).halfmove_clock == 0);
    chess_game_destroy(game);

    game = game_from_fen("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1");
    apply(game, "e1f1");
    apply(game, "e8f8");
    EXPECT(!has_move(game, "e5d6"));
    chess_game_destroy(game);

    game = game_from_fen("8/8/8/4KPpr/8/8/8/7k w - g6 0 1");
    EXPECT(!has_move(game, "f5g6"));
    chess_game_destroy(game);

    game = game_from_fen("4k3/8/8/4P3/8/8/8/4K3 w - d6 0 1");
    EXPECT(!has_move(game, "e5d6"));
    chess_game_destroy(game);

    game = game_from_fen("4k3/8/8/8/3pP3/8/8/4K3 b - e3 0 1");
    EXPECT(has_move(game, "d4e3"));
    apply(game, "d4e3");
    EXPECT(chess_game_piece_at(game, square("e4")) == 0);
    chess_game_destroy(game);
}

static void test_promotions(void)
{
    static const struct {
        char suffix;
        chess_piece_type_t type;
    } cases[] = {
        {'q', CHESS_PIECE_QUEEN},
        {'r', CHESS_PIECE_ROOK},
        {'b', CHESS_PIECE_BISHOP},
        {'n', CHESS_PIECE_KNIGHT},
    };
    for (size_t index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        chess_game_t *game =
            game_from_fen("4k3/P7/8/8/8/8/7p/4K3 w - - 0 1");
        char uci[] = "a7a8q";
        uci[4] = cases[index].suffix;
        EXPECT(has_move(game, uci));
        apply(game, uci);
        EXPECT(chess_piece_type(chess_game_piece_at(game, square("a8"))) ==
               cases[index].type);
        chess_game_destroy(game);

        game = game_from_fen("4k3/8/8/8/8/8/p7/4K3 b - - 0 1");
        char black_uci[] = "a2a1q";
        black_uci[4] = cases[index].suffix;
        EXPECT(has_move(game, black_uci));
        apply(game, black_uci);
        EXPECT(chess_piece_type(chess_game_piece_at(game, square("a1"))) ==
               cases[index].type);
        chess_game_destroy(game);
    }

    chess_game_t *game =
        game_from_fen("1r2k3/P7/8/8/8/8/8/4K3 w - - 0 1");
    EXPECT(has_move(game, "a7b8q"));
    EXPECT(!has_move(game, "a7b8"));
    apply(game, "a7b8q");
    EXPECT(chess_piece_type(chess_game_piece_at(game, square("b8"))) ==
           CHESS_PIECE_QUEEN);
    chess_game_destroy(game);
}

static void test_terminal_positions(void)
{
    chess_game_t *game =
        game_from_fen("7k/6Q1/6K1/8/8/8/8/8 b - - 0 1");
    chess_status_t status = chess_game_status(game);
    EXPECT(status.end_reason == CHESS_END_CHECKMATE);
    EXPECT(status.winner == CHESS_WHITE);
    const chess_move_t impossible = move_from_uci("h8h7");
    EXPECT(chess_game_apply(game, &impossible) == CHESS_ERR_GAME_OVER);
    chess_game_destroy(game);

    game = game_from_fen("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1");
    status = chess_game_status(game);
    EXPECT(status.end_reason == CHESS_END_STALEMATE);
    EXPECT(status.winner == -1);
    chess_game_destroy(game);

    game = game_from_fen("7k/8/5KQ1/8/8/8/8/8 w - - 0 1");
    const chess_move_t mate = move_from_uci("g6g7");
    EXPECT(chess_game_apply(game, &mate) == CHESS_OK);
    status = chess_game_status(game);
    EXPECT(status.end_reason == CHESS_END_CHECKMATE);
    EXPECT(status.winner == CHESS_WHITE);
    chess_game_destroy(game);

    game = game_from_fen("3Q4/7k/8/8/8/8/8/4K3 w - - 0 1");
    const chess_move_t top_rank_move = move_from_uci("d8d7");
    EXPECT(chess_game_apply(game, &top_rank_move) == CHESS_OK);
    chess_game_destroy(game);
}

static void test_draw_rules(void)
{
    chess_game_t *game = NULL;
    EXPECT(chess_game_create(&game) == CHESS_OK);
    apply(game, "g1f3");
    apply(game, "g8f6");
    apply(game, "f3g1");
    apply(game, "f6g8");
    EXPECT(chess_game_status(game).end_reason == CHESS_END_NONE);
    apply(game, "g1f3");
    apply(game, "g8f6");
    apply(game, "f3g1");
    apply(game, "f6g8");
    EXPECT(chess_game_status(game).end_reason ==
           CHESS_END_THREEFOLD_REPETITION);
    chess_game_destroy(game);

    game = game_from_fen("4k2r/8/8/8/8/8/8/R3K3 w - - 99 1");
    apply(game, "a1a2");
    EXPECT(chess_game_status(game).end_reason ==
           CHESS_END_FIFTY_MOVE_RULE);
    chess_game_destroy(game);

    game = game_from_fen("4k3/8/8/8/8/8/P7/R3K3 w - - 99 1");
    apply(game, "a2a3");
    EXPECT(chess_game_status(game).halfmove_clock == 0);
    EXPECT(chess_game_status(game).end_reason == CHESS_END_NONE);
    chess_game_destroy(game);

    game = game_from_fen("r3k3/8/8/8/8/8/8/R3K3 w - - 99 1");
    apply(game, "a1a8");
    EXPECT(chess_game_status(game).halfmove_clock == 0);
    EXPECT(chess_game_status(game).end_reason == CHESS_END_NONE);
    chess_game_destroy(game);

    static const char *insufficient[] = {
        "4k3/8/8/8/8/8/8/4K3 w - - 0 1",
        "4k3/8/8/8/8/8/8/2B1K3 w - - 0 1",
        "4k3/8/8/8/8/8/8/2N1K3 w - - 0 1",
        "4kb2/8/8/8/8/8/8/2B1K3 w - - 0 1",
    };
    for (size_t index = 0;
         index < sizeof(insufficient) / sizeof(insufficient[0]); ++index) {
        game = game_from_fen(insufficient[index]);
        EXPECT(chess_game_status(game).end_reason ==
               CHESS_END_INSUFFICIENT_MATERIAL);
        chess_game_destroy(game);
    }
}

static void test_resignation(void)
{
    chess_game_t *game = NULL;
    EXPECT(chess_game_create(&game) == CHESS_OK);
    EXPECT(chess_game_resign(game, CHESS_WHITE) == CHESS_OK);
    chess_status_t status = chess_game_status(game);
    EXPECT(status.end_reason == CHESS_END_RESIGNATION);
    EXPECT(status.winner == CHESS_BLACK);
    chess_game_destroy(game);
}

static void test_ai_difficulties(void)
{
    chess_game_t *game = NULL;
    EXPECT(chess_game_create(&game) == CHESS_OK);
    for (chess_ai_level_t level = CHESS_AI_EASY;
         level <= CHESS_AI_HARD;
         level = (chess_ai_level_t)(level + 1)) {
        chess_move_t move;
        EXPECT(chess_game_ai_pick_level(game, level, &move));
        EXPECT(chess_game_is_legal(game, &move));
    }
    chess_game_destroy(game);

    game = game_from_fen("7k/8/6K1/5Q2/8/8/8/8 w - - 0 1");
    chess_move_t mate;
    EXPECT(chess_game_ai_pick_level(game, CHESS_AI_HARD, &mate));
    EXPECT(chess_game_apply(game, &mate) == CHESS_OK);
    EXPECT(chess_game_status(game).end_reason == CHESS_END_CHECKMATE);
    EXPECT(chess_game_status(game).winner == CHESS_WHITE);
    chess_game_destroy(game);
}

static void test_perft(void)
{
    chess_game_t *game = NULL;
    EXPECT(chess_game_create(&game) == CHESS_OK);
    EXPECT(chess_game_perft(game, 1) == 20);
    EXPECT(chess_game_perft(game, 2) == 400);
    EXPECT(chess_game_perft(game, 3) == 8902);
    EXPECT(chess_game_perft(game, 4) == 197281);
    chess_game_destroy(game);

    game = game_from_fen(
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
    const uint64_t kiwi_1 = chess_game_perft(game, 1);
    const uint64_t kiwi_2 = chess_game_perft(game, 2);
    const uint64_t kiwi_3 = chess_game_perft(game, 3);
    EXPECT(kiwi_1 == 48);
    EXPECT(kiwi_2 == 2039);
    EXPECT(kiwi_3 == 97862);
    chess_game_destroy(game);

    game = game_from_fen(
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1");
    EXPECT(chess_game_perft(game, 1) == 14);
    EXPECT(chess_game_perft(game, 2) == 191);
    EXPECT(chess_game_perft(game, 3) == 2812);
    chess_game_destroy(game);
}

int main(void)
{
    test_initial_and_basic_moves();
    test_piece_movement_and_blocking();
    test_check_pin_block_and_king_safety();
    test_castling();
    test_en_passant();
    test_promotions();
    test_terminal_positions();
    test_draw_rules();
    test_resignation();
    test_ai_difficulties();
    test_perft();

    if (s_failures != 0) {
        fprintf(stderr, "%d chess test expectation(s) failed\n", s_failures);
        return 1;
    }
    puts("all chess rule and perft tests passed");
    return 0;
}
