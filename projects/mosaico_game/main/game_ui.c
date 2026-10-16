// SPDX-License-Identifier: Apache-2.0

#define GSP_BUNDLE_ENABLE_LEGACY_NAMES
#include "bundle_gsp.h"
#include "game_session.h"
#include "game_ui.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef ESP_PLATFORM
#include "sdkconfig.h"
#else
#define CONFIG_MOSAICO_GAME_DEV_PVP 1
#endif

#define FRAME 20
#define CELL 55
#define WAIT_X0 120
#define WAIT_Y 230
#define WAIT_X1 280
#define WAIT_STEP 8
#define WAIT_TICK_MS 50
#define WAIT_TIMEOUT_TICKS (12000 / WAIT_TICK_MS)
#define AI_DELAY_MS 450
#define BASE_PIECE_SLOT_COUNT 32

enum {
    SCR_S00 = 0,
    SCR_S10,
    SCR_S20,
    SCR_S21,
    SCR_S22,
    SCR_S23,
    SCR_S24,
    SCR_PLAY,
    SCR_S50,
    SCR_S51,
    SCR_S52,
};

typedef struct {
    gsp_component_key_t key;
    chess_color_t color;
    chess_piece_type_t type;
    int square;
    bool visible;
    bool used;
} piece_sprite_t;

#define PIECE_SLOT(key_, color_, type_) \
    { .key = (key_), .color = (color_), .type = (type_), .square = -1 },
static piece_sprite_t s_piece_sprites[] = {
#include "piece_pool.inc"
};
#undef PIECE_SLOT
static bool s_piece_tracking_initialized;

typedef struct {
    esp_gsp_handle_t ui;
    game_session_t *session;
    int screen;
    int selected;
    uint64_t dots;
    uint64_t captures;
    bool selection_visible;
    bool pve;
    chess_move_t pending_promotion;
    bool promotion_pending;
    bool has_last_move;
    chess_move_t last_move;
    void *ai_timer;
    bool notice_visible;
    void *wait_timer;
    int wait_ticks;
    int wait_x;
    int wait_direction;
} game_ui_state_t;

/* GSP exposes one application backend per handle and no user-data getter.
 * This module therefore owns one explicitly allocated backend instance. */
static game_ui_state_t *s_state;

static const gsp_component_key_t s_screens[] = {
    GSP_OBJ_KEY_S00, GSP_OBJ_KEY_S10, GSP_OBJ_KEY_S20,
    GSP_OBJ_KEY_S21, GSP_OBJ_KEY_S22, GSP_OBJ_KEY_S23,
    GSP_OBJ_KEY_S24, GSP_OBJ_KEY_PLAY, GSP_OBJ_KEY_S50,
    GSP_OBJ_KEY_S51, GSP_OBJ_KEY_S52,
};

static const gsp_component_key_t s_dots[64] = {
    GSP_OBJ_KEY_DOT00, GSP_OBJ_KEY_DOT01, GSP_OBJ_KEY_DOT02,
    GSP_OBJ_KEY_DOT03, GSP_OBJ_KEY_DOT04, GSP_OBJ_KEY_DOT05,
    GSP_OBJ_KEY_DOT06, GSP_OBJ_KEY_DOT07, GSP_OBJ_KEY_DOT08,
    GSP_OBJ_KEY_DOT09, GSP_OBJ_KEY_DOT10, GSP_OBJ_KEY_DOT11,
    GSP_OBJ_KEY_DOT12, GSP_OBJ_KEY_DOT13, GSP_OBJ_KEY_DOT14,
    GSP_OBJ_KEY_DOT15, GSP_OBJ_KEY_DOT16, GSP_OBJ_KEY_DOT17,
    GSP_OBJ_KEY_DOT18, GSP_OBJ_KEY_DOT19, GSP_OBJ_KEY_DOT20,
    GSP_OBJ_KEY_DOT21, GSP_OBJ_KEY_DOT22, GSP_OBJ_KEY_DOT23,
    GSP_OBJ_KEY_DOT24, GSP_OBJ_KEY_DOT25, GSP_OBJ_KEY_DOT26,
    GSP_OBJ_KEY_DOT27, GSP_OBJ_KEY_DOT28, GSP_OBJ_KEY_DOT29,
    GSP_OBJ_KEY_DOT30, GSP_OBJ_KEY_DOT31, GSP_OBJ_KEY_DOT32,
    GSP_OBJ_KEY_DOT33, GSP_OBJ_KEY_DOT34, GSP_OBJ_KEY_DOT35,
    GSP_OBJ_KEY_DOT36, GSP_OBJ_KEY_DOT37, GSP_OBJ_KEY_DOT38,
    GSP_OBJ_KEY_DOT39, GSP_OBJ_KEY_DOT40, GSP_OBJ_KEY_DOT41,
    GSP_OBJ_KEY_DOT42, GSP_OBJ_KEY_DOT43, GSP_OBJ_KEY_DOT44,
    GSP_OBJ_KEY_DOT45, GSP_OBJ_KEY_DOT46, GSP_OBJ_KEY_DOT47,
    GSP_OBJ_KEY_DOT48, GSP_OBJ_KEY_DOT49, GSP_OBJ_KEY_DOT50,
    GSP_OBJ_KEY_DOT51, GSP_OBJ_KEY_DOT52, GSP_OBJ_KEY_DOT53,
    GSP_OBJ_KEY_DOT54, GSP_OBJ_KEY_DOT55, GSP_OBJ_KEY_DOT56,
    GSP_OBJ_KEY_DOT57, GSP_OBJ_KEY_DOT58, GSP_OBJ_KEY_DOT59,
    GSP_OBJ_KEY_DOT60, GSP_OBJ_KEY_DOT61, GSP_OBJ_KEY_DOT62,
    GSP_OBJ_KEY_DOT63,
};

static const gsp_component_key_t s_captures[64] = {
    GSP_OBJ_KEY_CAP00, GSP_OBJ_KEY_CAP01, GSP_OBJ_KEY_CAP02,
    GSP_OBJ_KEY_CAP03, GSP_OBJ_KEY_CAP04, GSP_OBJ_KEY_CAP05,
    GSP_OBJ_KEY_CAP06, GSP_OBJ_KEY_CAP07, GSP_OBJ_KEY_CAP08,
    GSP_OBJ_KEY_CAP09, GSP_OBJ_KEY_CAP10, GSP_OBJ_KEY_CAP11,
    GSP_OBJ_KEY_CAP12, GSP_OBJ_KEY_CAP13, GSP_OBJ_KEY_CAP14,
    GSP_OBJ_KEY_CAP15, GSP_OBJ_KEY_CAP16, GSP_OBJ_KEY_CAP17,
    GSP_OBJ_KEY_CAP18, GSP_OBJ_KEY_CAP19, GSP_OBJ_KEY_CAP20,
    GSP_OBJ_KEY_CAP21, GSP_OBJ_KEY_CAP22, GSP_OBJ_KEY_CAP23,
    GSP_OBJ_KEY_CAP24, GSP_OBJ_KEY_CAP25, GSP_OBJ_KEY_CAP26,
    GSP_OBJ_KEY_CAP27, GSP_OBJ_KEY_CAP28, GSP_OBJ_KEY_CAP29,
    GSP_OBJ_KEY_CAP30, GSP_OBJ_KEY_CAP31, GSP_OBJ_KEY_CAP32,
    GSP_OBJ_KEY_CAP33, GSP_OBJ_KEY_CAP34, GSP_OBJ_KEY_CAP35,
    GSP_OBJ_KEY_CAP36, GSP_OBJ_KEY_CAP37, GSP_OBJ_KEY_CAP38,
    GSP_OBJ_KEY_CAP39, GSP_OBJ_KEY_CAP40, GSP_OBJ_KEY_CAP41,
    GSP_OBJ_KEY_CAP42, GSP_OBJ_KEY_CAP43, GSP_OBJ_KEY_CAP44,
    GSP_OBJ_KEY_CAP45, GSP_OBJ_KEY_CAP46, GSP_OBJ_KEY_CAP47,
    GSP_OBJ_KEY_CAP48, GSP_OBJ_KEY_CAP49, GSP_OBJ_KEY_CAP50,
    GSP_OBJ_KEY_CAP51, GSP_OBJ_KEY_CAP52, GSP_OBJ_KEY_CAP53,
    GSP_OBJ_KEY_CAP54, GSP_OBJ_KEY_CAP55, GSP_OBJ_KEY_CAP56,
    GSP_OBJ_KEY_CAP57, GSP_OBJ_KEY_CAP58, GSP_OBJ_KEY_CAP59,
    GSP_OBJ_KEY_CAP60, GSP_OBJ_KEY_CAP61, GSP_OBJ_KEY_CAP62,
    GSP_OBJ_KEY_CAP63,
};

static const uint16_t s_hit_actions[64] = {
    GSP_ACT_ID_HIT00, GSP_ACT_ID_HIT01, GSP_ACT_ID_HIT02,
    GSP_ACT_ID_HIT03, GSP_ACT_ID_HIT04, GSP_ACT_ID_HIT05,
    GSP_ACT_ID_HIT06, GSP_ACT_ID_HIT07, GSP_ACT_ID_HIT08,
    GSP_ACT_ID_HIT09, GSP_ACT_ID_HIT10, GSP_ACT_ID_HIT11,
    GSP_ACT_ID_HIT12, GSP_ACT_ID_HIT13, GSP_ACT_ID_HIT14,
    GSP_ACT_ID_HIT15, GSP_ACT_ID_HIT16, GSP_ACT_ID_HIT17,
    GSP_ACT_ID_HIT18, GSP_ACT_ID_HIT19, GSP_ACT_ID_HIT20,
    GSP_ACT_ID_HIT21, GSP_ACT_ID_HIT22, GSP_ACT_ID_HIT23,
    GSP_ACT_ID_HIT24, GSP_ACT_ID_HIT25, GSP_ACT_ID_HIT26,
    GSP_ACT_ID_HIT27, GSP_ACT_ID_HIT28, GSP_ACT_ID_HIT29,
    GSP_ACT_ID_HIT30, GSP_ACT_ID_HIT31, GSP_ACT_ID_HIT32,
    GSP_ACT_ID_HIT33, GSP_ACT_ID_HIT34, GSP_ACT_ID_HIT35,
    GSP_ACT_ID_HIT36, GSP_ACT_ID_HIT37, GSP_ACT_ID_HIT38,
    GSP_ACT_ID_HIT39, GSP_ACT_ID_HIT40, GSP_ACT_ID_HIT41,
    GSP_ACT_ID_HIT42, GSP_ACT_ID_HIT43, GSP_ACT_ID_HIT44,
    GSP_ACT_ID_HIT45, GSP_ACT_ID_HIT46, GSP_ACT_ID_HIT47,
    GSP_ACT_ID_HIT48, GSP_ACT_ID_HIT49, GSP_ACT_ID_HIT50,
    GSP_ACT_ID_HIT51, GSP_ACT_ID_HIT52, GSP_ACT_ID_HIT53,
    GSP_ACT_ID_HIT54, GSP_ACT_ID_HIT55, GSP_ACT_ID_HIT56,
    GSP_ACT_ID_HIT57, GSP_ACT_ID_HIT58, GSP_ACT_ID_HIT59,
    GSP_ACT_ID_HIT60, GSP_ACT_ID_HIT61, GSP_ACT_ID_HIT62,
    GSP_ACT_ID_HIT63,
};

static const gsp_component_key_t s_white_promotion_images[4] = {
    GSP_OBJ_KEY_PROMOTION_WQ_IMAGE, GSP_OBJ_KEY_PROMOTION_WR_IMAGE,
    GSP_OBJ_KEY_PROMOTION_WB_IMAGE, GSP_OBJ_KEY_PROMOTION_WN_IMAGE,
};

static const gsp_component_key_t s_black_promotion_images[4] = {
    GSP_OBJ_KEY_PROMOTION_BQ_IMAGE, GSP_OBJ_KEY_PROMOTION_BR_IMAGE,
    GSP_OBJ_KEY_PROMOTION_BB_IMAGE, GSP_OBJ_KEY_PROMOTION_BN_IMAGE,
};

static void cell_xy(int square, int *x, int *y)
{
    *x = FRAME + (square & 7) * CELL;
    *y = FRAME + (7 - (square >> 3)) * CELL;
}

static void piece_xy(int square, chess_piece_type_t type, int *x, int *y)
{
    int cell_x = 0;
    int cell_y = 0;
    const int width = type == CHESS_PIECE_PAWN ? 51 : 39;
    const int height = type == CHESS_PIECE_PAWN ? 84 : 64;
    cell_xy(square, &cell_x, &cell_y);
    *x = cell_x + (CELL - width) / 2;
    *y = cell_y + CELL - height;
}

static void show_only(game_ui_state_t *state, int screen)
{
    for (size_t index = 0;
         index < sizeof(s_screens) / sizeof(s_screens[0]); ++index) {
        (void)esp_gsp_component_set_visible(
            state->ui, s_screens[index], (int)index == screen);
    }
    (void)esp_gsp_set_press_feedback_enabled(
        state->ui, screen != SCR_PLAY);
    state->screen = screen;
}

static void hide_selection(game_ui_state_t *state)
{
    const uint64_t visible = state->dots;
    const uint64_t capture_visible = state->captures;
    state->dots = 0;
    state->captures = 0;
    for (int square = 0; square < 64; ++square) {
        if ((visible & (1ULL << square)) != 0) {
            (void)esp_gsp_component_set_visible(
                state->ui, s_dots[square], false);
        }
        if ((capture_visible & (1ULL << square)) != 0) {
            (void)esp_gsp_component_set_visible(
                state->ui, s_captures[square], false);
        }
    }
    if (state->selection_visible) {
        state->selection_visible = false;
        (void)esp_gsp_component_set_visible(
            state->ui, GSP_OBJ_KEY_SEL, false);
    }
    state->selected = -1;
}

static void show_selection(game_ui_state_t *state, int square)
{
    int x = 0;
    int y = 0;
    cell_xy(square, &x, &y);
    (void)esp_gsp_component_set_position(
        state->ui, GSP_OBJ_KEY_SEL, x, y);
    (void)esp_gsp_component_set_visible(
        state->ui, GSP_OBJ_KEY_SEL, true);
    state->selection_visible = true;
}

static void show_legal(game_ui_state_t *state, int from)
{
    chess_move_t moves[32];
    const size_t count = chess_game_generate_legal_from(
        game_session_game(state->session), from, moves,
        sizeof(moves) / sizeof(moves[0]));
    uint64_t wanted = 0;
    uint64_t captures = 0;
    const chess_game_t *game = game_session_game(state->session);
    const chess_status_t status = chess_game_status(game);
    for (size_t index = 0; index < count; ++index) {
        const uint64_t bit = 1ULL << moves[index].to;
        const bool en_passant =
            moves[index].to == status.en_passant_square &&
            chess_piece_type(chess_game_piece_at(game, from)) ==
                CHESS_PIECE_PAWN &&
            (moves[index].from & 7) != (moves[index].to & 7);
        if (chess_game_piece_at(game, moves[index].to) != 0 || en_passant) {
            captures |= bit;
        } else {
            wanted |= bit;
        }
    }
    for (int square = 0; square < 64; ++square) {
        const bool old_visible = (state->dots & (1ULL << square)) != 0;
        const bool new_visible = (wanted & (1ULL << square)) != 0;
        if (old_visible != new_visible) {
            (void)esp_gsp_component_set_visible(
                state->ui, s_dots[square], new_visible);
        }
        const bool old_capture =
            (state->captures & (1ULL << square)) != 0;
        const bool new_capture = (captures & (1ULL << square)) != 0;
        if (old_capture != new_capture) {
            (void)esp_gsp_component_set_visible(
                state->ui, s_captures[square], new_capture);
        }
    }
    state->dots = wanted;
    state->captures = captures;
    state->selected = from;
    show_selection(state, from);
}

static piece_sprite_t *find_free_sprite(chess_color_t color,
                                        chess_piece_type_t type)
{
    for (size_t index = 0;
         index < sizeof(s_piece_sprites) / sizeof(s_piece_sprites[0]);
         ++index) {
        piece_sprite_t *sprite = &s_piece_sprites[index];
        if (!sprite->used && sprite->color == color && sprite->type == type) {
            return sprite;
        }
    }
    return NULL;
}

static void initialize_piece_tracking(void)
{
    if (s_piece_tracking_initialized) {
        return;
    }

    /*
     * The first 32 slots are authored in the scene at the standard starting
     * squares and are initially visible.  Mirror that state instead of
     * hiding every promotion sprite and rebuilding the board in one callback;
     * that burst can exceed the GSP command queue.
     */
    for (size_t index = 0; index < BASE_PIECE_SLOT_COUNT; ++index) {
        piece_sprite_t *sprite = &s_piece_sprites[index];
        if (index < 8) {
            sprite->square = 56 + (int)index;
        } else if (index < 16) {
            sprite->square = 48 + (int)(index - 8);
        } else if (index < 24) {
            sprite->square = 8 + (int)(index - 16);
        } else {
            sprite->square = (int)(index - 24);
        }
        sprite->visible = true;
    }
    s_piece_tracking_initialized = true;
}

static void sync_pieces(game_ui_state_t *state)
{
    bool claimed[64] = {false};
    const chess_game_t *game = game_session_game(state->session);

    for (size_t index = 0;
         index < sizeof(s_piece_sprites) / sizeof(s_piece_sprites[0]);
         ++index) {
        piece_sprite_t *sprite = &s_piece_sprites[index];
        sprite->used = false;
        if (!sprite->visible || sprite->square < 0 ||
            sprite->square >= 64 || claimed[sprite->square]) {
            continue;
        }
        const uint8_t piece = chess_game_piece_at(game, sprite->square);
        if (piece != 0 && chess_piece_color(piece) == sprite->color &&
            chess_piece_type(piece) == sprite->type) {
            sprite->used = true;
            claimed[sprite->square] = true;
        }
    }

    for (int square = 0; square < 64; ++square) {
        const uint8_t piece = chess_game_piece_at(game, square);
        if (piece == 0 || claimed[square]) {
            continue;
        }
        piece_sprite_t *sprite =
            find_free_sprite(chess_piece_color(piece), chess_piece_type(piece));
        if (sprite == NULL) {
            continue;
        }
        int x = 0;
        int y = 0;
        piece_xy(square, sprite->type, &x, &y);
        if (sprite->visible) {
            (void)esp_gsp_component_animate_position_to(
                state->ui, sprite->key, x, y, 220, ESP_GSP_EASE_OUT);
        } else {
            (void)esp_gsp_component_set_position(
                state->ui, sprite->key, x, y);
            (void)esp_gsp_component_set_visible(
                state->ui, sprite->key, true);
        }
        sprite->square = square;
        sprite->visible = true;
        sprite->used = true;
        claimed[square] = true;
    }

    for (size_t index = 0;
         index < sizeof(s_piece_sprites) / sizeof(s_piece_sprites[0]);
         ++index) {
        piece_sprite_t *sprite = &s_piece_sprites[index];
        if (sprite->visible && !sprite->used) {
            (void)esp_gsp_component_set_visible(
                state->ui, sprite->key, false);
            sprite->visible = false;
            sprite->square = -1;
        }
    }
}

static void board_overlay_position(game_ui_state_t *state,
                                   gsp_component_key_t key,
                                   int square)
{
    const int x = (square & 7) * CELL;
    const int y = (7 - (square >> 3)) * CELL;
    (void)esp_gsp_component_set_position(state->ui, key, x, y);
}

static void sync_feedback(game_ui_state_t *state)
{
    const chess_game_t *game = game_session_game(state->session);
    if (state->has_last_move) {
        board_overlay_position(
            state, GSP_OBJ_KEY_LAST_FROM, state->last_move.from);
        board_overlay_position(
            state, GSP_OBJ_KEY_LAST_TO, state->last_move.to);
    }
    (void)esp_gsp_component_set_visible(
        state->ui, GSP_OBJ_KEY_LAST_FROM, state->has_last_move);
    (void)esp_gsp_component_set_visible(
        state->ui, GSP_OBJ_KEY_LAST_TO, state->has_last_move);

    const chess_status_t status = chess_game_status(game);
    int king_square = -1;
    if (status.in_check) {
        const uint8_t king =
            chess_piece_make(status.side_to_move, CHESS_PIECE_KING);
        for (int square = 0; square < 64; ++square) {
            if (chess_game_piece_at(game, square) == king) {
                king_square = square;
                break;
            }
        }
    }
    if (king_square >= 0) {
        board_overlay_position(state, GSP_OBJ_KEY_CHECK, king_square);
    }
    (void)esp_gsp_component_set_visible(
        state->ui, GSP_OBJ_KEY_CHECK, king_square >= 0);
}

static const char *end_reason_text(chess_end_reason_t reason)
{
    switch (reason) {
    case CHESS_END_CHECKMATE:
        return "将死";
    case CHESS_END_RESIGNATION:
        return "";
    case CHESS_END_STALEMATE:
        return "逼和";
    case CHESS_END_AGREED_DRAW:
        return "";
    case CHESS_END_THREEFOLD_REPETITION:
        return "三次重复局面";
    case CHESS_END_FIFTY_MOVE_RULE:
        return "五十步规则";
    case CHESS_END_INSUFFICIENT_MATERIAL:
        return "子力不足";
    default:
        return "";
    }
}

static void show_result(game_ui_state_t *state)
{
    const game_session_status_t status =
        game_session_status(state->session);
    int screen = SCR_S52;
    gsp_component_key_t reason_key = GSP_OBJ_KEY_S52_REASON;
    if (status.chess.winner >= 0) {
        const bool local_won =
            status.chess.winner == status.local_color;
        screen = local_won ? SCR_S50 : SCR_S51;
        reason_key =
            local_won ? GSP_OBJ_KEY_S50_REASON : GSP_OBJ_KEY_S51_REASON;
    }
    (void)esp_gsp_component_set_text(
        state->ui, reason_key, end_reason_text(status.chess.end_reason));
    show_only(state, screen);
}

static void after_move(game_ui_state_t *state);

static void show_wait_notice(game_ui_state_t *state)
{
    if (!state->notice_visible) {
        (void)esp_gsp_component_set_visible(
            state->ui, GSP_OBJ_KEY_NOTICE_WAIT, true);
        state->notice_visible = true;
    }
}

static void hide_notice(game_ui_state_t *state)
{
    if (state->notice_visible) {
        (void)esp_gsp_component_set_visible(
            state->ui, GSP_OBJ_KEY_NOTICE_WAIT, false);
        state->notice_visible = false;
    }
}

static void ai_tick(esp_gsp_handle_t ui, void *context)
{
    (void)ui;
    game_ui_state_t *state = context;
    if (state->ai_timer != NULL) {
        (void)esp_gsp_timer_delete(state->ui, state->ai_timer);
        state->ai_timer = NULL;
    }
    hide_notice(state);
    uint32_t generation = 0;
    chess_move_t move;
    const game_session_status_t status =
        game_session_status(state->session);
    if (status.mode == GAME_MODE_PVP &&
        status.phase == GAME_PHASE_WAITING_OPPONENT) {
#if CONFIG_MOSAICO_GAME_DEV_PVP
        if (chess_game_ai_pick(game_session_game(state->session), &move) &&
            game_session_apply_remote(state->session, &move) ==
                GAME_SESSION_OK) {
            state->last_move = move;
            state->has_last_move = true;
            after_move(state);
        }
#endif
    } else if (game_session_compute_ai(
            state->session, &generation, &move) == GAME_SESSION_OK &&
        game_session_commit_ai(
            state->session, generation, &move) == GAME_SESSION_OK) {
        state->last_move = move;
        state->has_last_move = true;
        after_move(state);
    }
}

static void after_move(game_ui_state_t *state)
{
    const game_session_status_t status =
        game_session_status(state->session);
    if (status.phase == GAME_PHASE_WAITING_OPPONENT) {
        show_wait_notice(state);
    }
    hide_selection(state);
    sync_pieces(state);
    sync_feedback(state);
    if (status.phase == GAME_PHASE_FINISHED) {
        show_result(state);
        return;
    }
    if ((status.phase == GAME_PHASE_AI_THINKING
#if CONFIG_MOSAICO_GAME_DEV_PVP
         || (status.mode == GAME_MODE_PVP &&
             status.phase == GAME_PHASE_WAITING_OPPONENT)
#endif
         ) &&
        state->ai_timer == NULL) {
        state->ai_timer =
            esp_gsp_timer_create(state->ui, AI_DELAY_MS, ai_tick, state);
        if (state->ai_timer == NULL) {
            ai_tick(state->ui, state);
        }
    }
}

static bool move_for_target(game_ui_state_t *state,
                            int from,
                            int to,
                            chess_move_t *move,
                            bool *needs_promotion)
{
    chess_move_t legal[32];
    const size_t count = chess_game_generate_legal_from(
        game_session_game(state->session), from, legal,
        sizeof(legal) / sizeof(legal[0]));
    bool found = false;
    *needs_promotion = false;
    for (size_t index = 0; index < count; ++index) {
        if (legal[index].to != to) {
            continue;
        }
        if (!found) {
            *move = legal[index];
            found = true;
        }
        if (legal[index].promotion != CHESS_PIECE_NONE) {
            *needs_promotion = true;
        }
    }
    return found;
}

static void commit_move(game_ui_state_t *state, const chess_move_t *move)
{
    if (game_session_apply_local(state->session, move) == GAME_SESSION_OK) {
        state->last_move = *move;
        state->has_last_move = true;
        after_move(state);
    }
}

static void tap_square(game_ui_state_t *state, int square)
{
    const game_session_status_t current =
        game_session_status(state->session);
    if (state->screen != SCR_PLAY || state->promotion_pending) {
        return;
    }
    if (current.phase != GAME_PHASE_ACTIVE) {
        if (current.phase == GAME_PHASE_WAITING_OPPONENT) {
            show_wait_notice(state);
        }
        return;
    }
    const chess_game_t *game = game_session_game(state->session);
    const uint8_t piece = chess_game_piece_at(game, square);
    if (state->selected >= 0) {
        chess_move_t move;
        bool needs_promotion = false;
        if (move_for_target(state, state->selected, square, &move,
                            &needs_promotion)) {
            if (needs_promotion) {
                state->pending_promotion = move;
                state->promotion_pending = true;
                const chess_color_t color = chess_piece_color(
                    chess_game_piece_at(game, state->selected));
                for (size_t index = 0; index < 4; ++index) {
                    (void)esp_gsp_component_set_visible(
                        state->ui, s_white_promotion_images[index],
                        color == CHESS_WHITE);
                    (void)esp_gsp_component_set_visible(
                        state->ui, s_black_promotion_images[index],
                        color == CHESS_BLACK);
                }
                (void)esp_gsp_component_set_visible(
                    state->ui, GSP_OBJ_KEY_PROMOTION, true);
            } else {
                commit_move(state, &move);
            }
            return;
        }
    }
    const game_session_status_t status =
        game_session_status(state->session);
    if (piece != 0 &&
        chess_piece_color(piece) == status.local_color &&
        chess_piece_color(piece) == status.chess.side_to_move) {
        show_legal(state, square);
    } else {
        hide_selection(state);
    }
}

static void choose_promotion(game_ui_state_t *state,
                             chess_piece_type_t promotion)
{
    if (!state->promotion_pending) {
        return;
    }
    chess_move_t move = state->pending_promotion;
    move.promotion = promotion;
    state->promotion_pending = false;
    (void)esp_gsp_component_set_visible(
        state->ui, GSP_OBJ_KEY_PROMOTION, false);
    commit_move(state, &move);
}

static void stop_ai(game_ui_state_t *state)
{
    if (state->ai_timer != NULL) {
        (void)esp_gsp_timer_delete(state->ui, state->ai_timer);
        state->ai_timer = NULL;
    }
}

static void start_game(game_ui_state_t *state,
                       game_mode_t mode,
                       chess_color_t local_color)
{
    stop_ai(state);
    state->pve = mode == GAME_MODE_PVE;
    state->promotion_pending = false;
    state->has_last_move = false;
    (void)esp_gsp_component_set_visible(
        state->ui, GSP_OBJ_KEY_DRAW, mode == GAME_MODE_PVP);
#if CONFIG_MOSAICO_GAME_DEV_PVP
    (void)esp_gsp_component_set_visible(
        state->ui, GSP_OBJ_KEY_DEV_DISCONNECT, mode == GAME_MODE_PVP);
#else
    (void)esp_gsp_component_set_visible(
        state->ui, GSP_OBJ_KEY_DEV_DISCONNECT, false);
#endif
    hide_notice(state);
    hide_selection(state);
    (void)esp_gsp_component_set_visible(
        state->ui, GSP_OBJ_KEY_MENU, false);
    (void)esp_gsp_component_set_visible(
        state->ui, GSP_OBJ_KEY_PROMOTION, false);
    (void)esp_gsp_component_set_visible(
        state->ui, GSP_OBJ_KEY_RESIGN_CONFIRM, false);
    (void)esp_gsp_component_set_visible(
        state->ui, GSP_OBJ_KEY_DRAW_REQUEST, false);
    (void)esp_gsp_component_set_visible(
        state->ui, GSP_OBJ_KEY_CONNECTION_LOST, false);
    (void)esp_gsp_component_set_visible(
        state->ui, GSP_OBJ_KEY_SYNC_ERROR, false);
    if (game_session_start(state->session, mode, local_color) !=
        GAME_SESSION_OK) {
        show_only(state, SCR_S10);
        return;
    }
    initialize_piece_tracking();
    sync_pieces(state);
    sync_feedback(state);
    show_only(state, SCR_PLAY);
}

static int hit_square(uint16_t action)
{
    for (int square = 0; square < 64; ++square) {
        if (s_hit_actions[square] == action) {
            return square;
        }
    }
    return -1;
}

static void reset_wait(game_ui_state_t *state)
{
    state->wait_ticks = 0;
    state->wait_x = WAIT_X0;
    state->wait_direction = 1;
    (void)esp_gsp_component_set_position(
        state->ui, GSP_OBJ_KEY_WAITBAR, WAIT_X0, WAIT_Y);
}

static void wait_tick(esp_gsp_handle_t ui, void *context)
{
    game_ui_state_t *state = context;
    if (state->screen != SCR_S20) {
        return;
    }
    state->wait_x += state->wait_direction * WAIT_STEP;
    if (state->wait_x >= WAIT_X1) {
        state->wait_x = WAIT_X1;
        state->wait_direction = -1;
    } else if (state->wait_x <= WAIT_X0) {
        state->wait_x = WAIT_X0;
        state->wait_direction = 1;
    }
    (void)esp_gsp_component_set_position(
        ui, GSP_OBJ_KEY_WAITBAR, state->wait_x, WAIT_Y);
    if (++state->wait_ticks >= WAIT_TIMEOUT_TICKS) {
        show_only(state, SCR_S22);
    }
}

static void on_ui_event(esp_gsp_handle_t ui,
                        const esp_gsp_event_t *event,
                        void *context)
{
    (void)ui;
    game_ui_state_t *state = context;
    if (event->type != ESP_GSP_EVENT_CALL) {
        return;
    }
    const int square = hit_square(event->action_id);
    if (square >= 0) {
        tap_square(state, square);
        return;
    }
    switch (event->action_id) {
    case GSP_ACT_ID_ENTER_CHESS:
        show_only(state, SCR_S10);
        break;
    case GSP_ACT_ID_GO_PVE:
        start_game(state, GAME_MODE_PVE, CHESS_WHITE);
        break;
    case GSP_ACT_ID_GO_PVP:
        show_only(state, SCR_S20);
        reset_wait(state);
        break;
    case GSP_ACT_ID_PVP_DEVELOPING:
#if CONFIG_MOSAICO_GAME_DEV_PVP
        show_only(state, SCR_S20);
        reset_wait(state);
#else
        show_only(state, SCR_S23);
#endif
        break;
    case GSP_ACT_ID_WAIT_CANCEL:
    case GSP_ACT_ID_BACK_S00:
        show_only(state, event->action_id == GSP_ACT_ID_BACK_S00
                             ? SCR_S00
                             : SCR_S10);
        break;
    case GSP_ACT_ID_SIM_PAIR_OK:
        show_only(state, SCR_S21);
        break;
    case GSP_ACT_ID_S21_CONTINUE:
        show_only(state, SCR_S24);
        break;
    case GSP_ACT_ID_S24_GO:
        start_game(state, GAME_MODE_PVP, CHESS_WHITE);
        break;
    case GSP_ACT_ID_OPEN_MENU:
        break; /* The scene's declarative action shows the menu. */
    case GSP_ACT_ID_RESIGN:
    case GSP_ACT_ID_LEAVE_GAME:
        (void)esp_gsp_component_set_visible(
            state->ui, GSP_OBJ_KEY_MENU, false);
        (void)esp_gsp_component_set_visible(
            state->ui, GSP_OBJ_KEY_RESIGN_CONFIRM, true);
        break;
    case GSP_ACT_ID_CANCEL_RESIGN:
        (void)esp_gsp_component_set_visible(
            state->ui, GSP_OBJ_KEY_RESIGN_CONFIRM, false);
        break;
    case GSP_ACT_ID_CONFIRM_RESIGN:
        stop_ai(state);
        (void)esp_gsp_component_set_visible(
            state->ui, GSP_OBJ_KEY_RESIGN_CONFIRM, false);
        if (game_session_resign(state->session) == GAME_SESSION_OK) {
            show_result(state);
        }
        break;
    case GSP_ACT_ID_OFFER_DRAW:
        if (game_session_offer_draw(state->session) == GAME_SESSION_OK) {
            (void)esp_gsp_component_set_visible(
                state->ui, GSP_OBJ_KEY_MENU, false);
            /* Development PvP loop: expose the peer response UI locally. */
            (void)esp_gsp_component_set_visible(
                state->ui, GSP_OBJ_KEY_DRAW_REQUEST, true);
        }
        break;
    case GSP_ACT_ID_DRAW_REJECT:
        (void)game_session_resolve_draw_offer(state->session, false);
        (void)esp_gsp_component_set_visible(
            state->ui, GSP_OBJ_KEY_DRAW_REQUEST, false);
        break;
    case GSP_ACT_ID_DRAW_ACCEPT:
        (void)esp_gsp_component_set_visible(
            state->ui, GSP_OBJ_KEY_DRAW_REQUEST, false);
        if (game_session_resolve_draw_offer(
                state->session, true) == GAME_SESSION_OK) {
            show_result(state);
        }
        break;
    case GSP_ACT_ID_DEV_DISCONNECT:
#if CONFIG_MOSAICO_GAME_DEV_PVP
        game_session_set_connected(state->session, false);
        (void)esp_gsp_component_set_visible(
            state->ui, GSP_OBJ_KEY_MENU, false);
        (void)esp_gsp_component_set_visible(
            state->ui, GSP_OBJ_KEY_CONNECTION_LOST, true);
#endif
        break;
    case GSP_ACT_ID_DEV_RECONNECT:
#if CONFIG_MOSAICO_GAME_DEV_PVP
        game_session_set_connected(state->session, true);
        (void)esp_gsp_component_set_visible(
            state->ui, GSP_OBJ_KEY_CONNECTION_LOST, false);
#endif
        break;
    case GSP_ACT_ID_DEV_SYNC_ERROR:
#if CONFIG_MOSAICO_GAME_DEV_PVP
        game_session_set_sync_error(state->session);
        (void)esp_gsp_component_set_visible(
            state->ui, GSP_OBJ_KEY_CONNECTION_LOST, false);
        (void)esp_gsp_component_set_visible(
            state->ui, GSP_OBJ_KEY_SYNC_ERROR, true);
#endif
        break;
    case GSP_ACT_ID_DEV_RETRY_SYNC:
#if CONFIG_MOSAICO_GAME_DEV_PVP
        game_session_set_connected(state->session, true);
        (void)esp_gsp_component_set_visible(
            state->ui, GSP_OBJ_KEY_SYNC_ERROR, false);
#endif
        break;
    case GSP_ACT_ID_PROMOTE_Q:
        choose_promotion(state, CHESS_PIECE_QUEEN);
        break;
    case GSP_ACT_ID_PROMOTE_R:
        choose_promotion(state, CHESS_PIECE_ROOK);
        break;
    case GSP_ACT_ID_PROMOTE_B:
        choose_promotion(state, CHESS_PIECE_BISHOP);
        break;
    case GSP_ACT_ID_PROMOTE_N:
        choose_promotion(state, CHESS_PIECE_KNIGHT);
        break;
    case GSP_ACT_ID_RESULT_MENU:
        stop_ai(state);
        show_only(state, SCR_S10);
        break;
    case GSP_ACT_ID_RESULT_AGAIN:
        if (state->pve) {
            start_game(state, GAME_MODE_PVE, CHESS_WHITE);
        } else {
            show_only(state, SCR_S20);
            reset_wait(state);
        }
        break;
    default:
        break;
    }
}

esp_gsp_err_t game_ui_init(esp_gsp_handle_t ui)
{
    if (ui == NULL || s_state != NULL) {
        return ESP_GSP_ERR_INVALID_ARG;
    }
    game_ui_state_t *state = calloc(1, sizeof(*state));
    if (state == NULL) {
        return ESP_GSP_ERR_NO_MEM;
    }
    state->ui = ui;
    state->screen = -1;
    state->selected = -1;
    if (game_session_create(&state->session) != GAME_SESSION_OK) {
        free(state);
        return ESP_GSP_ERR_NO_MEM;
    }
    esp_gsp_err_t error = esp_gsp_on_event(ui, on_ui_event, state);
    if (error != ESP_GSP_OK) {
        game_session_destroy(state->session);
        free(state);
        return error;
    }
    state->wait_timer =
        esp_gsp_timer_create(ui, WAIT_TICK_MS, wait_tick, state);
    if (state->wait_timer == NULL) {
        game_session_destroy(state->session);
        free(state);
        return ESP_GSP_ERR_NO_MEM;
    }
#if CONFIG_MOSAICO_GAME_DEV_PVP
    (void)esp_gsp_component_set_visible(
        ui, GSP_OBJ_KEY_SIM_OK, true);
#endif
    s_state = state;
    show_only(state, SCR_S00);
#ifndef ESP_PLATFORM
    /*
     * Native replay seam: start the real shared C backend at a supplied
     * position without adding a production UI or a second rules engine.
     */
    const char *sim_fen = getenv("MOSAICO_GAME_SIM_FEN");
    if (sim_fen != NULL && sim_fen[0] != '\0') {
        start_game(state, GAME_MODE_PVP, CHESS_WHITE);
        if (game_session_load_fen(state->session, sim_fen) ==
            GAME_SESSION_OK) {
            sync_pieces(state);
            sync_feedback(state);
        }
    }
#endif
    return ESP_GSP_OK;
}

void game_ui_deinit(esp_gsp_handle_t ui)
{
    if (ui == NULL) {
        return;
    }
    game_ui_state_t *state = s_state;
    if (state == NULL) {
        return;
    }
    stop_ai(state);
    if (state->wait_timer != NULL) {
        (void)esp_gsp_timer_delete(ui, state->wait_timer);
        state->wait_timer = NULL;
    }
    game_session_destroy(state->session);
    s_state = NULL;
    free(state);
}
