// Copyright 2026 Nick Brassel (@tzarc)
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// Expose the digitizer as a Generic Desktop absolute pointer rather than a
// stylus. Compositors treat touch-like digitizer input as not needing an
// on-screen cursor (KDE hides it entirely); an absolute pointer is handled
// as a real mouse, keeping the cursor visible at the reported position.
#define DIGITIZER_ABSOLUTE_POINTER
