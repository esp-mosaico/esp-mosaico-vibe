// SPDX-License-Identifier: Apache-2.0

#include "game_session.h"

#include <stdio.h>

static int s_failures;

#define EXPECT(condition)                                                       \
    do {                                                                        \
        if (!(condition)) {                                                     \
            fprintf(stderr, "%s:%d: expectation failed: %s\n",                 \
                    __FILE__, __LINE__, #condition);                            \
            ++s_failures;                                                       \
        }                                                                       \
    } while (0)

static chess_move_t move(const char *uci)
{
    return (chess_move_t) {
        .from = (uint8_t)(uci[0] - 'a' + (uci[1] - '1') * 8),
        .to = (uint8_t)(uci[2] - 'a' + (uci[3] - '1') * 8),
        .promotion = CHESS_PIECE_NONE,
    };
}

static void test_pve_turn_and_stale_ai(void)
{
    game_session_t *session = NULL;
    EXPECT(game_session_create(&session) == GAME_SESSION_OK);
    EXPECT(game_session_start_with_difficulty(
               session, GAME_MODE_PVE, CHESS_WHITE, CHESS_AI_HARD) ==
           GAME_SESSION_OK);
    EXPECT(game_session_status(session).ai_level == CHESS_AI_HARD);
    chess_move_t player = move("e2e4");
    EXPECT(game_session_apply_local(session, &player) == GAME_SESSION_OK);
    EXPECT(game_session_status(session).phase == GAME_PHASE_AI_THINKING);
    EXPECT(game_session_apply_local(session, &player) ==
           GAME_SESSION_NOT_ALLOWED);

    uint32_t old_generation = 0;
    chess_move_t ai;
    EXPECT(game_session_compute_ai(session, &old_generation, &ai) ==
           GAME_SESSION_OK);
    EXPECT(chess_game_is_legal(game_session_game(session), &ai));

    EXPECT(game_session_start_with_difficulty(
               session, GAME_MODE_PVE, CHESS_WHITE, CHESS_AI_EASY) ==
           GAME_SESSION_OK);
    EXPECT(game_session_status(session).ai_level == CHESS_AI_EASY);
    EXPECT(game_session_commit_ai(session, old_generation, &ai) ==
           GAME_SESSION_STALE_RESULT);

    player = move("d2d4");
    EXPECT(game_session_apply_local(session, &player) == GAME_SESSION_OK);
    uint32_t generation = 0;
    EXPECT(game_session_compute_ai(session, &generation, &ai) ==
           GAME_SESSION_OK);
    EXPECT(game_session_commit_ai(session, generation, &ai) ==
           GAME_SESSION_OK);
    EXPECT(game_session_status(session).phase == GAME_PHASE_ACTIVE);
    game_session_destroy(session);
}

static void test_pvp_turn_and_connection(void)
{
    game_session_t *session = NULL;
    EXPECT(game_session_create(&session) == GAME_SESSION_OK);
    EXPECT(game_session_start(session, GAME_MODE_PVP, CHESS_BLACK) ==
           GAME_SESSION_OK);
    EXPECT(game_session_status(session).phase ==
           GAME_PHASE_WAITING_OPPONENT);

    chess_move_t white = move("e2e4");
    EXPECT(game_session_apply_local(session, &white) ==
           GAME_SESSION_NOT_ALLOWED);
    EXPECT(game_session_apply_remote(session, &white) == GAME_SESSION_OK);
    EXPECT(game_session_status(session).phase == GAME_PHASE_ACTIVE);

    chess_move_t black = move("e7e5");
    EXPECT(game_session_apply_local(session, &black) == GAME_SESSION_OK);
    EXPECT(game_session_status(session).chess.end_reason == CHESS_END_NONE);
    game_session_destroy(session);

    EXPECT(game_session_create(&session) == GAME_SESSION_OK);
    EXPECT(game_session_start(session, GAME_MODE_PVP, CHESS_WHITE) ==
           GAME_SESSION_OK);
    game_session_set_connected(session, false);
    EXPECT(game_session_status(session).phase == GAME_PHASE_DISCONNECTED);
    EXPECT(game_session_apply_local(session, &white) ==
           GAME_SESSION_NOT_ALLOWED);
    game_session_set_connected(session, true);
    EXPECT(game_session_status(session).phase == GAME_PHASE_ACTIVE);
    game_session_set_sync_error(session);
    EXPECT(game_session_status(session).phase == GAME_PHASE_SYNC_ERROR);
    game_session_destroy(session);
}

static void test_reset_and_resign(void)
{
    game_session_t *session = NULL;
    EXPECT(game_session_create(&session) == GAME_SESSION_OK);
    EXPECT(game_session_start(session, GAME_MODE_PVE, CHESS_WHITE) ==
           GAME_SESSION_OK);
    const uint32_t generation = game_session_status(session).generation;
    chess_move_t first = move("g1f3");
    EXPECT(game_session_apply_local(session, &first) == GAME_SESSION_OK);
    EXPECT(game_session_resign(session) == GAME_SESSION_OK);
    EXPECT(game_session_status(session).chess.end_reason ==
           CHESS_END_RESIGNATION);
    EXPECT(game_session_start(session, GAME_MODE_PVE, CHESS_WHITE) ==
           GAME_SESSION_OK);
    const game_session_status_t status = game_session_status(session);
    EXPECT(status.generation != generation);
    EXPECT(status.chess.end_reason == CHESS_END_NONE);
    EXPECT(status.chess.side_to_move == CHESS_WHITE);
    EXPECT(status.ai_level == CHESS_AI_NORMAL);
    game_session_destroy(session);
}

static void test_network_replay_and_remote_resign(void)
{
    game_session_t *session = NULL;
    EXPECT(game_session_create(&session) == GAME_SESSION_OK);
    EXPECT(game_session_start(session, GAME_MODE_PVP, CHESS_BLACK) ==
           GAME_SESSION_OK);
    chess_move_t white = move("e2e4");
    chess_move_t black = move("e7e5");
    EXPECT(game_session_apply_synced(session, &white) == GAME_SESSION_OK);
    EXPECT(game_session_apply_synced(session, &black) == GAME_SESSION_OK);
    EXPECT(game_session_status(session).chess.side_to_move == CHESS_WHITE);
    EXPECT(game_session_resign_color(session, CHESS_WHITE) ==
           GAME_SESSION_OK);
    const game_session_status_t status = game_session_status(session);
    EXPECT(status.phase == GAME_PHASE_FINISHED);
    EXPECT(status.chess.end_reason == CHESS_END_RESIGNATION);
    EXPECT(status.chess.winner == CHESS_BLACK);
    game_session_destroy(session);
}

static void test_pvp_checkmate_finishes_both_sides(void)
{
    static const char *sequence[] = {
        "f2f3", "e7e5", "g2g4", "d8h4",
    };
    static const chess_color_t colors[] = {
        CHESS_WHITE, CHESS_BLACK,
    };
    for (size_t color_index = 0;
         color_index < sizeof(colors) / sizeof(colors[0]); ++color_index) {
        const chess_color_t local_color = colors[color_index];
        game_session_t *session = NULL;
        EXPECT(game_session_create(&session) == GAME_SESSION_OK);
        EXPECT(game_session_start(
                   session, GAME_MODE_PVP,
                   local_color) == GAME_SESSION_OK);
        for (size_t ply = 0;
             ply < sizeof(sequence) / sizeof(sequence[0]); ++ply) {
            chess_move_t next = move(sequence[ply]);
            const chess_color_t mover =
                (ply & 1U) == 0U ? CHESS_WHITE : CHESS_BLACK;
            const game_session_err_t error =
                mover == local_color
                    ? game_session_apply_local(session, &next)
                    : game_session_apply_remote(session, &next);
            EXPECT(error == GAME_SESSION_OK);
        }
        const game_session_status_t status =
            game_session_status(session);
        EXPECT(status.phase == GAME_PHASE_FINISHED);
        EXPECT(status.chess.end_reason == CHESS_END_CHECKMATE);
        EXPECT(status.chess.winner == CHESS_BLACK);
        game_session_destroy(session);
    }
}

int main(void)
{
    test_pve_turn_and_stale_ai();
    test_pvp_turn_and_connection();
    test_reset_and_resign();
    test_network_replay_and_remote_resign();
    test_pvp_checkmate_finishes_both_sides();
    if (s_failures != 0) {
        fprintf(stderr, "%d session expectation(s) failed\n", s_failures);
        return 1;
    }
    puts("all game session tests passed");
    return 0;
}
