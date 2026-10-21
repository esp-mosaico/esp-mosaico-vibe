// SPDX-License-Identifier: Apache-2.0

#include "chess.h"

#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define CHESS_MAX_MOVES 256
#define CHESS_MAX_HISTORY 1024

enum {
    CASTLE_WHITE_KING = 1,
    CASTLE_WHITE_QUEEN = 2,
    CASTLE_BLACK_KING = 4,
    CASTLE_BLACK_QUEEN = 8,
};

typedef struct {
    uint8_t board[64];
    uint8_t side;
    uint8_t castling;
    int8_t en_passant;
    uint16_t halfmove;
    uint16_t fullmove;
    chess_end_reason_t end_reason;
    int8_t winner;
} chess_position_t;

struct chess_game {
    chess_position_t position;
    uint64_t history[CHESS_MAX_HISTORY];
    uint16_t history_count;
};

typedef struct {
    chess_move_t *moves;
    chess_position_t *positions;
    unsigned max_depth;
} perft_workspace_t;

typedef struct {
    chess_move_t *moves;
    chess_position_t *positions;
    unsigned max_depth;
    chess_color_t root_color;
} ai_workspace_t;

static const int8_t s_knight_steps[8][2] = {
    {1, 2}, {2, 1}, {2, -1}, {1, -2},
    {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2},
};
static const int8_t s_king_steps[8][2] = {
    {1, 0}, {1, 1}, {0, 1}, {-1, 1},
    {-1, 0}, {-1, -1}, {0, -1}, {1, -1},
};
static const int8_t s_bishop_steps[4][2] = {
    {1, 1}, {1, -1}, {-1, 1}, {-1, -1},
};
static const int8_t s_rook_steps[4][2] = {
    {1, 0}, {-1, 0}, {0, 1}, {0, -1},
};

static int square_at(int file, int rank)
{
    return file >= 0 && file < 8 && rank >= 0 && rank < 8
               ? file + rank * 8
               : -1;
}

static int square_file(int square)
{
    return square & 7;
}

static int square_rank(int square)
{
    return square >> 3;
}

uint8_t chess_piece_make(chess_color_t color, chess_piece_type_t type)
{
    if (type < CHESS_PIECE_PAWN || type > CHESS_PIECE_KING) {
        return 0;
    }
    return (uint8_t)type + (color == CHESS_BLACK ? 8 : 0);
}

chess_color_t chess_piece_color(uint8_t piece)
{
    return piece >= 9 ? CHESS_BLACK : CHESS_WHITE;
}

chess_piece_type_t chess_piece_type(uint8_t piece)
{
    if (piece >= 9) {
        piece = (uint8_t)(piece - 8);
    }
    return piece <= CHESS_PIECE_KING ? (chess_piece_type_t)piece
                                     : CHESS_PIECE_NONE;
}

static int find_king(const chess_position_t *position, chess_color_t color)
{
    const uint8_t king = chess_piece_make(color, CHESS_PIECE_KING);
    for (int square = 0; square < 64; ++square) {
        if (position->board[square] == king) {
            return square;
        }
    }
    return -1;
}

static bool ray_attacks(const chess_position_t *position,
                        int from,
                        int to,
                        const int8_t (*steps)[2],
                        size_t step_count)
{
    const int from_file = square_file(from);
    const int from_rank = square_rank(from);
    for (size_t direction = 0; direction < step_count; ++direction) {
        for (int distance = 1; distance < 8; ++distance) {
            const int square = square_at(
                from_file + steps[direction][0] * distance,
                from_rank + steps[direction][1] * distance);
            if (square < 0) {
                break;
            }
            if (square == to) {
                return true;
            }
            if (position->board[square] != 0) {
                break;
            }
        }
    }
    return false;
}

static bool piece_attacks(const chess_position_t *position, int from, int to)
{
    const uint8_t piece = position->board[from];
    const chess_piece_type_t type = chess_piece_type(piece);
    const int file_delta = square_file(to) - square_file(from);
    const int rank_delta = square_rank(to) - square_rank(from);
    const int abs_file = file_delta < 0 ? -file_delta : file_delta;
    const int abs_rank = rank_delta < 0 ? -rank_delta : rank_delta;

    switch (type) {
    case CHESS_PIECE_PAWN:
        return abs_file == 1 &&
               rank_delta == (chess_piece_color(piece) == CHESS_WHITE ? 1 : -1);
    case CHESS_PIECE_KNIGHT:
        return (abs_file == 1 && abs_rank == 2) ||
               (abs_file == 2 && abs_rank == 1);
    case CHESS_PIECE_BISHOP:
        return ray_attacks(position, from, to, s_bishop_steps, 4);
    case CHESS_PIECE_ROOK:
        return ray_attacks(position, from, to, s_rook_steps, 4);
    case CHESS_PIECE_QUEEN:
        return ray_attacks(position, from, to, s_bishop_steps, 4) ||
               ray_attacks(position, from, to, s_rook_steps, 4);
    case CHESS_PIECE_KING:
        return abs_file <= 1 && abs_rank <= 1 && (abs_file + abs_rank) != 0;
    default:
        return false;
    }
}

static bool square_attacked(const chess_position_t *position,
                            int square,
                            chess_color_t by_color)
{
    for (int from = 0; from < 64; ++from) {
        const uint8_t piece = position->board[from];
        if (piece != 0 && chess_piece_color(piece) == by_color &&
            piece_attacks(position, from, square)) {
            return true;
        }
    }
    return false;
}

static bool position_in_check(const chess_position_t *position,
                              chess_color_t color)
{
    const int king = find_king(position, color);
    return king < 0 || square_attacked(position, king,
                                      color == CHESS_WHITE ? CHESS_BLACK
                                                           : CHESS_WHITE);
}

static void append_move(chess_move_t *moves,
                        size_t capacity,
                        size_t *count,
                        int from,
                        int to,
                        chess_piece_type_t promotion)
{
    if (*count < capacity) {
        moves[*count] = (chess_move_t) {
            .from = (uint8_t)from,
            .to = (uint8_t)to,
            .promotion = promotion,
        };
    }
    ++*count;
}

static void append_pawn_move(chess_move_t *moves,
                             size_t capacity,
                             size_t *count,
                             int from,
                             int to)
{
    const int target_rank = square_rank(to);
    if (target_rank == 0 || target_rank == 7) {
        append_move(moves, capacity, count, from, to, CHESS_PIECE_QUEEN);
        append_move(moves, capacity, count, from, to, CHESS_PIECE_ROOK);
        append_move(moves, capacity, count, from, to, CHESS_PIECE_BISHOP);
        append_move(moves, capacity, count, from, to, CHESS_PIECE_KNIGHT);
    } else {
        append_move(moves, capacity, count, from, to, CHESS_PIECE_NONE);
    }
}

static void generate_ray_moves(const chess_position_t *position,
                               int from,
                               const int8_t (*steps)[2],
                               size_t step_count,
                               chess_move_t *moves,
                               size_t capacity,
                               size_t *count)
{
    const chess_color_t color = chess_piece_color(position->board[from]);
    const int from_file = square_file(from);
    const int from_rank = square_rank(from);
    for (size_t direction = 0; direction < step_count; ++direction) {
        for (int distance = 1; distance < 8; ++distance) {
            const int to = square_at(from_file + steps[direction][0] * distance,
                                     from_rank + steps[direction][1] * distance);
            if (to < 0) {
                break;
            }
            const uint8_t target = position->board[to];
            if (target != 0 && chess_piece_color(target) == color) {
                break;
            }
            if (chess_piece_type(target) != CHESS_PIECE_KING) {
                append_move(moves, capacity, count, from, to,
                            CHESS_PIECE_NONE);
            }
            if (target != 0) {
                break;
            }
        }
    }
}

static bool castle_allowed(const chess_position_t *position,
                           chess_color_t color,
                           bool king_side)
{
    const uint8_t right = color == CHESS_WHITE
                              ? (king_side ? CASTLE_WHITE_KING
                                           : CASTLE_WHITE_QUEEN)
                              : (king_side ? CASTLE_BLACK_KING
                                           : CASTLE_BLACK_QUEEN);
    if ((position->castling & right) == 0 || position_in_check(position, color)) {
        return false;
    }
    const int rank = color == CHESS_WHITE ? 0 : 7;
    const int king = square_at(4, rank);
    const int rook = square_at(king_side ? 7 : 0, rank);
    if (position->board[king] != chess_piece_make(color, CHESS_PIECE_KING) ||
        position->board[rook] != chess_piece_make(color, CHESS_PIECE_ROOK)) {
        return false;
    }
    const int direction = king_side ? 1 : -1;
    for (int square = king + direction; square != rook; square += direction) {
        if (position->board[square] != 0) {
            return false;
        }
    }
    const chess_color_t opponent =
        color == CHESS_WHITE ? CHESS_BLACK : CHESS_WHITE;
    return !square_attacked(position, king + direction, opponent) &&
           !square_attacked(position, king + 2 * direction, opponent);
}

static size_t generate_pseudo_from(const chess_position_t *position,
                                   int from,
                                   chess_move_t *moves,
                                   size_t capacity)
{
    if (from < 0 || from >= 64 || position->board[from] == 0) {
        return 0;
    }
    const uint8_t piece = position->board[from];
    const chess_color_t color = chess_piece_color(piece);
    const chess_piece_type_t type = chess_piece_type(piece);
    const int file = square_file(from);
    const int rank = square_rank(from);
    size_t count = 0;

    if (type == CHESS_PIECE_PAWN) {
        const int direction = color == CHESS_WHITE ? 1 : -1;
        const int start_rank = color == CHESS_WHITE ? 1 : 6;
        const int one = square_at(file, rank + direction);
        if (one >= 0 && position->board[one] == 0) {
            append_pawn_move(moves, capacity, &count, from, one);
            const int two = square_at(file, rank + 2 * direction);
            if (rank == start_rank && two >= 0 && position->board[two] == 0) {
                append_move(moves, capacity, &count, from, two,
                            CHESS_PIECE_NONE);
            }
        }
        for (int file_step = -1; file_step <= 1; file_step += 2) {
            const int to = square_at(file + file_step, rank + direction);
            if (to < 0) {
                continue;
            }
            const uint8_t target = position->board[to];
            const bool normal_capture =
                target != 0 && chess_piece_color(target) != color &&
                chess_piece_type(target) != CHESS_PIECE_KING;
            const int en_passant_pawn =
                to + (color == CHESS_WHITE ? -8 : 8);
            const bool en_passant_capture =
                to == position->en_passant && target == 0 &&
                en_passant_pawn >= 0 && en_passant_pawn < 64 &&
                position->board[en_passant_pawn] ==
                    chess_piece_make(
                        color == CHESS_WHITE ? CHESS_BLACK : CHESS_WHITE,
                        CHESS_PIECE_PAWN);
            if (normal_capture || en_passant_capture) {
                append_pawn_move(moves, capacity, &count, from, to);
            }
        }
        return count;
    }

    if (type == CHESS_PIECE_KNIGHT || type == CHESS_PIECE_KING) {
        const int8_t (*steps)[2] =
            type == CHESS_PIECE_KNIGHT ? s_knight_steps : s_king_steps;
        const size_t step_count = 8;
        for (size_t index = 0; index < step_count; ++index) {
            const int to = square_at(file + steps[index][0],
                                     rank + steps[index][1]);
            if (to < 0) {
                continue;
            }
            const uint8_t target = position->board[to];
            if ((target == 0 || chess_piece_color(target) != color) &&
                chess_piece_type(target) != CHESS_PIECE_KING) {
                append_move(moves, capacity, &count, from, to,
                            CHESS_PIECE_NONE);
            }
        }
        if (type == CHESS_PIECE_KING) {
            if (castle_allowed(position, color, true)) {
                append_move(moves, capacity, &count, from,
                            square_at(6, color == CHESS_WHITE ? 0 : 7),
                            CHESS_PIECE_NONE);
            }
            if (castle_allowed(position, color, false)) {
                append_move(moves, capacity, &count, from,
                            square_at(2, color == CHESS_WHITE ? 0 : 7),
                            CHESS_PIECE_NONE);
            }
        }
        return count;
    }

    if (type == CHESS_PIECE_BISHOP || type == CHESS_PIECE_QUEEN) {
        generate_ray_moves(position, from, s_bishop_steps, 4, moves, capacity,
                           &count);
    }
    if (type == CHESS_PIECE_ROOK || type == CHESS_PIECE_QUEEN) {
        generate_ray_moves(position, from, s_rook_steps, 4, moves, capacity,
                           &count);
    }
    return count;
}

static void clear_castling_right(chess_position_t *position, int square)
{
    switch (square) {
    case 4:
        position->castling &= (uint8_t)~(CASTLE_WHITE_KING |
                                         CASTLE_WHITE_QUEEN);
        break;
    case 0:
        position->castling &= (uint8_t)~CASTLE_WHITE_QUEEN;
        break;
    case 7:
        position->castling &= (uint8_t)~CASTLE_WHITE_KING;
        break;
    case 60:
        position->castling &= (uint8_t)~(CASTLE_BLACK_KING |
                                         CASTLE_BLACK_QUEEN);
        break;
    case 56:
        position->castling &= (uint8_t)~CASTLE_BLACK_QUEEN;
        break;
    case 63:
        position->castling &= (uint8_t)~CASTLE_BLACK_KING;
        break;
    default:
        break;
    }
}

static void apply_to_position(chess_position_t *position,
                              const chess_move_t *move)
{
    const uint8_t piece = position->board[move->from];
    const chess_color_t color = chess_piece_color(piece);
    const chess_piece_type_t type = chess_piece_type(piece);
    const bool pawn_move = type == CHESS_PIECE_PAWN;
    bool capture = position->board[move->to] != 0;

    clear_castling_right(position, move->from);
    clear_castling_right(position, move->to);

    if (pawn_move && move->to == position->en_passant &&
        position->board[move->to] == 0) {
        const int captured = move->to + (color == CHESS_WHITE ? -8 : 8);
        position->board[captured] = 0;
        capture = true;
    }

    position->board[move->to] = piece;
    position->board[move->from] = 0;

    if (type == CHESS_PIECE_KING &&
        abs(square_file(move->to) - square_file(move->from)) == 2) {
        const bool king_side = move->to > move->from;
        const int rook_from = square_at(king_side ? 7 : 0,
                                        square_rank(move->from));
        const int rook_to = square_at(king_side ? 5 : 3,
                                      square_rank(move->from));
        position->board[rook_to] = position->board[rook_from];
        position->board[rook_from] = 0;
    }

    if (pawn_move && (square_rank(move->to) == 0 ||
                      square_rank(move->to) == 7)) {
        position->board[move->to] = chess_piece_make(color, move->promotion);
    }

    position->en_passant = -1;
    if (pawn_move &&
        abs(square_rank(move->to) - square_rank(move->from)) == 2) {
        position->en_passant =
            (int8_t)((move->from + move->to) / 2);
    }
    position->halfmove = pawn_move || capture
                             ? 0
                             : (uint16_t)(position->halfmove + 1);
    if (color == CHESS_BLACK) {
        ++position->fullmove;
    }
    position->side = color == CHESS_WHITE ? CHESS_BLACK : CHESS_WHITE;
}

static bool legal_on_position(const chess_position_t *position,
                              const chess_move_t *move)
{
    chess_position_t trial = *position;
    const chess_color_t moving_color =
        chess_piece_color(position->board[move->from]);
    apply_to_position(&trial, move);
    return !position_in_check(&trial, moving_color);
}

static size_t generate_legal_from_position(const chess_position_t *position,
                                           int from,
                                           chess_move_t *moves,
                                           size_t capacity)
{
    if (from < 0 || from >= 64 || position->board[from] == 0 ||
        chess_piece_color(position->board[from]) != position->side) {
        return 0;
    }
    chess_move_t pseudo[32];
    const size_t pseudo_count =
        generate_pseudo_from(position, from, pseudo,
                             sizeof(pseudo) / sizeof(pseudo[0]));
    size_t count = 0;
    for (size_t index = 0; index < pseudo_count; ++index) {
        if (legal_on_position(position, &pseudo[index])) {
            if (count < capacity) {
                moves[count] = pseudo[index];
            }
            ++count;
        }
    }
    return count;
}

static size_t generate_legal_position(const chess_position_t *position,
                                      chess_move_t *moves,
                                      size_t capacity)
{
    size_t count = 0;
    for (int from = 0; from < 64; ++from) {
        chess_move_t from_moves[32];
        const size_t from_count =
            generate_legal_from_position(position, from, from_moves,
                                         sizeof(from_moves) /
                                             sizeof(from_moves[0]));
        for (size_t index = 0; index < from_count; ++index) {
            if (count < capacity) {
                moves[count] = from_moves[index];
            }
            ++count;
        }
    }
    return count;
}

static uint64_t position_hash(const chess_position_t *position)
{
    uint64_t hash = 1469598103934665603ULL;
    for (int square = 0; square < 64; ++square) {
        hash ^= (uint64_t)position->board[square] +
                ((uint64_t)square << 8);
        hash *= 1099511628211ULL;
    }
    hash ^= position->side;
    hash *= 1099511628211ULL;
    hash ^= position->castling;
    hash *= 1099511628211ULL;
    int effective_en_passant = -1;
    if (position->en_passant >= 0) {
        const int target = position->en_passant;
        const chess_color_t side = (chess_color_t)position->side;
        const int captured =
            target + (side == CHESS_WHITE ? -8 : 8);
        const bool capturable_pawn =
            captured >= 0 && captured < 64 &&
            position->board[captured] ==
                chess_piece_make(
                    side == CHESS_WHITE ? CHESS_BLACK : CHESS_WHITE,
                    CHESS_PIECE_PAWN);
        const int pawn_rank =
            square_rank(target) +
            (position->side == CHESS_WHITE ? -1 : 1);
        for (int file_step = -1;
             capturable_pawn && file_step <= 1;
             file_step += 2) {
            const int from =
                square_at(square_file(target) + file_step, pawn_rank);
            if (from < 0 ||
                position->board[from] != chess_piece_make(
                    (chess_color_t)position->side, CHESS_PIECE_PAWN)) {
                continue;
            }
            const chess_move_t move = {
                .from = (uint8_t)from,
                .to = (uint8_t)target,
                .promotion = CHESS_PIECE_NONE,
            };
            if (legal_on_position(position, &move)) {
                effective_en_passant = target;
                break;
            }
        }
    }
    hash ^= (uint64_t)(effective_en_passant + 1);
    return hash * 1099511628211ULL;
}

static bool insufficient_material(const chess_position_t *position)
{
    int bishops = 0;
    int knights = 0;
    int bishop_color = -1;
    for (int square = 0; square < 64; ++square) {
        const chess_piece_type_t type =
            chess_piece_type(position->board[square]);
        if (type == CHESS_PIECE_NONE || type == CHESS_PIECE_KING) {
            continue;
        }
        if (type == CHESS_PIECE_PAWN || type == CHESS_PIECE_ROOK ||
            type == CHESS_PIECE_QUEEN) {
            return false;
        }
        if (type == CHESS_PIECE_KNIGHT) {
            ++knights;
        } else if (type == CHESS_PIECE_BISHOP) {
            const int color = (square_file(square) + square_rank(square)) & 1;
            if (bishop_color >= 0 && bishop_color != color) {
                return false;
            }
            bishop_color = color;
            ++bishops;
        }
    }
    if (bishops == 0) {
        return knights <= 1;
    }
    return knights == 0 && bishop_color >= 0;
}

static int repetition_count(const chess_game_t *game, uint64_t hash)
{
    int count = 0;
    for (uint16_t index = 0; index < game->history_count; ++index) {
        if (game->history[index] == hash) {
            ++count;
        }
    }
    return count;
}

static void update_terminal_status(chess_game_t *game)
{
    chess_position_t *position = &game->position;
    chess_move_t one_move;
    if (generate_legal_position(position, &one_move, 1) == 0) {
        if (position_in_check(position, (chess_color_t)position->side)) {
            position->end_reason = CHESS_END_CHECKMATE;
            position->winner =
                position->side == CHESS_WHITE ? CHESS_BLACK : CHESS_WHITE;
        } else {
            position->end_reason = CHESS_END_STALEMATE;
            position->winner = -1;
        }
    } else if (insufficient_material(position)) {
        position->end_reason = CHESS_END_INSUFFICIENT_MATERIAL;
        position->winner = -1;
    } else if (position->halfmove >= 100) {
        position->end_reason = CHESS_END_FIFTY_MOVE_RULE;
        position->winner = -1;
    } else if (repetition_count(game, position_hash(position)) >= 3) {
        position->end_reason = CHESS_END_THREEFOLD_REPETITION;
        position->winner = -1;
    }
}

static void reset_position(chess_position_t *position)
{
    static const uint8_t back_rank[8] = {
        CHESS_PIECE_ROOK, CHESS_PIECE_KNIGHT, CHESS_PIECE_BISHOP,
        CHESS_PIECE_QUEEN, CHESS_PIECE_KING, CHESS_PIECE_BISHOP,
        CHESS_PIECE_KNIGHT, CHESS_PIECE_ROOK,
    };
    memset(position, 0, sizeof(*position));
    for (int file = 0; file < 8; ++file) {
        position->board[file] =
            chess_piece_make(CHESS_WHITE,
                             (chess_piece_type_t)back_rank[file]);
        position->board[8 + file] =
            chess_piece_make(CHESS_WHITE, CHESS_PIECE_PAWN);
        position->board[48 + file] =
            chess_piece_make(CHESS_BLACK, CHESS_PIECE_PAWN);
        position->board[56 + file] =
            chess_piece_make(CHESS_BLACK,
                             (chess_piece_type_t)back_rank[file]);
    }
    position->side = CHESS_WHITE;
    position->castling = CASTLE_WHITE_KING | CASTLE_WHITE_QUEEN |
                         CASTLE_BLACK_KING | CASTLE_BLACK_QUEEN;
    position->en_passant = -1;
    position->fullmove = 1;
    position->winner = -1;
}

chess_err_t chess_game_create(chess_game_t **out_game)
{
    if (out_game == NULL) {
        return CHESS_ERR_INVALID_ARG;
    }
    chess_game_t *game = calloc(1, sizeof(*game));
    if (game == NULL) {
        return CHESS_ERR_NO_MEMORY;
    }
    chess_game_reset(game);
    *out_game = game;
    return CHESS_OK;
}

void chess_game_destroy(chess_game_t *game)
{
    free(game);
}

chess_err_t chess_game_clone(const chess_game_t *game,
                             chess_game_t **out_clone)
{
    if (game == NULL || out_clone == NULL) {
        return CHESS_ERR_INVALID_ARG;
    }
    chess_game_t *clone = malloc(sizeof(*clone));
    if (clone == NULL) {
        return CHESS_ERR_NO_MEMORY;
    }
    *clone = *game;
    *out_clone = clone;
    return CHESS_OK;
}

void chess_game_reset(chess_game_t *game)
{
    if (game == NULL) {
        return;
    }
    reset_position(&game->position);
    game->history_count = 1;
    game->history[0] = position_hash(&game->position);
}

static uint8_t fen_piece(char value)
{
    chess_color_t color = isupper((unsigned char)value) ? CHESS_WHITE
                                                        : CHESS_BLACK;
    switch (tolower((unsigned char)value)) {
    case 'p':
        return chess_piece_make(color, CHESS_PIECE_PAWN);
    case 'n':
        return chess_piece_make(color, CHESS_PIECE_KNIGHT);
    case 'b':
        return chess_piece_make(color, CHESS_PIECE_BISHOP);
    case 'r':
        return chess_piece_make(color, CHESS_PIECE_ROOK);
    case 'q':
        return chess_piece_make(color, CHESS_PIECE_QUEEN);
    case 'k':
        return chess_piece_make(color, CHESS_PIECE_KING);
    default:
        return 0;
    }
}

chess_err_t chess_game_load_fen(chess_game_t *game, const char *fen)
{
    if (game == NULL || fen == NULL) {
        return CHESS_ERR_INVALID_ARG;
    }
    chess_position_t position = {
        .en_passant = -1,
        .fullmove = 1,
        .winner = -1,
    };
    int rank = 7;
    int file = 0;
    const char *cursor = fen;
    while (*cursor != '\0' && *cursor != ' ') {
        if (*cursor == '/') {
            if (file != 8 || rank == 0) {
                return CHESS_ERR_INVALID_POSITION;
            }
            --rank;
            file = 0;
        } else if (isdigit((unsigned char)*cursor)) {
            file += *cursor - '0';
        } else {
            const uint8_t piece = fen_piece(*cursor);
            if (piece == 0 || file >= 8) {
                return CHESS_ERR_INVALID_POSITION;
            }
            position.board[square_at(file++, rank)] = piece;
        }
        if (file > 8) {
            return CHESS_ERR_INVALID_POSITION;
        }
        ++cursor;
    }
    if (rank != 0 || file != 8 || *cursor++ != ' ') {
        return CHESS_ERR_INVALID_POSITION;
    }
    if (*cursor == 'w') {
        position.side = CHESS_WHITE;
    } else if (*cursor == 'b') {
        position.side = CHESS_BLACK;
    } else {
        return CHESS_ERR_INVALID_POSITION;
    }
    if (*++cursor != ' ') {
        return CHESS_ERR_INVALID_POSITION;
    }
    ++cursor;
    if (*cursor == '-') {
        ++cursor;
    } else {
        while (*cursor != '\0' && *cursor != ' ') {
            switch (*cursor++) {
            case 'K':
                position.castling |= CASTLE_WHITE_KING;
                break;
            case 'Q':
                position.castling |= CASTLE_WHITE_QUEEN;
                break;
            case 'k':
                position.castling |= CASTLE_BLACK_KING;
                break;
            case 'q':
                position.castling |= CASTLE_BLACK_QUEEN;
                break;
            default:
                return CHESS_ERR_INVALID_POSITION;
            }
        }
    }
    if (*cursor++ != ' ') {
        return CHESS_ERR_INVALID_POSITION;
    }
    if (*cursor == '-') {
        ++cursor;
    } else {
        if (cursor[0] < 'a' || cursor[0] > 'h' ||
            cursor[1] < '1' || cursor[1] > '8') {
            return CHESS_ERR_INVALID_POSITION;
        }
        position.en_passant =
            (int8_t)square_at(cursor[0] - 'a', cursor[1] - '1');
        cursor += 2;
    }
    if (*cursor++ != ' ') {
        return CHESS_ERR_INVALID_POSITION;
    }
    char *end = NULL;
    const unsigned long halfmove = strtoul(cursor, &end, 10);
    if (end == cursor || *end != ' ' || halfmove > UINT16_MAX) {
        return CHESS_ERR_INVALID_POSITION;
    }
    position.halfmove = (uint16_t)halfmove;
    cursor = end + 1;
    const unsigned long fullmove = strtoul(cursor, &end, 10);
    if (end == cursor || (*end != '\0' && *end != '\n') ||
        fullmove == 0 || fullmove > UINT16_MAX) {
        return CHESS_ERR_INVALID_POSITION;
    }
    position.fullmove = (uint16_t)fullmove;

    if (find_king(&position, CHESS_WHITE) < 0 ||
        find_king(&position, CHESS_BLACK) < 0) {
        return CHESS_ERR_INVALID_POSITION;
    }
    game->position = position;
    game->history_count = 1;
    game->history[0] = position_hash(&game->position);
    update_terminal_status(game);
    return CHESS_OK;
}

uint8_t chess_game_piece_at(const chess_game_t *game, int square)
{
    return game != NULL && square >= 0 && square < 64
               ? game->position.board[square]
               : 0;
}

chess_color_t chess_game_side_to_move(const chess_game_t *game)
{
    return game != NULL ? (chess_color_t)game->position.side : CHESS_WHITE;
}

bool chess_game_in_check(const chess_game_t *game, chess_color_t color)
{
    return game == NULL || position_in_check(&game->position, color);
}

chess_status_t chess_game_status(const chess_game_t *game)
{
    chess_status_t status = {
        .side_to_move = CHESS_WHITE,
        .end_reason = CHESS_END_NONE,
        .winner = -1,
        .en_passant_square = -1,
        .fullmove_number = 1,
    };
    if (game != NULL) {
        status.side_to_move = (chess_color_t)game->position.side;
        status.end_reason = game->position.end_reason;
        status.winner = game->position.winner;
        status.in_check =
            position_in_check(&game->position, status.side_to_move);
        status.en_passant_square = game->position.en_passant;
        status.halfmove_clock = game->position.halfmove;
        status.fullmove_number = game->position.fullmove;
    }
    return status;
}

uint64_t chess_game_position_hash(const chess_game_t *game)
{
    return game != NULL ? position_hash(&game->position) : 0;
}

size_t chess_game_generate_legal(const chess_game_t *game,
                                 chess_move_t *moves,
                                 size_t capacity)
{
    if (game == NULL || (moves == NULL && capacity != 0) ||
        game->position.end_reason != CHESS_END_NONE) {
        return 0;
    }
    return generate_legal_position(&game->position, moves, capacity);
}

size_t chess_game_generate_legal_from(const chess_game_t *game,
                                      int from,
                                      chess_move_t *moves,
                                      size_t capacity)
{
    if (game == NULL || (moves == NULL && capacity != 0) ||
        game->position.end_reason != CHESS_END_NONE) {
        return 0;
    }
    return generate_legal_from_position(&game->position, from, moves, capacity);
}

bool chess_game_is_legal(const chess_game_t *game, const chess_move_t *move)
{
    if (game == NULL || move == NULL || move->from >= 64 || move->to >= 64 ||
        game->position.end_reason != CHESS_END_NONE) {
        return false;
    }
    chess_move_t legal[32];
    const size_t count =
        generate_legal_from_position(&game->position, move->from, legal,
                                     sizeof(legal) / sizeof(legal[0]));
    for (size_t index = 0; index < count; ++index) {
        if (legal[index].from == move->from &&
            legal[index].to == move->to &&
            legal[index].promotion == move->promotion) {
            return true;
        }
    }
    return false;
}

chess_err_t chess_game_apply(chess_game_t *game, const chess_move_t *move)
{
    if (game == NULL || move == NULL) {
        return CHESS_ERR_INVALID_ARG;
    }
    if (game->position.end_reason != CHESS_END_NONE) {
        return CHESS_ERR_GAME_OVER;
    }
    if (!chess_game_is_legal(game, move)) {
        return CHESS_ERR_ILLEGAL_MOVE;
    }
    apply_to_position(&game->position, move);
    if (game->history_count < CHESS_MAX_HISTORY) {
        game->history[game->history_count++] =
            position_hash(&game->position);
    } else {
        memmove(game->history, game->history + 1,
                (CHESS_MAX_HISTORY - 1) * sizeof(game->history[0]));
        game->history[CHESS_MAX_HISTORY - 1] =
            position_hash(&game->position);
    }
    update_terminal_status(game);
    return CHESS_OK;
}

chess_err_t chess_game_resign(chess_game_t *game, chess_color_t color)
{
    if (game == NULL || (color != CHESS_WHITE && color != CHESS_BLACK)) {
        return CHESS_ERR_INVALID_ARG;
    }
    if (game->position.end_reason != CHESS_END_NONE) {
        return CHESS_ERR_GAME_OVER;
    }
    game->position.end_reason = CHESS_END_RESIGNATION;
    game->position.winner = color == CHESS_WHITE ? CHESS_BLACK : CHESS_WHITE;
    return CHESS_OK;
}

static int piece_value(uint8_t piece)
{
    switch (chess_piece_type(piece)) {
    case CHESS_PIECE_PAWN:
        return 100;
    case CHESS_PIECE_KNIGHT:
        return 320;
    case CHESS_PIECE_BISHOP:
        return 330;
    case CHESS_PIECE_ROOK:
        return 500;
    case CHESS_PIECE_QUEEN:
        return 900;
    case CHESS_PIECE_KING:
        return 20000;
    default:
        return 0;
    }
}

static int positional_value(const chess_position_t *position,
                            chess_color_t root_color)
{
    int score = 0;
    for (int square = 0; square < 64; ++square) {
        const uint8_t piece = position->board[square];
        if (piece == 0) {
            continue;
        }
        const chess_color_t color = chess_piece_color(piece);
        const chess_piece_type_t type = chess_piece_type(piece);
        int value = piece_value(piece);
        const int file = square_file(square);
        const int rank = square_rank(square);
        const int center_file = file < 4 ? file : 7 - file;
        const int center_rank = rank < 4 ? rank : 7 - rank;
        if (type == CHESS_PIECE_PAWN) {
            value += (color == CHESS_WHITE ? rank : 7 - rank) * 4;
        } else if (type == CHESS_PIECE_KNIGHT ||
                   type == CHESS_PIECE_BISHOP) {
            value += (center_file + center_rank) * 4;
        }
        score += color == root_color ? value : -value;
    }
    return score;
}

static int ai_search(const chess_position_t *position,
                     unsigned depth,
                     int alpha,
                     int beta,
                     ai_workspace_t *workspace,
                     unsigned ply)
{
    chess_move_t *moves = workspace->moves + ply * CHESS_MAX_MOVES;
    const size_t count =
        generate_legal_position(position, moves, CHESS_MAX_MOVES);
    if (count == 0) {
        if (!position_in_check(position, (chess_color_t)position->side)) {
            return 0;
        }
        return position->side == workspace->root_color
                   ? -1000000 + (int)ply
                   : 1000000 - (int)ply;
    }
    if (depth == 0 || insufficient_material(position) ||
        position->halfmove >= 100) {
        return positional_value(position, workspace->root_color);
    }

    const bool maximize = position->side == workspace->root_color;
    int best = maximize ? INT_MIN : INT_MAX;
    for (size_t index = 0; index < count && index < CHESS_MAX_MOVES; ++index) {
        chess_position_t *child = &workspace->positions[ply];
        *child = *position;
        apply_to_position(child, &moves[index]);
        const int score = ai_search(
            child, depth - 1, alpha, beta, workspace, ply + 1);
        if (maximize) {
            if (score > best) {
                best = score;
            }
            if (best > alpha) {
                alpha = best;
            }
        } else {
            if (score < best) {
                best = score;
            }
            if (best < beta) {
                beta = best;
            }
        }
        if (beta <= alpha) {
            break;
        }
    }
    return best;
}

static int easy_move_score(const chess_position_t *position,
                           const chess_move_t *move)
{
    int score = piece_value(position->board[move->to]);
    if (move->to == position->en_passant &&
        chess_piece_type(position->board[move->from]) == CHESS_PIECE_PAWN &&
        position->board[move->to] == 0) {
        score += piece_value(
            chess_piece_make(
                position->side == CHESS_WHITE ? CHESS_BLACK : CHESS_WHITE,
                CHESS_PIECE_PAWN));
    }
    if (move->promotion != CHESS_PIECE_NONE) {
        score += piece_value(
                     chess_piece_make(
                         (chess_color_t)position->side, move->promotion)) -
                 piece_value(position->board[move->from]);
    }
    chess_position_t trial = *position;
    apply_to_position(&trial, move);
    if (position_in_check(&trial, (chess_color_t)trial.side)) {
        score += 25;
    }
    return score;
}

bool chess_game_ai_pick_level(const chess_game_t *game,
                              chess_ai_level_t level,
                              chess_move_t *move)
{
    if (game == NULL || move == NULL ||
        level < CHESS_AI_EASY || level > CHESS_AI_HARD ||
        game->position.end_reason != CHESS_END_NONE) {
        return false;
    }
    const unsigned depth =
        level == CHESS_AI_EASY ? 1U :
        level == CHESS_AI_NORMAL ? 2U : 3U;
    ai_workspace_t workspace = {
        .moves = calloc((size_t)(depth + 1U) * CHESS_MAX_MOVES,
                        sizeof(chess_move_t)),
        .positions = calloc(depth + 1U, sizeof(chess_position_t)),
        .max_depth = depth + 1U,
        .root_color = (chess_color_t)game->position.side,
    };
    if (workspace.moves == NULL || workspace.positions == NULL) {
        free(workspace.moves);
        free(workspace.positions);
        return false;
    }

    chess_move_t *moves = workspace.moves;
    const size_t count =
        generate_legal_position(&game->position, moves, CHESS_MAX_MOVES);
    if (count == 0 || count > CHESS_MAX_MOVES) {
        free(workspace.moves);
        free(workspace.positions);
        return false;
    }
    size_t best_index = 0;
    int best_score = INT_MIN;
    for (size_t index = 0; index < count; ++index) {
        int score = easy_move_score(&game->position, &moves[index]);
        if (level != CHESS_AI_EASY) {
            chess_position_t *child = &workspace.positions[0];
            *child = game->position;
            apply_to_position(child, &moves[index]);
            score = ai_search(
                child, depth - 1, INT_MIN + 1, INT_MAX,
                &workspace, 1);
        }
        if (score > best_score) {
            best_score = score;
            best_index = index;
        }
    }
    *move = moves[best_index];
    free(workspace.moves);
    free(workspace.positions);
    return true;
}

bool chess_game_ai_pick(const chess_game_t *game, chess_move_t *move)
{
    return chess_game_ai_pick_level(game, CHESS_AI_NORMAL, move);
}

static uint64_t perft_position(const chess_position_t *position,
                               unsigned depth,
                               perft_workspace_t *workspace,
                               unsigned ply)
{
    if (depth == 0) {
        return 1;
    }
    chess_move_t *moves = workspace->moves + ply * CHESS_MAX_MOVES;
    const size_t count =
        generate_legal_position(position, moves, CHESS_MAX_MOVES);
    uint64_t nodes = 0;
    for (size_t index = 0; index < count && index < CHESS_MAX_MOVES; ++index) {
        chess_position_t *child = &workspace->positions[ply];
        *child = *position;
        apply_to_position(child, &moves[index]);
        nodes += perft_position(child, depth - 1, workspace, ply + 1);
    }
    return nodes;
}

uint64_t chess_game_perft(const chess_game_t *game, unsigned depth)
{
    if (game == NULL || depth == 0) {
        return game == NULL ? 0 : 1;
    }
    perft_workspace_t workspace = {
        .moves = calloc((size_t)depth * CHESS_MAX_MOVES,
                        sizeof(chess_move_t)),
        .positions = calloc(depth, sizeof(chess_position_t)),
        .max_depth = depth,
    };
    if (workspace.moves == NULL || workspace.positions == NULL) {
        free(workspace.moves);
        free(workspace.positions);
        return 0;
    }
    const uint64_t nodes =
        perft_position(&game->position, depth, &workspace, 0);
    free(workspace.moves);
    free(workspace.positions);
    return nodes;
}
