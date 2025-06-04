// Copyright 2024-2025 Nick Brassel (@tzarc)
// SPDX-License-Identifier: GPL-2.0-or-later
#include <string.h>
#include <stdbool.h>
#include <ch.h>
#include "rgb_matrix.h"
#include "debug.h"
#include "rgb_matrix.h"
#include "color.h"
#include "timer.h"
#include "sync_timer.h"
#include <lib/lib8tion/lib8tion.h>
#include "rv32_rgb_runner.inl.h"
#include "runner/rv32_runner.h"

extern int rand(void);

#ifndef RGB_MATRIX_RV32_RUNNER_RAM
#    define RGB_MATRIX_RV32_RUNNER_RAM 2048 // This must be kept in-sync with flatfile.lds, TEXT section length
#endif                                      // RGB_MATRIX_RV32_RUNNER_RAM

#define MINI_RV32_RAM_SIZE (RGB_MATRIX_RV32_RUNNER_RAM)

// This needs to match the RAM address for the chip in use, STM32 seem to be 0x20000000 onwards, with 0x11100000 used as SYSCON-like support
#define MINIRV32_MMIO_RANGE(n) (0x20000000 <= (n) && (n) < (0x20000000 + (512 * 1024)))

#define MINIRV32_HANDLE_MEM_LOAD_CONTROL(addy, rval) \
    do {                                             \
        rval = rv32vm_handle_load(addy);             \
    } while (0)

static uint32_t rv32vm_handle_load(uint32_t addy);

#define MINIRV32_HANDLE_MEM_STORE_CONTROL(addy, val)    \
    do {                                                \
        if (rv32vm_handle_store(addy, val)) return val; \
    } while (0)

static uint32_t rv32vm_handle_store(uint32_t addy, uint32_t val);

#define MINIRV32_STEPPROTO MINIRV32_DECORATE int32_t MiniRV32IMAStepRGB(struct MiniRV32IMAState* state, uint8_t* image, uint32_t vProcAddress, uint32_t elapsedUs, int count)

#define MINIRV32_IMPLEMENTATION
#include "lib/mini-rv32ima/mini-rv32ima/mini-rv32ima.h"

static uint8_t                 rgb_ram_area[MINI_RV32_RAM_SIZE];
static struct MiniRV32IMAState rgb_core;

static uint32_t rv32vm_handle_load(uint32_t addy) {
    return 0;
}

static uint32_t rv32vm_handle_store(uint32_t addy, uint32_t val) {
    return 0;
}

typedef enum rv32vm_ecall_result_t {
    RV32_CONTINUE = 0,
    RV32_TERMINATE,
} rv32vm_ecall_result_t;

static rv32vm_ecall_result_t rv32vm_ecall_handler(void) {
    switch (rgb_core.regs[rv32reg_x17_a7]) {
        case RV32_EXIT:
            rgb_core.pc = MINIRV32_RAM_IMAGE_OFFSET;
            return RV32_TERMINATE;
        case RV32_ECALL_RGB_TIMER: {
            extern uint32_t g_rgb_timer;
            rgb_core.regs[rv32reg_x10_a0] = g_rgb_timer;
        } break;
        case RV32_ECALL_RGB_MATRIX_CONFIG_HSV: {
            RV32_HSV hsv = {
                .h = rgb_matrix_config.hsv.h,
                .s = rgb_matrix_config.hsv.s,
                .v = rgb_matrix_config.hsv.v,
            };
            union {
                uint32_t raw;
                RV32_HSV hsv;
            } out = {
                .hsv.h = hsv.h,
                .hsv.s = hsv.s,
                .hsv.v = hsv.v,
            };
            rgb_core.regs[rv32reg_x10_a0] = out.raw;
        } break;
        case RV32_ECALL_RGB_MATRIX_CONFIG_SPEED:
            rgb_core.regs[rv32reg_x10_a0] = rgb_matrix_config.speed;
            break;
        case RV32_ECALL_RAND:
            rgb_core.regs[rv32reg_x10_a0] = rand();
            break;
        case RV32_ECALL_SCALE16BY8:
            rgb_core.regs[rv32reg_x10_a0] = scale16by8(rgb_core.regs[rv32reg_x10_a0], rgb_core.regs[rv32reg_x11_a1]);
            break;
        case RV32_ECALL_SCALE8:
            rgb_core.regs[rv32reg_x10_a0] = scale8(rgb_core.regs[rv32reg_x10_a0], rgb_core.regs[rv32reg_x11_a1]);
            break;
        case RV32_ECALL_ABS8:
            rgb_core.regs[rv32reg_x10_a0] = abs8(rgb_core.regs[rv32reg_x10_a0]);
            break;
        case RV32_ECALL_SIN8:
            rgb_core.regs[rv32reg_x10_a0] = sin8(rgb_core.regs[rv32reg_x10_a0]);
            break;
        case RV32_ECALL_HSV_TO_RGB: {
            union {
                uint32_t raw;
                RV32_HSV hsv;
            } in    = {.raw = rgb_core.regs[rv32reg_x10_a0]};
            RGB rgb = hsv_to_rgb((HSV){.h = in.hsv.h, .s = in.hsv.s, .v = in.hsv.v});
            union {
                uint32_t raw;
                RV32_RGB rgb;
            } out                         = {.rgb.r = rgb.r, .rgb.g = rgb.g, .rgb.b = rgb.b};
            rgb_core.regs[rv32reg_x10_a0] = out.raw;
        } break;
        case RV32_ECALL_TIMER_READ32:
            rgb_core.regs[rv32reg_x10_a0] = sync_timer_read32();
            break;
        case RV32_ECALL_RGB_MATRIX_SET_COLOR:
            rgb_matrix_set_color(rgb_core.regs[rv32reg_x10_a0], rgb_core.regs[rv32reg_x11_a1], rgb_core.regs[rv32reg_x12_a2], rgb_core.regs[rv32reg_x13_a3]);
            break;
    }
    return RV32_CONTINUE;
}

static void rv32vm_invoke(rv32_api_t api) {
    if (timer_read32() < 5000) return; // don't do anything for first 5 seconds of bootup, so we can actually let console connect, but also bootmagic will run

    static bool has_checked = false;
    static bool can_run     = false;
    if (!has_checked) {
        has_checked           = true;
        uint32_t required_ram = (((uint32_t)rv32_runner_bin[0]) << 0 | ((uint32_t)rv32_runner_bin[1]) << 8 | ((uint32_t)rv32_runner_bin[2]) << 16 | ((uint32_t)rv32_runner_bin[3]) << 24) - MINIRV32_RAM_IMAGE_OFFSET;
        dprintf("Required RAM: %d\n", (int)required_ram);
        if (required_ram > MINI_RV32_RAM_SIZE) {
            dprintf("Not enough RAM for MiniRV32IMAStepRGB: %d > %d\n", (int)required_ram, (int)MINI_RV32_RAM_SIZE);
            return;
        }
        can_run = true;
    }

    if (!can_run) return;

    rgb_core.pc = MINIRV32_RAM_IMAGE_OFFSET + 4; // +4 because first u32 is RAM sizing info
    rgb_core.extraflags |= 3;                    // Machine mode
    rgb_core.regs[rv32reg_x5_t0] = (uint32_t)api;

    uint32_t start = timer_read32();
    while (true) {
        if (timer_elapsed32(start) > 1) {
            // Failsafe -- if the VM runs for more than 1ms, terminate it
            return;
        }
        int ret = MiniRV32IMAStepRGB(&rgb_core, rgb_ram_area, 0, 0, 16384);
        // dprintf("pc: %08x, ret: %d, rgb_core.mcause: %d\n", (int)rgb_core.pc, ret, (int)rgb_core.mcause);
        switch (ret) {
            case 0:
                if (rgb_core.mcause == 11) { // machine-mode ecall
                    int exec = rv32vm_ecall_handler();
                    switch (exec) {
                        case RV32_CONTINUE:
                            rgb_core.mcause = 0;
                            rgb_core.pc     = rgb_core.mepc + 4;
                            break;
                        case RV32_TERMINATE:
                            return;
                    }
                } else if (rgb_core.mcause != 0) {
                    dprintf("Unknown mcause: %d\n", (int)rgb_core.mcause);
                    return;
                }
                break;
            default:
                dprintf("Unknown return code: %d\n", ret);
                return;
        }
    }
}

uint32_t get_systick_count(void) {
    return chVTGetSystemTimeX();
}

static bool should_dump_exec_times = false;

void rv32vm_effect_init_impl(effect_params_t* params) {
    static bool initial = false;
    if (!initial) {
        initial      = true;
        debug_enable = true;
        memset(&rgb_core, 0, sizeof(rgb_core));
        memset(rgb_ram_area, 0, sizeof(rgb_ram_area));
        memcpy(rgb_ram_area, rv32_runner_bin, rv32_runner_bin_len);
        rv32vm_invoke(RV32_EFFECT_CTORS);
    }

    if (!should_dump_exec_times && false) {
        should_dump_exec_times = true;
        dprintf("Exec time: %d\n", (int)get_systick_count());
    }
    if (should_dump_exec_times) {
        dprintf("Exec time: %d\n", (int)get_systick_count());
    }
    rv32vm_invoke(RV32_EFFECT_INIT);
}

void rv32vm_effect_begin_iter_impl(effect_params_t* params, uint8_t led_min, uint8_t led_max) {
    rgb_core.regs[rv32reg_x10_a0] = (uint32_t)(uintptr_t)params;
    rgb_core.regs[rv32reg_x11_a1] = (uint32_t)led_min;
    rgb_core.regs[rv32reg_x12_a2] = (uint32_t)led_max;
    rv32vm_invoke(RV32_EFFECT_BEGIN_ITER);
}

void rv32vm_effect_led_impl(effect_params_t* params, uint8_t led_index) {
    rgb_core.regs[rv32reg_x10_a0] = (uint32_t)(uintptr_t)params;
    rgb_core.regs[rv32reg_x11_a1] = (uint32_t)led_index;
    rv32vm_invoke(RV32_EFFECT_LED);
}

void rv32vm_effect_end_iter_impl(effect_params_t* params) {
    rgb_core.regs[rv32reg_x10_a0] = (uint32_t)(uintptr_t)params;
    rv32vm_invoke(RV32_EFFECT_END_ITER);
}
