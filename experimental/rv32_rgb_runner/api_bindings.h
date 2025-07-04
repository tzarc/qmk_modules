// Copyright 2024-2025 Nick Brassel (@tzarc)
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// thunksuffix, ret_type, name, argcount, argtype1, argtype2, ...
#define RV32RGB_HYPERCALLS(X)                            \
    X(VOID, void, exit_vm, 0)                            \
    X(RET, uint32_t, timer_read32, 0)                    \
    X(RET, uint32_t, rgb_timer, 0)                       \
    X(RET, uint32_t, rand, 0)                            \
    X(RET, uint16_t, scale16by8, 2, uint16_t, uint8_t)   \
    X(RET, uint8_t, scale8, 2, uint8_t, uint8_t)         \
    X(RET, uint8_t, abs8, 1, uint8_t)                    \
    X(RET, uint8_t, sin8, 1, uint8_t)                    \
    X(RET, RV32_HSV, rgb_matrix_config_hsv, 0)           \
    X(RET, uint8_t, rgb_matrix_config_speed, 0)          \
    X(RET, RV32_RGB, rgb_matrix_hsv_to_rgb, 1, RV32_HSV) \
    X(VOID, void, rgb_matrix_set_color, 4, int, uint8_t, uint8_t, uint8_t)

// thunksuffix, ret_type, name, argcount, argtype1, argtype2, ...
#define RV32RGB_GUESTCALLS(X)                                     \
    X(VOID, void, ctors, 0, void)                                 \
    X(VOID, void, dtors, 0, void)                                 \
    X(VOID, void, effect_init, 1, void *)                         \
    X(VOID, void, effect_begin_iter, 3, void *, uint8_t, uint8_t) \
    X(VOID, void, effect_led, 2, void *, uint8_t)                 \
    X(VOID, void, effect_end_iter, 1, void *)
