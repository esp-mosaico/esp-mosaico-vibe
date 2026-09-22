// SPDX-License-Identifier: Apache-2.0

#include <assert.h>
#include <stdbool.h>

static bool hardware_supported;
static bool panel_ok;
static bool draw_ok;
static bool handoff;
static bool feedback_active;
static int feedback_starts;
static int panel_calls;
static int draw_calls;
static int handoff_publications;

static void mosaico_boot_handoff_clear(void) { handoff = false; }

static bool hardware_version_supported(void) {
    assert(!handoff);  // A previous boot's marker must already be cleared.
    return hardware_supported;
}

static void boot_feedback_start(void) {
    feedback_active = true;
    ++feedback_starts;
}

static void boot_feedback_stop(void) { feedback_active = false; }

static bool panel_init(void) {
    assert(!handoff);
    ++panel_calls;
    return panel_ok;
}

static bool draw_splash(void) {
    assert(!handoff);
    ++draw_calls;
    return draw_ok;
}

static void mosaico_boot_handoff_publish(void) {
    assert(hardware_supported && panel_calls == 1 && panel_ok);
    assert(draw_calls == 1 && draw_ok);
    assert(!feedback_active);
    handoff = true;
    ++handoff_publications;
}

#define ESP_LOGW(...) ((void)0)
#define ESP_LOGI(...) ((void)0)
#include "splash_entry.inc"

int main(void) {
    // Cover unsupported hardware, panel failure, draw failure and success,
    // including repeated invocations with a stale handoff marker.
    for (int flags = 0; flags < 8; ++flags) {
        hardware_supported = (flags & 1) != 0;
        panel_ok = (flags & 2) != 0;
        draw_ok = (flags & 4) != 0;
        handoff = true;
        feedback_active = false;
        feedback_starts = panel_calls = draw_calls = handoff_publications = 0;

        const bool expected = hardware_supported && panel_ok && draw_ok;
        assert(mosaico_boot_splash_show() == expected);
        assert(handoff == expected);
        assert(handoff_publications == (expected ? 1 : 0));
        assert(!feedback_active);
        assert(feedback_starts == (hardware_supported ? 1 : 0));
        assert(panel_calls == (hardware_supported ? 1 : 0));
        assert(draw_calls == (hardware_supported && panel_ok ? 1 : 0));
    }
    return 0;
}
