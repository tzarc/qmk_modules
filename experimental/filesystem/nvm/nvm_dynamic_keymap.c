// Copyright 2022-2025 Nick Brassel (@tzarc)
// SPDX-License-Identifier: GPL-2.0-or-later

#include "keycodes.h"
#include "filesystem.h"
#include "nvm_filesystem.h"
#include "dynamic_keymap.h"
#include "nvm_dynamic_keymap.h"
#include "action_layer.h"
#include "keymap_introspection.h"
#include "community_modules.h"
#include "printf.h"

// TODO:
// - Validate macros bounds checking
// - Make layer count fully dynamic, disregard DYNAMIC_KEYMAP_LAYER_COUNT
// - More efficient layer write updates -- changing one key should not require a write of the full layer

static layer_state_t dynamic_keymap_dirty_layers;
static uint32_t      dynamic_keymap_dirty_cache[DYNAMIC_KEYMAP_LAYER_COUNT][((MATRIX_ROWS) * (MATRIX_COLS)) / 32];
static uint16_t      dynamic_keymap_layer_cache[DYNAMIC_KEYMAP_LAYER_COUNT][MATRIX_ROWS][MATRIX_COLS];

static __attribute__((used)) bool is_keymap_dirty(uint8_t layer, uint8_t row, uint8_t col) {
    // Assume layer/row/col already bounds-checked by caller
    if (!(dynamic_keymap_dirty_layers & (1 << layer))) {
        return false;
    }
    size_t index = (row * (MATRIX_COLS)) + col;
    return (dynamic_keymap_dirty_cache[layer][index / 32] & (1 << (index % 32))) != 0;
}

static void set_keymap_dirty(uint8_t layer, uint8_t row, uint8_t col, bool val) {
    // Assume layer/row/col already bounds-checked by caller
    size_t index = (row * (MATRIX_COLS)) + col;
    if (val) {
        dynamic_keymap_dirty_cache[layer][index / 32] |= (1 << (index % 32));
        dynamic_keymap_dirty_layers |= (1 << layer);
    } else {
        dynamic_keymap_dirty_cache[layer][index / 32] &= ~(1 << (index % 32));
    }
}

void nvm_dynamic_keymap_erase(void) {
    fs_rmdir("layers", true);
    fs_mkdir("layers");
}

void nvm_dynamic_keymap_macro_erase(void) {
    fs_rmdir("macros", true);
    fs_mkdir("macros");
}

uint8_t keymap_layer_count(void) {
    return DYNAMIC_KEYMAP_LAYER_COUNT;
}

uint16_t nvm_dynamic_keymap_read_keycode(uint8_t layer, uint8_t row, uint8_t column) {
    if (layer >= keymap_layer_count() || row >= MATRIX_ROWS || column >= MATRIX_COLS) return KC_NO;
    return dynamic_keymap_layer_cache[layer][row][column];
}

void nvm_dynamic_keymap_update_keycode(uint8_t layer, uint8_t row, uint8_t column, uint16_t keycode) {
    if (layer >= keymap_layer_count() || row >= MATRIX_ROWS || column >= MATRIX_COLS) return;
    if (dynamic_keymap_layer_cache[layer][row][column] != keycode) {
        dynamic_keymap_layer_cache[layer][row][column] = keycode;
        set_keymap_dirty(layer, row, column, true);
    }
}

void nvm_dynamic_keymap_save(void) {
    // TODO: Implement a more efficient way to save only the dirty keycodes, as littlefs prefers appending data instead of writing the whole thing
    for (int i = 0; i < (sizeof(layer_state_t) * 8); ++i) {
        if (dynamic_keymap_dirty_layers & (1 << i)) {
            char filename[16] = {0};
            snprintf(filename, sizeof(filename), "layers/key%02d", i);
            fs_update_block(filename, dynamic_keymap_layer_cache[i], sizeof(dynamic_keymap_layer_cache[i]));
            dynamic_keymap_dirty_layers &= ~(1 << i);
        }
    }
}

void nvm_dynamic_keymap_load(void) {
    for (int i = 0; i < (sizeof(layer_state_t) * 8); ++i) {
        char filename[16] = {0};
        snprintf(filename, sizeof(filename), "layers/key%02d", i);
        if (fs_exists(filename)) {
            fs_read_block(filename, dynamic_keymap_layer_cache[i], sizeof(dynamic_keymap_layer_cache[i]));
        }
    }
}

#if defined(ENCODER_ENABLE) && defined(ENCODER_MAP_ENABLE)
#    include "encoder.h"
static layer_state_t dynamic_encodermap_dirty_layers;
static uint32_t      dynamic_encodermap_dirty_cache[DYNAMIC_KEYMAP_LAYER_COUNT][(NUM_ENCODERS) * (NUM_DIRECTIONS) / 32];
static uint16_t      dynamic_encodermap_layer_cache[DYNAMIC_KEYMAP_LAYER_COUNT][NUM_ENCODERS][NUM_DIRECTIONS];

static __attribute__((used)) bool is_encodermap_dirty(uint8_t layer, uint8_t encoder_idx, bool clockwise) {
    // Assume layer/encoder_idx already bounds-checked by caller
    if (!(dynamic_encodermap_dirty_layers & (1 << layer))) {
        return false;
    }
    size_t index = (encoder_idx * (NUM_DIRECTIONS)) + (clockwise ? 0 : 1);
    return (dynamic_encodermap_dirty_cache[layer][index / 32] & (1 << (index % 32))) != 0;
}

static void set_encodermap_dirty(uint8_t layer, uint8_t encoder_idx, bool clockwise, bool val) {
    // Assume layer/encoder_idx already bounds-checked by caller
    size_t index = (encoder_idx * (NUM_DIRECTIONS)) + (clockwise ? 0 : 1);
    if (val) {
        dynamic_encodermap_dirty_cache[layer][index / 32] |= (1 << (index % 32));
        dynamic_encodermap_dirty_layers |= (1 << layer);
    } else {
        dynamic_encodermap_dirty_cache[layer][index / 32] &= ~(1 << (index % 32));
    }
}

uint16_t nvm_dynamic_keymap_read_encoder(uint8_t layer, uint8_t encoder_id, bool clockwise) {
    if (layer >= encodermap_layer_count() || encoder_id >= NUM_ENCODERS) return KC_NO;
    return dynamic_encodermap_layer_cache[layer][encoder_id][clockwise ? 0 : 1];
}

void nvm_dynamic_keymap_update_encoder(uint8_t layer, uint8_t encoder_id, bool clockwise, uint16_t keycode) {
    if (layer >= encodermap_layer_count() || encoder_id >= NUM_ENCODERS) return;
    if (dynamic_encodermap_layer_cache[layer][encoder_id][clockwise ? 0 : 1] != keycode) {
        dynamic_encodermap_layer_cache[layer][encoder_id][clockwise ? 0 : 1] = keycode;
        set_encodermap_dirty(layer, encoder_id, clockwise, true);
    }
}

void nvm_dynamic_encodermap_save(void) {
    // TODO: Implement a more efficient way to save only the dirty keycodes, as littlefs prefers appending data instead of writing the whole thing
    for (int i = 0; i < (sizeof(layer_state_t) * 8); ++i) {
        if (dynamic_encodermap_dirty_layers & (1 << i)) {
            char filename[16] = {0};
            snprintf(filename, sizeof(filename), "layers/enc%02d", i);
            fs_update_block(filename, dynamic_encodermap_layer_cache[i], sizeof(dynamic_encodermap_layer_cache[i]));
            dynamic_encodermap_dirty_layers &= ~(1 << i);
        }
    }
}

void nvm_dynamic_encodermap_load(void) {
    for (int i = 0; i < (sizeof(layer_state_t) * 8); ++i) {
        char filename[16] = {0};
        snprintf(filename, sizeof(filename), "layers/enc%02d", i);
        if (fs_exists(filename)) {
            fs_read_block(filename, dynamic_encodermap_layer_cache[i], sizeof(dynamic_encodermap_layer_cache[i]));
        }
    }
}

#endif // defined(ENCODER_ENABLE) && defined(ENCODER_MAP_ENABLE)

void nvm_dynamic_keymap_read_buffer(uint32_t offset, uint32_t size, uint8_t *data) {
    memset(data, 0, size);
    if (offset + size >= sizeof(dynamic_keymap_layer_cache)) {
        size = sizeof(dynamic_keymap_layer_cache) - offset;
    }
    memcpy(data, dynamic_keymap_layer_cache, size);
}

void nvm_dynamic_keymap_update_buffer(uint32_t offset, uint32_t size, uint8_t *data) {
    if (offset + size >= sizeof(dynamic_keymap_layer_cache)) {
        size = sizeof(dynamic_keymap_layer_cache) - offset;
    }
    uint32_t  end = offset + size;
    uint16_t *src = (uint16_t *)data;
    while (offset < end) {
        uint16_t layer = offset / (MATRIX_ROWS * MATRIX_COLS * 2);
        uint16_t row   = (offset / (MATRIX_COLS * 2)) % MATRIX_ROWS;
        uint16_t col   = (offset / 2) % MATRIX_COLS;
        nvm_dynamic_keymap_update_keycode(layer, row, col, *src);
        offset += 2;
        src++;
    }
}

bool dynamic_macro_dirty        = false;
char dynamic_macro_buffer[1024] = {0};

uint32_t nvm_dynamic_keymap_macro_size(void) {
    return sizeof(dynamic_macro_buffer);
}

void nvm_dynamic_keymap_macro_read_buffer(uint32_t offset, uint32_t size, uint8_t *data) {
    memset(data, 0, size);
    if (offset + size >= sizeof(dynamic_macro_buffer)) {
        size = sizeof(dynamic_macro_buffer) - offset;
    }
    memcpy(data, dynamic_macro_buffer + offset, size);
}
void nvm_dynamic_keymap_macro_update_buffer(uint32_t offset, uint32_t size, uint8_t *data) {
    if (offset + size >= sizeof(dynamic_macro_buffer)) {
        size = sizeof(dynamic_macro_buffer) - offset;
    }
    if (memcmp(dynamic_macro_buffer + offset, data, size) != 0) {
        memcpy(dynamic_macro_buffer + offset, data, size);
        dynamic_macro_dirty = true;
    }
}

void nvm_dynamic_keymap_macro_reset(void) {
    nvm_dynamic_keymap_macro_erase();
    memset(dynamic_macro_buffer, 0, sizeof(dynamic_macro_buffer));
    dynamic_macro_dirty = false;
}

void nvm_dynamic_keymap_macro_save(void) {
    if (dynamic_macro_dirty) {
        int   n           = 0;
        char *terminator  = dynamic_macro_buffer + sizeof(dynamic_macro_buffer);
        char *macro_start = dynamic_macro_buffer;
        while (macro_start < terminator) {
            char *macro_end = macro_start;
            while (*macro_end != 0 && macro_end < terminator) {
                macro_end++;
            }
            if (macro_end - macro_start > 0) {
                char filename[18] = {0};
                snprintf(filename, sizeof(filename), "macros/%02d", n);
                fs_update_block(filename, macro_start, macro_end - macro_start);
            }
            ++n;
            macro_start = macro_end + 1;
        }
        dynamic_macro_dirty = false;
    }
}

void nvm_dynamic_keymap_macro_load(void) {
    memset(dynamic_macro_buffer, 0, sizeof(dynamic_macro_buffer));
    int    n         = 0;
    char  *ptr       = dynamic_macro_buffer;
    size_t max_count = sizeof(dynamic_macro_buffer);
    while (true) {
        char filename[18] = {0};
        snprintf(filename, sizeof(filename), "macros/%02d", n);
        if (fs_exists(filename)) {
            fs_fd_t fd = fs_open(filename, FS_READ);
            if (fd != INVALID_FILESYSTEM_FD) {
                size_t count = fs_read(fd, ptr, max_count);
                fs_close(fd);

                if (count > 0) {
                    ptr += (count + 1);
                    max_count -= (count + 1);
                } else {
                    break;
                }
            }
        } else {
            break;
        }
        ++n;
    }
}

////////////////////////////////////////////////////////////////////////////////
// Base hooks

void keyboard_post_init_filesystem() {
    keyboard_post_init_filesystem_kb();
    nvm_dynamic_keymap_load();
    nvm_dynamic_keymap_macro_load();
#if defined(ENCODER_ENABLE) && defined(ENCODER_MAP_ENABLE)
    nvm_dynamic_encodermap_load();
#endif // defined(ENCODER_ENABLE) && defined(ENCODER_MAP_ENABLE)
}

void housekeeping_task_filesystem(void) {
    // Throttle saves to every 250ms
    static uint32_t last_exec = 0;
    if (timer_elapsed32(last_exec) >= 250) {
        last_exec = timer_read32();

        nvm_dynamic_keymap_save();
        nvm_dynamic_keymap_macro_save();
#if defined(ENCODER_ENABLE) && defined(ENCODER_MAP_ENABLE)
        nvm_dynamic_encodermap_save();
#endif
    }
}
