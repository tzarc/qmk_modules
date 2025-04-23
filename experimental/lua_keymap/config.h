// Copyright 2025 Nick Brassel (@tzarc)
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#define KEYCODE_LOOKUP_MEMOISE

#define lua_writestring(s, l)              \
    do {                                   \
        extern int8_t sendchar(uint8_t c); \
        for (size_t n = 0; n < l; ++n) {   \
            sendchar(s[n]);                \
        }                                  \
    } while (0)

#define lua_writestringerror(s, p)                \
    do {                                          \
        extern int dprintf(const char *fmt, ...); \
        dprintf((s), (p));                        \
    } while (0)

#define lua_writeline()                    \
    do {                                   \
        extern int8_t sendchar(uint8_t c); \
        sendchar('\n');                    \
    } while (0)
