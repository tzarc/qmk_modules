// Copyright 2025 Nick Brassel (@tzarc)
// SPDX-License-Identifier: GPL-2.0-or-later
#include QMK_KEYBOARD_H

ASSERT_COMMUNITY_MODULES_MIN_API_VERSION(20250122);

uint32_t delayed_hello_world2(uint32_t trigger_time, void *cb_arg) {
    print("Hello, world! I'm a QMK based keyboard!\n");
    return 0;
}

void keyboard_post_init_hello_world2(void) {
    keyboard_post_init_hello_world2_kb();
    defer_exec(10000, delayed_hello_world2, NULL);
}

bool process_record_hello_world2(uint16_t keycode, keyrecord_t *record) {
    if (!process_record_hello_world2_kb(keycode, record)) {
        return false;
    }

    switch (keycode) {
        case CM_HELLO2:
            if (record->event.pressed) {
                SEND_STRING("Hello there.");
                break;
            }
    }

    return true;
}
