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

#define MAX_STACK_BUFFER_SIZE 32

static bool fs_chunked_data_compare(fs_fd_t fd, const void *data, size_t size) {
    uint8_t        stack_buffer[MAX_STACK_BUFFER_SIZE];
    size_t         remaining = size;
    size_t         offset    = 0;
    const uint8_t *data_ptr  = (const uint8_t *)data;

    while (remaining > 0) {
        size_t chunk_size = remaining > MAX_STACK_BUFFER_SIZE ? MAX_STACK_BUFFER_SIZE : remaining;
        if (fs_read(fd, stack_buffer, chunk_size) != chunk_size) {
            return false; // Read error, assume different
        }
        if (memcmp(stack_buffer, data_ptr + offset, chunk_size) != 0) {
            return false; // Data differs
        }
        offset += chunk_size;
        remaining -= chunk_size;
    }
    return true; // All chunks match
}

size_t fs_read_block(const char *filename, void *data, size_t size) {
    fs_fd_t fd = fs_open(filename, FS_READ);
    if (fd == INVALID_FILESYSTEM_FD) {
        fs_dprintf("could not open file\n");
        memset(data, 0, size);
        return 0;
    }
    size_t read_bytes;
    if ((read_bytes = fs_read(fd, data, size)) != size) {
        fs_dprintf("did not read correct number of bytes\n");
        memset(data, 0, size);
        fs_close(fd);
        return read_bytes;
    }
    fs_close(fd);
    fs_hexdump("read", filename, data, size);
    return size;
}

void fs_update_block(const char *filename, const void *data, size_t size) {
    fs_hexdump("save", filename, data, size);

    // Check if data has changed using chunked comparison
    fs_fd_t read_fd = fs_open(filename, FS_READ);
    if (read_fd != INVALID_FILESYSTEM_FD) {
        if (fs_chunked_data_compare(read_fd, data, size)) {
            fs_dprintf("no change, skipping write\n");
            fs_close(read_fd);
            return;
        }
        fs_close(read_fd);
    }
    fs_fd_t fd = fs_open(filename, FS_WRITE | FS_TRUNCATE);
    if (fd == INVALID_FILESYSTEM_FD) {
        fs_dprintf("could not open file\n");
        return;
    }
    if (fs_write(fd, data, size) != size) {
        fs_dprintf("did not write correct number of bytes\n");
    }
    fs_close(fd);

#if defined(FILESYSTEM_VERIFY_WRITES)
    // Verify write integrity for data safety
    fs_fd_t verify_fd = fs_open(filename, FS_READ);
    if (verify_fd != INVALID_FILESYSTEM_FD) {
        if (!fs_chunked_data_compare(verify_fd, data, size)) {
            fs_dprintf("readback mismatch!\n");
        }
        fs_close(verify_fd);
    }
#endif // FILESYSTEM_VERIFY_WRITES
}

#define FS_RW_TYPED(type, suffix)                                     \
    static type fs_read_##suffix(const char *filename) {              \
        type data;                                                    \
        fs_read_block(filename, &data, sizeof(type));                 \
        return data;                                                  \
    }                                                                 \
    static void fs_update_##suffix(const char *filename, type data) { \
        fs_update_block(filename, &data, sizeof(type));               \
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

layer_state_t nvm_eeconfig_read_default_layer(void) {
    layer_state_t layer_state = 0;
    fs_read_block(EECONFIG_DEFAULT_LAYER, &layer_state, sizeof(layer_state_t));
    return layer_state;
}

void nvm_eeconfig_update_default_layer(layer_state_t val) {
    fs_update_block(EECONFIG_DEFAULT_LAYER, &val, sizeof(layer_state_t));
}

void nvm_eeconfig_read_keymap(keymap_config_t *keymap_config) {
    fs_read_block(EECONFIG_KEYMAP, keymap_config, sizeof(keymap_config_t));
}
void nvm_eeconfig_update_keymap(const keymap_config_t *keymap_config) {
    fs_update_block(EECONFIG_KEYMAP, keymap_config, sizeof(keymap_config_t));
}

#ifdef AUDIO_ENABLE
static const char EECONFIG_AUDIO[] = "ee/audio";
void              nvm_eeconfig_read_audio(audio_config_t *audio_config) {
    fs_read_block(EECONFIG_AUDIO, audio_config, sizeof(audio_config_t));
}
void nvm_eeconfig_update_audio(const audio_config_t *audio_config) {
    fs_update_block(EECONFIG_AUDIO, audio_config, sizeof(audio_config_t));
}
#endif // AUDIO_ENABLE

#ifdef UNICODE_COMMON_ENABLE
static const char EECONFIG_UNICODEMODE[] = "ee/unicodemode";
void              nvm_eeconfig_read_unicode_mode(unicode_config_t *unicode_config) {
    fs_read_block(EECONFIG_UNICODEMODE, unicode_config, sizeof(unicode_config_t));
}
void nvm_eeconfig_update_unicode_mode(const unicode_config_t *unicode_config) {
    fs_update_block(EECONFIG_UNICODEMODE, unicode_config, sizeof(unicode_config_t));
}
#endif // UNICODE_COMMON_ENABLE

#ifdef BACKLIGHT_ENABLE
static const char EECONFIG_BACKLIGHT[] = "ee/backlight";
void              nvm_eeconfig_read_backlight(backlight_config_t *backlight_config) {
    fs_read_block(EECONFIG_BACKLIGHT, backlight_config, sizeof(backlight_config_t));
}
void nvm_eeconfig_update_backlight(const backlight_config_t *backlight_config) {
    fs_update_block(EECONFIG_BACKLIGHT, backlight_config, sizeof(backlight_config_t));
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
    fs_read_block(EECONFIG_RGB_MATRIX, rgb_matrix_config, sizeof(rgb_config_t));
}
void nvm_eeconfig_update_rgb_matrix(const rgb_config_t *rgb_matrix_config) {
    fs_update_block(EECONFIG_RGB_MATRIX, rgb_matrix_config, sizeof(rgb_config_t));
}
#endif // RGB_MATRIX_ENABLE

#ifdef LED_MATRIX_ENABLE
static const char EECONFIG_LED_MATRIX[] = "ee/led_matrix";
void              nvm_eeconfig_read_led_matrix(led_eeconfig_t *led_matrix_config) {
    fs_read_block(EECONFIG_LED_MATRIX, led_matrix_config, sizeof(led_eeconfig_t));
}
void nvm_eeconfig_update_led_matrix(const led_eeconfig_t *led_matrix_config) {
    fs_update_block(EECONFIG_LED_MATRIX, led_matrix_config, sizeof(led_eeconfig_t));
}
#endif // LED_MATRIX_ENABLE

#ifdef RGBLIGHT_ENABLE
static const char EECONFIG_RGBLIGHT[] = "ee/rgblight";
void              nvm_eeconfig_read_rgblight(rgblight_config_t *rgblight_config) {
    fs_read_block(EECONFIG_RGBLIGHT, rgblight_config, sizeof(rgblight_config_t));
}
void nvm_eeconfig_update_rgblight(const rgblight_config_t *rgblight_config) {
    fs_update_block(EECONFIG_RGBLIGHT, rgblight_config, sizeof(rgblight_config_t));
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
    fs_read_block(EECONFIG_HAPTIC, haptic_config, sizeof(haptic_config_t));
}
void nvm_eeconfig_update_haptic(const haptic_config_t *haptic_config) {
    fs_update_block(EECONFIG_HAPTIC, haptic_config, sizeof(haptic_config_t));
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
    if (nvm_eeconfig_is_kb_datablock_valid()) {
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
    fs_update_u32(EECONFIG_KEYBOARD, (EECONFIG_KB_DATA_VERSION));

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
    fs_seek(fd, (EECONFIG_KB_DATA_SIZE)-1, FS_SEEK_SET);
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
    if (nvm_eeconfig_is_user_datablock_valid()) {
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
    fs_seek(fd, (EECONFIG_USER_DATA_SIZE)-1, FS_SEEK_SET);
    fs_write(fd, "", 1);
    fs_close(fd);
}

#endif // (EECONFIG_USER_DATA_SIZE) > 0
