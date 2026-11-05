// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stdint.h>

/* Native-only haptic helper shared by standalone example firmware. */
void raylib_lite_native_feedback_init(void);
void raylib_lite_native_feedback_pulse(uint8_t strength, uint16_t duration_ms);
void raylib_lite_native_feedback_pattern(uint8_t first_strength, uint16_t first_ms,
                                     uint8_t second_strength, uint16_t gap_ms,
                                     uint16_t second_ms);
void raylib_lite_native_feedback_stop(void);
