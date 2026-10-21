// SPDX-License-Identifier: Apache-2.0

#include "game_session.h"

#include <stdio.h>
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

static chess_move_t move(const char *uci)
{
    chess_piece_type_t promotion = CHESS_PIECE_NONE;
    if (strlen(uci) == 5) {
        switch (uci[4]) {
        case 'q':
            promotion = CHESS_PIECE_QUEEN;
            break;
        case 'r':
            promotion = CHESS_PIECE_ROOK;
            break;
        case 'b':
            promotion = CHESS_PIECE_BISHOP;
            break;
        case 'n':
            promotion = CHESS_PIECE_KNIGHT;
            break;
        default:
            break;
        }
    }
    return (chess_move_t) {
        .from = (uint8_t)(uci[0] - 'a' + (uci[1] - '1') * 8),
        .to = (uint8_t)(uci[2] - 'a' + (uci[3] - '1') * 8),
        .promotion = promotion,
    };
}

static game_session_status_t run_pve(void)
{
    game_session_t *session = NULL;
    EXPECT(game_session_create(&session) == GAME_SESSION_OK);
    EXPECT(game_session_start_with_difficulty(
               session, GAME_MODE_PVE, CHESS_WHITE, CHESS_AI_NORMAL) ==
           GAME_SESSION_OK);
    EXPECT(game_session_status(session).ai_level == CHESS_AI_NORMAL);

    chess_move_t player = move("e2e4");
    EXPECT(game_session_apply_local(session, &player) == GAME_SESSION_OK);
    EXPECT(game_session_status(session).phase == GAME_PHASE_AI_THINKING);

    uint32_t generation = 0;
    chess_move_t ai;
    EXPECT(game_session_compute_ai(session, &generation, &ai) ==
           GAME_SESSION_OK);
    EXPECT(chess_game_is_legal(game_session_game(session), &ai));
    EXPECT(game_session_commit_ai(session, generation, &ai) ==
           GAME_SESSION_OK);
    EXPECT(game_session_status(session).phase == GAME_PHASE_ACTIVE);

    EXPECT(game_session_load_fen(
               session, "4k3/P7/8/8/8/8/8/4K3 w - - 0 1") ==
           GAME_SESSION_OK);
    chess_move_t promote = move("a7a8n");
    EXPECT(game_session_apply_local(session, &promote) == GAME_SESSION_OK);
    EXPECT(chess_piece_type(
               chess_game_piece_at(game_session_game(session), 56)) ==
           CHESS_PIECE_KNIGHT);

    EXPECT(game_session_load_fen(
               session, "7k/6Q1/6K1/8/8/8/8/8 b - - 0 1") ==
           GAME_SESSION_OK);
    EXPECT(game_session_status(session).chess.end_reason ==
           CHESS_END_CHECKMATE);

    EXPECT(game_session_start(session, GAME_MODE_PVE, CHESS_WHITE) ==
           GAME_SESSION_OK);
    EXPECT(game_session_load_fen(
               session, "7k/5Q2/6K1/8/8/8/8/8 b - - 0 1") ==
           GAME_SESSION_OK);
    EXPECT(game_session_status(session).chess.end_reason ==
           CHESS_END_STALEMATE);

    EXPECT(game_session_start(session, GAME_MODE_PVE, CHESS_WHITE) ==
           GAME_SESSION_OK);
    EXPECT(game_session_resign(session) == GAME_SESSION_OK);
    game_session_status_t final = game_session_status(session);
    EXPECT(final.phase == GAME_PHASE_FINISHED);
    EXPECT(final.chess.end_reason == CHESS_END_RESIGNATION);
    game_session_destroy(session);
    return final;
}

static game_session_status_t run_pvp(void)
{
    game_session_t *session = NULL;
    EXPECT(game_session_create(&session) == GAME_SESSION_OK);
    EXPECT(game_session_start(session, GAME_MODE_PVP, CHESS_BLACK) ==
           GAME_SESSION_OK);
    EXPECT(game_session_status(session).phase ==
           GAME_PHASE_WAITING_OPPONENT);

    chess_move_t white = move("e2e4");
    chess_move_t black = move("e7e5");
    EXPECT(game_session_apply_remote(session, &white) == GAME_SESSION_OK);
    EXPECT(game_session_apply_local(session, &black) == GAME_SESSION_OK);
    EXPECT(game_session_status(session).phase ==
           GAME_PHASE_WAITING_OPPONENT);

    game_session_set_connected(session, false);
    EXPECT(game_session_status(session).phase == GAME_PHASE_DISCONNECTED);
    game_session_set_connected(session, true);
    EXPECT(game_session_status(session).phase ==
           GAME_PHASE_WAITING_OPPONENT);
    game_session_set_sync_error(session);
    EXPECT(game_session_status(session).phase == GAME_PHASE_SYNC_ERROR);

    EXPECT(game_session_start(session, GAME_MODE_PVP, CHESS_WHITE) ==
           GAME_SESSION_OK);
    EXPECT(game_session_resign(session) == GAME_SESSION_OK);
    game_session_status_t final = game_session_status(session);
    EXPECT(final.phase == GAME_PHASE_FINISHED);
    EXPECT(final.chess.end_reason == CHESS_END_RESIGNATION);
    game_session_destroy(session);
    return final;
}

static void write_evidence(const char *path,
                           game_session_status_t pve,
                           game_session_status_t pvp)
{
    if (path == NULL) {
        return;
    }
    FILE *file = fopen(path, "w");
    EXPECT(file != NULL);
    if (file == NULL) {
        return;
    }
    fprintf(file,
            "{\n"
            "  \"schema\": \"mosaico-chess-flow/v1\",\n"
            "  \"pve\": {\"phase\": %d, \"end_reason\": %d, "
            "\"winner\": %d},\n"
            "  \"pvp\": {\"phase\": %d, \"end_reason\": %d, "
            "\"winner\": %d}\n"
            "}\n",
            pve.phase, pve.chess.end_reason, pve.chess.winner,
            pvp.phase, pvp.chess.end_reason, pvp.chess.winner);
    EXPECT(fclose(file) == 0);
}

int main(int argc, char **argv)
{
    const game_session_status_t pve = run_pve();
    const game_session_status_t pvp = run_pvp();
    write_evidence(argc > 1 ? argv[1] : NULL, pve, pvp);
    if (s_failures != 0) {
        fprintf(stderr, "%d flow expectation(s) failed\n", s_failures);
        return 1;
    }
    puts("PvE and PvP shared-C flow scenarios passed");
    return 0;
}
