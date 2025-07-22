// Copyright 2024-2025 Nick Brassel (@tzarc)
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#define MAKE_GUESTCALL_VOID_0(ret_type, name, ...)                                \
    void __attribute__((section(".thunks." #name "_thunk"))) name##_thunk(void) { \
        extern void name(void);                                                   \
        if (!name) return;                                                        \
        register uintptr_t a0 asm("a0");                                          \
        register uintptr_t a1 asm("a1");                                          \
        name();                                                                   \
        a0 = 0;                                                                   \
        a1 = 0;                                                                   \
    }

#define MAKE_GUESTCALL_RET_0(ret_type, name, ...)                                                     \
    void __attribute__((section(".thunks." #name "_thunk"))) name##_thunk(void) {                     \
        _Static_assert(sizeof(ret_type) <= sizeof(ecall_ret), "Return type too large for ecall_ret"); \
        extern void name(void);                                                                       \
        if (!name) return;                                                                            \
        register uintptr_t a0 asm("a0");                                                              \
        register uintptr_t a1 asm("a1");                                                              \
        ret_type           r   = name();                                                              \
        ecall_ret          ret = {.a0 = 0, .a1 = 0};                                                  \
        memcpy(&ret, &r, sizeof(ret_type));                                                           \
        a0 = ret.a0;                                                                                  \
        a1 = ret.a1;                                                                                  \
    }

#define MAKE_GUESTCALL_VOID_1(ret_type, name, argtype1)                                             \
    void __attribute__((section(".thunks." #name "_thunk"))) name##_thunk(void) {                   \
        _Static_assert(sizeof(argtype1) <= sizeof(uintptr_t), "Argument type too large for ecall"); \
        extern void name(argtype1);                                                                 \
        if (!name) return;                                                                          \
        register uintptr_t a0 asm("a0");                                                            \
        register uintptr_t a1 asm("a1");                                                            \
        uintptr_t          v0 = a0;                                                                 \
        argtype1           arg1;                                                                    \
        memcpy(&arg1, &v0, sizeof(argtype1));                                                       \
        name(arg1);                                                                                 \
        a0 = 0;                                                                                     \
        a1 = 0;                                                                                     \
    }

#define MAKE_GUESTCALL_RET_1(ret_type, name, argtype1)                                                \
    void __attribute__((section(".thunks." #name "_thunk"))) name##_thunk(void) {                     \
        _Static_assert(sizeof(ret_type) <= sizeof(ecall_ret), "Return type too large for ecall_ret"); \
        _Static_assert(sizeof(argtype1) <= sizeof(uintptr_t), "Argument type too large for ecall");   \
        extern ret_type name(argtype1);                                                               \
        if (!name) return;                                                                            \
        register uintptr_t a0 asm("a0");                                                              \
        register uintptr_t a1 asm("a1");                                                              \
        uintptr_t          v0 = a0;                                                                   \
        argtype1           arg1;                                                                      \
        memcpy(&arg1, &v0, sizeof(argtype1));                                                         \
        ret_type  r   = name(arg1);                                                                   \
        ecall_ret ret = {.a0 = 0, .a1 = 0};                                                           \
        memcpy(&ret, &r, sizeof(ret_type));                                                           \
        a0 = ret.a0;                                                                                  \
        a1 = ret.a1;                                                                                  \
    }

#define MAKE_GUESTCALL_VOID_2(ret_type, name, argtype1, argtype2)                                     \
    void __attribute__((section(".thunks." #name "_thunk"))) name##_thunk(void) {                     \
        _Static_assert(sizeof(argtype1) <= sizeof(uintptr_t), "Argument type 1 too large for ecall"); \
        _Static_assert(sizeof(argtype2) <= sizeof(uintptr_t), "Argument type 2 too large for ecall"); \
        extern void name(argtype1, argtype2);                                                         \
        if (!name) return;                                                                            \
        register uintptr_t a0 asm("a0");                                                              \
        register uintptr_t a1 asm("a1");                                                              \
        uintptr_t          v0 = a0;                                                                   \
        uintptr_t          v1 = a1;                                                                   \
        argtype1           arg1;                                                                      \
        argtype2           arg2;                                                                      \
        memcpy(&arg1, &v0, sizeof(argtype1));                                                         \
        memcpy(&arg2, &v1, sizeof(argtype2));                                                         \
        name(arg1, arg2);                                                                             \
        a0 = 0;                                                                                       \
        a1 = 0;                                                                                       \
    }

#define MAKE_GUESTCALL_RET_2(ret_type, name, argtype1, argtype2)                                      \
    void __attribute__((section(".thunks." #name "_thunk"))) name##_thunk(void) {                     \
        _Static_assert(sizeof(ret_type) <= sizeof(ecall_ret), "Return type too large for ecall_ret"); \
        _Static_assert(sizeof(argtype1) <= sizeof(uintptr_t), "Argument type 1 too large for ecall"); \
        _Static_assert(sizeof(argtype2) <= sizeof(uintptr_t), "Argument type 2 too large for ecall"); \
        extern ret_type name(argtype1, argtype2);                                                     \
        if (!name) return;                                                                            \
        register uintptr_t a0 asm("a0");                                                              \
        register uintptr_t a1 asm("a1");                                                              \
        uintptr_t          v0 = a0;                                                                   \
        uintptr_t          v1 = a1;                                                                   \
        argtype1           arg1;                                                                      \
        argtype2           arg2;                                                                      \
        memcpy(&arg1, &v0, sizeof(argtype1));                                                         \
        memcpy(&arg2, &v1, sizeof(argtype2));                                                         \
        ret_type  r   = name(arg1, arg2);                                                             \
        ecall_ret ret = {.a0 = 0, .a1 = 0};                                                           \
        memcpy(&ret, &r, sizeof(ret_type));                                                           \
        a0 = ret.a0;                                                                                  \
        a1 = ret.a1;                                                                                  \
    }

#define MAKE_GUESTCALL_VOID_3(ret_type, name, argtype1, argtype2, argtype3)                           \
    void __attribute__((section(".thunks." #name "_thunk"))) name##_thunk(void) {                     \
        _Static_assert(sizeof(argtype1) <= sizeof(uintptr_t), "Argument type 1 too large for ecall"); \
        _Static_assert(sizeof(argtype2) <= sizeof(uintptr_t), "Argument type 2 too large for ecall"); \
        _Static_assert(sizeof(argtype3) <= sizeof(uintptr_t), "Argument type 3 too large for ecall"); \
        extern void name(argtype1, argtype2, argtype3);                                               \
        if (!name) return;                                                                            \
        register uintptr_t a0 asm("a0");                                                              \
        register uintptr_t a1 asm("a1");                                                              \
        register uintptr_t a2 asm("a2");                                                              \
        uintptr_t          v0 = a0;                                                                   \
        uintptr_t          v1 = a1;                                                                   \
        uintptr_t          v2 = a2;                                                                   \
        argtype1           arg1;                                                                      \
        argtype2           arg2;                                                                      \
        argtype3           arg3;                                                                      \
        memcpy(&arg1, &v0, sizeof(argtype1));                                                         \
        memcpy(&arg2, &v1, sizeof(argtype2));                                                         \
        memcpy(&arg3, &v2, sizeof(argtype3));                                                         \
        name(arg1, arg2, arg3);                                                                       \
        a0 = 0;                                                                                       \
        a1 = 0;                                                                                       \
    }

#define MAKE_GUESTCALL_RET_3(ret_type, name, argtype1, argtype2, argtype3)                            \
    void __attribute__((section(".thunks." #name "_thunk"))) name##_thunk(void) {                     \
        _Static_assert(sizeof(ret_type) <= sizeof(ecall_ret), "Return type too large for ecall_ret"); \
        _Static_assert(sizeof(argtype1) <= sizeof(uintptr_t), "Argument type 1 too large for ecall"); \
        _Static_assert(sizeof(argtype2) <= sizeof(uintptr_t), "Argument type 2 too large for ecall"); \
        _Static_assert(sizeof(argtype3) <= sizeof(uintptr_t), "Argument type 3 too large for ecall"); \
        extern ret_type name(argtype1, argtype2, argtype3);                                           \
        if (!name) return;                         \
        register uintptr_t a0 asm("a0");           \
        register uintptr_t a1 asm("a1");           \
        register uintptr_t a2 asm("a2");           \
        uintptr_t          v0 = a0;                \
        uintptr_t          v1 = a1;                \
        uintptr_t          v2 = a2;                \
        argtype1           arg1;                   \
        argtype2           arg2;                   \
        argtype3           arg3;                   \
        memcpy(&arg1, &v0, sizeof(argtype1));      \
        memcpy(&arg2, &v1, sizeof(argtype2));      \
        memcpy(&arg3, &v2, sizeof(argtype3));      \
        ret_type  r   = name(arg1, arg2, arg3);    \
        ecall_ret ret = {.a0 = 0, .a1 = 0};        \
        memcpy(&ret, &r, sizeof(ret_type));        \
        a0 = ret.a0;                               \
        a1 = ret.a1;                               \
    }
