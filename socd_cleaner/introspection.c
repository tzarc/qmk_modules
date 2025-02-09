// Copyright 2025 Nick Brassel (@tzarc)
// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef COMMUNITY_MODULE_SOCD_CLEANER_ENABLE

uint16_t socd_count_raw(void) {
    return ARRAY_SIZE(socd);
}
__attribute__((weak)) uint16_t socd_count(void) {
    return socd_count_raw();
}

socd_cleaner_t* socd_get_raw(uint16_t socd_idx) {
    if (socd_idx >= socd_count_raw()) {
        return NULL;
    }
    return &socd[socd_idx];
}

__attribute__((weak)) socd_cleaner_t* socd_get(uint16_t socd_idx) {
    return socd_get_raw(socd_idx);
}

#endif // COMMUNITY_MODULE_SOCD_CLEANER_ENABLE
