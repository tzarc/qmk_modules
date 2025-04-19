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

// Keep track of how many keys have been altered per layer
static uint16_t dynamic_keymap_altered_count[DYNAMIC_KEYMAP_LAYER_COUNT] = {0};
// Keep track of the altered keys by bitmask
static uint32_t dynamic_keymap_altered_keys[DYNAMIC_KEYMAP_LAYER_COUNT][(((MATRIX_ROWS) * (MATRIX_COLS)) + 31) / 32];
// The "live" copy of the keymap, cached in RAM
static uint16_t dynamic_keymap_layer_cache[DYNAMIC_KEYMAP_LAYER_COUNT][MATRIX_ROWS][MATRIX_COLS];

// Scratch area to hold the to-be-written keycode data, either as a full keymap or as a list of keycode overrides, whichever is smaller
static struct __attribute__((packed)) {
    uint8_t write_mode; // 0 == full keymap, 1 == overrides
    union {
        uint16_t keycodes[MATRIX_ROWS][MATRIX_COLS];
        struct {
            uint8_t  row;
            uint8_t  col;
            uint16_t keycode;
        } overrides[(((((MATRIX_ROWS) + 1) / 2) * 2) * (((MATRIX_COLS + 1) / 2) * 2)) / (sizeof(uint32_t) / sizeof(uint16_t))];
    };
} dynamic_keymap_scratch;

static bool is_key_altered(uint8_t layer, uint8_t row, uint8_t col) {
    size_t index = (row * (MATRIX_COLS)) + col;
    return (dynamic_keymap_altered_keys[layer][index / 32] & (1 << (index % 32))) != 0;
}

static void set_key_altered(uint8_t layer, uint8_t row, uint8_t col, bool val) {
    // Assume layer/row/col already bounds-checked by caller
    size_t index = (row * (MATRIX_COLS)) + col;

    // Update the altered key count for the layer if we've had a change
    bool orig_val = dynamic_keymap_altered_keys[layer][index / 32] & (1 << (index % 32)) ? true : false;
    if (val != orig_val) {
        dynamic_keymap_altered_count[layer] += val ? 1 : -1;
    }

    if (val) {
        // Mark the key index as altered
        dynamic_keymap_altered_keys[layer][index / 32] |= (1 << (index % 32));
    } else {
        // Unmark the key index from being altered
        dynamic_keymap_altered_keys[layer][index / 32] &= ~(1 << (index % 32));
    }
}

static void nvm_dynamic_keymap_reset_cache_layer_to_raw(uint8_t layer);
static void nvm_dynamic_keymap_reset_cache_to_raw(void);

void nvm_dynamic_keymap_erase(void) {
    fs_rmdir("layers", true);
    fs_mkdir("layers");
    nvm_dynamic_keymap_reset_cache_to_raw();
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
    dynamic_keymap_layer_cache[layer][row][column] = keycode;
    set_key_altered(layer, row, column, keycode == keycode_at_keymap_location_raw(layer, row, column));
}

void nvm_dynamic_keymap_save(void) {
    // TODO: Implement a more efficient way to save only the altered keycodes, as littlefs prefers appending data instead of writing the whole thing
    for (int layer = 0; layer < (sizeof(layer_state_t) * 8); ++layer) {
        char filename[18] = {0};
        snprintf(filename, sizeof(filename), "layers/key%02d", layer);
        if (dynamic_keymap_altered_count[layer] == 0) {
            // If nothing has been altered, delete any existing file as we'll just use the raw keymap for this layer
            fs_delete(filename);
        } else {
            if (sizeof(dynamic_keymap_scratch.keycodes) <= (sizeof(dynamic_keymap_scratch.overrides[0]) * dynamic_keymap_altered_count[layer])) {
                // write the entire layer to filesystem
                dynamic_keymap_scratch.write_mode = 0;
                memcpy(dynamic_keymap_scratch.keycodes, dynamic_keymap_layer_cache[layer], sizeof(dynamic_keymap_scratch.keycodes));
                fs_update_block(filename, &dynamic_keymap_scratch, sizeof(uint8_t) + sizeof(dynamic_keymap_scratch.keycodes));
            } else {
                // write the overrides to filesystem
                dynamic_keymap_scratch.write_mode = 1;
                size_t idx                        = 0;
                for (int row = 0; row < MATRIX_ROWS; ++row) {
                    for (int col = 0; col < MATRIX_COLS; ++col) {
                        if (is_key_altered(layer, row, col)) {
                            dynamic_keymap_scratch.overrides[idx].row     = row;
                            dynamic_keymap_scratch.overrides[idx].col     = col;
                            dynamic_keymap_scratch.overrides[idx].keycode = dynamic_keymap_layer_cache[layer][row][col];
                            ++idx;
                        }
                    }
                }
                fs_update_block(filename, &dynamic_keymap_scratch, sizeof(uint8_t) + (sizeof(dynamic_keymap_scratch.overrides[0]) * dynamic_keymap_altered_count[layer]));
            }
        }
    }
}

void nvm_dynamic_keymap_load(void) {
    for (int layer = 0; layer < (sizeof(layer_state_t) * 8); ++layer) {
        char filename[18] = {0};
        snprintf(filename, sizeof(filename), "layers/key%02d", layer);
        nvm_dynamic_keymap_reset_cache_layer_to_raw(layer);
        if (!fs_exists(filename)) {
            continue;
        }
        fs_read_block(filename, &dynamic_keymap_scratch, sizeof(dynamic_keymap_scratch));
        if (dynamic_keymap_scratch.write_mode == 0) {
            // full keymap
            for (int row = 0; row < MATRIX_ROWS; ++row) {
                for (int col = 0; col < MATRIX_COLS; ++col) {
                    nvm_dynamic_keymap_update_keycode(layer, row, col, dynamic_keymap_scratch.keycodes[row][col]);
                }
            }
        } else {
            // overrides
            for (int j = 0; j < dynamic_keymap_altered_count[layer]; ++j) {
                uint8_t  row     = dynamic_keymap_scratch.overrides[j].row;
                uint8_t  column  = dynamic_keymap_scratch.overrides[j].col;
                uint16_t keycode = dynamic_keymap_scratch.overrides[j].keycode;
                nvm_dynamic_keymap_update_keycode(layer, row, column, keycode);
            }
        }
    }
}

#if defined(ENCODER_ENABLE) && defined(ENCODER_MAP_ENABLE)
#    include "encoder.h"
static layer_state_t dynamic_encodermap_altered_layers;
static uint32_t      dynamic_encodermap_altered_cache[DYNAMIC_KEYMAP_LAYER_COUNT][(NUM_ENCODERS) * (NUM_DIRECTIONS) / 32];
static uint16_t      dynamic_encodermap_layer_cache[DYNAMIC_KEYMAP_LAYER_COUNT][NUM_ENCODERS][NUM_DIRECTIONS];

static __attribute__((used)) bool is_encodermap_altered(uint8_t layer, uint8_t encoder_idx, bool clockwise) {
    // Assume layer/encoder_idx already bounds-checked by caller
    if (!(dynamic_encodermap_altered_layers & (1 << layer))) {
        return false;
    }
    size_t index = (encoder_idx * (NUM_DIRECTIONS)) + (clockwise ? 0 : 1);
    return (dynamic_encodermap_altered_cache[layer][index / 32] & (1 << (index % 32))) != 0;
}

static void set_encodermap_altered(uint8_t layer, uint8_t encoder_idx, bool clockwise, bool val) {
    // Assume layer/encoder_idx already bounds-checked by caller
    size_t index = (encoder_idx * (NUM_DIRECTIONS)) + (clockwise ? 0 : 1);
    if (val) {
        dynamic_encodermap_altered_cache[layer][index / 32] |= (1 << (index % 32));
        dynamic_encodermap_altered_layers |= (1 << layer);
    } else {
        dynamic_encodermap_altered_cache[layer][index / 32] &= ~(1 << (index % 32));
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
        set_encodermap_altered(layer, encoder_id, clockwise, true);
    }
}

void nvm_dynamic_encodermap_save(void) {
    // TODO: Implement a more efficient way to save only the altered keycodes, as littlefs prefers appending data instead of writing the whole thing
    for (int i = 0; i < (sizeof(layer_state_t) * 8); ++i) {
        if (dynamic_encodermap_altered_layers & (1 << i)) {
            char filename[16] = {0};
            snprintf(filename, sizeof(filename), "layers/enc%02d", i);
            fs_update_block(filename, dynamic_encodermap_layer_cache[i], sizeof(dynamic_encodermap_layer_cache[i]));
            dynamic_encodermap_altered_layers &= ~(1 << i);
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

bool dynamic_macro_altered      = false;
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
        dynamic_macro_altered = true;
    }
}

void nvm_dynamic_keymap_macro_reset(void) {
    nvm_dynamic_keymap_macro_erase();
    memset(dynamic_macro_buffer, 0, sizeof(dynamic_macro_buffer));
    dynamic_macro_altered = false;
}

void nvm_dynamic_keymap_macro_save(void) {
    if (dynamic_macro_altered) {
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
        dynamic_macro_altered = false;
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
// Helpers

static void nvm_dynamic_keymap_reset_cache_layer_to_raw(uint8_t layer) {
    for (int j = 0; j < MATRIX_ROWS; ++j) {
        for (int k = 0; k < MATRIX_COLS; ++k) {
            dynamic_keymap_layer_cache[layer][j][k] = keycode_at_keymap_location_raw(layer, j, k);
        }
    }
    dynamic_keymap_altered_count[layer] = 0;
    memset(dynamic_keymap_altered_keys[layer], 0, sizeof(dynamic_keymap_altered_keys[layer]));

#if defined(ENCODER_ENABLE) && defined(ENCODER_MAP_ENABLE)
    for (int j = 0; j < NUM_ENCODERS; ++j) {
        for (int k = 0; k < NUM_DIRECTIONS; ++k) {
            dynamic_encodermap_layer_cache[layer][j][k] = keycode_at_encodermap_location_raw(layer, j, k);
        }
    }
#endif // defined(ENCODER_ENABLE) && defined(ENCODER_MAP_ENABLE)
}

static void nvm_dynamic_keymap_reset_cache_to_raw(void) {
    for (int i = 0; i < keymap_layer_count(); ++i) {
        nvm_dynamic_keymap_reset_cache_layer_to_raw(i);
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
