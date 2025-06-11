// Copyright 2025 Nick Brassel (@tzarc)
// SPDX-License-Identifier: GPL-2.0-or-later
#include <stdbool.h>
#include <string.h>
#include QMK_KEYBOARD_H
#include "filesystem.h"

bool fs_is_path_safe(const char *path) {
    int pos = 0;

    // Skip leading slash
    if (path[0] == '/') pos++;

    while (path[pos]) {
        int segment_start = pos;

        // Find end of current segment
        while (path[pos] && path[pos] != '/') {
            pos++;
        }

        int segment_len = pos - segment_start;

        // Disallow if segment is "." or ".."
        if ((segment_len == 1 && path[segment_start] == '.')                                      // "."
            || (segment_len == 2 && path[segment_start] == '.' && path[segment_start + 1] == '.') // ".."
        ) {
            return false;
        }

        // Disallow if consecutive slashes are found
        if (path[pos] == '/') {
            if (path[pos + 1] == '/') {
                return false;
            }
            pos++; // Skip the slash
        }
    }

    return true;
}

bool fs_is_path_depth_valid(const char *path, int max_depth) {
    if (!path || *path == '\0') {
        return false;
    }

    int depth = 0;

    // Skip leading slash
    if (*path == '/') {
        path++;
    }

    // Count path segments
    while (*path) {
        if (*path != '/') {
            // Found start of a segment
            depth++;
            if (depth > max_depth) {
                return false;
            }
            // Skip to end of this segment
            while (*path && *path != '/') {
                path++;
            }
        } else {
            // Skip consecutive slashes
            path++;
        }
    }

    return true;
}

static void fs_dump(const char *path) {
    fs_fd_t fd = fs_opendir(path);
    if (fd == INVALID_FILESYSTEM_FD) {
        dprintf("could not open %s\n", path);
        return;
    }

    fs_dirent_t *dirent = fs_readdir(fd);
    while (dirent != NULL) {
        if (dirent->is_dir) {
            if (strcmp(dirent->name, ".") == 0 || strcmp(dirent->name, "..") == 0) {
                dirent = fs_readdir(fd);
                continue;
            }
            char new_path[256] = {0};
            snprintf(new_path, sizeof(new_path), "%s/%s", (path[0] == '/' && path[1] == '\0') ? "" : path, dirent->name);
            dprintf("D: %s\n", new_path);
            fs_dump(new_path);
        } else {
            dprintf("F: %s/%s\n", path, dirent->name);
        }
        dirent = fs_readdir(fd);
    }
    fs_closedir(fd);
}

bool process_record_filesystem(uint16_t keycode, keyrecord_t *record) {
    if (!process_record_filesystem_kb(keycode, record)) {
        return false;
    }

    switch (keycode) {
        case FS_DUMP: {
            if (record->event.pressed) {
                fs_dump("/");
            }
            return true;
        }
        default:
            break;
    }

    return true;
}
