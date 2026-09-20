// SPDX-License-Identifier: Apache-2.0

#include "hello_ui.h"

#include <stdio.h>

#include "bundle_gsp.h"

#define CELEBRATION_TICK_MS 100
#define CELEBRATION_TICKS 30
#define CONFETTI_FRAMES 6

/* Visibility frames keep the animation portable across device and sim_bridge. */
static esp_gsp_err_t (*const confetti_visible[])(esp_gsp_handle_t, bool) = {
    gsp_hello_confetti_0_set_visible, gsp_hello_confetti_1_set_visible,
    gsp_hello_confetti_2_set_visible, gsp_hello_confetti_3_set_visible,
    gsp_hello_confetti_4_set_visible, gsp_hello_confetti_5_set_visible,
};

static void remember_error(hello_ui_t *state, esp_gsp_err_t err)
{
    if (err != ESP_GSP_OK) {
        state->last_error = err;
    }
}

static void celebrate_tick(esp_gsp_handle_t ui, void *ctx)
{
    hello_ui_t *state = ctx;
    if (++state->celebration_tick >= CELEBRATION_TICKS) {
        remember_error(state, gsp_hello_count_set_text(ui, "00"));
        remember_error(state, gsp_hello_celebration_set_visible(ui, false));
        remember_error(state, esp_gsp_timer_delete(ui, state->timer));
        state->timer = NULL;
        state->count = 0;
        return;
    }
    remember_error(state, confetti_visible[state->confetti_frame](ui, false));
    state->confetti_frame = (state->confetti_frame + 1) % CONFETTI_FRAMES;
    remember_error(state, confetti_visible[state->confetti_frame](ui, true));
}

static void say_hello(esp_gsp_handle_t ui, const esp_gsp_event_t *event,
                      void *ctx)
{
    hello_ui_t *state = ctx;
    if (!gsp_hello_event_is_say_hello(event) || state->timer) {
        return;
    }
    if (state->count == 99) {
        state->celebration_tick = 0;
        /* Allocate before showing the overlay: no stuck celebration on failure. */
        state->timer = esp_gsp_timer_create(ui, CELEBRATION_TICK_MS,
                                           celebrate_tick, state);
        if (!state->timer) {
            remember_error(state, ESP_GSP_ERR_NO_MEM);
            return;
        }
        remember_error(state, gsp_hello_celebration_set_visible(ui, true));
        state->count = 100;
        return;
    }
    char text[3];
    const uint32_t next = state->count + 1;
    snprintf(text, sizeof(text), "%02u", (unsigned)next);
    const esp_gsp_err_t err = gsp_hello_count_set_text(ui, text);
    remember_error(state, err);
    if (err == ESP_GSP_OK) {
        state->count = next;
    }
}

esp_gsp_err_t hello_ui_init(esp_gsp_handle_t ui, hello_ui_t *state)
{
    if (!ui || !state) {
        return ESP_GSP_ERR_INVALID_ARG;
    }
    *state = (hello_ui_t){0};
    remember_error(state, gsp_hello_count_set_text(ui, "00"));
    remember_error(state, gsp_hello_celebration_set_visible(ui, false));
    for (unsigned i = 0; i < CONFETTI_FRAMES; ++i) {
        remember_error(state, confetti_visible[i](ui, i == 0));
    }
    if (state->last_error == ESP_GSP_OK) {
        remember_error(state, esp_gsp_on_event(ui, say_hello, state));
    }
    return state->last_error;
}

void hello_ui_deinit(esp_gsp_handle_t ui, hello_ui_t *state)
{
    if (ui && state) {
        remember_error(state, esp_gsp_on_event(ui, NULL, NULL));
        if (state->timer) {
            remember_error(state, esp_gsp_timer_delete(ui, state->timer));
            state->timer = NULL;
        }
    }
}
