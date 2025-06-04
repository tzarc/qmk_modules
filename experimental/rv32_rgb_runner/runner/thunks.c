// Copyright 2025 Nick Brassel (@tzarc)
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdint.h>

extern void effect_init(void *params) __attribute__((weak));
extern void effect_begin_iter(void *params, uint8_t led_min, uint8_t led_max) __attribute__((weak));
extern void effect_led(void *params, uint8_t led_index) __attribute__((weak));
extern void effect_end_iter(void *params) __attribute__((weak));

static void __attribute__((noinline, section(".thunks.invoke_fptr_array"))) invoke_fptr_array(void *start, void *end) {
    for (void (*p)(void) = (void (*)(void))start; p != (void (*)(void))end; p++) {
        p();
    }
}

void __attribute__((section(".thunks.ctors_thunk"))) ctors_thunk(void) {
    extern uintptr_t __preinit_array_start;
    extern uintptr_t __preinit_array_end;
    invoke_fptr_array(&__preinit_array_start, &__preinit_array_end);
    extern uintptr_t __init_array_start;
    extern uintptr_t __init_array_end;
    invoke_fptr_array(&__init_array_start, &__init_array_end);
}

void __attribute__((section(".thunks.dtors_thunk"))) dtors_thunk(void) {
    extern uintptr_t __fini_array_start;
    extern uintptr_t __fini_array_end;
    invoke_fptr_array(&__fini_array_start, &__fini_array_end);
}

void __attribute__((section(".thunks.effect_init"))) effect_init_thunk(void) {
    if (!effect_init) return;
    register uintptr_t a0 asm("a0");
    effect_init((void *)a0);
}

void __attribute__((section(".thunks.effect_begin_iter"))) effect_begin_iter_thunk(void) {
    if (!effect_begin_iter) return;
    register uintptr_t a0 asm("a0");
    register uintptr_t a1 asm("a1");
    register uintptr_t a2 asm("a2");
    effect_begin_iter((void *)a0, (uint8_t)a1, (uint8_t)a2);
}

void __attribute__((section(".thunks.effect_led"))) effect_led_thunk(void) {
    if (!effect_led) return;
    register uintptr_t a0 asm("a0");
    register uintptr_t a1 asm("a1");
    effect_led((void *)a0, (uint8_t)a1);
}

void __attribute__((section(".thunks.effect_end_iter"))) effect_end_iter_thunk(void) {
    if (!effect_end_iter) return;
    register uintptr_t a0 asm("a0");
    effect_end_iter((void *)a0);
}
