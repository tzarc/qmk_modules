// Copyright 2022-2025 Nick Brassel (@tzarc)
// SPDX-License-Identifier: GPL-2.0-or-later

#include "filesystem.h"
#include "nvm_filesystem.h"
#include "via.h"

void nvm_via_erase(void) {
    fs_rmdir("via", true);
}

void nvm_via_read_magic(uint8_t *magic0, uint8_t *magic1, uint8_t *magic2) {
    char p[3] = {0};
    fs_read_block("via/magic", p, 3);
    *magic0 = p[0];
    *magic1 = p[1];
    *magic2 = p[2];
}

void nvm_via_update_magic(uint8_t magic0, uint8_t magic1, uint8_t magic2) {
    char p[3] = {magic0, magic1, magic2};
    fs_update_block("via/magic", p, 3);
}

uint32_t nvm_via_read_layout_options(void) {
    uint32_t value = 0;
    fs_read_block("via/layout_options", &value, sizeof(value));
    return value;
}

void nvm_via_update_layout_options(uint32_t val) {
    fs_update_block("via/layout_options", &val, sizeof(val));
}

uint32_t nvm_via_read_custom_config(void *buf, uint32_t offset, uint32_t length) {
#if VIA_EEPROM_CUSTOM_CONFIG_SIZE > 0
    char config[VIA_EEPROM_CUSTOM_CONFIG_SIZE] = {0};
    fs_read_block("via/custom_config", config, VIA_EEPROM_CUSTOM_CONFIG_SIZE);
    memcpy(buf, config + offset, length);
    return length;
#else
    return 0;
#endif
}

uint32_t nvm_via_update_custom_config(const void *buf, uint32_t offset, uint32_t length) {
#if VIA_EEPROM_CUSTOM_CONFIG_SIZE > 0
    char config[VIA_EEPROM_CUSTOM_CONFIG_SIZE] = {0};
    fs_read_block("via/custom_config", config, VIA_EEPROM_CUSTOM_CONFIG_SIZE);
    memcpy(config + offset, buf, length);
    fs_update_block("via/custom_config", config, VIA_EEPROM_CUSTOM_CONFIG_SIZE);
    return length;
#else
    return 0;
#endif
}
