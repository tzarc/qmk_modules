// Copyright 2022-2025 Nick Brassel (@tzarc)
// SPDX-License-Identifier: GPL-2.0-or-later
#include <stdbool.h>
#include <ch.h>
#include <string.h>
#include "chmtx.h"
#include "filesystem.h"
#include "lfs.h"

/*
 * ERROR HANDLING CONVENTIONS:
 * - File descriptor functions: return INVALID_FILESYSTEM_FD (0) on error
 * - Boolean functions: return false on error, true on success
 * - Size/offset functions: return (fs_size_t)-1 or (fs_offset_t)-1 on error
 * - Pointer functions: return NULL on error
 * - All functions log errors using fs_dprintf when CONSOLE_ENABLE is defined
 */

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Internal Implementation Details
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/** @brief Minimum valid file descriptor value (avoiding invalid, stdin/stdout/stderr) */
#define FIRST_VALID_FD 4

/** @brief Maximum path buffer size in bytes for deepest possible paths */
#define MAX_PATH_BUFFER_SIZE ((LFS_NAME_MAX) * (FS_MAX_FILE_DEPTH) + ((FS_MAX_FILE_DEPTH) - 1) + 1)

/** @brief Current file descriptor counter for allocation */
static fs_fd_t current_fd = FIRST_VALID_FD;

/**
 * @brief File descriptor type enumeration
 */
typedef enum fs_lfs_fd_type_t {
    FD_TYPE_EMPTY, /**< Unused handle slot */
    FD_TYPE_DIR,   /**< Directory handle */
    FD_TYPE_FILE   /**< File handle */
} fs_lfs_fd_type_t;

/**
 * @brief File/directory handle structure
 *
 * Union-based structure to handle both file and directory operations
 * while maintaining type safety.
 */
typedef struct fs_lfs_handle_t {
    fs_fd_t          fd;   /**< File descriptor number */
    fs_lfs_fd_type_t type; /**< Type of handle (file/dir/empty) */
    union {
        struct {
            lfs_dir_t       dir_handle; /**< LittleFS directory handle */
            struct lfs_info entry_info; /**< Directory entry information */
            fs_dirent_t     dirent;     /**< Public directory entry (data replicated from entry_info) */
        } dir;
        struct {
            lfs_file_t             file_handle; /**< LittleFS file handle */
            struct lfs_file_config cfg;         /**< File configuration */
        } file;
    };
} fs_lfs_handle_t;

/** @brief LittleFS configuration provided by the underlying driver */
extern const struct lfs_config lfs_cfg;

/** @brief LittleFS filesystem instance */
static lfs_t lfs;

/** @brief Array of open file/directory handles */
static fs_lfs_handle_t fs_handles[FS_MAX_NUM_OPEN_FDS];

/**
 * @brief Macro to find and operate on a file descriptor handle
 *
 * Searches through the handle array for a matching file descriptor and type,
 * then executes the provided code block with access to the handle.
 *
 * @param search_fd File descriptor to search for
 * @param fd_type Expected type of the file descriptor
 * @param block Code block to execute if handle is found
 */
#define FIND_FD_GET_HANDLE(search_fd, fd_type, block)                              \
    do {                                                                           \
        for (int __find_idx = 0; __find_idx < FS_MAX_NUM_OPEN_FDS; ++__find_idx) { \
            fs_lfs_handle_t *handle = &fs_handles[__find_idx];                     \
            if (handle->fd == (search_fd) && handle->type == (fd_type)) {          \
                {                                                                  \
                    block                                                          \
                }                                                                  \
            }                                                                      \
        }                                                                          \
    } while (0)

/**
 * @brief Validate file descriptor range and format
 *
 * @param fd File descriptor to validate
 * @return true if fd is in valid range, false otherwise
 */
static inline bool is_valid_fd(fs_fd_t fd) {
    return fd != INVALID_FILESYSTEM_FD && fd >= FIRST_VALID_FD;
}

/**
 * @brief Check if a file descriptor can be allocated
 *
 * Validates that the file descriptor is in valid range and not currently in use.
 *
 * @param fd File descriptor to check
 * @return true if fd can be used, false if invalid or already in use
 */
static inline bool fd_can_be_used(fs_fd_t fd) {
    if (!is_valid_fd(fd)) {
        return false;
    }
    for (int i = 0; i < FS_MAX_NUM_OPEN_FDS; ++i) {
        if (fs_handles[i].fd == fd) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Allocate a new file descriptor
 *
 * Searches for an available file descriptor using a round-robin approach
 * with intentional uint16_t wraparound.
 *
 * @return New file descriptor, or INVALID_FILESYSTEM_FD if none available
 */
static inline fs_fd_t allocate_fd(void) {
    fs_fd_t first = current_fd;
    do {
        // Prevent overflow by checking before increment
        if (current_fd == UINT16_MAX) {
            current_fd = FIRST_VALID_FD;
        } else {
            ++current_fd;
        }
        if (fd_can_be_used(current_fd)) {
            return current_fd;
        }
        if (current_fd == first) {
            // If we've looped back around to the first, everything is already allocated (yikes!). Need to exit with a failure.
            return INVALID_FILESYSTEM_FD;
        }
    } while (true);
}

/**
 * @brief Macro for LittleFS API calls with error logging
 *
 * Wraps LittleFS API calls to provide consistent error logging when
 * CONSOLE_ENABLE is defined.
 *
 * @param api LittleFS API function to call
 * @param ... Arguments to pass to the API function
 */
#define LFS_API_CALL(api, ...)                      \
    ({                                              \
        int ret = api(__VA_ARGS__);                 \
        if (ret < 0) {                              \
            fs_dprintf(#api " returned %d\n", ret); \
        }                                           \
        ret;                                        \
    })

/** @brief Mutex for filesystem thread safety */
static MUTEX_DECL(fs_mutex);

/**
 * @brief Acquire filesystem lock
 *
 * @return true (lock always succeeds with ChibiOS mutexes)
 */
static bool fs_lock(void) {
    chMtxLock(&fs_mutex);
    return true;
}

/**
 * @brief Release filesystem lock
 *
 * @return true (unlock always succeeds with ChibiOS mutexes)
 */
static bool fs_unlock(void) {
    chMtxUnlock(&fs_mutex);
    return true;
}

/**
 * @brief RAII-style helper for automatic filesystem unlock
 *
 * Used with GCC cleanup attribute to ensure filesystem is unlocked
 * when the corresponding variable goes out of scope.
 *
 * @param locked Pointer to lock status boolean
 */
static void fs_unlock_helper(bool *locked) {
    if (*locked) {
        fs_unlock();
    }
}

/**
 * @brief Macro for automatic filesystem lock/unlock with RAII
 *
 * Acquires filesystem lock and automatically releases it when the
 * variable goes out of scope.
 *
 * @param ret_on_fail Value to return if lock acquisition fails
 */
#define FS_AUTO_LOCK_UNLOCK(ret_on_fail)                                       \
    bool __fs_lock __attribute__((__cleanup__(fs_unlock_helper))) = fs_lock(); \
    do {                                                                       \
        if (!__fs_lock) {                                                      \
            return ret_on_fail;                                                \
        }                                                                      \
    } while (0)

/**
 * @brief Skip automatic unlock (for transfer of ownership)
 *
 * Prevents the RAII cleanup from running, useful when transferring
 * lock ownership to another scope, or deferring to another function.
 */
#define FS_SKIP_AUTO_UNLOCK() \
    do {                      \
        __fs_lock = false;    \
    } while (0)

// Forward declarations for device functions
extern bool  fs_device_init(void);
extern void *fs_device_filebuf(int file_idx);

// Forward declarations for internal functions
static void         fs_unmount_helper(bool *mounted);
static fs_dirent_t *fs_readdir_explicit_nolock(lfs_dir_t *dir_handle, struct lfs_info *entry_info, fs_dirent_t *dirent);
static bool         fs_format_nolock(void);
static bool         fs_init_nolock(void);
static bool         fs_mount_nolock(void);
static void         fs_unmount_nolock(void);
static bool         fs_is_mounted_nolock(void);
static bool         fs_mkdir_nolock(const char *path);
static bool         fs_rmdir_nolock(const char *path, bool recursive, int depth);
static fs_fd_t      fs_opendir_nolock(const char *path);
static fs_dirent_t *fs_readdir_nolock(fs_fd_t fd);
static void         fs_closedir_nolock(fs_fd_t fd);
static bool         fs_exists_nolock(const char *path);
static bool         fs_delete_nolock(const char *path);
static fs_fd_t      fs_open_nolock(const char *filename, fs_mode_t mode);
static fs_offset_t  fs_seek_nolock(fs_fd_t fd, fs_offset_t offset, fs_whence_t whence);
static fs_offset_t  fs_tell_nolock(fs_fd_t fd);
static fs_size_t    fs_read_nolock(fs_fd_t fd, void *buffer, fs_size_t length);
static fs_size_t    fs_write_nolock(fs_fd_t fd, const void *buffer, fs_size_t length);
static bool         fs_is_eof_nolock(fs_fd_t fd);
static void         fs_close_nolock(fs_fd_t fd);

/** @brief Mount reference counter for nested mount/unmount calls */
static int mount_count = 0;

/**
 * @brief RAII-style helper for automatic filesystem unmount
 *
 * @param mounted Pointer to mount status boolean
 */
static void fs_unmount_helper(bool *mounted) {
    if (*mounted) {
        fs_unmount_nolock();
    }
}

/**
 * @brief Macro for automatic filesystem mount/unmount with RAII
 *
 * Mounts filesystem and automatically unmounts it when the
 * corresponding variable goes out of scope.
 *
 * @param ret_on_fail Value to return if mount fails
 */
#define FS_AUTO_MOUNT_UNMOUNT(ret_on_fail)                                               \
    bool __fs_mount __attribute__((__cleanup__(fs_unmount_helper))) = fs_mount_nolock(); \
    do {                                                                                 \
        if (!__fs_mount) {                                                               \
            return ret_on_fail;                                                          \
        }                                                                                \
    } while (0)

/**
 * @brief Skip automatic unmount (for keeping filesystem mounted)
 */
#define FS_SKIP_AUTO_UNMOUNT() \
    do {                       \
        __fs_mount = false;    \
    } while (0)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Internal LittleFS Implementation Functions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Format the filesystem (internal, not thread-safe)
 *
 * Unmounts any existing filesystem, formats the storage device,
 * and reinitializes the filesystem.
 *
 * @return true on success, false on failure
 */
static bool fs_format_nolock(void) {
    while (fs_is_mounted_nolock()) {
        fs_unmount_nolock();
    }
    if (LFS_API_CALL(lfs_format, &lfs, &lfs_cfg) < 0) {
        return false;
    }
    return fs_init_nolock();
}

/**
 * @brief Initialize the filesystem (internal, not thread-safe)
 *
 * Initializes the device layer and mounts the filesystem.
 * Clears all file handles.
 *
 * @return true on success, false on failure
 */
static bool fs_init_nolock(void) {
    while (fs_is_mounted_nolock()) {
        fs_unmount_nolock();
    }
    memset(fs_handles, 0, sizeof(fs_handles));
    return fs_device_init() && fs_mount_nolock();
}

/**
 * @brief Mount the filesystem (internal, not thread-safe)
 *
 * Attempts to mount the filesystem, formatting if necessary.
 * Implements reference counting for nested mount calls.
 *
 * @return true on success, false on failure
 */
static bool fs_mount_nolock(void) {
    if (!fs_is_mounted_nolock()) {
        // reformat if we can't mount the filesystem
        // this should only happen on the first boot
        if (LFS_API_CALL(lfs_mount, &lfs, &lfs_cfg) < 0) {
            if (!fs_format_nolock()) {
                return false;
            }
            if (LFS_API_CALL(lfs_mount, &lfs, &lfs_cfg) < 0) {
                return false;
            }
        }
    }
    ++mount_count;
    return true;
}

/**
 * @brief Unmount the filesystem (internal, not thread-safe)
 *
 * Decrements reference count and unmounts when count reaches zero.
 */
static void fs_unmount_nolock(void) {
    if (fs_is_mounted_nolock()) {
        --mount_count;
        if (mount_count == 0) {
            LFS_API_CALL(lfs_unmount, &lfs);
        }
    }
}

/**
 * @brief Check if filesystem is currently mounted (internal, not thread-safe)
 *
 * @return true if mounted, false otherwise
 */
static bool fs_is_mounted_nolock(void) {
    return mount_count > 0;
}

/**
 * @brief Create directory (internal, not thread-safe)
 *
 * @param path Directory path to create
 * @return true on success or if directory already exists, false on failure
 */
static bool fs_mkdir_nolock(const char *path) {
    FS_AUTO_MOUNT_UNMOUNT(false);

    int err = LFS_API_CALL(lfs_mkdir, &lfs, path);
    return err >= 0 || err == LFS_ERR_EXIST; // Allow for already existing directories to count as success
}

/**
 * @brief Remove directory (internal, not thread-safe)
 *
 * @param path Directory path to remove
 * @param recursive Whether to remove contents recursively
 * @param depth Current recursion depth (for overflow protection)
 * @return true on success, false on failure
 */
static bool fs_rmdir_nolock(const char *path, bool recursive, int depth) {
    if (depth > FS_MAX_FILE_DEPTH) {
        return false; // Prevent stack overflow and enforce depth limits
    }

    FS_AUTO_MOUNT_UNMOUNT(false);

    if (recursive) {
        lfs_dir_t dir;
        if (LFS_API_CALL(lfs_dir_open, &lfs, &dir, path) < 0) {
            return false;
        }

        bool            success = true;
        struct lfs_info info;
        fs_dirent_t     dirent;
        char            child_path[MAX_PATH_BUFFER_SIZE] = {0};
        while (success && fs_readdir_explicit_nolock(&dir, &info, &dirent) != NULL) {
            if (!strcmp(dirent.name, ".") || !strcmp(dirent.name, "..")) {
                continue;
            }
            // Note: Buffer overflow impossible due to filesystem depth limits and buffer sizing
            strlcpy(child_path, path, sizeof(child_path));
            strlcat(child_path, "/", sizeof(child_path));
            strlcat(child_path, dirent.name, sizeof(child_path));
            if (info.type == LFS_TYPE_DIR) {
                if (!fs_rmdir_nolock(child_path, true, depth + 1)) {
                    success = false;
                }
            } else {
                if (!fs_delete_nolock(child_path)) {
                    success = false;
                }
            }
        }

        // Always close directory handle before returning
        if (LFS_API_CALL(lfs_dir_close, &lfs, &dir) < 0) {
            success = false;
        }

        if (!success) {
            return false;
        }
    }

    return fs_delete_nolock(path);
}

/**
 * @brief Open a directory (internal, not thread-safe)
 *
 * @param path Directory path to open
 * @return File descriptor on success, INVALID_FILESYSTEM_FD on failure
 */
static fs_fd_t fs_opendir_nolock(const char *path) {
    FIND_FD_GET_HANDLE(INVALID_FILESYSTEM_FD, FD_TYPE_EMPTY, {
        FS_AUTO_MOUNT_UNMOUNT(INVALID_FILESYSTEM_FD);

        if (LFS_API_CALL(lfs_dir_open, &lfs, &handle->dir.dir_handle, path) < 0) {
            return INVALID_FILESYSTEM_FD;
        }

        fs_fd_t fd   = allocate_fd();
        handle->fd   = fd;
        handle->type = FD_TYPE_DIR;

        // Intentionally don't unmount while we have an open directory
        FS_SKIP_AUTO_UNMOUNT();
        return fd;
    });
    return INVALID_FILESYSTEM_FD;
}

/**
 * @brief Read directory entry (internal helper)
 *
 * @param dir_handle LittleFS directory handle
 * @param entry_info LittleFS entry info structure to fill
 * @param dirent Public directory entry structure to fill
 * @return Pointer to dirent on success, NULL on end-of-directory or error
 */
static fs_dirent_t *fs_readdir_explicit_nolock(lfs_dir_t *dir_handle, struct lfs_info *entry_info, fs_dirent_t *dirent) {
    if (!fs_is_mounted_nolock()) {
        return NULL;
    }

    int err = LFS_API_CALL(lfs_dir_read, &lfs, dir_handle, entry_info);
    if (err <= 0) { // error (<0), or no more entries (==0)
        return NULL;
    }

    dirent->is_dir = entry_info->type == LFS_TYPE_DIR;
    dirent->size   = entry_info->size;
    dirent->name   = entry_info->name;
    return dirent;
}

/**
 * @brief Read directory entry (internal, not thread-safe)
 *
 * @param fd Directory file descriptor
 * @return Pointer to directory entry on success, NULL on end or error
 */
static fs_dirent_t *fs_readdir_nolock(fs_fd_t fd) {
    FIND_FD_GET_HANDLE(fd, FD_TYPE_DIR, {
        // Offload to helper
        return fs_readdir_explicit_nolock(&handle->dir.dir_handle, &handle->dir.entry_info, &handle->dir.dirent);
    });
    return NULL;
}

/**
 * @brief Close directory (internal, not thread-safe)
 *
 * @param fd Directory file descriptor to close
 */
static void fs_closedir_nolock(fs_fd_t fd) {
    FIND_FD_GET_HANDLE(fd, FD_TYPE_DIR, {
        LFS_API_CALL(lfs_dir_close, &lfs, &handle->dir.dir_handle);

        handle->fd   = INVALID_FILESYSTEM_FD;
        handle->type = FD_TYPE_EMPTY;

        fs_unmount_nolock(); // we can unmount here, mirrors the open in fs_opendir()
        return;
    });
}

/**
 * @brief Check if file/directory exists (internal, not thread-safe)
 *
 * @param path Path to check
 * @return true if exists, false otherwise
 */
static bool fs_exists_nolock(const char *path) {
    FS_AUTO_MOUNT_UNMOUNT(false);
    struct lfs_info info;
    int             err = lfs_stat(&lfs, path, &info); // Note: Don't use LFS_API_CALL here - file not existing is expected, not an error
    return err >= 0;
}

/**
 * @brief Delete file or directory (internal, not thread-safe)
 *
 * @param path Path to delete
 * @return true on success or if already deleted, false on failure
 */
static bool fs_delete_nolock(const char *path) {
    FS_AUTO_MOUNT_UNMOUNT(false);
    if (!fs_exists_nolock(path)) {
        return true;
    }
    int err = LFS_API_CALL(lfs_remove, &lfs, path);
    return err >= 0 || err == LFS_ERR_NOENT; // Allow for already deleted files to count as success
}

/**
 * @brief Open file (internal, not thread-safe)
 *
 * @param filename File path to open
 * @param mode File access mode (read/write/truncate flags)
 * @return File descriptor on success, INVALID_FILESYSTEM_FD on failure
 */
static fs_fd_t fs_open_nolock(const char *filename, fs_mode_t mode) {
    FIND_FD_GET_HANDLE(INVALID_FILESYSTEM_FD, FD_TYPE_EMPTY, {
        FS_AUTO_MOUNT_UNMOUNT(INVALID_FILESYSTEM_FD);

        int flags = 0;
        if ((mode & FS_READ) && (mode & FS_WRITE)) {
            flags |= LFS_O_RDWR | LFS_O_CREAT;
        } else if (mode & FS_READ) {
            flags |= LFS_O_RDONLY;
        } else if (mode & FS_WRITE) {
            flags |= LFS_O_WRONLY | LFS_O_CREAT;
        }
        if (mode & FS_TRUNCATE) {
            flags |= LFS_O_TRUNC;
        }

        memset(&handle->file.cfg, 0, sizeof(handle->file.cfg));
        handle->file.cfg.buffer = fs_device_filebuf(__find_idx);

        if (LFS_API_CALL(lfs_file_opencfg, &lfs, &handle->file.file_handle, filename, flags, &handle->file.cfg) < 0) {
            return INVALID_FILESYSTEM_FD;
        }

        fs_fd_t fd   = allocate_fd();
        handle->fd   = fd;
        handle->type = FD_TYPE_FILE;

        // Intentionally don't unmount while we have an open file
        FS_SKIP_AUTO_UNMOUNT();
        return fd;
    });
    return INVALID_FILESYSTEM_FD;
}

/**
 * @brief Seek to position in file (internal, not thread-safe)
 *
 * @param fd File descriptor
 * @param offset Offset in bytes
 * @param whence Seek origin (SET/CUR/END)
 * @return New position on success, -1 on failure
 */
static fs_offset_t fs_seek_nolock(fs_fd_t fd, fs_offset_t offset, fs_whence_t whence) {
    FIND_FD_GET_HANDLE(fd, FD_TYPE_FILE, {
        if (!fs_is_mounted_nolock()) {
            return -1;
        }

        // Validate enum values match at compile time
        _Static_assert((int)FS_SEEK_SET == (int)LFS_SEEK_SET, "FS_SEEK_SET must match LFS_SEEK_SET");
        _Static_assert((int)FS_SEEK_CUR == (int)LFS_SEEK_CUR, "FS_SEEK_CUR must match LFS_SEEK_CUR");
        _Static_assert((int)FS_SEEK_END == (int)LFS_SEEK_END, "FS_SEEK_END must match LFS_SEEK_END");

        if (LFS_API_CALL(lfs_file_seek, &lfs, &handle->file.file_handle, (lfs_soff_t)offset, (int)whence) < 0) {
            return -1;
        }

        fs_offset_t current_pos = (fs_offset_t)LFS_API_CALL(lfs_file_tell, &lfs, &handle->file.file_handle);
        return (current_pos < 0) ? -1 : current_pos;
    });
    return -1;
}

/**
 * @brief Get current file position (internal, not thread-safe)
 *
 * @param fd File descriptor
 * @return Current position in bytes, or -1 on failure
 */
static fs_offset_t fs_tell_nolock(fs_fd_t fd) {
    FIND_FD_GET_HANDLE(fd, FD_TYPE_FILE, {
        if (!fs_is_mounted_nolock()) {
            return -1;
        }

        fs_offset_t offset = (fs_offset_t)LFS_API_CALL(lfs_file_tell, &lfs, &handle->file.file_handle);
        return (offset < 0) ? -1 : offset;
    });
    return -1;
}

/**
 * @brief Read data from file (internal, not thread-safe)
 *
 * @param fd File descriptor
 * @param buffer Buffer to store read data
 * @param length Number of bytes to read
 * @return Number of bytes read, or -1 on failure
 */
static fs_size_t fs_read_nolock(fs_fd_t fd, void *buffer, fs_size_t length) {
    FIND_FD_GET_HANDLE(fd, FD_TYPE_FILE, {
        if (!fs_is_mounted_nolock()) {
            return -1;
        }

        fs_size_t ret = (fs_size_t)LFS_API_CALL(lfs_file_read, &lfs, &handle->file.file_handle, buffer, (lfs_size_t)length);
        return (ret < 0) ? -1 : ret;
    });
    return -1;
}

/**
 * @brief Write data to file (internal, not thread-safe)
 *
 * @param fd File descriptor
 * @param buffer Buffer containing data to write
 * @param length Number of bytes to write
 * @return Number of bytes written, or -1 on failure
 */
static fs_size_t fs_write_nolock(fs_fd_t fd, const void *buffer, fs_size_t length) {
    FIND_FD_GET_HANDLE(fd, FD_TYPE_FILE, {
        if (!fs_is_mounted_nolock()) {
            return -1;
        }

        fs_size_t ret = (fs_size_t)LFS_API_CALL(lfs_file_write, &lfs, &handle->file.file_handle, buffer, (lfs_size_t)length);
        return (ret < 0) ? -1 : ret;
    });
    return -1;
}

/**
 * @brief Check if file is at end-of-file (internal, not thread-safe)
 *
 * @param fd File descriptor
 * @return true if at EOF or on error, false otherwise
 */
static bool fs_is_eof_nolock(fs_fd_t fd) {
    FIND_FD_GET_HANDLE(fd, FD_TYPE_FILE, {
        if (!fs_is_mounted_nolock()) {
            return true;
        }

        lfs_soff_t orig_offset = LFS_API_CALL(lfs_file_tell, &lfs, &handle->file.file_handle);
        if (orig_offset < 0) {
            return true;
        }

        lfs_soff_t end_offset = LFS_API_CALL(lfs_file_seek, &lfs, &handle->file.file_handle, 0, LFS_SEEK_END);
        if (end_offset < 0) {
            return true;
        }

        bool is_at_eof = (orig_offset == end_offset);
        LFS_API_CALL(lfs_file_seek, &lfs, &handle->file.file_handle, orig_offset, LFS_SEEK_SET);
        return is_at_eof;
    });
    return true;
}

/**
 * @brief Close file (internal, not thread-safe)
 *
 * @param fd File descriptor to close
 */
static void fs_close_nolock(fs_fd_t fd) {
    FIND_FD_GET_HANDLE(fd, FD_TYPE_FILE, {
        LFS_API_CALL(lfs_file_close, &lfs, &handle->file.file_handle);

        handle->fd   = INVALID_FILESYSTEM_FD;
        handle->type = FD_TYPE_EMPTY;

        fs_unmount_nolock(); // we can unmount here, mirrors the open in fs_open()
        return;
    });
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Public API Functions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool fs_format(void) {
    fs_dprintf("\n");
    FS_AUTO_LOCK_UNLOCK(false);
    return fs_format_nolock();
}

bool fs_init(void) {
    fs_dprintf("\n");
    FS_AUTO_LOCK_UNLOCK(false);
    return fs_init_nolock();
}

bool fs_mount(void) {
    FS_AUTO_LOCK_UNLOCK(false);
    return fs_mount_nolock();
}

void fs_unmount(void) {
    FS_AUTO_LOCK_UNLOCK();
    fs_unmount_nolock();
}

bool fs_is_mounted(void) {
    FS_AUTO_LOCK_UNLOCK(false);
    return fs_is_mounted_nolock();
}

bool fs_mkdir(const char *path) {
    if (!fs_is_path_safe(path) || !fs_is_path_depth_valid(path, FS_MAX_DIR_DEPTH)) {
        return false;
    }

    fs_dprintf("%s\n", path ? path : "(null)");
    FS_AUTO_LOCK_UNLOCK(false);
    return fs_mkdir_nolock(path);
}

bool fs_rmdir(const char *path, bool recursive) {
    if (!fs_is_path_safe(path) || !fs_is_path_depth_valid(path, FS_MAX_DIR_DEPTH)) {
        return false;
    }

    fs_dprintf("%s - %s\n", path ? path : "(null)", recursive ? "recursive" : "non-recursive");
    FS_AUTO_LOCK_UNLOCK(false);
    return fs_rmdir_nolock(path, recursive, 0);
}

fs_fd_t fs_opendir(const char *path) {
    if (!fs_is_path_safe(path) || !fs_is_path_depth_valid(path, FS_MAX_DIR_DEPTH)) {
        return INVALID_FILESYSTEM_FD;
    }

    fs_fd_t fd;
    {
        FS_AUTO_LOCK_UNLOCK(INVALID_FILESYSTEM_FD);
        fd = fs_opendir_nolock(path);
    }
    fs_dprintf("%s, fd=%d\n", path ? path : "(null)", (int)fd);
    return fd;
}

fs_dirent_t *fs_readdir(fs_fd_t fd) {
    fs_dprintf("%d\n", (int)fd);
    FS_AUTO_LOCK_UNLOCK(NULL);
    return fs_readdir_nolock(fd);
}

void fs_closedir(fs_fd_t fd) {
    fs_dprintf("%d\n", (int)fd);
    FS_AUTO_LOCK_UNLOCK();
    fs_closedir_nolock(fd);
}

bool fs_exists(const char *path) {
    if (!fs_is_path_safe(path) || !fs_is_path_depth_valid(path, FS_MAX_FILE_DEPTH)) {
        return false;
    }

    fs_dprintf("%s\n", path);
    FS_AUTO_LOCK_UNLOCK(false);
    return fs_exists_nolock(path);
}

bool fs_delete(const char *path) {
    if (!fs_is_path_safe(path) || !fs_is_path_depth_valid(path, FS_MAX_FILE_DEPTH)) {
        return false;
    }

    fs_dprintf("%s\n", path);
    FS_AUTO_LOCK_UNLOCK(false);
    return fs_delete_nolock(path);
}

fs_fd_t fs_open(const char *filename, fs_mode_t mode) {
    if (!fs_is_path_safe(filename) || !fs_is_path_depth_valid(filename, FS_MAX_FILE_DEPTH)) {
        return INVALID_FILESYSTEM_FD;
    }

    fs_fd_t fd;
    {
        FS_AUTO_LOCK_UNLOCK(INVALID_FILESYSTEM_FD);
        fd = fs_open_nolock(filename, mode);
    }
#ifdef CONSOLE_ENABLE
    char mode_str[8] = {0};
    if (mode & FS_READ) {
        strlcat(mode_str, "r", sizeof(mode_str));
    }
    if (mode & FS_WRITE) {
        strlcat(mode_str, "w", sizeof(mode_str));
    }
    if (mode & FS_TRUNCATE) {
        strlcat(mode_str, "t", sizeof(mode_str));
    }
    fs_dprintf("%s, mode=%s, fd=%d\n", filename ? filename : "(null)", mode_str, (int)fd);
#endif // CONSOLE_ENABLE
    return fd;
}

fs_offset_t fs_seek(fs_fd_t fd, fs_offset_t offset, fs_whence_t whence) {
    FS_AUTO_LOCK_UNLOCK(-1);
    return fs_seek_nolock(fd, offset, whence);
}

fs_offset_t fs_tell(fs_fd_t fd) {
    FS_AUTO_LOCK_UNLOCK(-1);
    return fs_tell_nolock(fd);
}

fs_size_t fs_read(fs_fd_t fd, void *buffer, fs_size_t length) {
    FS_AUTO_LOCK_UNLOCK(-1);
    return fs_read_nolock(fd, buffer, length);
}

fs_size_t fs_write(fs_fd_t fd, const void *buffer, fs_size_t length) {
    FS_AUTO_LOCK_UNLOCK(-1);
    return fs_write_nolock(fd, buffer, length);
}

bool fs_is_eof(fs_fd_t fd) {
    FS_AUTO_LOCK_UNLOCK(true);
    return fs_is_eof_nolock(fd);
}

void fs_close(fs_fd_t fd) {
    fs_dprintf("%d\n", (int)fd);
    FS_AUTO_LOCK_UNLOCK();
    fs_close_nolock(fd);
}

void fs_dump_info(void) {
#if defined(CONSOLE_ENABLE)
    struct lfs_fsinfo fs_info;
    lfs_ssize_t       size;
    {
        FS_AUTO_LOCK_UNLOCK();
        if ((size = lfs_fs_size(&lfs)) < 0) {
            return;
        }
        if (lfs_fs_stat(&lfs, &fs_info) < 0) {
            return;
        }
    }
    fs_dprintf("LFS disk version: 0x%08x, block size: %d bytes, block count: %d, allocated blocks: %d, name_max: %d bytes, file_max: %d bytes, attr_max: %d bytes\n", //
               (int)fs_info.disk_version, (int)fs_info.block_size, (int)fs_info.block_count, (int)size,                                                               //
               (int)fs_info.name_max, (int)fs_info.file_max, (int)fs_info.attr_max);

#endif
}
