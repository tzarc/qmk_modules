// Copyright 2024-2026 Nick Brassel (@tzarc)
// SPDX-License-Identifier: GPL-2.0-or-later
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "rv32_runner.h"
#include "internal/ecall.h"
#include "internal/hypercalls.h"

#define X(thunksuffix, ret_type, name, argcount, ...) MAKE_HYPERCALL_##thunksuffix##_##argcount(ret_type, name, ##__VA_ARGS__)
RV32RGB_HYPERCALLS(X)
#undef X

#define MAX_RGB_MATRIX_LED_COUNT 256

static uint8_t time_offsets[MAX_RGB_MATRIX_LED_COUNT] = {0};

RV32_HSV hsv;
uint8_t  speed;
uint32_t g_rgb_timer;

void __attribute__((constructor)) effect_ctor(void) {
    hsv.h       = 0;
    hsv.s       = 255;
    hsv.v       = 255;
    speed       = 128;
    g_rgb_timer = 0;
}

void effect_init(void *params) {
    static bool initial = false;
    if (!initial) {
        initial = true;
        for (uint16_t i = 0; i < MAX_RGB_MATRIX_LED_COUNT; i++) {
            time_offsets[i] = rand();
        }
    }
}

void effect_begin_iter(void *params, uint8_t led_min, uint8_t led_max) {
    hsv         = rgb_matrix_config_hsv();
    speed       = rgb_matrix_config_speed();
    g_rgb_timer = rgb_timer();
}

void effect_led(void *params, uint8_t led_index) {
    uint16_t time = scale16by8((g_rgb_timer / 2) + (((uint16_t)time_offsets[led_index]) << 5), speed / 16);
    uint8_t  v    = scale8(abs8(sin8(time) - 128) * 2, hsv.v);
    RV32_RGB rgb  = rgb_matrix_hsv_to_rgb((RV32_HSV){.h = hsv.h, .s = hsv.s, .v = v});
    rgb_matrix_set_color(led_index, rgb.r, rgb.g, rgb.b);
}
