// Copyright 2026 Nick Brassel (@tzarc)
// SPDX-License-Identifier: GPL-2.0-or-later
#include QMK_KEYBOARD_H
#include "mousegrid.h"

ASSERT_COMMUNITY_MODULES_MIN_API_VERSION(1, 0, 0);

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

// All positions and scales are stored as integers in the range [0, MOUSEGRID_RANGE].
// The float conversion only occurs at the point of digitizer output.
#define MOUSEGRID_RANGE 16384

// Grid dimensions for the directional mode (default 3x3).
#ifndef MOUSEGRID_HORIZONTAL_GRID
#    define MOUSEGRID_HORIZONTAL_GRID 3
#endif
#ifndef MOUSEGRID_VERTICAL_GRID
#    define MOUSEGRID_VERTICAL_GRID 3
#endif

// Minimum active region scale before clamping. 164 ≈ 1% of MOUSEGRID_RANGE.
#ifndef MOUSEGRID_MIN_SCALE
#    define MOUSEGRID_MIN_SCALE 164
#endif

// Active region scale applied by MG_NEAR. 3277 ≈ 20% of MOUSEGRID_RANGE.
#ifndef MOUSEGRID_LOCAL_SCALE
#    define MOUSEGRID_LOCAL_SCALE 3277
#endif

// Each directional pick zooms in by RESCALE_NUM / (RESCALE_DEN * grid_size).
// The default of 10/9 leaves a small overlap between adjacent segments so that
// ambiguous picks can still reach the target at the cost of one extra step.
#ifndef MOUSEGRID_RESCALE_NUM
#    define MOUSEGRID_RESCALE_NUM 10
#endif
#ifndef MOUSEGRID_RESCALE_DEN
#    define MOUSEGRID_RESCALE_DEN 9
#endif

// Maximum number of moves that can be undone with MG_UNDO.
#ifndef MOUSEGRID_UNDO_DEPTH
#    define MOUSEGRID_UNDO_DEPTH 6
#endif

// Time in ms between each animation step shown by MG_ANIM.
#ifndef MOUSEGRID_ANIMATION_STEP
#    define MOUSEGRID_ANIMATION_STEP 75
#endif

// Animation style. Set to MOUSEGRID_ANIM_CORNERS to visit only the four
// corner segments instead of all eight surrounding segments.
#define MOUSEGRID_ANIM_FULL    0
#define MOUSEGRID_ANIM_CORNERS 1
#ifndef MOUSEGRID_ANIMATION
#    define MOUSEGRID_ANIMATION MOUSEGRID_ANIM_FULL
#endif

// ---------------------------------------------------------------------------
// Internal types
// ---------------------------------------------------------------------------

typedef struct {
    uint16_t x, y;         // 0..MOUSEGRID_RANGE
    uint16_t scale_x, scale_y; // 1..MOUSEGRID_RANGE
} mousegrid_cursor_t;

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

static mousegrid_cursor_t mg_cursor;
static uint8_t            mg_undo_count;
static mousegrid_cursor_t mg_undo_stack[MOUSEGRID_UNDO_DEPTH];

static bool     mg_anim_active    = false;
static bool     mg_anim_loop      = false;
static uint16_t mg_anim_since     = 0;
static uint8_t  mg_anim_last_step = 0;

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static mousegrid_cursor_t mg_cursor_init(void) {
    return (mousegrid_cursor_t){
        .x       = MOUSEGRID_RANGE / 2,
        .y       = MOUSEGRID_RANGE / 2,
        .scale_x = MOUSEGRID_RANGE,
        .scale_y = MOUSEGRID_RANGE,
    };
}

// Returns a new cursor moved by (dx, dy) grid steps and then zoomed in.
// Does not modify mg_cursor directly so it can be used for animation preview.
static mousegrid_cursor_t mg_cursor_step(mousegrid_cursor_t c, int8_t dx, int8_t dy) {
    int32_t new_x = (int32_t)c.x + (int32_t)(c.scale_x / MOUSEGRID_HORIZONTAL_GRID) * dx;
    int32_t new_y = (int32_t)c.y + (int32_t)(c.scale_y / MOUSEGRID_VERTICAL_GRID) * dy;
    c.x = (uint16_t)(new_x < 0 ? 0 : (new_x > MOUSEGRID_RANGE ? MOUSEGRID_RANGE : new_x));
    c.y = (uint16_t)(new_y < 0 ? 0 : (new_y > MOUSEGRID_RANGE ? MOUSEGRID_RANGE : new_y));

    uint32_t nsx = (uint32_t)c.scale_x * MOUSEGRID_RESCALE_NUM / MOUSEGRID_RESCALE_DEN / MOUSEGRID_HORIZONTAL_GRID;
    uint32_t nsy = (uint32_t)c.scale_y * MOUSEGRID_RESCALE_NUM / MOUSEGRID_RESCALE_DEN / MOUSEGRID_VERTICAL_GRID;
    c.scale_x    = (uint16_t)(nsx < MOUSEGRID_MIN_SCALE ? MOUSEGRID_MIN_SCALE : nsx);
    c.scale_y    = (uint16_t)(nsy < MOUSEGRID_MIN_SCALE ? MOUSEGRID_MIN_SCALE : nsy);
    return c;
}

// Float conversion only happens here, at the point of digitizer output.
static void mg_send(void) {
    digitizer_in_range_on();
    digitizer_set_position((float)mg_cursor.x / MOUSEGRID_RANGE, (float)mg_cursor.y / MOUSEGRID_RANGE);
}

static void mg_push_undo(void) {
    if (mg_undo_count < MOUSEGRID_UNDO_DEPTH) {
        mg_undo_stack[mg_undo_count++] = mg_cursor;
    }
}

static void mg_pop_undo(void) {
    mg_cursor = mg_undo_count > 0 ? mg_undo_stack[--mg_undo_count] : mg_cursor_init();
}

static void mg_anim_restart(void) {
    mg_anim_active    = true;
    mg_anim_since     = timer_read();
    mg_anim_last_step = UINT8_MAX; // Force step 0 to be shown on first task tick.
}

static void mg_reset(void) {
    mg_cursor      = mg_cursor_init();
    mg_undo_count  = 0;
    mg_anim_active = false;
    mg_anim_loop   = false;
}

void mousegrid_reset(void) {
    mg_reset();
    mg_send();
}

static void mg_direction(int8_t dx, int8_t dy) {
    mg_push_undo();
    mg_cursor = mg_cursor_step(mg_cursor, dx, dy);
    mg_send();
    if (mg_anim_active) {
        mg_anim_restart();
    }
}

void mousegrid_anim_start(void) {
    mg_anim_loop = true;
    mg_anim_restart();
}

void mousegrid_anim_stop(void) {
    bool was_active = mg_anim_active;
    mg_anim_active  = false;
    mg_anim_loop    = false;
    if (was_active) {
        mg_send();
    }
}

// ---------------------------------------------------------------------------
// Module hooks
// ---------------------------------------------------------------------------

void keyboard_post_init_mousegrid(void) {
    keyboard_post_init_mousegrid_kb();
    mg_reset();
}

void housekeeping_task_mousegrid(void) {
    housekeeping_task_mousegrid_kb();

    if (!mg_anim_active) return;

    uint8_t step = (uint8_t)(timer_elapsed(mg_anim_since) / MOUSEGRID_ANIMATION_STEP);
    if (step == mg_anim_last_step) return;
    mg_anim_last_step = step;

    int8_t dx, dy;

#if MOUSEGRID_ANIMATION == MOUSEGRID_ANIM_FULL
    // Visits all 8 surrounding segments, starting and ending at top-center.
    // clang-format off
    static const int8_t dx_table[] = { 0,  1,  1,  1,  0, -1, -1, -1,  0 };
    static const int8_t dy_table[] = {-1, -1,  0,  1,  1,  1,  0, -1, -1 };
    // clang-format on
    if (step >= 9) {
        if (mg_anim_loop) { mg_anim_restart(); } else { mg_anim_active = false; mg_send(); }
        return;
    }
    dx = dx_table[step];
    dy = dy_table[step];
#elif MOUSEGRID_ANIMATION == MOUSEGRID_ANIM_CORNERS
    // Visits the four corner segments only.
    // clang-format off
    static const int8_t dx_table[] = { 1,  1, -1, -1 };
    static const int8_t dy_table[] = {-1,  1,  1, -1 };
    // clang-format on
    if (step >= 4) {
        if (mg_anim_loop) { mg_anim_restart(); } else { mg_anim_active = false; mg_send(); }
        return;
    }
    dx = dx_table[step];
    dy = dy_table[step];
#else
#    error "MOUSEGRID_ANIMATION must be MOUSEGRID_ANIM_FULL or MOUSEGRID_ANIM_CORNERS"
#endif

    mousegrid_cursor_t preview = mg_cursor_step(mg_cursor, dx, dy);
    digitizer_in_range_on();
    digitizer_set_position((float)preview.x / MOUSEGRID_RANGE, (float)preview.y / MOUSEGRID_RANGE);
}

bool process_record_mousegrid(uint16_t keycode, keyrecord_t *record) {
    if (!process_record_mousegrid_kb(keycode, record)) {
        return false;
    }

    // LOOP starts on press and stops on release, so it is handled before the
    // press-only gate below.
    if (keycode == MOUSEGRID_LOOP) {
        if (record->event.pressed) {
            mousegrid_anim_start();
        } else {
            mousegrid_anim_stop();
        }
        return false;
    }

    if (!record->event.pressed) {
        return true;
    }

    switch (keycode) {
        // clang-format off
        case MOUSEGRID_TL: mg_direction(-1, -1); return false;
        case MOUSEGRID_T:  mg_direction( 0, -1); return false;
        case MOUSEGRID_TR: mg_direction( 1, -1); return false;
        case MOUSEGRID_L:  mg_direction(-1,  0); return false;
        case MOUSEGRID_C:  mg_direction( 0,  0); return false;
        case MOUSEGRID_R:  mg_direction( 1,  0); return false;
        case MOUSEGRID_BL: mg_direction(-1,  1); return false;
        case MOUSEGRID_B:  mg_direction( 0,  1); return false;
        case MOUSEGRID_BR: mg_direction( 1,  1); return false;
        // clang-format on

        case MOUSEGRID_RST:
            mousegrid_reset();
            return false;

        case MOUSEGRID_ANIM:
            mg_anim_loop = false;
            mg_anim_restart();
            return false;

        case MOUSEGRID_UNDO:
            mg_anim_active = false;
            mg_anim_loop   = false;
            mg_pop_undo();
            mg_send();
            return false;

        case MOUSEGRID_NEAR:
            mg_anim_active    = false;
            mg_anim_loop      = false;
            mg_cursor.scale_x = MOUSEGRID_LOCAL_SCALE;
            mg_cursor.scale_y = MOUSEGRID_LOCAL_SCALE;
            return false;

        case MOUSEGRID_STOP:
            mousegrid_anim_stop();
            return false;
    }

    return true;
}
