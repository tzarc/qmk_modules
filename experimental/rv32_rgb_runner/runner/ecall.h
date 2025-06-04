// Copyright 2024-2025 Nick Brassel (@tzarc)
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <stdint.h>

typedef struct ecall_ret {
    uintptr_t a0;
    uintptr_t a1;
} ecall_ret;

__attribute__((always_inline)) static inline ecall_ret _ecall7(int n, uintptr_t arg0, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4, uintptr_t arg5, uintptr_t arg6) {
    register uintptr_t a0 asm("a0") = (uintptr_t)(arg0);
    register uintptr_t a1 asm("a1") = (uintptr_t)(arg1);
    register uintptr_t a2 asm("a2") = (uintptr_t)(arg2);
    register uintptr_t a3 asm("a3") = (uintptr_t)(arg3);
    register uintptr_t a4 asm("a4") = (uintptr_t)(arg4);
    register uintptr_t a5 asm("a5") = (uintptr_t)(arg5);
    register uintptr_t a6 asm("a6") = (uintptr_t)(arg6);
    register uintptr_t a7 asm("a7") = (uintptr_t)(n);
    asm volatile("ecall" : "+r"(a0), "+r"(a1) : "r"(a2), "r"(a3), "r"(a4), "r"(a5), "r"(a6), "r"(a7) : "memory");
    return (ecall_ret){.a0 = a0, .a1 = a1};
}

__attribute__((always_inline)) static inline ecall_ret _ecall6(int n, uintptr_t arg0, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4, uintptr_t arg5) {
    register uintptr_t a0 asm("a0") = (uintptr_t)(arg0);
    register uintptr_t a1 asm("a1") = (uintptr_t)(arg1);
    register uintptr_t a2 asm("a2") = (uintptr_t)(arg2);
    register uintptr_t a3 asm("a3") = (uintptr_t)(arg3);
    register uintptr_t a4 asm("a4") = (uintptr_t)(arg4);
    register uintptr_t a5 asm("a5") = (uintptr_t)(arg5);
    register uintptr_t a7 asm("a7") = (uintptr_t)(n);
    asm volatile("ecall" : "+r"(a0), "+r"(a1) : "r"(a2), "r"(a3), "r"(a4), "r"(a5), "r"(a7) : "memory");
    return (ecall_ret){.a0 = a0, .a1 = a1};
}

__attribute__((always_inline)) static inline ecall_ret _ecall5(int n, uintptr_t arg0, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4) {
    register uintptr_t a0 asm("a0") = (uintptr_t)(arg0);
    register uintptr_t a1 asm("a1") = (uintptr_t)(arg1);
    register uintptr_t a2 asm("a2") = (uintptr_t)(arg2);
    register uintptr_t a3 asm("a3") = (uintptr_t)(arg3);
    register uintptr_t a4 asm("a4") = (uintptr_t)(arg4);
    register uintptr_t a7 asm("a7") = (uintptr_t)(n);
    asm volatile("ecall" : "+r"(a0), "+r"(a1) : "r"(a2), "r"(a3), "r"(a4), "r"(a7) : "memory");
    return (ecall_ret){.a0 = a0, .a1 = a1};
}

__attribute__((always_inline)) static inline ecall_ret _ecall4(int n, uintptr_t arg0, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3) {
    register uintptr_t a0 asm("a0") = (uintptr_t)(arg0);
    register uintptr_t a1 asm("a1") = (uintptr_t)(arg1);
    register uintptr_t a2 asm("a2") = (uintptr_t)(arg2);
    register uintptr_t a3 asm("a3") = (uintptr_t)(arg3);
    register uintptr_t a7 asm("a7") = (uintptr_t)(n);
    asm volatile("ecall" : "+r"(a0), "+r"(a1) : "r"(a2), "r"(a3), "r"(a7) : "memory");
    return (ecall_ret){.a0 = a0, .a1 = a1};
}

__attribute__((always_inline)) static inline ecall_ret _ecall3(int n, uintptr_t arg0, uintptr_t arg1, uintptr_t arg2) {
    register uintptr_t a0 asm("a0") = (uintptr_t)(arg0);
    register uintptr_t a1 asm("a1") = (uintptr_t)(arg1);
    register uintptr_t a2 asm("a2") = (uintptr_t)(arg2);
    register uintptr_t a7 asm("a7") = (uintptr_t)(n);
    asm volatile("ecall" : "+r"(a0), "+r"(a1) : "r"(a2), "r"(a7) : "memory");
    return (ecall_ret){.a0 = a0, .a1 = a1};
}

__attribute__((always_inline)) static inline ecall_ret _ecall2(int n, uintptr_t arg0, uintptr_t arg1) {
    register uintptr_t a0 asm("a0") = (uintptr_t)(arg0);
    register uintptr_t a1 asm("a1") = (uintptr_t)(arg1);
    register uintptr_t a7 asm("a7") = (uintptr_t)(n);
    asm volatile("ecall" : "+r"(a0), "+r"(a1) : "r"(a7) : "memory");
    return (ecall_ret){.a0 = a0, .a1 = a1};
}

__attribute__((always_inline)) static inline ecall_ret _ecall1(int n, uintptr_t arg0) {
    register uintptr_t a0 asm("a0") = (uintptr_t)(arg0);
    register uintptr_t a1 asm("a1");
    register uintptr_t a7 asm("a7") = (uintptr_t)(n);
    asm volatile("ecall" : "+r"(a0), "=r"(a1) : "r"(a7) : "memory");
    return (ecall_ret){.a0 = a0, .a1 = a1};
}

__attribute__((always_inline)) static inline ecall_ret _ecall0(int n) {
    register uintptr_t a0 asm("a0");
    register uintptr_t a1 asm("a1");
    register uintptr_t a7 asm("a7") = (uintptr_t)(n);
    asm volatile("ecall" : "=r"(a0), "=r"(a1) : "r"(a7) : "memory");
    return (ecall_ret){.a0 = a0, .a1 = a1};
}
