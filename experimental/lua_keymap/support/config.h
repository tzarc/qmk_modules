// Copyright 2025 Nick Brassel (@tzarc)
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#define lua_writestring(s, l)            \
    do {                                 \
        for (size_t n = 0; n < l; ++n) { \
            putchar(s[n]);               \
        }                                \
    } while (0)

#define lua_writestringerror(s, p) \
    do {                           \
        printf((s), (p));          \
    } while (0)

#define lua_writeline() \
    do {                \
        putchar('\n');  \
    } while (0)
