// Copyright 2022-2025 Nick Brassel (@tzarc)
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdlib.h>

void fs_read_block(const char *filename, void *data, size_t size);
void fs_update_block(const char *filename, const void *data, size_t size);
