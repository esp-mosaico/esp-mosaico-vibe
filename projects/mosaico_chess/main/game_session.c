// SPDX-License-Identifier: Apache-2.0

#include "game_session.h"

#include <stdlib.h>

struct game_session {
    chess_game_t *game;
    game_mode_t mode;
    game_phase_t phase;
    chess_ai_level_t ai_level;
    chess_color_t local_color;
    uint32_t generation;
    bool connected;
    bool started;
};

static chess_color_t opposite(chess_color_t color)
{
    return color == CHESS_WHITE ? CHESS_BLACK : CHESS_WHITE;
}

static void refresh_phase(game_session_t *session)
{
    if (!session->started) {
        return;
    }
    if (chess_game_status(session->game).end_reason != CHESS_END_NONE) {
        session->phase = GAME_PHASE_FINISHED;
        return;
    }
    if (session->mode == GAME_MODE_PVP && !session->connected) {
        session->phase = GAME_PHASE_DISCONNECTED;
        return;
    }
    const bool local_turn =
        chess_game_side_to_move(session->game) == session->local_color;
    if (session->mode == GAME_MODE_PVE && !local_turn) {
        session->phase = GAME_PHASE_AI_THINKING;
    } else if (session->mode == GAME_MODE_PVP && !local_turn) {
        session->phase = GAME_PHASE_WAITING_OPPONENT;
    } else {
        session->phase = GAME_PHASE_ACTIVE;
    }
}

static game_session_err_t apply_move(game_session_t *session,
                                     const chess_move_t *move)
{
    const chess_err_t error = chess_game_apply(session->game, move);
    if (error == CHESS_ERR_GAME_OVER || error == CHESS_ERR_ILLEGAL_MOVE) {
        return GAME_SESSION_NOT_ALLOWED;
    }
    if (error != CHESS_OK) {
        return GAME_SESSION_RULE_ERROR;
    }
    refresh_phase(session);
    return GAME_SESSION_OK;
}

game_session_err_t game_session_create(game_session_t **out_session)
{
    if (out_session == NULL) {
        return GAME_SESSION_INVALID_ARG;
    }
    game_session_t *session = calloc(1, sizeof(*session));
    if (session == NULL) {
        return GAME_SESSION_NO_MEMORY;
    }
    if (chess_game_create(&session->game) != CHESS_OK) {
        free(session);
        return GAME_SESSION_NO_MEMORY;
    }
    session->connected = true;
    *out_session = session;
    return GAME_SESSION_OK;
}

void game_session_destroy(game_session_t *session)
{
    if (session == NULL) {
        return;
    }
    chess_game_destroy(session->game);
    free(session);
}

game_session_err_t game_session_start(game_session_t *session,
                                      game_mode_t mode,
                                      chess_color_t local_color)
{
    return game_session_start_with_difficulty(
        session, mode, local_color, CHESS_AI_NORMAL);
}

game_session_err_t game_session_start_with_difficulty(
    game_session_t *session,
    game_mode_t mode,
    chess_color_t local_color,
    chess_ai_level_t ai_level)
{
    if (session == NULL ||
        (mode != GAME_MODE_PVE && mode != GAME_MODE_PVP) ||
        (local_color != CHESS_WHITE && local_color != CHESS_BLACK) ||
        ai_level < CHESS_AI_EASY || ai_level > CHESS_AI_HARD) {
        return GAME_SESSION_INVALID_ARG;
    }
    if (mode == GAME_MODE_PVE && local_color != CHESS_WHITE) {
        return GAME_SESSION_NOT_ALLOWED;
    }
    ++session->generation;
    chess_game_reset(session->game);
    session->mode = mode;
    session->ai_level = ai_level;
    session->local_color = local_color;
    session->connected = true;
    session->started = true;
    refresh_phase(session);
    return GAME_SESSION_OK;
}

game_session_err_t game_session_load_fen(game_session_t *session,
                                         const char *fen)
{
    if (session == NULL || fen == NULL) {
        return GAME_SESSION_INVALID_ARG;
    }
    if (!session->started) {
        return GAME_SESSION_NOT_ALLOWED;
    }
    if (chess_game_load_fen(session->game, fen) != CHESS_OK) {
        return GAME_SESSION_RULE_ERROR;
    }
    ++session->generation;
    refresh_phase(session);
    return GAME_SESSION_OK;
}

game_session_status_t game_session_status(const game_session_t *session)
{
    game_session_status_t status = {
        .mode = GAME_MODE_PVE,
        .phase = GAME_PHASE_FINISHED,
        .ai_level = CHESS_AI_NORMAL,
        .local_color = CHESS_WHITE,
        .connected = false,
    };
    if (session != NULL) {
        status.mode = session->mode;
        status.phase = session->phase;
        status.ai_level = session->ai_level;
        status.local_color = session->local_color;
        status.generation = session->generation;
        status.connected = session->connected;
        status.chess = chess_game_status(session->game);
    }
    return status;
}

const chess_game_t *game_session_game(const game_session_t *session)
{
    return session != NULL ? session->game : NULL;
}

game_session_err_t game_session_apply_local(game_session_t *session,
                                            const chess_move_t *move)
{
    if (session == NULL || move == NULL) {
        return GAME_SESSION_INVALID_ARG;
    }
    if (!session->started || session->phase != GAME_PHASE_ACTIVE ||
        chess_game_side_to_move(session->game) != session->local_color) {
        return GAME_SESSION_NOT_ALLOWED;
    }
    return apply_move(session, move);
}

game_session_err_t game_session_apply_remote(game_session_t *session,
                                             const chess_move_t *move)
{
    if (session == NULL || move == NULL) {
        return GAME_SESSION_INVALID_ARG;
    }
    if (!session->started || session->mode != GAME_MODE_PVP ||
        !session->connected ||
        session->phase != GAME_PHASE_WAITING_OPPONENT ||
        chess_game_side_to_move(session->game) !=
            opposite(session->local_color)) {
        return GAME_SESSION_NOT_ALLOWED;
    }
    return apply_move(session, move);
}

game_session_err_t game_session_apply_synced(game_session_t *session,
                                             const chess_move_t *move)
{
    if (session == NULL || move == NULL) {
        return GAME_SESSION_INVALID_ARG;
    }
    if (!session->started || session->mode != GAME_MODE_PVP ||
        session->phase == GAME_PHASE_FINISHED) {
        return GAME_SESSION_NOT_ALLOWED;
    }
    return apply_move(session, move);
}

game_session_err_t game_session_compute_ai(const game_session_t *session,
                                           uint32_t *generation,
                                           chess_move_t *move)
{
    if (session == NULL || generation == NULL || move == NULL) {
        return GAME_SESSION_INVALID_ARG;
    }
    if (!session->started || session->mode != GAME_MODE_PVE ||
        session->phase != GAME_PHASE_AI_THINKING) {
        return GAME_SESSION_NOT_ALLOWED;
    }
    chess_game_t *snapshot = NULL;
    if (chess_game_clone(session->game, &snapshot) != CHESS_OK) {
        return GAME_SESSION_NO_MEMORY;
    }
    const bool found =
        chess_game_ai_pick_level(snapshot, session->ai_level, move);
    chess_game_destroy(snapshot);
    if (!found) {
        return GAME_SESSION_RULE_ERROR;
    }
    *generation = session->generation;
    return GAME_SESSION_OK;
}

game_session_err_t game_session_commit_ai(game_session_t *session,
                                          uint32_t generation,
                                          const chess_move_t *move)
{
    if (session == NULL || move == NULL) {
        return GAME_SESSION_INVALID_ARG;
    }
    if (generation != session->generation) {
        return GAME_SESSION_STALE_RESULT;
    }
    if (!session->started || session->mode != GAME_MODE_PVE ||
        session->phase != GAME_PHASE_AI_THINKING) {
        return GAME_SESSION_NOT_ALLOWED;
    }
    return apply_move(session, move);
}

game_session_err_t game_session_resign(game_session_t *session)
{
    if (session == NULL) {
        return GAME_SESSION_INVALID_ARG;
    }
    return game_session_resign_color(session, session->local_color);
}

game_session_err_t game_session_resign_color(game_session_t *session,
                                             chess_color_t color)
{
    if (session == NULL) {
        return GAME_SESSION_INVALID_ARG;
    }
    if (!session->started || session->phase == GAME_PHASE_FINISHED ||
        (color != CHESS_WHITE && color != CHESS_BLACK)) {
        return GAME_SESSION_NOT_ALLOWED;
    }
    if (chess_game_resign(session->game, color) != CHESS_OK) {
        return GAME_SESSION_RULE_ERROR;
    }
    refresh_phase(session);
    return GAME_SESSION_OK;
}

void game_session_set_connected(game_session_t *session, bool connected)
{
    if (session == NULL || !session->started ||
        session->mode != GAME_MODE_PVP ||
        session->phase == GAME_PHASE_FINISHED) {
        return;
    }
    session->connected = connected;
    refresh_phase(session);
}

void game_session_set_sync_error(game_session_t *session)
{
    if (session == NULL || !session->started ||
        session->mode != GAME_MODE_PVP ||
        session->phase == GAME_PHASE_FINISHED) {
        return;
    }
    session->phase = GAME_PHASE_SYNC_ERROR;
}
