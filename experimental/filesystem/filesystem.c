// Copyright 2025 Nick Brassel (@tzarc)
// SPDX-License-Identifier: GPL-2.0-or-later
#include QMK_KEYBOARD_H
#include "filesystem.h"

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
            char new_path[LFS_NAME_MAX] = {0};
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
    }

    return true;
}
