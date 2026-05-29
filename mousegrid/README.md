# Mouse Grid

Adds grid-based absolute mouse cursor positioning using the HID Digitizer interface. The cursor is moved to an exact screen position rather than nudged relatively, making it possible to reach any point on screen with a small number of keystrokes.

Add the following to the list of modules in your `keymap.json` to enable this module:

```json
{
    "modules": ["tzarc/mousegrid"]
}
```

`DIGITIZER_ENABLE` is set automatically.

## How it works

The screen is treated as a coordinate space from 0 to 16384 in each axis. `MG_RST` resets the cursor to the center of the screen and sets the active region to the full screen. Each directional key (`MG_TL` through `MG_BR`) picks one of the nine segments of the active region, moves the cursor to the center of that segment, and shrinks the active region down to cover that segment. Repeating this zooms the cursor in on the target.

By default, each step zooms in by slightly less than exactly 1/3 in each dimension (the default rescale ratio is 10/9 rather than exactly 1). This leaves a small overlap between adjacent segments so that an ambiguous pick can still reach the target, at the cost of one extra step.

On a 1920×1080 screen, four iterations of a 3×3 grid reach a region of approximately 35×20 pixels, which is sufficient for most UI elements including menu items.

All arithmetic is done in fixed-point integers. The float conversion happens only when sending the position to the digitizer, which matters on MCUs without a hardware FPU.

## Keycodes

| Key | Alias | Description |
|---|---|---|
| `MOUSEGRID_RST` | `MG_RST` | Reset cursor to center; set active region to full screen |
| `MOUSEGRID_TL` | `MG_TL` | Pick top-left segment |
| `MOUSEGRID_T` | `MG_T` | Pick top segment |
| `MOUSEGRID_TR` | `MG_TR` | Pick top-right segment |
| `MOUSEGRID_L` | `MG_L` | Pick left segment |
| `MOUSEGRID_C` | `MG_C` | Pick center segment |
| `MOUSEGRID_R` | `MG_R` | Pick right segment |
| `MOUSEGRID_BL` | `MG_BL` | Pick bottom-left segment |
| `MOUSEGRID_B` | `MG_B` | Pick bottom segment |
| `MOUSEGRID_BR` | `MG_BR` | Pick bottom-right segment |
| `MOUSEGRID_UNDO` | `MG_UNDO` | Undo the last segment pick |
| `MOUSEGRID_ANIM` | `MG_ANIM` | Run the animation once to show where each segment would land |
| `MOUSEGRID_NEAR` | `MG_NEAR` | Shrink the active region to a local area around the current cursor position |
| `MOUSEGRID_LOOP` | `MG_LOOP` | Hold to run the animation continuously; release to stop |
| `MOUSEGRID_STOP` | `MG_STOP` | Stop any running animation and restore the cursor position |

`MG_NEAR` does not move the cursor — it sets the active region scale to `MOUSEGRID_LOCAL_SCALE` centered on the current position. Subsequent directional picks then navigate within that local area rather than the full screen. Use this when the target is near the current cursor position and a full reset would overshoot.

`MG_ANIM` moves the cursor to each of the nine segment centers in sequence, pausing `MOUSEGRID_ANIMATION_STEP` ms at each position, then returns the cursor to its actual position. This is useful when learning the layout or when the active region is small enough that it is hard to judge visually where each pick would land.

`MG_LOOP` runs the same animation continuously for as long as the key is held, stopping and restoring the cursor when released. Directional picks made while `MG_LOOP` is held are applied normally and the animation restarts from the new cursor position, so the hold can be used to navigate while continuously previewing the available segments.

`MG_STOP` stops any running animation immediately and restores the cursor to its actual position. It is most useful when the animation was started programmatically via `mousegrid_anim_start()`.

Directional picks (`MG_TL` through `MG_BR`) do not stop a running animation. If the animation is active when a pick is made, it restarts from the new cursor position.

## Public API

The following functions can be called directly from user code, for example to drive behaviour from a custom key, encoder, or combo:

```c
void mousegrid_reset(void);       // Reset cursor to center and clear undo stack
void mousegrid_anim_start(void);  // Start the looping animation
void mousegrid_anim_stop(void);   // Stop any running animation and restore cursor
```

Include `mousegrid.h` to use them.

A common pattern is a dedicated key that resets the grid and previews the segments for as long as it is held, ready for a directional pick on the target layer:

```c
#include "mousegrid.h"

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    switch (keycode) {
        case MO(_MOUSE):
            if (record->event.pressed) {
                mousegrid_reset();
                mousegrid_anim_start();
            } else {
                mousegrid_anim_stop();
            }
            return true; // Allow normal layer handling to continue.
    }
    return true;
}
```

## Configuration

Set any of the following in your `config.h`:

| Define | Default | Description |
|---|---|---|
| `MOUSEGRID_HORIZONTAL_GRID` | `3` | Number of columns in the segment grid |
| `MOUSEGRID_VERTICAL_GRID` | `3` | Number of rows in the segment grid |
| `MOUSEGRID_RESCALE_NUM` | `10` | Numerator of the per-pick zoom factor |
| `MOUSEGRID_RESCALE_DEN` | `9` | Denominator of the per-pick zoom factor. Effective multiplier is `NUM / (DEN × grid_size)`. The default 10/9 leaves overlap between segments |
| `MOUSEGRID_MIN_SCALE` | `164` | Minimum active region scale in fixed-point units (16384 = full screen). Default ≈ 1% |
| `MOUSEGRID_LOCAL_SCALE` | `3277` | Active region scale applied by `MG_NEAR`. Default ≈ 20% of the screen |
| `MOUSEGRID_UNDO_DEPTH` | `6` | Number of picks that can be undone with `MG_UNDO` |
| `MOUSEGRID_ANIMATION_STEP` | `75` | Time in ms between animation frames for `MG_ANIM` and `MG_LOOP` |
| `MOUSEGRID_ANIMATION` | `MOUSEGRID_ANIM_FULL` | Animation style: `MOUSEGRID_ANIM_FULL` visits all 8 surrounding segments; `MOUSEGRID_ANIM_CORNERS` visits only the 4 corner segments |
