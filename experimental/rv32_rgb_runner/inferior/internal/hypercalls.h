// Copyright 2024-2025 Nick Brassel (@tzarc)
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#define MAKE_HYPERCALL_VOID_0(ret_type, name, ...) \
    static inline void name(void) {                \
        _ecall0(RV32_ECALL_##name);                \
    }

#define MAKE_HYPERCALL_RET_0(ret_type, name, ...) \
    static inline ret_type name(void) {           \
        ecall_ret r = _ecall0(RV32_ECALL_##name); \
        ret_type  ret;                            \
        memcpy(&ret, &r, sizeof(ret_type));       \
        return ret;                               \
    }

#define MAKE_HYPERCALL_VOID_1(ret_type, name, argtype1, ...) \
    static inline void name(argtype1 arg1) {                 \
        uintptr_t v1;                                        \
        memcpy(&v1, &arg1, sizeof(argtype1));                \
        _ecall1(RV32_ECALL_##name, v1);                      \
    }

#define MAKE_HYPERCALL_RET_1(ret_type, name, argtype1, ...) \
    static inline ret_type name(argtype1 arg1) {            \
        uintptr_t v1;                                       \
        memcpy(&v1, &arg1, sizeof(argtype1));               \
        ecall_ret r = _ecall1(RV32_ECALL_##name, v1);       \
        ret_type  ret;                                      \
        memcpy(&ret, &r, sizeof(ret_type));                 \
        return ret;                                         \
    }

#define MAKE_HYPERCALL_VOID_2(ret_type, name, argtype1, argtype2, ...) \
    static inline void name(argtype1 arg1, argtype2 arg2) {            \
        uintptr_t v1, v2;                                              \
        memcpy(&v1, &arg1, sizeof(argtype1));                          \
        memcpy(&v2, &arg2, sizeof(argtype2));                          \
        _ecall2(RV32_ECALL_##name, v1, v2);                            \
    }

#define MAKE_HYPERCALL_RET_2(ret_type, name, argtype1, argtype2, ...) \
    static inline ret_type name(argtype1 arg1, argtype2 arg2) {       \
        uintptr_t v1, v2;                                             \
        memcpy(&v1, &arg1, sizeof(argtype1));                         \
        memcpy(&v2, &arg2, sizeof(argtype2));                         \
        ecall_ret r = _ecall2(RV32_ECALL_##name, v1, v2);             \
        ret_type  ret;                                                \
        memcpy(&ret, &r, sizeof(ret_type));                           \
        return ret;                                                   \
    }

#define MAKE_HYPERCALL_VOID_3(ret_type, name, argtype1, argtype2, argtype3, ...) \
    static inline void name(argtype1 arg1, argtype2 arg2, argtype3 arg3) {       \
        uintptr_t v1, v2, v3;                                                    \
        memcpy(&v1, &arg1, sizeof(argtype1));                                    \
        memcpy(&v2, &arg2, sizeof(argtype2));                                    \
        memcpy(&v3, &arg3, sizeof(argtype3));                                    \
        _ecall3(RV32_ECALL_##name, v1, v2, v3);                                  \
    }

#define MAKE_HYPERCALL_RET_3(ret_type, name, argtype1, argtype2, argtype3, ...) \
    static inline ret_type name(argtype1 arg1, argtype2 arg2, argtype3 arg3) {  \
        uintptr_t v1, v2, v3;                                                   \
        memcpy(&v1, &arg1, sizeof(argtype1));                                   \
        memcpy(&v2, &arg2, sizeof(argtype2));                                   \
        memcpy(&v3, &arg3, sizeof(argtype3));                                   \
        ecall_ret r = _ecall3(RV32_ECALL_##name, v1, v2, v3);                   \
        ret_type  ret;                                                          \
        memcpy(&ret, &r, sizeof(ret_type));                                     \
        return ret;                                                             \
    }

#define MAKE_HYPERCALL_VOID_4(ret_type, name, argtype1, argtype2, argtype3, argtype4, ...) \
    static inline void name(argtype1 arg1, argtype2 arg2, argtype3 arg3, argtype4 arg4) {  \
        uintptr_t v1, v2, v3, v4;                                                          \
        memcpy(&v1, &arg1, sizeof(argtype1));                                              \
        memcpy(&v2, &arg2, sizeof(argtype2));                                              \
        memcpy(&v3, &arg3, sizeof(argtype3));                                              \
        memcpy(&v4, &arg4, sizeof(argtype4));                                              \
        _ecall4(RV32_ECALL_##name, v1, v2, v3, v4);                                        \
    }

#define MAKE_HYPERCALL_RET_4(ret_type, name, argtype1, argtype2, argtype3, argtype4, ...)     \
    static inline ret_type name(argtype1 arg1, argtype2 arg2, argtype3 arg3, argtype4 arg4) { \
        uintptr_t v1, v2, v3, v4;                                                             \
        memcpy(&v1, &arg1, sizeof(argtype1));                                                 \
        memcpy(&v2, &arg2, sizeof(argtype2));                                                 \
        memcpy(&v3, &arg3, sizeof(argtype3));                                                 \
        memcpy(&v4, &arg4, sizeof(argtype4));                                                 \
        ecall_ret r = _ecall4(RV32_ECALL_##name, v1, v2, v3, v4);                             \
        ret_type  ret;                                                                        \
        memcpy(&ret, &r, sizeof(ret_type));                                                   \
        return ret;                                                                           \
    }
