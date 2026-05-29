// Copyright 2026 Nick Brassel (@tzarc)
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <stdint.h>
#include <stdbool.h>

/**
 * Reset the cursor to the center of the screen with full-screen active region,
 * and clear the undo stack.
 */
void mousegrid_reset(void);

/**
 * Start the animation, repeating continuously until mousegrid_anim_stop() is
 * called. Intended to be called on key-down for a hold-to-animate binding.
 */
void mousegrid_anim_start(void);

/**
 * Stop any running animation and restore the cursor to its current position.
 * Intended to be called on key-up for a hold-to-animate binding.
 */
void mousegrid_anim_stop(void);
