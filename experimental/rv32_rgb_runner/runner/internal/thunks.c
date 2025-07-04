// Copyright 2024-2025 Nick Brassel (@tzarc)
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdint.h>
#include <string.h>

#include "../../api_bindings.h"
#include "guestcalls.h"

extern void effect_init(void *params) __attribute__((weak));
extern void effect_begin_iter(void *params, uint8_t led_min, uint8_t led_max) __attribute__((weak));
extern void effect_led(void *params, uint8_t led_index) __attribute__((weak));
extern void effect_end_iter(void *params) __attribute__((weak));

static void __attribute__((noinline)) invoke_fptr_array(void *start, void *end) {
    for (void (*p)(void) = (void (*)(void))start; p != (void (*)(void))end; p++) {
        p();
    }
}

void ctors(void) {
    extern uintptr_t __preinit_array_start;
    extern uintptr_t __preinit_array_end;
    invoke_fptr_array(&__preinit_array_start, &__preinit_array_end);
    extern uintptr_t __init_array_start;
    extern uintptr_t __init_array_end;
    invoke_fptr_array(&__init_array_start, &__init_array_end);
}

void dtors(void) {
    extern uintptr_t __fini_array_start;
    extern uintptr_t __fini_array_end;
    invoke_fptr_array(&__fini_array_start, &__fini_array_end);
}

#define X(thunksuffix, ret_type, name, argcount, ...) MAKE_GUESTCALL_##thunksuffix##_##argcount(name, ##__VA_ARGS__)
RV32RGB_GUESTCALLS(X)
#undef X
