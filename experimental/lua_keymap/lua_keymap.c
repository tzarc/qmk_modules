// Copyright 2025 Nick Brassel (@tzarc)
// SPDX-License-Identifier: GPL-2.0-or-later

#include <quantum.h>

extern void test_lua(void);

bool lua_test_executed = false;

void housekeeping_task_lua_keymap() {
    if (!lua_test_executed && timer_read32() > 15000) {
        lua_test_executed = true;
        test_lua();
    }
}
