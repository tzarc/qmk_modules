// Copyright 2022-2025 Nick Brassel (@tzarc)
// SPDX-License-Identifier: GPL-2.0-or-later
#include <string.h>
#include "nvm_eeconfig.h"
#include "filesystem.h"
#include "eeconfig.h"
#include "debug.h"
#include "keycode_config.h"

#ifdef AUDIO_ENABLE
#    include "audio.h"
#endif

#ifdef BACKLIGHT_ENABLE
#    include "backlight.h"
#endif

#ifdef RGBLIGHT_ENABLE
#    include "rgblight.h"
#endif

#ifdef RGB_MATRIX_ENABLE
#    include "rgb_matrix_types.h"
#endif

#ifdef LED_MATRIX_ENABLE
#    include "led_matrix_types.h"
#endif

#ifdef UNICODE_COMMON_ENABLE
#    include "unicode.h"
#endif

#ifdef HAPTIC_ENABLE
#    include "haptic.h"
#endif

void nvm_eeconfig_erase(void) {
    fs_rmdir("ee", true);
    fs_mkdir("ee");
}

// TODO
// littlefs doesn't like writes mid-way through a file.
// Swap to a mechanism much like classic QMK wear-leveling:
// - Write the full data first time around
// - Subsequent writes are (header+data) appended -- header contains (offset+length)
// - Once a threahold is reached, rewrite the entire file with the live copy of data instead of playing back the log

void fs_read_block(const char *filename, void *data, size_t size) {
    fs_fd_t fd = fs_open(filename, FS_READ);
    if (fd == INVALID_FILESYSTEM_FD) {
        memset(data, 0, size);
        return;
    }
    if (fs_read(fd, data, size) != size) {
        memset(data, 0, size);
    }
    fs_close(fd);
}

void fs_update_block(const char *filename, const void *data, size_t size) {
    uint8_t buffer[size];
    fs_read_block(filename, buffer, size);
    if (memcmp(buffer, data, size) == 0) {
        return;
    }
    fs_fd_t fd = fs_open(filename, FS_WRITE | FS_TRUNCATE);
    if (fd == INVALID_FILESYSTEM_FD) {
        return;
    }
    fs_write(fd, data, size);
    fs_close(fd);
}

#define FS_RW_TYPED(type, suffix)                                     \
    static type fs_read_##suffix(const char *filename) {              \
        type data;                                                    \
        fs_read_block(filename, &data, sizeof(data));                 \
        return data;                                                  \
    }                                                                 \
    static void fs_update_##suffix(const char *filename, type data) { \
        fs_update_block(filename, &data, sizeof(data));               \
    }

FS_RW_TYPED(uint32_t, u32)
FS_RW_TYPED(uint16_t, u16)
FS_RW_TYPED(uint8_t, u8)

static const char EECONFIG_MAGIC[]         = "ee/magic";
static const char EECONFIG_DEBUG[]         = "ee/debug";
static const char EECONFIG_DEFAULT_LAYER[] = "ee/default_layer";
static const char EECONFIG_KEYMAP[]        = "ee/keymap";
static const char EECONFIG_KEYBOARD[]      = "ee/keyboard";
static const char EECONFIG_USER[]          = "ee/user";
static const char EECONFIG_HANDEDNESS[]    = "ee/handedness";
static const char EECONFIG_KEYMAP_HASH[]   = "ee/keymap_hash";

bool nvm_eeconfig_is_enabled(void) {
    return fs_read_u16(EECONFIG_MAGIC) == EECONFIG_MAGIC_NUMBER;
}

bool nvm_eeconfig_is_disabled(void) {
    return fs_read_u16(EECONFIG_MAGIC) == EECONFIG_MAGIC_NUMBER_OFF;
}

void nvm_eeconfig_enable(void) {
    fs_update_u16(EECONFIG_MAGIC, EECONFIG_MAGIC_NUMBER);
}

void nvm_eeconfig_disable(void) {
    nvm_eeconfig_erase();
    fs_update_u16(EECONFIG_MAGIC, EECONFIG_MAGIC_NUMBER_OFF);
}

void nvm_eeconfig_read_debug(debug_config_t *debug_config) {
    debug_config->raw = fs_read_u8(EECONFIG_DEBUG);
}
void nvm_eeconfig_update_debug(const debug_config_t *debug_config) {
    fs_update_u8(EECONFIG_DEBUG, debug_config->raw);
}

uint8_t nvm_eeconfig_read_default_layer(void) {
    return fs_read_u8(EECONFIG_DEFAULT_LAYER);
}
void nvm_eeconfig_update_default_layer(uint8_t val) {
    fs_update_u8(EECONFIG_DEFAULT_LAYER, val);
}

void nvm_eeconfig_read_keymap(keymap_config_t *keymap_config) {
    keymap_config->raw = fs_read_u16(EECONFIG_KEYMAP);
}
void nvm_eeconfig_update_keymap(const keymap_config_t *keymap_config) {
    fs_update_u16(EECONFIG_KEYMAP, keymap_config->raw);
}

#ifdef AUDIO_ENABLE
static const char EECONFIG_AUDIO[] = "ee/audio";
void              nvm_eeconfig_read_audio(audio_config_t *audio_config) {
    audio_config->raw = fs_read_u8(EECONFIG_AUDIO);
}
void nvm_eeconfig_update_audio(const audio_config_t *audio_config) {
    fs_update_u8(EECONFIG_AUDIO, audio_config->raw);
}
#endif // AUDIO_ENABLE

#ifdef UNICODE_COMMON_ENABLE
static const char EECONFIG_UNICODEMODE[] = "ee/unicodemode";
void              nvm_eeconfig_read_unicode_mode(unicode_config_t *unicode_config) {
    unicode_config->raw = fs_read_u8(EECONFIG_UNICODEMODE);
}
void nvm_eeconfig_update_unicode_mode(const unicode_config_t *unicode_config) {
    fs_update_u8(EECONFIG_UNICODEMODE, unicode_config->raw);
}
#endif // UNICODE_COMMON_ENABLE

#ifdef BACKLIGHT_ENABLE
static const char EECONFIG_BACKLIGHT[] = "ee/backlight";
void              nvm_eeconfig_read_backlight(backlight_config_t *backlight_config) {
    backlight_config->raw = fs_read_u8(EECONFIG_BACKLIGHT);
}
void nvm_eeconfig_update_backlight(const backlight_config_t *backlight_config) {
    fs_update_u8(EECONFIG_BACKLIGHT, backlight_config->raw);
}
#endif // BACKLIGHT_ENABLE

#ifdef STENO_ENABLE
static const char EECONFIG_STENOMODE[] = "ee/stenomode";
uint8_t           nvm_eeconfig_read_steno_mode(void) {
    return fs_read_u8(EECONFIG_STENOMODE);
}
void nvm_eeconfig_update_steno_mode(uint8_t val) {
    fs_update_u8(EECONFIG_STENOMODE, val);
}
#endif // STENO_ENABLE

#ifdef RGB_MATRIX_ENABLE
static const char EECONFIG_RGB_MATRIX[] = "ee/rgb_matrix";
void              nvm_eeconfig_read_rgb_matrix(rgb_config_t *rgb_matrix_config) {
    fs_read_block(EECONFIG_RGB_MATRIX, &rgb_matrix_config, sizeof(rgb_config_t));
}
void nvm_eeconfig_update_rgb_matrix(const rgb_config_t *rgb_matrix_config) {
    fs_update_block(EECONFIG_RGB_MATRIX, &rgb_matrix_config, sizeof(rgb_config_t));
}
#endif // RGB_MATRIX_ENABLE

#ifdef LED_MATRIX_ENABLE
static const char EECONFIG_LED_MATRIX[] = "ee/led_matrix";
void              nvm_eeconfig_read_led_matrix(led_eeconfig_t *led_matrix_config) {
    fs_read_block(EECONFIG_LED_MATRIX, &led_matrix_config, sizeof(led_eeconfig_t));
}
void nvm_eeconfig_update_led_matrix(const led_eeconfig_t *led_matrix_config) {
    fs_update_block(EECONFIG_LED_MATRIX, &led_matrix_config, sizeof(led_eeconfig_t));
}
#endif // LED_MATRIX_ENABLE

#ifdef RGBLIGHT_ENABLE
static const char EECONFIG_RGBLIGHT[] = "ee/rgblight";
void              nvm_eeconfig_read_rgblight(rgblight_config_t *rgblight_config) {
    fs_read_block(EECONFIG_RGBLIGHT, &rgblight_config, sizeof(rgblight_config_t));
}
void nvm_eeconfig_update_rgblight(const rgblight_config_t *rgblight_config) {
    fs_update_block(EECONFIG_RGBLIGHT, &rgblight_config, sizeof(rgblight_config_t));
}
#endif // RGBLIGHT_ENABLE

#if (EECONFIG_KB_DATA_SIZE) == 0
uint32_t nvm_eeconfig_read_kb(void) {
    return fs_read_u32(EECONFIG_KEYBOARD);
}
void nvm_eeconfig_update_kb(uint32_t val) {
    fs_update_u32(EECONFIG_KEYBOARD, val);
}
#endif // (EECONFIG_KB_DATA_SIZE) == 0

#if (EECONFIG_USER_DATA_SIZE) == 0
uint32_t nvm_eeconfig_read_user(void) {
    return fs_read_u32(EECONFIG_USER);
}
void nvm_eeconfig_update_user(uint32_t val) {
    fs_update_u32(EECONFIG_USER, val);
}
#endif // (EECONFIG_USER_DATA_SIZE) == 0

#ifdef HAPTIC_ENABLE
static const char EECONFIG_HAPTIC[] = "ee/haptic";
void              nvm_eeconfig_read_haptic(haptic_config_t *haptic_config) {
    haptic_config->raw = fs_read_u32(EECONFIG_HAPTIC);
}
void nvm_eeconfig_update_haptic(const haptic_config_t *haptic_config) {
    fs_update_u32(EECONFIG_HAPTIC, haptic_config->raw);
}
#endif // HAPTIC_ENABLE

bool nvm_eeconfig_read_handedness(void) {
    return !!fs_read_u8(EECONFIG_HANDEDNESS);
}
void nvm_eeconfig_update_handedness(bool val) {
    fs_update_u8(EECONFIG_HANDEDNESS, !!val);
}

uint32_t nvm_eeconfig_read_keymap_hash(void) {
    return fs_read_u32(EECONFIG_KEYMAP_HASH);
}
void nvm_eeconfig_update_keymap_hash(uint32_t val) {
    fs_update_u32(EECONFIG_KEYMAP_HASH, val);
}

#if (EECONFIG_KB_DATA_SIZE) > 0
static const char EECONFIG_KB_DATABLOCK[] = "ee/kb_datablock";

bool nvm_eeconfig_is_kb_datablock_valid(void) {
    return fs_read_u32(EECONFIG_KEYBOARD) == (EECONFIG_KB_DATA_VERSION);
}

uint32_t nvm_eeconfig_read_kb_datablock(void *data, uint32_t offset, uint32_t length) {
    if (eeconfig_is_user_datablock_valid()) {
        fs_fd_t fd = fs_open(EECONFIG_KB_DATABLOCK, FS_READ);
        if (fd == INVALID_FILESYSTEM_FD) {
            memset(data, 0, length);
            return length;
        }
        fs_seek(fd, offset, FS_SEEK_SET);
        if (fs_read(fd, data, length) != length) {
            memset(data, 0, length);
        }
        fs_close(fd);
        return length;
    } else {
        memset(data, 0, length);
        return length;
    }
}

uint32_t nvm_eeconfig_update_kb_datablock(const void *data, uint32_t offset, uint32_t length) {
    fs_update_u32(EECONFIG_USER, (EECONFIG_USER_DATA_VERSION));

    fs_fd_t fd = fs_open(EECONFIG_KB_DATABLOCK, FS_WRITE);
    if (fd == INVALID_FILESYSTEM_FD) {
        return 0;
    }
    fs_seek(fd, offset, FS_SEEK_SET);
    if (fs_write(fd, data, length) != length) {
        fs_close(fd);
        return 0;
    }
    fs_close(fd);
    return length;
}

void nvm_eeconfig_init_kb_datablock(void) {
    fs_update_u32(EECONFIG_KEYBOARD, (EECONFIG_KB_DATA_VERSION));
    fs_delete(EECONFIG_KB_DATABLOCK);
    fs_fd_t fd = fs_open(EECONFIG_KB_DATABLOCK, FS_WRITE);
    if (fd == INVALID_FILESYSTEM_FD) {
        return;
    }
    fs_seek(fd, -1, FS_SEEK_SET);
    fs_write(fd, "", 1);
    fs_close(fd);
}

#endif // (EECONFIG_KB_DATA_SIZE) > 0

#if (EECONFIG_USER_DATA_SIZE) > 0
static const char EECONFIG_USER_DATABLOCK[] = "ee/user_datablock";

bool nvm_eeconfig_is_user_datablock_valid(void) {
    return fs_read_u32(EECONFIG_USER) == (EECONFIG_USER_DATA_VERSION);
}

uint32_t nvm_eeconfig_read_user_datablock(void *data, uint32_t offset, uint32_t length) {
    if (eeconfig_is_user_datablock_valid()) {
        fs_fd_t fd = fs_open(EECONFIG_USER_DATABLOCK, FS_READ);
        if (fd == INVALID_FILESYSTEM_FD) {
            memset(data, 0, length);
            return length;
        }
        fs_seek(fd, offset, FS_SEEK_SET);
        if (fs_read(fd, data, length) != length) {
            memset(data, 0, length);
        }
        fs_close(fd);
        return length;
    } else {
        memset(data, 0, length);
        return length;
    }
}

uint32_t nvm_eeconfig_update_user_datablock(const void *data, uint32_t offset, uint32_t length) {
    fs_update_u32(EECONFIG_USER, (EECONFIG_USER_DATA_VERSION));

    fs_fd_t fd = fs_open(EECONFIG_USER_DATABLOCK, FS_WRITE);
    if (fd == INVALID_FILESYSTEM_FD) {
        return 0;
    }
    fs_seek(fd, offset, FS_SEEK_SET);
    if (fs_write(fd, data, length) != length) {
        fs_close(fd);
        return 0;
    }
    fs_close(fd);
    return length;
}

void nvm_eeconfig_init_user_datablock(void) {
    fs_update_u32(EECONFIG_USER, (EECONFIG_USER_DATA_VERSION));
    fs_delete(EECONFIG_USER_DATABLOCK);
    fs_fd_t fd = fs_open(EECONFIG_USER_DATABLOCK, FS_WRITE);
    if (fd == INVALID_FILESYSTEM_FD) {
        return;
    }
    fs_seek(fd, -1, FS_SEEK_SET);
    fs_write(fd, "", 1);
    fs_close(fd);
}

#endif // (EECONFIG_USER_DATA_SIZE) > 0
