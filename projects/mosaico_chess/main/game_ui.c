// SPDX-License-Identifier: Apache-2.0

#define GSP_BUNDLE_ENABLE_LEGACY_NAMES
#include "bundle_gsp.h"
#include "chess_peer_link.h"
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
#define PIECE_W 34
#define PIECE_H 68
#define WAIT_X0 127
#define WAIT_X1 273
#define WAIT_STEP 8
#define WAIT_TICK_MS 50
#define WAIT_TIMEOUT_TICKS (12000 / WAIT_TICK_MS)
#define PAIR_TRANSITION_MS 3040
#define AI_DELAY_MS 450
#define NAVIGATION_TICK_MS 10
#define PEER_TICK_MS 50
#define MENU_SWIPE_MIN_Y 80
#define MENU_SWIPE_MAX_X 120
#define BASE_PIECE_SLOT_COUNT 32

enum {
    SCR_S00 = 0,
    SCR_S10,
    SCR_S11,
    SCR_S20,
    SCR_S20_SUCCESS_ANIM,
    SCR_S20_FAIL_ANIM,
    SCR_S21,
    SCR_S21_WAIT,
    SCR_S22,
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
    chess_ai_level_t ai_level;
    chess_move_t pending_promotion;
    bool promotion_pending;
    bool has_last_move;
    chess_move_t last_move;
    void *ai_timer;
    void *navigation_timer;
    void *peer_timer;
    void *transition_timer;
    void *wait_timer;
    int wait_ticks;
    int wait_x;
    int wait_direction;
    bool pointer_pressed;
    bool pointer_started_in_play;
    bool pending_menu;
    int pointer_start_x;
    int pointer_start_y;
    bool board_flipped;
    bool syncing;
    bool english;
    bool paired_as_black;
    uint16_t sync_total_ply;
    int transition_target;
    int pending_screen;
} game_ui_state_t;

/* GSP exposes one application backend per handle and no user-data getter.
 * This module therefore owns one explicitly allocated backend instance. */
static game_ui_state_t *s_state;

#ifndef ESP_PLATFORM
static int sim_square_from_text(const char *text)
{
    if (text == NULL || text[0] < 'a' || text[0] > 'h' ||
        text[1] < '1' || text[1] > '8') {
        return -1;
    }
    return text[0] - 'a' + (text[1] - '1') * 8;
}
#endif

static const gsp_component_key_t s_screens[] = {
    GSP_OBJ_KEY_S00, GSP_OBJ_KEY_S10, GSP_OBJ_KEY_S11,
    GSP_OBJ_KEY_S20,
    GSP_OBJ_KEY_S20_SUCCESS_ANIM, GSP_OBJ_KEY_S20_FAIL_ANIM,
    GSP_OBJ_KEY_S21, GSP_OBJ_KEY_S21_WAIT, GSP_OBJ_KEY_S22,
    GSP_OBJ_KEY_PLAY, GSP_OBJ_KEY_S50,
    GSP_OBJ_KEY_S51, GSP_OBJ_KEY_S52,
};

typedef struct {
    gsp_component_key_t zh;
    gsp_component_key_t en;
} localized_asset_pair_t;

static const localized_asset_pair_t s_localized_assets[] = {
    {GSP_OBJ_KEY_S00_BG_ZH, GSP_OBJ_KEY_S00_BG_EN},
    {GSP_OBJ_KEY_S10_BG_ZH, GSP_OBJ_KEY_S10_BG_EN},
    {GSP_OBJ_KEY_S11_BG_ZH, GSP_OBJ_KEY_S11_BG_EN},
    {GSP_OBJ_KEY_S20_BG_ZH, GSP_OBJ_KEY_S20_BG_EN},
    {
        GSP_OBJ_KEY_S20_SUCCESS_ANIM_BG_ZH,
        GSP_OBJ_KEY_S20_SUCCESS_ANIM_BG_EN,
    },
    {
        GSP_OBJ_KEY_S20_FAIL_ANIM_BG_ZH,
        GSP_OBJ_KEY_S20_FAIL_ANIM_BG_EN,
    },
    {
        GSP_OBJ_KEY_S21_WAIT_BG_ZH,
        GSP_OBJ_KEY_S21_WAIT_BG_EN,
    },
    {GSP_OBJ_KEY_S22_BG_ZH, GSP_OBJ_KEY_S22_BG_EN},
    {GSP_OBJ_KEY_S50_BG_ZH, GSP_OBJ_KEY_S50_BG_EN},
    {GSP_OBJ_KEY_S51_BG_ZH, GSP_OBJ_KEY_S51_BG_EN},
    {GSP_OBJ_KEY_S52_BG_ZH, GSP_OBJ_KEY_S52_BG_EN},
    {GSP_OBJ_KEY_PROMOTION_PANEL_ZH, GSP_OBJ_KEY_PROMOTION_PANEL_EN},
    {
        GSP_OBJ_KEY_RESIGN_CONFIRM_PANEL_ZH,
        GSP_OBJ_KEY_RESIGN_CONFIRM_PANEL_EN,
    },
    {
        GSP_OBJ_KEY_CONNECTION_LOST_PANEL_ZH,
        GSP_OBJ_KEY_CONNECTION_LOST_PANEL_EN,
    },
    {
        GSP_OBJ_KEY_SYNC_ERROR_PANEL_ZH,
        GSP_OBJ_KEY_SYNC_ERROR_PANEL_EN,
    },
    {GSP_OBJ_KEY_MENU_PANEL_ZH, GSP_OBJ_KEY_MENU_PANEL_EN},
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

static int display_square(const game_ui_state_t *state, int square)
{
    return state->board_flipped ? 63 - square : square;
}

static void cell_xy(const game_ui_state_t *state, int square, int *x, int *y)
{
    const int shown = display_square(state, square);
    *x = FRAME + (shown & 7) * CELL;
    *y = FRAME + (7 - (shown >> 3)) * CELL;
}

static void piece_xy(const game_ui_state_t *state, int square,
                     chess_piece_type_t type, int *x, int *y)
{
    (void)type;
    int cell_x = 0;
    int cell_y = 0;
    cell_xy(state, square, &cell_x, &cell_y);
    *x = cell_x + (CELL - PIECE_W) / 2;
    *y = cell_y + CELL - PIECE_H - 4;
}

static void apply_language(game_ui_state_t *state);

static void show_only(game_ui_state_t *state, int screen)
{
    for (size_t index = 0;
         index < sizeof(s_screens) / sizeof(s_screens[0]); ++index) {
        (void)esp_gsp_component_set_visible(
            state->ui, s_screens[index], false);
    }
    state->screen = screen;
    apply_language(state);
    (void)esp_gsp_component_set_visible(
        state->ui, s_screens[screen], true);
    (void)esp_gsp_set_press_feedback_enabled(
        state->ui, screen != SCR_PLAY);
#ifndef ESP_PLATFORM
    (void)esp_gsp_component_set_visible(
        state->ui, GSP_OBJ_KEY_SIM_PEER_READY,
        screen == SCR_S21_WAIT);
#endif
}

static void apply_language(game_ui_state_t *state)
{
    for (size_t index = 0;
         index < sizeof(s_localized_assets) / sizeof(s_localized_assets[0]);
         ++index) {
        (void)esp_gsp_component_set_visible(
            state->ui, s_localized_assets[index].zh, !state->english);
        (void)esp_gsp_component_set_visible(
            state->ui, s_localized_assets[index].en, state->english);
    }
    (void)esp_gsp_component_set_visible(
        state->ui, GSP_OBJ_KEY_S21_BG_ZH,
        !state->paired_as_black && !state->english);
    (void)esp_gsp_component_set_visible(
        state->ui, GSP_OBJ_KEY_S21_BG_EN,
        !state->paired_as_black && state->english);
    (void)esp_gsp_component_set_visible(
        state->ui, GSP_OBJ_KEY_S21_BG_BLACK_ZH,
        state->paired_as_black && !state->english);
    (void)esp_gsp_component_set_visible(
        state->ui, GSP_OBJ_KEY_S21_BG_BLACK_EN,
        state->paired_as_black && state->english);
}

static void show_localized_overlay(
    game_ui_state_t *state, gsp_component_key_t key)
{
    (void)esp_gsp_component_set_visible(state->ui, key, true);
    apply_language(state);
}

static void track_pointer(esp_gsp_handle_t ui, int32_t x, int32_t y,
                          bool pressed, void *context)
{
    (void)ui;
    game_ui_state_t *state = context;
    if (pressed && !state->pointer_pressed) {
        state->pointer_start_x = x;
        state->pointer_start_y = y;
        state->pointer_started_in_play = state->screen == SCR_PLAY;
    } else if (!pressed && state->pointer_pressed) {
        const int delta_x = x - state->pointer_start_x;
        const int delta_y = state->pointer_start_y - y;
        if (state->pointer_started_in_play &&
            delta_y >= MENU_SWIPE_MIN_Y &&
            abs(delta_x) <= MENU_SWIPE_MAX_X) {
            state->pending_menu = true;
        }
        state->pointer_started_in_play = false;
    }
    state->pointer_pressed = pressed;
}

static void finish_pending_navigation(esp_gsp_handle_t ui, void *context)
{
    (void)ui;
    game_ui_state_t *state = context;
    if (state->pointer_pressed) {
        return;
    }
    if (state->pending_menu) {
        state->pending_menu = false;
        show_localized_overlay(state, GSP_OBJ_KEY_MENU);
    }
    if (state->pending_screen < 0) {
        return;
    }
    const int screen = state->pending_screen;
    state->pending_screen = -1;
    show_only(state, screen);
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
                state->ui, s_dots[display_square(state, square)], false);
        }
        if ((capture_visible & (1ULL << square)) != 0) {
            (void)esp_gsp_component_set_visible(
                state->ui, s_captures[display_square(state, square)], false);
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
    cell_xy(state, square, &x, &y);
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
                state->ui, s_dots[display_square(state, square)], new_visible);
        }
        const bool old_capture =
            (state->captures & (1ULL << square)) != 0;
        const bool new_capture = (captures & (1ULL << square)) != 0;
        if (old_capture != new_capture) {
            (void)esp_gsp_component_set_visible(
                state->ui, s_captures[display_square(state, square)],
                new_capture);
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
            int x = 0;
            int y = 0;
            piece_xy(state, sprite->square, sprite->type, &x, &y);
            (void)esp_gsp_component_set_position(
                state->ui, sprite->key, x, y);
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
        piece_xy(state, square, sprite->type, &x, &y);
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
    const int shown = display_square(state, square);
    const int x = (shown & 7) * CELL;
    const int y = (7 - (shown >> 3)) * CELL;
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

static void show_result(game_ui_state_t *state)
{
    const game_session_status_t status =
        game_session_status(state->session);
    int screen = SCR_S52;
    if (status.chess.winner >= 0) {
        const bool local_won =
            status.chess.winner == status.local_color;
        screen = local_won ? SCR_S50 : SCR_S51;
    }
    show_only(state, screen);
}

static void after_move(game_ui_state_t *state);

static void ai_tick(esp_gsp_handle_t ui, void *context)
{
    (void)ui;
    game_ui_state_t *state = context;
    if (state->ai_timer != NULL) {
        (void)esp_gsp_timer_delete(state->ui, state->ai_timer);
        state->ai_timer = NULL;
    }
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
    const uint64_t before_hash =
        chess_game_position_hash(game_session_game(state->session));
    if (game_session_apply_local(state->session, move) == GAME_SESSION_OK) {
        const uint64_t after_hash =
            chess_game_position_hash(game_session_game(state->session));
        if (!state->pve &&
            !chess_peer_link_send_move(move, before_hash, after_hash)) {
            game_session_set_sync_error(state->session);
            show_localized_overlay(state, GSP_OBJ_KEY_SYNC_ERROR);
        }
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
                show_localized_overlay(state, GSP_OBJ_KEY_PROMOTION);
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
                       chess_color_t local_color,
                       chess_ai_level_t ai_level)
{
    stop_ai(state);
    hide_selection(state);
    state->pve = mode == GAME_MODE_PVE;
    state->ai_level = ai_level;
    state->board_flipped =
        mode == GAME_MODE_PVP && local_color == CHESS_BLACK;
    state->syncing = false;
    state->promotion_pending = false;
    state->has_last_move = false;
    (void)esp_gsp_component_set_visible(
        state->ui, GSP_OBJ_KEY_MENU, false);
    (void)esp_gsp_component_set_visible(
        state->ui, GSP_OBJ_KEY_PROMOTION, false);
    (void)esp_gsp_component_set_visible(
        state->ui, GSP_OBJ_KEY_RESIGN_CONFIRM, false);
    (void)esp_gsp_component_set_visible(
        state->ui, GSP_OBJ_KEY_CONNECTION_LOST, false);
    (void)esp_gsp_component_set_visible(
        state->ui, GSP_OBJ_KEY_SYNC_ERROR, false);
    if (game_session_start_with_difficulty(
            state->session, mode, local_color, ai_level) != GAME_SESSION_OK) {
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

static void stop_pair_transition(game_ui_state_t *state)
{
    if (state->transition_timer != NULL) {
        (void)esp_gsp_timer_delete(
            state->ui, state->transition_timer);
        state->transition_timer = NULL;
    }
    state->transition_target = -1;
}

static void finish_pair_transition(esp_gsp_handle_t ui, void *context)
{
    (void)ui;
    game_ui_state_t *state = context;
    const int target = state->transition_target;
    stop_pair_transition(state);
    if (target >= 0) {
        show_only(state, target);
    }
}

static void start_pair_transition(game_ui_state_t *state,
                                  int animation_screen,
                                  int target_screen)
{
    stop_pair_transition(state);
    state->transition_target = target_screen;
    show_only(state, animation_screen);
    state->transition_timer = esp_gsp_timer_create(
        state->ui, PAIR_TRANSITION_MS,
        finish_pair_transition, state);
    if (state->transition_timer == NULL) {
        finish_pair_transition(state->ui, state);
    }
}

static void reset_wait(game_ui_state_t *state)
{
    stop_pair_transition(state);
    state->wait_ticks = 0;
    state->wait_x = WAIT_X0;
    state->wait_direction = 1;
    state->paired_as_black = false;
    apply_language(state);
    (void)gsp_main_waitbar_set_x(state->ui, WAIT_X0);
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
    (void)gsp_main_waitbar_set_x(ui, state->wait_x);
    if (++state->wait_ticks >= WAIT_TIMEOUT_TICKS) {
        start_pair_transition(
            state, SCR_S20_FAIL_ANIM, SCR_S22);
    }
}

static void peer_tick(esp_gsp_handle_t ui, void *context)
{
    (void)ui;
    game_ui_state_t *state = context;
    chess_peer_event_t event;
    while (chess_peer_link_poll_event(&event)) {
        switch (event.kind) {
        case CHESS_PEER_EVENT_PAIRED:
            state->paired_as_black = event.local_color == CHESS_BLACK;
            apply_language(state);
            start_pair_transition(
                state, SCR_S20_SUCCESS_ANIM, SCR_S21);
            break;
        case CHESS_PEER_EVENT_STARTED:
            start_game(state, GAME_MODE_PVP, event.local_color,
                       CHESS_AI_NORMAL);
            (void)chess_peer_link_set_position(
                0, chess_game_position_hash(
                       game_session_game(state->session)));
            break;
        case CHESS_PEER_EVENT_REMOTE_MOVE: {
            const uint64_t before_hash = chess_game_position_hash(
                game_session_game(state->session));
            if (before_hash != event.data.remote_move.before_hash ||
                game_session_apply_remote(
                    state->session, &event.data.remote_move.move) !=
                    GAME_SESSION_OK) {
                game_session_set_sync_error(state->session);
                show_localized_overlay(state, GSP_OBJ_KEY_SYNC_ERROR);
                (void)chess_peer_link_request_sync(0, before_hash);
                break;
            }
            const uint64_t after_hash = chess_game_position_hash(
                game_session_game(state->session));
            if (after_hash != event.data.remote_move.after_hash ||
                !chess_peer_link_accept_remote_move(
                    event.data.remote_move.ply,
                    &event.data.remote_move.move, after_hash)) {
                game_session_set_sync_error(state->session);
                show_localized_overlay(state, GSP_OBJ_KEY_SYNC_ERROR);
                (void)chess_peer_link_request_sync(0, after_hash);
                break;
            }
            state->last_move = event.data.remote_move.move;
            state->has_last_move = true;
            after_move(state);
            break;
        }
        case CHESS_PEER_EVENT_REMOTE_RESIGNED:
            if (game_session_resign_color(
                    state->session, event.data.resign.color) ==
                GAME_SESSION_OK) {
                show_result(state);
            }
            break;
        case CHESS_PEER_EVENT_DISCONNECTED:
            game_session_set_connected(state->session, false);
            show_localized_overlay(state, GSP_OBJ_KEY_CONNECTION_LOST);
            break;
        case CHESS_PEER_EVENT_RECONNECTED: {
            game_session_set_connected(state->session, true);
            (void)esp_gsp_component_set_visible(
                state->ui, GSP_OBJ_KEY_CONNECTION_LOST, false);
            const game_session_status_t status =
                game_session_status(state->session);
            if (status.local_color == CHESS_BLACK) {
                (void)chess_peer_link_request_sync(
                    0, chess_game_position_hash(
                           game_session_game(state->session)));
            }
            break;
        }
        case CHESS_PEER_EVENT_SYNC_BEGIN: {
            const game_session_status_t status =
                game_session_status(state->session);
            state->syncing = true;
            state->sync_total_ply = 0;
            state->has_last_move = false;
            (void)game_session_start(
                state->session, GAME_MODE_PVP, status.local_color);
            break;
        }
        case CHESS_PEER_EVENT_SYNC_CHUNK:
            if (!state->syncing) {
                break;
            }
            for (uint8_t i = 0;
                 i < event.data.sync_chunk.count; ++i) {
                if (game_session_apply_synced(
                        state->session,
                        &event.data.sync_chunk.moves[i]) != GAME_SESSION_OK) {
                    state->syncing = false;
                    game_session_set_sync_error(state->session);
                    show_localized_overlay(state, GSP_OBJ_KEY_SYNC_ERROR);
                    break;
                }
                state->last_move = event.data.sync_chunk.moves[i];
                state->has_last_move = true;
            }
            state->sync_total_ply =
                event.data.sync_chunk.total_ply;
            break;
        case CHESS_PEER_EVENT_SYNC_COMPLETE: {
            const uint64_t hash = chess_game_position_hash(
                game_session_game(state->session));
            if (!state->syncing ||
                !chess_peer_link_complete_sync(
                    state->sync_total_ply, hash)) {
                game_session_set_sync_error(state->session);
                show_localized_overlay(state, GSP_OBJ_KEY_SYNC_ERROR);
                break;
            }
            state->syncing = false;
            (void)esp_gsp_component_set_visible(
                state->ui, GSP_OBJ_KEY_SYNC_ERROR, false);
            sync_pieces(state);
            sync_feedback(state);
            after_move(state);
            break;
        }
        case CHESS_PEER_EVENT_SYNC_ERROR:
            game_session_set_sync_error(state->session);
            show_localized_overlay(state, GSP_OBJ_KEY_SYNC_ERROR);
            break;
        }
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
        tap_square(state, state->board_flipped ? 63 - square : square);
        return;
    }
    switch (event->action_id) {
    case GSP_ACT_ID_TOGGLE_LANGUAGE:
        state->english = !state->english;
        apply_language(state);
        break;
    case GSP_ACT_ID_ENTER_CHESS:
        show_only(state, SCR_S10);
        break;
    case GSP_ACT_ID_GO_PVE:
        /*
         * The mode and difficulty hit areas overlap.  Do not reveal S11
         * until this touch has been released, so its release cannot trigger
         * a difficulty button.
         */
        if (state->pointer_pressed) {
            state->pending_screen = SCR_S11;
        } else {
            show_only(state, SCR_S11);
        }
        break;
    case GSP_ACT_ID_DIFFICULTY_EASY:
        start_game(
            state, GAME_MODE_PVE, CHESS_WHITE, CHESS_AI_EASY);
        break;
    case GSP_ACT_ID_DIFFICULTY_NORMAL:
        start_game(
            state, GAME_MODE_PVE, CHESS_WHITE, CHESS_AI_NORMAL);
        break;
    case GSP_ACT_ID_DIFFICULTY_HARD:
        start_game(
            state, GAME_MODE_PVE, CHESS_WHITE, CHESS_AI_HARD);
        break;
    case GSP_ACT_ID_GO_PVP:
        show_only(state, SCR_S20);
        reset_wait(state);
        (void)chess_peer_link_begin_pairing();
        break;
    case GSP_ACT_ID_PVP_DEVELOPING:
        show_only(state, SCR_S20);
        reset_wait(state);
        (void)chess_peer_link_begin_pairing();
        break;
    case GSP_ACT_ID_WAIT_CANCEL:
    case GSP_ACT_ID_BACK_S00:
        if (event->action_id == GSP_ACT_ID_WAIT_CANCEL) {
            chess_peer_link_leave();
        }
        stop_pair_transition(state);
        show_only(state, event->action_id == GSP_ACT_ID_BACK_S00
                             ? SCR_S00
                             : SCR_S10);
        break;
    case GSP_ACT_ID_BACK_S10:
        show_only(state, SCR_S10);
        break;
    case GSP_ACT_ID_SIM_PAIR_OK:
#if CONFIG_MOSAICO_GAME_DEV_PVP
        state->paired_as_black = false;
        apply_language(state);
        start_pair_transition(
            state, SCR_S20_SUCCESS_ANIM, SCR_S21);
#endif
        break;
    case GSP_ACT_ID_S21_CONTINUE:
        stop_pair_transition(state);
#ifdef ESP_PLATFORM
        (void)chess_peer_link_set_ready();
#endif
        state->wait_x = WAIT_X0;
        state->wait_direction = 1;
        show_only(state, SCR_S21_WAIT);
        break;
    case GSP_ACT_ID_SIM_PEER_READY:
#ifndef ESP_PLATFORM
        start_game(
            state, GAME_MODE_PVP, CHESS_WHITE, CHESS_AI_NORMAL);
#endif
        break;
    case GSP_ACT_ID_OPEN_MENU:
        show_localized_overlay(state, GSP_OBJ_KEY_MENU);
        break;
    case GSP_ACT_ID_RESIGN:
        (void)esp_gsp_component_set_visible(
            state->ui, GSP_OBJ_KEY_MENU, false);
        show_localized_overlay(state, GSP_OBJ_KEY_RESIGN_CONFIRM);
        break;
    case GSP_ACT_ID_CANCEL_RESIGN:
        (void)esp_gsp_component_set_visible(
            state->ui, GSP_OBJ_KEY_RESIGN_CONFIRM, false);
        break;
    case GSP_ACT_ID_CONFIRM_RESIGN:
        stop_ai(state);
        (void)esp_gsp_component_set_visible(
            state->ui, GSP_OBJ_KEY_RESIGN_CONFIRM, false);
        if (!state->pve) {
            const game_session_status_t status =
                game_session_status(state->session);
            (void)chess_peer_link_send_resign(status.local_color);
        }
        if (game_session_resign(state->session) == GAME_SESSION_OK) {
            show_result(state);
        }
        break;
    case GSP_ACT_ID_CONNECTION_MENU:
        stop_ai(state);
        (void)esp_gsp_component_set_visible(
            state->ui, GSP_OBJ_KEY_CONNECTION_LOST, false);
        (void)esp_gsp_component_set_visible(
            state->ui, GSP_OBJ_KEY_SYNC_ERROR, false);
        chess_peer_link_leave();
        show_only(state, SCR_S10);
        break;
    case GSP_ACT_ID_DEV_RECONNECT:
#ifdef ESP_PLATFORM
        (void)chess_peer_link_request_sync(
            0, chess_game_position_hash(
                   game_session_game(state->session)));
#elif CONFIG_MOSAICO_GAME_DEV_PVP
        game_session_set_connected(state->session, true);
        (void)esp_gsp_component_set_visible(
            state->ui, GSP_OBJ_KEY_CONNECTION_LOST, false);
#endif
        break;
    case GSP_ACT_ID_DEV_RETRY_SYNC:
#ifdef ESP_PLATFORM
        (void)chess_peer_link_request_sync(
            0, chess_game_position_hash(
                   game_session_game(state->session)));
#elif CONFIG_MOSAICO_GAME_DEV_PVP
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
        chess_peer_link_leave();
        show_only(state, SCR_S10);
        break;
    case GSP_ACT_ID_RESULT_AGAIN:
        if (state->pve) {
            start_game(
                state, GAME_MODE_PVE, CHESS_WHITE, state->ai_level);
        } else {
            show_only(state, SCR_S20);
            reset_wait(state);
            (void)chess_peer_link_begin_pairing();
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
    state->pending_screen = -1;
    state->transition_target = -1;
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
    error = esp_gsp_set_pointer_observer(ui, track_pointer, state);
    if (error != ESP_GSP_OK) {
        game_session_destroy(state->session);
        free(state);
        return error;
    }
    state->navigation_timer = esp_gsp_timer_create(
        ui, NAVIGATION_TICK_MS, finish_pending_navigation, state);
    if (state->navigation_timer == NULL) {
        (void)esp_gsp_set_pointer_observer(ui, NULL, NULL);
        game_session_destroy(state->session);
        free(state);
        return ESP_GSP_ERR_NO_MEM;
    }
    state->wait_timer =
        esp_gsp_timer_create(ui, WAIT_TICK_MS, wait_tick, state);
    if (state->wait_timer == NULL) {
        (void)esp_gsp_timer_delete(ui, state->navigation_timer);
        state->navigation_timer = NULL;
        (void)esp_gsp_set_pointer_observer(ui, NULL, NULL);
        game_session_destroy(state->session);
        free(state);
        return ESP_GSP_ERR_NO_MEM;
    }
    (void)chess_peer_link_init();
    state->peer_timer =
        esp_gsp_timer_create(ui, PEER_TICK_MS, peer_tick, state);
    if (state->peer_timer == NULL) {
        chess_peer_link_deinit();
        (void)esp_gsp_timer_delete(ui, state->wait_timer);
        (void)esp_gsp_timer_delete(ui, state->navigation_timer);
        state->wait_timer = NULL;
        state->navigation_timer = NULL;
        (void)esp_gsp_set_pointer_observer(ui, NULL, NULL);
        game_session_destroy(state->session);
        free(state);
        return ESP_GSP_ERR_NO_MEM;
    }
#if CONFIG_MOSAICO_GAME_DEV_PVP
    (void)esp_gsp_component_set_visible(
        ui, GSP_OBJ_KEY_SIM_OK, true);
#endif
    s_state = state;
    apply_language(state);
    show_only(state, SCR_S00);
#ifndef ESP_PLATFORM
    /*
     * Native replay seam: start the real shared C backend at a supplied
     * position without adding a production UI or a second rules engine.
     */
    const char *sim_language = getenv("MOSAICO_GAME_SIM_LANGUAGE");
    if (sim_language != NULL && strcmp(sim_language, "en") == 0) {
        state->english = true;
        apply_language(state);
    }
    const char *sim_fen = getenv("MOSAICO_GAME_SIM_FEN");
    if (sim_fen != NULL && sim_fen[0] != '\0') {
        const char *sim_color = getenv("MOSAICO_GAME_SIM_COLOR");
        const chess_color_t local_color =
            sim_color != NULL && strcmp(sim_color, "black") == 0 ?
                CHESS_BLACK : CHESS_WHITE;
        start_game(
            state, GAME_MODE_PVP, local_color, CHESS_AI_NORMAL);
        if (game_session_load_fen(state->session, sim_fen) ==
            GAME_SESSION_OK) {
            sync_pieces(state);
            sync_feedback(state);
            const char *sim_move = getenv("MOSAICO_GAME_SIM_MOVE");
            const int from = sim_square_from_text(sim_move);
            const int to =
                sim_move != NULL ? sim_square_from_text(sim_move + 2) : -1;
            if (from >= 0 && to >= 0) {
                const chess_move_t move = {
                    .from = (uint8_t)from,
                    .to = (uint8_t)to,
                    .promotion = CHESS_PIECE_NONE,
                };
                if (game_session_apply_local(state->session, &move) ==
                    GAME_SESSION_OK) {
                    state->last_move = move;
                    state->has_last_move = true;
                    after_move(state);
                }
            }
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
    stop_pair_transition(state);
    if (state->peer_timer != NULL) {
        (void)esp_gsp_timer_delete(ui, state->peer_timer);
        state->peer_timer = NULL;
    }
    if (state->wait_timer != NULL) {
        (void)esp_gsp_timer_delete(ui, state->wait_timer);
        state->wait_timer = NULL;
    }
    if (state->navigation_timer != NULL) {
        (void)esp_gsp_timer_delete(ui, state->navigation_timer);
        state->navigation_timer = NULL;
    }
    (void)esp_gsp_set_pointer_observer(ui, NULL, NULL);
    chess_peer_link_deinit();
    game_session_destroy(state->session);
    s_state = NULL;
    free(state);
}
