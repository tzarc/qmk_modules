// Copyright 2025 Nick Brassel (@tzarc)
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#ifdef SFDP_DEBUG_OUTPUT
#    include <debug.h>
#    define sfdp_dprintf(...)     \
        do {                      \
            dprintf("SFDP: ");    \
            dprintf(__VA_ARGS__); \
        } while (0)
#else
#    define sfdp_dprintf(...) \
        do {                  \
        } while (0)
#endif // SFDP_DEBUG_OUTPUT

bool sfdp_init(void);
