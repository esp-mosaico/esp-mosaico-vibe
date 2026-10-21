// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "chess.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct game_session game_session_t;

typedef enum {
    GAME_MODE_PVE = 0,
    GAME_MODE_PVP,
} game_mode_t;

typedef enum {
    GAME_PHASE_ACTIVE = 0,
    GAME_PHASE_AI_THINKING,
    GAME_PHASE_WAITING_OPPONENT,
    GAME_PHASE_DISCONNECTED,
    GAME_PHASE_SYNC_ERROR,
    GAME_PHASE_FINISHED,
} game_phase_t;

typedef enum {
    GAME_SESSION_OK = 0,
    GAME_SESSION_INVALID_ARG,
    GAME_SESSION_NO_MEMORY,
    GAME_SESSION_NOT_ALLOWED,
    GAME_SESSION_STALE_RESULT,
    GAME_SESSION_RULE_ERROR,
} game_session_err_t;

typedef struct {
    game_mode_t mode;
    game_phase_t phase;
    chess_ai_level_t ai_level;
    chess_color_t local_color;
    uint32_t generation;
    bool connected;
    chess_status_t chess;
} game_session_status_t;

game_session_err_t game_session_create(game_session_t **out_session);
void game_session_destroy(game_session_t *session);
game_session_err_t game_session_start(game_session_t *session,
                                      game_mode_t mode,
                                      chess_color_t local_color);
game_session_err_t game_session_start_with_difficulty(
    game_session_t *session,
    game_mode_t mode,
    chess_color_t local_color,
    chess_ai_level_t ai_level);
game_session_err_t game_session_load_fen(game_session_t *session,
                                         const char *fen);

game_session_status_t game_session_status(const game_session_t *session);
const chess_game_t *game_session_game(const game_session_t *session);

game_session_err_t game_session_apply_local(game_session_t *session,
                                            const chess_move_t *move);
game_session_err_t game_session_apply_remote(game_session_t *session,
                                             const chess_move_t *move);
game_session_err_t game_session_apply_synced(game_session_t *session,
                                             const chess_move_t *move);
game_session_err_t game_session_compute_ai(const game_session_t *session,
                                           uint32_t *generation,
                                           chess_move_t *move);
game_session_err_t game_session_commit_ai(game_session_t *session,
                                          uint32_t generation,
                                          const chess_move_t *move);

game_session_err_t game_session_resign(game_session_t *session);
game_session_err_t game_session_resign_color(game_session_t *session,
                                             chess_color_t color);

void game_session_set_connected(game_session_t *session, bool connected);
void game_session_set_sync_error(game_session_t *session);
