// Copyright 2022-2025 Nick Brassel (@tzarc)
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdbool.h>
#include "keycodes.h"
#include "filesystem.h"
#include "nvm_filesystem.h"
#include "dynamic_keymap.h"
#include "nvm_dynamic_keymap.h"
#include "action_layer.h"
#include "keymap_introspection.h"
#include "community_modules.h"

#if defined(ENCODER_ENABLE) && defined(ENCODER_MAP_ENABLE)
#    include "encoder.h"

// Encoder direction constants for array indexing
#    define ENCODER_ARRAYINDEX_CW 0  // Clockwise direction array index
#    define ENCODER_ARRAYINDEX_CCW 1 // Counter-clockwise direction array index
#endif                               // defined(ENCODER_ENABLE) && defined(ENCODER_MAP_ENABLE)

// TODO:
// - Validate macros bounds checking
// - Make layer count fully dynamic, disregard DYNAMIC_KEYMAP_LAYER_COUNT

// Keep track of if anything is actually dirty
static layer_state_t dynamic_keymap_layer_dirty = 0;
// Keep track of how many keys have been altered per layer
static uint16_t dynamic_keymap_altered_count[DYNAMIC_KEYMAP_LAYER_COUNT] = {0};
// Keep track of the altered keys by bitmask
static uint32_t dynamic_keymap_altered_keys[DYNAMIC_KEYMAP_LAYER_COUNT][(((MATRIX_ROWS) * (MATRIX_COLS)) + 31) / 32];
// The "live" copy of the keymap, cached in RAM
static uint16_t dynamic_keymap_layer_cache[DYNAMIC_KEYMAP_LAYER_COUNT][MATRIX_ROWS][MATRIX_COLS];

// Override entry structure definitions
typedef struct keymap_override_entry_t {
    uint8_t  row;
    uint8_t  col;
    uint16_t keycode;
} keymap_override_entry_t;

#if defined(ENCODER_ENABLE) && defined(ENCODER_MAP_ENABLE)
typedef struct encodermap_override_entry_t {
    uint8_t  encoder_id;
    uint8_t  enc_dir;
    uint16_t keycode;
} encodermap_override_entry_t;
#endif

// Helper macros for calculating override array sizes
#define ALIGN_TO_2(x) ((((x) + 1) / 2) * 2)
#define OVERRIDE_ENTRY_SIZE(entry_type) ((sizeof(entry_type) / sizeof(uint16_t))

// Calculate maximum number of overrides that fit in the same space as full arrays
#define MAX_KEYMAP_OVERRIDES ((ALIGN_TO_2(MATRIX_ROWS) * ALIGN_TO_2(MATRIX_COLS)) / OVERRIDE_ENTRY_SIZE(keymap_override_entry_t)))

#if defined(ENCODER_ENABLE) && defined(ENCODER_MAP_ENABLE)
#    define MAX_ENCODERMAP_OVERRIDES ((ALIGN_TO_2(NUM_ENCODERS) * ALIGN_TO_2(NUM_DIRECTIONS)) / OVERRIDE_ENTRY_SIZE(encodermap_override_entry_t)))
#endif

// Scratch area to hold the to-be-saved keycode data, either as a full keymap or as a list of keycode overrides, whichever is smaller
static struct __attribute__((packed)) {
    uint8_t write_mode; // 0 == full layer, 1 == overrides
    union {
        // The complete keymap layer, all keycodes in a single block for all rows/cols
        uint16_t keymap_layer[MATRIX_ROWS][MATRIX_COLS];

        // The keymap layer overrides, a list of row/col/keycode triplets
        // This is a more efficient way to save the keymap if only a few keys have been altered
        keymap_override_entry_t keymap_layer_overrides[MAX_KEYMAP_OVERRIDES];

#if defined(ENCODER_ENABLE) && defined(ENCODER_MAP_ENABLE)
        // The complete encodermap layer, all keycodes in a single block for all encoders
        uint16_t encodermap_layer[NUM_ENCODERS][NUM_DIRECTIONS];

        // The encodermap layer overrides, a list of encoder/keycode pairs
        // This is a more efficient way to save the encodermap if only a few encoders mappings have been altered
        encodermap_override_entry_t encodermap_layer_overrides[MAX_ENCODERMAP_OVERRIDES];
#endif // defined(ENCODER_ENABLE) && defined(ENCODER_MAP_ENABLE)
    };
} dynamic_keymap_scratch;

static bool is_key_altered(uint8_t layer, uint8_t row, uint8_t col) {
    // Assume layer/encoder_idx already bounds-checked by caller
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
#if defined(ENCODER_ENABLE) && defined(ENCODER_MAP_ENABLE)
static void nvm_dynamic_encodermap_reset_cache_layer_to_raw(uint8_t layer);
static void nvm_dynamic_encodermap_reset_cache_to_raw(void);
#endif // defined(ENCODER_ENABLE) && defined(ENCODER_MAP_ENABLE)

void nvm_dynamic_keymap_erase(void) {
    fs_rmdir("layers", true);
    fs_mkdir("layers");
    nvm_dynamic_keymap_reset_cache_to_raw();
#if defined(ENCODER_ENABLE) && defined(ENCODER_MAP_ENABLE)
    nvm_dynamic_encodermap_reset_cache_to_raw();
#endif // defined(ENCODER_ENABLE) && defined(ENCODER_MAP_ENABLE)
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
    set_key_altered(layer, row, column, keycode != keycode_at_keymap_location_raw(layer, row, column));
    dynamic_keymap_layer_dirty |= (1 << layer);
}

void nvm_dynamic_keymap_save(void) {
    // Skip saving if nothing has changed
    if (!dynamic_keymap_layer_dirty) {
        return;
    }

    // TODO: Implement a more efficient way to save only the altered keycodes, as littlefs prefers appending data instead of writing the whole thing
    for (int layer = 0; layer < (sizeof(layer_state_t) * 8); ++layer) {
        // Skip layers that haven't been modified
        if (!(dynamic_keymap_layer_dirty & (1 << layer))) {
            continue;
        }
        char filename[18] = {0};
        snprintf(filename, sizeof(filename), "layers/key%02d", layer);
        if (dynamic_keymap_altered_count[layer] == 0) {
            // If nothing has been altered, delete any existing file as we'll just use the raw keymap for this layer
            fs_delete(filename);
        } else {
            if (sizeof(dynamic_keymap_scratch.keymap_layer) <= (sizeof(dynamic_keymap_scratch.keymap_layer_overrides[0]) * dynamic_keymap_altered_count[layer])) {
                // write the entire layer to filesystem
                dynamic_keymap_scratch.write_mode = 0;
                memcpy(dynamic_keymap_scratch.keymap_layer, dynamic_keymap_layer_cache[layer], sizeof(dynamic_keymap_scratch.keymap_layer));
                fs_update_block(filename, &dynamic_keymap_scratch, sizeof(uint8_t) + sizeof(dynamic_keymap_scratch.keymap_layer));
            } else {
                // write the overrides to filesystem
                dynamic_keymap_scratch.write_mode = 1;
                size_t idx                        = 0;
                for (int row = 0; row < MATRIX_ROWS; ++row) {
                    for (int col = 0; col < MATRIX_COLS; ++col) {
                        if (is_key_altered(layer, row, col)) {
                            dynamic_keymap_scratch.keymap_layer_overrides[idx].row     = row;
                            dynamic_keymap_scratch.keymap_layer_overrides[idx].col     = col;
                            dynamic_keymap_scratch.keymap_layer_overrides[idx].keycode = dynamic_keymap_layer_cache[layer][row][col];
                            ++idx;
                        }
                    }
                }
                fs_update_block(filename, &dynamic_keymap_scratch, sizeof(uint8_t) + (sizeof(dynamic_keymap_scratch.keymap_layer_overrides[0]) * dynamic_keymap_altered_count[layer]));
            }
        }
    }

    dynamic_keymap_layer_dirty = 0;
}

void nvm_dynamic_keymap_load(void) {
    for (int layer = 0; layer < (sizeof(layer_state_t) * 8); ++layer) {
        char filename[18] = {0};
        snprintf(filename, sizeof(filename), "layers/key%02d", layer);
        nvm_dynamic_keymap_reset_cache_layer_to_raw(layer);
        fs_fd_t fd = fs_open(filename, FS_READ);
        if (fd == INVALID_FILESYSTEM_FD) {
            fs_dprintf("could not open file\n");
            continue;
        }
        size_t bytes_read = fs_read(fd, &dynamic_keymap_scratch, sizeof(dynamic_keymap_scratch));
        fs_close(fd);
        fs_hexdump("read", filename, &dynamic_keymap_scratch, bytes_read);
        if (dynamic_keymap_scratch.write_mode == 0) {
            // full keymap
            for (int row = 0; row < MATRIX_ROWS; ++row) {
                for (int col = 0; col < MATRIX_COLS; ++col) {
                    nvm_dynamic_keymap_update_keycode(layer, row, col, dynamic_keymap_scratch.keymap_layer[row][col]);
                }
            }
        } else {
            // overrides
            int count = (bytes_read - sizeof(uint8_t)) / sizeof(dynamic_keymap_scratch.keymap_layer_overrides[0]);
            fs_dprintf("keymap layer %d override count: %d\n", layer, count);
            for (int j = 0; j < count; ++j) {
                uint8_t  row     = dynamic_keymap_scratch.keymap_layer_overrides[j].row;
                uint8_t  column  = dynamic_keymap_scratch.keymap_layer_overrides[j].col;
                uint16_t keycode = dynamic_keymap_scratch.keymap_layer_overrides[j].keycode;
                nvm_dynamic_keymap_update_keycode(layer, row, column, keycode);
            }
        }
    }
}

#if defined(ENCODER_ENABLE) && defined(ENCODER_MAP_ENABLE)
// Keep track of if anything is actually dirty
static layer_state_t dynamic_encodermap_layer_dirty = 0;
// Keep track of how many keys have been altered per layer
static uint16_t dynamic_encodermap_altered_count[DYNAMIC_KEYMAP_LAYER_COUNT] = {0};
// Keep track of the altered keys by bitmask
static uint32_t dynamic_encodermap_altered_keys[DYNAMIC_KEYMAP_LAYER_COUNT][(((NUM_ENCODERS) * (NUM_DIRECTIONS)) + 31) / 32];
// The "live" copy of the keymap, cached in RAM
static uint16_t dynamic_encodermap_layer_cache[DYNAMIC_KEYMAP_LAYER_COUNT][NUM_ENCODERS][NUM_DIRECTIONS];

static bool is_encodermap_altered(uint8_t layer, uint8_t encoder_idx, bool clockwise) {
    // Assume layer/encoder_idx already bounds-checked by caller
    size_t index = (encoder_idx * (NUM_DIRECTIONS)) + (clockwise ? ENCODER_ARRAYINDEX_CW : ENCODER_ARRAYINDEX_CCW);
    return (dynamic_encodermap_altered_keys[layer][index / 32] & (1 << (index % 32))) != 0;
}

static void set_encodermap_altered(uint8_t layer, uint8_t encoder_idx, bool clockwise, bool val) {
    // Assume layer/encoder_idx already bounds-checked by caller
    size_t index = (encoder_idx * (NUM_DIRECTIONS)) + (clockwise ? 0 : 1);

    // Update the altered key count for the layer if we've had a change
    bool orig_val = dynamic_encodermap_altered_keys[layer][index / 32] & (1 << (index % 32)) ? true : false;
    if (val != orig_val) {
        dynamic_encodermap_altered_count[layer] += val ? 1 : -1;
    }

    if (val) {
        // Mark the key index as altered
        dynamic_encodermap_altered_keys[layer][index / 32] |= (1 << (index % 32));
    } else {
        // Unmark the key index from being altered
        dynamic_encodermap_altered_keys[layer][index / 32] &= ~(1 << (index % 32));
    }
}

uint16_t nvm_dynamic_keymap_read_encoder(uint8_t layer, uint8_t encoder_id, bool clockwise) {
    if (layer >= encodermap_layer_count() || encoder_id >= NUM_ENCODERS) return KC_NO;
    return dynamic_encodermap_layer_cache[layer][encoder_id][clockwise ? ENCODER_ARRAYINDEX_CW : ENCODER_ARRAYINDEX_CCW];
}

void nvm_dynamic_keymap_update_encoder(uint8_t layer, uint8_t encoder_id, bool clockwise, uint16_t keycode) {
    if (layer >= encodermap_layer_count() || encoder_id >= NUM_ENCODERS) return;
    dynamic_encodermap_layer_cache[layer][encoder_id][clockwise ? ENCODER_ARRAYINDEX_CW : ENCODER_ARRAYINDEX_CCW] = keycode;
    set_encodermap_altered(layer, encoder_id, clockwise, keycode != keycode_at_encodermap_location_raw(layer, encoder_id, clockwise));
    dynamic_encodermap_layer_dirty |= (1 << layer);
}

void nvm_dynamic_encodermap_save(void) {
    // Skip saving if nothing has changed
    if (!dynamic_encodermap_layer_dirty) {
        return;
    }

    // TODO: Implement a more efficient way to save only the altered keycodes, as littlefs prefers appending data instead of writing the whole thing
    for (int layer = 0; layer < (sizeof(layer_state_t) * 8); ++layer) {
        // Skip layers that haven't been modified
        if (!(dynamic_encodermap_layer_dirty & (1 << layer))) {
            continue;
        }
        char filename[18] = {0};
        snprintf(filename, sizeof(filename), "layers/enc%02d", layer);
        if (dynamic_encodermap_altered_count[layer] == 0) {
            // If nothing has been altered, delete any existing file as we'll just use the raw keymap for this layer
            fs_delete(filename);
        } else {
            if (sizeof(dynamic_keymap_scratch.encodermap_layer) <= sizeof(dynamic_keymap_scratch.encodermap_layer_overrides[0]) * dynamic_encodermap_altered_count[layer]) {
                // write the entire layer to filesystem
                dynamic_keymap_scratch.write_mode = 0;
                memcpy(dynamic_keymap_scratch.encodermap_layer, dynamic_encodermap_layer_cache[layer], sizeof(dynamic_keymap_scratch.encodermap_layer));
                fs_update_block(filename, &dynamic_keymap_scratch, sizeof(uint8_t) + sizeof(dynamic_keymap_scratch.encodermap_layer));
            } else {
                // write the overrides to filesystem
                dynamic_keymap_scratch.write_mode = 1;
                size_t idx                        = 0;
                for (int enc_id = 0; enc_id < NUM_ENCODERS; ++enc_id) {
                    for (int enc_dir = 0; enc_dir < NUM_DIRECTIONS; ++enc_dir) {
                        if (is_encodermap_altered(layer, enc_id, (enc_dir == ENCODER_ARRAYINDEX_CW))) {
                            dynamic_keymap_scratch.encodermap_layer_overrides[idx].encoder_id = enc_id;
                            dynamic_keymap_scratch.encodermap_layer_overrides[idx].enc_dir    = enc_dir;
                            dynamic_keymap_scratch.encodermap_layer_overrides[idx].keycode    = dynamic_encodermap_layer_cache[layer][enc_id][enc_dir];
                            ++idx;
                        }
                    }
                }
                fs_update_block(filename, &dynamic_keymap_scratch, sizeof(uint8_t) + (sizeof(dynamic_keymap_scratch.encodermap_layer_overrides[0]) * dynamic_encodermap_altered_count[layer]));
            }
        }
    }
    dynamic_encodermap_layer_dirty = 0;
}

void nvm_dynamic_encodermap_load(void) {
    for (int layer = 0; layer < (sizeof(layer_state_t) * 8); ++layer) {
        char filename[18] = {0};
        snprintf(filename, sizeof(filename), "layers/enc%02d", layer);
        nvm_dynamic_encodermap_reset_cache_layer_to_raw(layer);
        fs_fd_t fd = fs_open(filename, FS_READ);
        if (fd == INVALID_FILESYSTEM_FD) {
            fs_dprintf("could not open file\n");
            continue;
        }
        size_t bytes_read = fs_read(fd, &dynamic_keymap_scratch, sizeof(dynamic_keymap_scratch));
        fs_close(fd);
        fs_hexdump("read", filename, &dynamic_keymap_scratch, bytes_read);
        if (dynamic_keymap_scratch.write_mode == 0) {
            // full keymap
            for (int enc_id = 0; enc_id < NUM_ENCODERS; ++enc_id) {
                for (int enc_dir = 0; enc_dir < NUM_DIRECTIONS; ++enc_dir) {
                    nvm_dynamic_keymap_update_encoder(layer, enc_id, (enc_dir == ENCODER_ARRAYINDEX_CW), dynamic_keymap_scratch.encodermap_layer[enc_id][enc_dir]);
                }
            }
        } else {
            // overrides
            int count = (bytes_read - sizeof(uint8_t)) / sizeof(dynamic_keymap_scratch.encodermap_layer_overrides[0]);
            fs_dprintf("keymap layer %d override count: %d\n", layer, count);
            for (int j = 0; j < count; ++j) {
                uint8_t  encoder_id = dynamic_keymap_scratch.encodermap_layer_overrides[j].encoder_id;
                uint8_t  enc_dir    = dynamic_keymap_scratch.encodermap_layer_overrides[j].enc_dir;
                uint16_t keycode    = dynamic_keymap_scratch.encodermap_layer_overrides[j].keycode;
                nvm_dynamic_keymap_update_encoder(layer, encoder_id, (enc_dir == ENCODER_ARRAYINDEX_CW), keycode);
            }
        }
    }
}

#endif // defined(ENCODER_ENABLE) && defined(ENCODER_MAP_ENABLE)

void nvm_dynamic_keymap_read_buffer(uint32_t offset, uint32_t size, uint8_t *data) {
    memset(data, 0, size);
    if (offset + size >= sizeof(dynamic_keymap_layer_cache)) {
        size = sizeof(dynamic_keymap_layer_cache) - offset;
    }
    memcpy(data, (uint8_t *)dynamic_keymap_layer_cache + offset, size);
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
}

static void nvm_dynamic_keymap_reset_cache_to_raw(void) {
    for (int i = 0; i < keymap_layer_count(); ++i) {
        nvm_dynamic_keymap_reset_cache_layer_to_raw(i);
    }
}

#if defined(ENCODER_ENABLE) && defined(ENCODER_MAP_ENABLE)
static void nvm_dynamic_encodermap_reset_cache_layer_to_raw(uint8_t layer) {
    for (int j = 0; j < NUM_ENCODERS; ++j) {
        for (int k = 0; k < NUM_DIRECTIONS; ++k) {
            dynamic_encodermap_layer_cache[layer][j][k] = keycode_at_encodermap_location_raw(layer, j, k);
        }
    }
    dynamic_encodermap_altered_count[layer] = 0;
    memset(dynamic_encodermap_altered_keys[layer], 0, sizeof(dynamic_encodermap_altered_keys[layer]));
}

static void nvm_dynamic_encodermap_reset_cache_to_raw(void) {
    for (int i = 0; i < keymap_layer_count(); ++i) {
        nvm_dynamic_encodermap_reset_cache_layer_to_raw(i);
    }
}
#endif // defined(ENCODER_ENABLE) && defined(ENCODER_MAP_ENABLE)

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
