// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct chess_game chess_game_t;

typedef enum {
    CHESS_BLACK = 0,
    CHESS_WHITE = 1,
} chess_color_t;

typedef enum {
    CHESS_PIECE_NONE = 0,
    CHESS_PIECE_PAWN = 1,
    CHESS_PIECE_KNIGHT,
    CHESS_PIECE_BISHOP,
    CHESS_PIECE_ROOK,
    CHESS_PIECE_QUEEN,
    CHESS_PIECE_KING,
} chess_piece_type_t;

typedef enum {
    CHESS_END_NONE = 0,
    CHESS_END_CHECKMATE,
    CHESS_END_RESIGNATION,
    CHESS_END_STALEMATE,
    CHESS_END_THREEFOLD_REPETITION,
    CHESS_END_FIFTY_MOVE_RULE,
    CHESS_END_INSUFFICIENT_MATERIAL,
} chess_end_reason_t;

typedef enum {
    CHESS_AI_EASY = 0,
    CHESS_AI_NORMAL,
    CHESS_AI_HARD,
} chess_ai_level_t;

typedef enum {
    CHESS_OK = 0,
    CHESS_ERR_INVALID_ARG,
    CHESS_ERR_NO_MEMORY,
    CHESS_ERR_ILLEGAL_MOVE,
    CHESS_ERR_GAME_OVER,
    CHESS_ERR_INVALID_POSITION,
    CHESS_ERR_CAPACITY,
} chess_err_t;

typedef struct {
    uint8_t from;
    uint8_t to;
    chess_piece_type_t promotion;
} chess_move_t;

typedef struct {
    chess_color_t side_to_move;
    chess_end_reason_t end_reason;
    int winner; /* CHESS_WHITE/CHESS_BLACK, or -1 for no winner. */
    bool in_check;
    int en_passant_square;
    uint16_t halfmove_clock;
    uint16_t fullmove_number;
} chess_status_t;

chess_err_t chess_game_create(chess_game_t **out_game);
void chess_game_destroy(chess_game_t *game);
chess_err_t chess_game_clone(const chess_game_t *game, chess_game_t **out_clone);
void chess_game_reset(chess_game_t *game);
chess_err_t chess_game_load_fen(chess_game_t *game, const char *fen);

uint8_t chess_game_piece_at(const chess_game_t *game, int square);
chess_color_t chess_game_side_to_move(const chess_game_t *game);
bool chess_game_in_check(const chess_game_t *game, chess_color_t color);
chess_status_t chess_game_status(const chess_game_t *game);
uint64_t chess_game_position_hash(const chess_game_t *game);

size_t chess_game_generate_legal(const chess_game_t *game,
                                 chess_move_t *moves,
                                 size_t capacity);
size_t chess_game_generate_legal_from(const chess_game_t *game,
                                      int from,
                                      chess_move_t *moves,
                                      size_t capacity);
bool chess_game_is_legal(const chess_game_t *game, const chess_move_t *move);
chess_err_t chess_game_apply(chess_game_t *game, const chess_move_t *move);
chess_err_t chess_game_resign(chess_game_t *game, chess_color_t color);

bool chess_game_ai_pick(const chess_game_t *game, chess_move_t *move);
bool chess_game_ai_pick_level(const chess_game_t *game,
                              chess_ai_level_t level,
                              chess_move_t *move);
uint64_t chess_game_perft(const chess_game_t *game, unsigned depth);

/* Piece encoding helpers used by the portable UI. */
uint8_t chess_piece_make(chess_color_t color, chess_piece_type_t type);
chess_color_t chess_piece_color(uint8_t piece);
chess_piece_type_t chess_piece_type(uint8_t piece);
