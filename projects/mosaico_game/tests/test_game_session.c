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
    EXPECT(game_session_start(session, GAME_MODE_PVE, CHESS_WHITE) ==
           GAME_SESSION_OK);
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

    EXPECT(game_session_start(session, GAME_MODE_PVE, CHESS_WHITE) ==
           GAME_SESSION_OK);
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

static void test_pvp_turn_draw_and_connection(void)
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

    EXPECT(game_session_offer_draw(session) == GAME_SESSION_OK);
    EXPECT(game_session_status(session).draw_state ==
           GAME_DRAW_OFFER_SENT);
    EXPECT(game_session_resolve_draw_offer(session, false) ==
           GAME_SESSION_OK);
    EXPECT(game_session_offer_draw(session) == GAME_SESSION_OK);
    EXPECT(game_session_resolve_draw_offer(session, true) ==
           GAME_SESSION_OK);
    EXPECT(game_session_status(session).chess.end_reason ==
           CHESS_END_AGREED_DRAW);
    game_session_destroy(session);

    EXPECT(game_session_create(&session) == GAME_SESSION_OK);
    EXPECT(game_session_start(session, GAME_MODE_PVP, CHESS_BLACK) ==
           GAME_SESSION_OK);
    EXPECT(game_session_apply_remote(session, &white) == GAME_SESSION_OK);
    EXPECT(game_session_offer_draw(session) == GAME_SESSION_OK);
    chess_move_t black = move("e7e5");
    EXPECT(game_session_apply_local(session, &black) == GAME_SESSION_OK);
    EXPECT(game_session_status(session).draw_state == GAME_DRAW_NONE);

    EXPECT(game_session_receive_draw_offer(session) == GAME_SESSION_OK);
    EXPECT(game_session_respond_draw(session, false) == GAME_SESSION_OK);
    EXPECT(game_session_status(session).chess.end_reason == CHESS_END_NONE);
    EXPECT(game_session_receive_draw_offer(session) == GAME_SESSION_OK);
    EXPECT(game_session_respond_draw(session, true) == GAME_SESSION_OK);
    EXPECT(game_session_status(session).chess.end_reason ==
           CHESS_END_AGREED_DRAW);
    EXPECT(game_session_status(session).phase == GAME_PHASE_FINISHED);
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
    EXPECT(status.draw_state == GAME_DRAW_NONE);
    game_session_destroy(session);
}

int main(void)
{
    test_pve_turn_and_stale_ai();
    test_pvp_turn_draw_and_connection();
    test_reset_and_resign();
    if (s_failures != 0) {
        fprintf(stderr, "%d session expectation(s) failed\n", s_failures);
        return 1;
    }
    puts("all game session tests passed");
    return 0;
}
