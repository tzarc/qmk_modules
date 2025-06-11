// Copyright 2022-2025 Nick Brassel (@tzarc)
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

// Configurable: Maximum directory depth allowed (levels deep)
#ifndef FS_MAX_DIR_DEPTH
#    define FS_MAX_DIR_DEPTH 3 // Directories limited to 3 levels deep (e.g., "/level1/level2/level3")
#endif

// Configurable: Maximum file path depth allowed (levels deep)
#ifndef FS_MAX_FILE_DEPTH
#    define FS_MAX_FILE_DEPTH ((FS_MAX_DIR_DEPTH) + 1) // Files limited to 4 levels deep (e.g., "/level1/level2/level3/file.txt")
#endif

/** @brief File descriptor type */
typedef uint16_t fs_fd_t;
/** @brief File offset type (signed for negative seeks) */
typedef int32_t fs_offset_t;
/** @brief File size type (signed for errors) */
typedef int32_t fs_size_t;

/** @brief Invalid file descriptor constant */
#define INVALID_FILESYSTEM_FD ((fs_fd_t)0)

// Configurable: Maximum number of concurrent open file descriptors
#ifndef FS_MAX_NUM_OPEN_FDS
#    define FS_MAX_NUM_OPEN_FDS 6
#endif

/** @brief File seek origin */
typedef enum fs_whence_t {
    FS_SEEK_SET = 0, /**< Seek relative to start position */
    FS_SEEK_CUR = 1, /**< Seek relative to the current file position */
    FS_SEEK_END = 2  /**< Seek relative to the end of the file */
} fs_whence_t;

/** @brief File access mode flags */
typedef enum fs_mode_t {
    FS_READ     = 1 << 0, /**< Read an existing file */
    FS_WRITE    = 1 << 1, /**< Write to a file, creating it if necessary */
    FS_TRUNCATE = 1 << 2, /**< Truncate the file to zero length */
} fs_mode_t;

/**
 * @brief Format the filesystem
 *
 * Erases all data and creates a new empty filesystem.
 * Thread-safe.
 *
 * @return true on success, false on failure
 */
bool fs_format(void);

/**
 * @brief Initialize the filesystem
 *
 * Initializes the underlying device and mounts the filesystem.
 * Creates a new filesystem if none exists.
 * Thread-safe.
 *
 * @return true on success, false on failure
 */
bool fs_init(void);

/**
 * @brief Mount the filesystem
 *
 * Makes the filesystem available for operations. Uses reference counting
 * to support nested mount calls.
 * Thread-safe.
 *
 * @return true on success, false on failure
 */
bool fs_mount(void);

/**
 * @brief Unmount the filesystem
 *
 * Decrements mount reference count and unmounts when it reaches zero.
 * Thread-safe.
 */
void fs_unmount(void);

/**
 * @brief Check if filesystem is mounted
 *
 * Thread-safe.
 *
 * @return true if mounted, false otherwise
 */
bool fs_is_mounted(void);

/**
 * @brief Directory entry structure
 *
 * Contains information about a file or directory entry returned by fs_readdir().
 */
typedef struct fs_dirent_t {
    const char *name;   /**< Entry name. WARNING: Pointer becomes invalid after next fs_readdir() call */
    fs_size_t   size;   /**< File size in bytes (only relevant for files) */
    bool        is_dir; /**< True if entry is a directory, false if it's a file */
} fs_dirent_t;

/**
 * @brief Create directory
 *
 * Creates a new directory. Succeeds if directory already exists.
 * Thread-safe.
 *
 * @param path Directory path to create
 * @return true on success, false on failure
 */
bool fs_mkdir(const char *path);

/**
 * @brief Remove directory
 *
 * Removes a directory. If recursive is true, removes all contents.
 * Thread-safe.
 *
 * @param path Directory path to remove
 * @param recursive Whether to remove contents recursively
 * @return true on success, false on failure
 */
bool fs_rmdir(const char *path, bool recursive);

/**
 * @brief Open a directory for reading
 *
 * Opens a directory and returns a file descriptor for reading entries.
 * Thread-safe.
 *
 * @param path Directory path to open
 * @return File descriptor on success, INVALID_FILESYSTEM_FD on failure
 */
fs_fd_t fs_opendir(const char *path);

/**
 * @brief Read next directory entry
 *
 * Returns the next entry in an open directory.
 * Thread-safe.
 *
 * @param fd Directory file descriptor from fs_opendir()
 * @return Pointer to directory entry on success, NULL on end or error
 */
fs_dirent_t *fs_readdir(fs_fd_t fd);

/**
 * @brief Close directory
 *
 * Closes an open directory and frees its file descriptor.
 * Thread-safe.
 *
 * @param fd Directory file descriptor to close
 */
void fs_closedir(fs_fd_t fd);

/**
 * @brief Check if file or directory exists
 *
 * Thread-safe.
 *
 * @param path Path to check
 * @return true if exists, false otherwise
 */
bool fs_exists(const char *path);

/**
 * @brief Delete file or directory
 *
 * Removes a file or empty directory.
 * Thread-safe.
 *
 * @param path Path to delete
 * @return true on success, false on failure
 */
bool fs_delete(const char *path);

/**
 * @brief Open file
 *
 * Opens a file with specified access mode.
 * Thread-safe.
 *
 * @param filename File path to open
 * @param mode Access mode (FS_READ, FS_WRITE, FS_TRUNCATE flags)
 * @return File descriptor on success, INVALID_FILESYSTEM_FD on failure
 */
fs_fd_t fs_open(const char *filename, fs_mode_t mode);

/**
 * @brief Seek to position in file
 *
 * Changes the file position.
 * Thread-safe.
 *
 * @param fd File descriptor
 * @param offset Offset in bytes
 * @param whence Seek origin (FS_SEEK_SET/FS_SEEK_CUR/FS_SEEK_END)
 * @return New position in bytes on success, -1 on failure
 */
fs_offset_t fs_seek(fs_fd_t fd, fs_offset_t offset, fs_whence_t whence);

/**
 * @brief Get current file position
 *
 * Returns the current file position.
 * Thread-safe.
 *
 * @param fd File descriptor
 * @return Current position in bytes, or -1 on failure
 */
fs_offset_t fs_tell(fs_fd_t fd);

/**
 * @brief Read data from file
 *
 * Reads up to length bytes from the file.
 * Thread-safe.
 *
 * @param fd File descriptor
 * @param buffer Buffer to store read data
 * @param length Maximum number of bytes to read
 * @return Number of bytes actually read, or -1 on failure
 */
fs_size_t fs_read(fs_fd_t fd, void *buffer, fs_size_t length);

/**
 * @brief Write data to file
 *
 * Writes up to length bytes to the file.
 * Thread-safe.
 *
 * @param fd File descriptor
 * @param buffer Buffer containing data to write
 * @param length Number of bytes to write
 * @return Number of bytes actually written, or -1 on failure
 */
fs_size_t fs_write(fs_fd_t fd, const void *buffer, fs_size_t length);

/**
 * @brief Check if file is at end-of-file
 *
 * Tests whether the file position is at the end of the file.
 * Thread-safe.
 *
 * @param fd File descriptor
 * @return true if at EOF or on error, false otherwise
 */
bool fs_is_eof(fs_fd_t fd);

/**
 * @brief Close file
 *
 * Closes an open file and frees its file descriptor.
 * Thread-safe.
 *
 * @param fd File descriptor to close
 */
void fs_close(fs_fd_t fd);

/**
 * @brief Validate path safety
 *
 * Checks for path components like "." and ".." and disallows them. Mainly
 * used to guard against buffer overflows during directory traversal.
 *
 * @param path Path string to validate
 * @return true if path is safe, false if problematic
 */
bool fs_is_path_safe(const char *path);

/**
 * @brief Validate path depth doesn't exceed configured limits
 *
 * Counts the number of path segments to ensure filesystem depth limits
 * are not exceeded.
 *
 * @param path Path string to validate
 * @param max_depth Maximum allowed depth (number of segments)
 * @return true if path depth is valid, false if too deep or empty
 */
bool fs_is_path_depth_valid(const char *path, int max_depth);

/**
 * @brief Dump filesystem information to console
 *
 * Prints detailed filesystem information. Only available when
 * CONSOLE_ENABLE is defined.
 * Thread-safe.
 */
void fs_dump_info(void);

// Debug macros - only active when FILESYSTEM_DEBUG and CONSOLE_ENABLE are defined
#if defined(FILESYSTEM_DEBUG) && defined(CONSOLE_ENABLE)
#    include <debug.h>
#    include <print.h>
/** @brief Debug printf with function name prefix */
#    define fs_dprintf(...)            \
        do {                           \
            dprintf("%s: ", __func__); \
            dprintf(__VA_ARGS__);      \
        } while (0)
/** @brief Debug hexdump with name and file labels */
#    define fs_hexdump(name, file, buf, length)        \
        do {                                           \
            dprintf("[%s (%s)]: ", name, file);        \
            const uint8_t *p = (const uint8_t *)(buf); \
            for (int i = 0; i < (length); ++i) {       \
                dprintf(" %02X", (int)p[i]);           \
            }                                          \
            dprintf("\n");                             \
        } while (0)
#else
/** @brief Debug printf (disabled) */
#    define fs_dprintf(...) \
        do {                \
        } while (0)
/** @brief Debug hexdump (disabled) */
#    define fs_hexdump(...) \
        do {                \
        } while (0)
#endif
