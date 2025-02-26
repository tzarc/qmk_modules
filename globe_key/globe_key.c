// Copyright 2025 Nick Brassel (@tzarc)
// SPDX-License-Identifier: GPL-2.0-or-later
#include QMK_KEYBOARD_H

bool process_record_globe_key(uint16_t keycode, keyrecord_t *record) {
    switch (keycode) {
        case KC_GLOBE:
            if (record->event.pressed) {
                host_consumer_send(AC_NEXT_KEYBOARD_LAYOUT_SELECT);
            } else {
                host_consumer_send(0);
            }
            return false;
    }
    return true;
}
