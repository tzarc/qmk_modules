// Copyright 2024-2025 Nick Brassel (@tzarc)
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <stdint.h>

// List of rv32 register indices
enum {
    rv32reg_x0_zero = 0,
    rv32reg_x1_ra,
    rv32reg_x2_sp,
    rv32reg_x3_gp,
    rv32reg_x4_tp,
    rv32reg_x5_t0,
    rv32reg_x6_t1,
    rv32reg_x7_t2,
    rv32reg_x8_s0,
    rv32reg_x9_s1,
    rv32reg_x10_a0,
    rv32reg_x11_a1,
    rv32reg_x12_a2,
    rv32reg_x13_a3,
    rv32reg_x14_a4,
    rv32reg_x15_a5,
    rv32reg_x16_a6,
    rv32reg_x17_a7,
    rv32reg_x18_s2,
    rv32reg_x19_s3,
    rv32reg_x20_s4,
    rv32reg_x21_s5,
    rv32reg_x22_s6,
    rv32reg_x23_s7,
    rv32reg_x24_s8,
    rv32reg_x25_s9,
    rv32reg_x26_s10,
    rv32reg_x27_s11,
    rv32reg_x28_t3,
    rv32reg_x29_t4,
    rv32reg_x30_t5,
    rv32reg_x31_t6,
};

// This list needs to be kept in-sync with the api_table in rv32_runner.S
typedef enum rv32_api_t {
    RV32_EFFECT_CTORS = 0,
    RV32_EFFECT_DTORS,
    RV32_EFFECT_INIT,
    RV32_EFFECT_BEGIN_ITER,
    RV32_EFFECT_LED,
    RV32_EFFECT_END_ITER,
} rv32_api_t;

// This list needs to be kept in sync with rv32_runner.c
typedef enum rv32_calls_t {
    RV32_EXIT = 0,
    RV32_ECALL_TIMER_READ32,
    RV32_ECALL_RGB_TIMER,
    RV32_ECALL_RGB_MATRIX_CONFIG_HSV,
    RV32_ECALL_RGB_MATRIX_CONFIG_SPEED,
    RV32_ECALL_RAND,
    RV32_ECALL_SCALE16BY8,
    RV32_ECALL_SCALE8,
    RV32_ECALL_ABS8,
    RV32_ECALL_SIN8,
    RV32_ECALL_HSV_TO_RGB,
    RV32_ECALL_RGB_MATRIX_SET_COLOR,
} rv32_calls_t;

typedef struct __attribute__((packed)) RV32_HSV {
    uint8_t h;
    uint8_t s;
    uint8_t v;
} RV32_HSV;

typedef struct __attribute__((packed)) RV32_RGB {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} RV32_RGB;
