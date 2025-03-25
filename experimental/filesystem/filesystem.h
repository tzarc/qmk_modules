// Copyright 2022-2025 Nick Brassel (@tzarc)
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef uint16_t fs_fd_t;
typedef int32_t  fs_offset_t;
typedef int32_t  fs_size_t;
#define INVALID_FILESYSTEM_FD ((fs_fd_t)0)

#ifndef MAX_NUM_OPEN_FDS
#    define MAX_NUM_OPEN_FDS 6
#endif

typedef enum fs_whence_t {
    FS_SEEK_SET = 0, // Seek relative to start position
    FS_SEEK_CUR = 1, // Seek relative to the current file position
    FS_SEEK_END = 2  // Seek relative to the end of the file
} fs_whence_t;

typedef enum fs_mode_t {
    FS_READ     = 1 << 0, // Read an existing file
    FS_WRITE    = 1 << 1, // Write to a file, creating it if necessary
    FS_TRUNCATE = 1 << 2, // Truncate the file to zero length
} fs_mode_t;

bool fs_format(void);
bool fs_init(void);
bool fs_mount(void);
void fs_unmount(void);
bool fs_is_mounted(void);

typedef struct fs_dirent_t {
    const char *name;
    fs_size_t   size; // only relevant for files
    bool        is_dir;
} fs_dirent_t;

bool         fs_mkdir(const char *path);
bool         fs_rmdir(const char *path, bool recursive);
fs_fd_t      fs_opendir(const char *path);
fs_dirent_t *fs_readdir(fs_fd_t fd);
void         fs_closedir(fs_fd_t fd);

bool fs_exists(const char *path);
bool fs_delete(const char *path);

fs_fd_t     fs_open(const char *filename, fs_mode_t mode);
fs_offset_t fs_seek(fs_fd_t fd, fs_offset_t offset, fs_whence_t whence);
fs_offset_t fs_tell(fs_fd_t fd);
fs_size_t   fs_read(fs_fd_t fd, void *buffer, fs_size_t length);
fs_size_t   fs_write(fs_fd_t fd, const void *buffer, fs_size_t length);
bool        fs_is_eof(fs_fd_t fd);
void        fs_close(fs_fd_t fd);

#if defined(FILESYSTEM_DEBUG) && defined(CONSOLE_ENABLE)
#    include <debug.h>
#    include <print.h>
#    define fs_dprintf(...)            \
        do {                           \
            dprintf("%s: ", __func__); \
            dprintf(__VA_ARGS__);      \
        } while (0)
#else
#    define fs_dprintf(...) \
        do {                \
        } while (0)
#endif
