// Copyright 2024-2025 Nick Brassel (@tzarc)
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#define MAKE_GUESTCALL_VOID_0(name, ...)                                          \
    void __attribute__((section(".thunks." #name "_thunk"))) name##_thunk(void) { \
        extern void name(void);                                                   \
        if (!name) return;                                                        \
        register uintptr_t a0 asm("a0");                                          \
        register uintptr_t a1 asm("a1");                                          \
        name();                                                                   \
        a0 = 0;                                                                   \
        a1 = 0;                                                                   \
    }

#define MAKE_GUESTCALL_RET_0(name, ret_type, ...)                                 \
    void __attribute__((section(".thunks." #name "_thunk"))) name##_thunk(void) { \
        extern void name(void);                                                   \
        if (!name) return;                                                        \
        register uintptr_t a0 asm("a0");                                          \
        register uintptr_t a1 asm("a1");                                          \
        ret_type           r = name();                                            \
        ecall_ret          ret;                                                   \
        memcpy(&ret, &r, sizeof(ecall_ret));                                      \
        a0 = ret.a0;                                                              \
        a1 = ret.a1;                                                              \
    }

#define MAKE_GUESTCALL_VOID_1(name, argtype1)                                     \
    void __attribute__((section(".thunks." #name "_thunk"))) name##_thunk(void) { \
        extern void name(argtype1);                                               \
        if (!name) return;                                                        \
        register uintptr_t a0 asm("a0");                                          \
        register uintptr_t a1 asm("a1");                                          \
        uintptr_t          v0 = a0;                                               \
        argtype1           arg1;                                                  \
        memcpy(&arg1, &v0, sizeof(argtype1));                                     \
        name(arg1);                                                               \
        a0 = 0;                                                                   \
        a1 = 0;                                                                   \
    }

#define MAKE_GUESTCALL_RET_1(name, ret_type, argtype1)                            \
    void __attribute__((section(".thunks." #name "_thunk"))) name##_thunk(void) { \
        extern ret_type name(argtype1);                                           \
        if (!name) return;                                                        \
        register uintptr_t a0 asm("a0");                                          \
        register uintptr_t a1 asm("a1");                                          \
        uintptr_t          v0 = a0;                                               \
        argtype1           arg1;                                                  \
        memcpy(&arg1, &v0, sizeof(argtype1));                                     \
        ret_type  r = name(arg1);                                                 \
        ecall_ret ret;                                                            \
        memcpy(&ret, &r, sizeof(ecall_ret));                                      \
        a0 = ret.a0;                                                              \
        a1 = ret.a1;                                                              \
    }

#define MAKE_GUESTCALL_VOID_2(name, argtype1, argtype2)                           \
    void __attribute__((section(".thunks." #name "_thunk"))) name##_thunk(void) { \
        extern void name(argtype1, argtype2);                                     \
        if (!name) return;                                                        \
        register uintptr_t a0 asm("a0");                                          \
        register uintptr_t a1 asm("a1");                                          \
        uintptr_t          v0 = a0;                                               \
        uintptr_t          v1 = a1;                                               \
        argtype1           arg1;                                                  \
        argtype2           arg2;                                                  \
        memcpy(&arg1, &v0, sizeof(argtype1));                                     \
        memcpy(&arg2, &v1, sizeof(argtype2));                                     \
        name(arg1, arg2);                                                         \
        a0 = 0;                                                                   \
        a1 = 0;                                                                   \
    }

#define MAKE_GUESTCALL_RET_2(name, ret_type, argtype1, argtype2)                  \
    void __attribute__((section(".thunks." #name "_thunk"))) name##_thunk(void) { \
        extern ret_type name(argtype1, argtype2);                                 \
        if (!name) return;                                                        \
        register uintptr_t a0 asm("a0");                                          \
        register uintptr_t a1 asm("a1");                                          \
        uintptr_t          v0 = a0;                                               \
        uintptr_t          v1 = a1;                                               \
        argtype1           arg1;                                                  \
        argtype2           arg2;                                                  \
        memcpy(&arg1, &v0, sizeof(argtype1));                                     \
        memcpy(&arg2, &v1, sizeof(argtype2));                                     \
        ret_type  r = name(arg1, arg2);                                           \
        ecall_ret ret;                                                            \
        memcpy(&ret, &r, sizeof(ecall_ret));                                      \
        a0 = ret.a0;                                                              \
        a1 = ret.a1;                                                              \
    }

#define MAKE_GUESTCALL_VOID_3(name, argtype1, argtype2, argtype3)                 \
    void __attribute__((section(".thunks." #name "_thunk"))) name##_thunk(void) { \
        extern void name(argtype1, argtype2, argtype3);                           \
        if (!name) return;                                                        \
        register uintptr_t a0 asm("a0");                                          \
        register uintptr_t a1 asm("a1");                                          \
        register uintptr_t a2 asm("a2");                                          \
        uintptr_t          v0 = a0;                                               \
        uintptr_t          v1 = a1;                                               \
        uintptr_t          v2 = a2;                                               \
        argtype1           arg1;                                                  \
        argtype2           arg2;                                                  \
        argtype3           arg3;                                                  \
        memcpy(&arg1, &v0, sizeof(argtype1));                                     \
        memcpy(&arg2, &v1, sizeof(argtype2));                                     \
        memcpy(&arg3, &v2, sizeof(argtype3));                                     \
        name(arg1, arg2, arg3);                                                   \
        a0 = 0;                                                                   \
        a1 = 0;                                                                   \
    }

#define MAKE_GUESTCALL_RET_3(name, ret_type, argtype1, argtype2, argtype3)        \
    void __attribute__((section(".thunks." #name "_thunk"))) name##_thunk(void) { \
        extern ret_type name(argtype1, argtype2, argtype3);                       \
        if (!name) return;                                                        \
        register uintptr_t a0 asm("a0");                                          \
        register uintptr_t a1 asm("a1");                                          \
        register uintptr_t a2 asm("a2");                                          \
        uintptr_t          v0 = a0;                                               \
        uintptr_t          v1 = a1;                                               \
        uintptr_t          v2 = a2;                                               \
        argtype1           arg1;                                                  \
        argtype2           arg2;                                                  \
        argtype3           arg3;                                                  \
        memcpy(&arg1, &v0, sizeof(argtype1));                                     \
        memcpy(&arg2, &v1, sizeof(argtype2));                                     \
        memcpy(&arg3, &v2, sizeof(argtype3));                                     \
        ret_type  r = name(arg1, arg2, arg3);                                     \
        ecall_ret ret;                                                            \
        memcpy(&ret, &r, sizeof(ecall_ret));                                      \
        a0 = ret.a0;                                                              \
        a1 = ret.a1;                                                              \
    }
