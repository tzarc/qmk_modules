// Copyright 2022-2025 Nick Brassel (@tzarc)
// SPDX-License-Identifier: GPL-2.0-or-later
#include <ch.h>
#include <string.h>
#include "chmtx.h"
#include "filesystem.h"
#include "lfs.h"

// Can't use fd of 0, or 1/2/3 (stdin/stdout/stderr)
#define FIRST_VALID_FD 4

static fs_fd_t current_fd = FIRST_VALID_FD;

typedef enum fs_lfs_fd_type_t { FD_TYPE_EMPTY, FD_TYPE_DIR, FD_TYPE_FILE } fs_lfs_fd_type_t;

typedef struct fs_lfs_handle_t {
    fs_fd_t          fd;
    fs_lfs_fd_type_t type;
    union {
        struct {
            lfs_dir_t       dir_handle;
            struct lfs_info entry_info;
            fs_dirent_t     dirent; // data replicated from entry_info
        } dir;
        struct {
            lfs_file_t             file_handle;
            struct lfs_file_config cfg;
        } file;
    };
} fs_lfs_handle_t;

extern const struct lfs_config lfs_cfg; // provided by underlying driver

static lfs_t           lfs;
static fs_lfs_handle_t fs_handles[MAX_NUM_OPEN_FDS];

#define FIND_FD_GET_HANDLE(search_fd, fd_type, block)                           \
    do {                                                                        \
        for (int __find_idx = 0; __find_idx < MAX_NUM_OPEN_FDS; ++__find_idx) { \
            fs_lfs_handle_t *handle = &fs_handles[__find_idx];                  \
            if (handle->fd == (search_fd) && handle->type == (fd_type)) {       \
                {                                                               \
                    block                                                       \
                }                                                               \
            }                                                                   \
        }                                                                       \
    } while (0)

static inline bool fd_can_be_used(fs_fd_t fd) {
    if (fd < FIRST_VALID_FD) {
        return false;
    }
    for (int i = 0; i < MAX_NUM_OPEN_FDS; ++i) {
        if (fs_handles[i].fd == fd) {
            return false;
        }
    }
    return true;
}

static inline fs_fd_t allocate_fd(void) {
    fs_fd_t first = ++current_fd;
    while (!fd_can_be_used(current_fd)) {
        ++current_fd;
        if (current_fd == first) {
            // If we've looped back around to the first, everything is already allocated (yikes!). Need to exit with a failure.
            return INVALID_FILESYSTEM_FD;
        }
    }
    return current_fd;
}

#define LFS_API_CALL(api, ...)                      \
    ({                                              \
        int ret = api(__VA_ARGS__);                 \
        if (ret < 0) {                              \
            fs_dprintf(#api " returned %d\n", ret); \
        }                                           \
        ret;                                        \
    })

static MUTEX_DECL(fs_mutex);

static inline bool fs_lock(void) {
    chMtxLock(&fs_mutex);
    return true;
}

static inline bool fs_unlock(void) {
    chMtxUnlock(&fs_mutex);
    return true;
}

static void fs_unlock_helper(bool *locked) {
    if (*locked) {
        fs_unlock();
    }
}

#define FS_AUTO_LOCK_UNLOCK(ret_on_fail)                                       \
    bool __fs_lock __attribute__((__cleanup__(fs_unlock_helper))) = fs_lock(); \
    do {                                                                       \
        if (!__fs_lock) {                                                      \
            return ret_on_fail;                                                \
        }                                                                      \
    } while (0)

#define FS_SKIP_AUTO_UNLOCK() \
    do {                      \
        __fs_lock = false;    \
    } while (0)

extern bool fs_device_init(void);

static bool fs_init_nolock(void);
static bool fs_mount_nolock(void);
static void fs_unmount_nolock(void);
static bool fs_delete_nolock(const char *path);

static int mount_count = 0;

static bool fs_is_mounted_nolock(void) {
    return mount_count > 0;
}

static bool fs_format_nolock(void) {
    while (fs_is_mounted_nolock()) {
        fs_unmount_nolock();
    }
    if (LFS_API_CALL(lfs_format, &lfs, &lfs_cfg) < 0) {
        return false;
    }
    return fs_init_nolock();
}

static bool fs_init_nolock(void) {
    while (fs_is_mounted_nolock()) {
        fs_unmount_nolock();
    }
    memset(fs_handles, 0, sizeof(fs_handles));
    return fs_device_init() && fs_mount_nolock();
}

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

static void fs_unmount_nolock(void) {
    if (fs_is_mounted_nolock()) {
        --mount_count;
        if (mount_count == 0) {
            LFS_API_CALL(lfs_unmount, &lfs);
        }
    }
}

static void fs_unmount_helper(bool *mounted) {
    if (*mounted) {
        fs_unmount_nolock();
    }
}

#define FS_AUTO_MOUNT_UNMOUNT(ret_on_fail)                                               \
    bool __fs_mount __attribute__((__cleanup__(fs_unmount_helper))) = fs_mount_nolock(); \
    do {                                                                                 \
        if (!__fs_mount) {                                                               \
            return ret_on_fail;                                                          \
        }                                                                                \
    } while (0)

#define FS_SKIP_AUTO_UNMOUNT() \
    do {                       \
        __fs_mount = false;    \
    } while (0)

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

static fs_dirent_t *fs_readdir_nolock(fs_fd_t fd) {
    FIND_FD_GET_HANDLE(fd, FD_TYPE_DIR, {
        // Offload to helper
        return fs_readdir_explicit_nolock(&handle->dir.dir_handle, &handle->dir.entry_info, &handle->dir.dirent);
    });
    return NULL;
}

static void fs_closedir_nolock(fs_fd_t fd) {
    FIND_FD_GET_HANDLE(fd, FD_TYPE_DIR, {
        LFS_API_CALL(lfs_dir_close, &lfs, &handle->dir.dir_handle);

        handle->fd   = INVALID_FILESYSTEM_FD;
        handle->type = FD_TYPE_EMPTY;

        fs_unmount_nolock(); // we can unmount here, mirrors the open in fs_opendir()
        return;
    });
}

static bool fs_mkdir_nolock(const char *path) {
    FS_AUTO_MOUNT_UNMOUNT(false);

    int err = LFS_API_CALL(lfs_mkdir, &lfs, path);
    return err >= 0 || err == LFS_ERR_EXIST; // Allow for already existing directories to count as success
}

static bool fs_rmdir_nolock(const char *path, bool recursive) {
    FS_AUTO_MOUNT_UNMOUNT(false);

    if (recursive) {
        lfs_dir_t dir;
        if (LFS_API_CALL(lfs_dir_open, &lfs, &dir, path) < 0) {
            return false;
        }

        struct lfs_info info;
        fs_dirent_t     dirent;
        char            child_path[LFS_NAME_MAX] = {0};
        while (fs_readdir_explicit_nolock(&dir, &info, &dirent) != NULL) {
            if (!strcmp(dirent.name, ".") || !strcmp(dirent.name, "..")) {
                continue;
            }
            strlcpy(child_path, path, sizeof(child_path));
            strlcat(child_path, "/", sizeof(child_path));
            strlcat(child_path, dirent.name, sizeof(child_path));
            if (info.type == LFS_TYPE_DIR) {
                if (!fs_rmdir_nolock(child_path, true)) {
                    return false;
                }
            } else {
                if (!fs_delete_nolock(child_path)) {
                    return false;
                }
            }
        }

        if (LFS_API_CALL(lfs_dir_close, &lfs, &dir) < 0) {
            return false;
        }
    }

    return fs_delete_nolock(path);
}

static bool fs_exists_nolock(const char *path) {
    FS_AUTO_MOUNT_UNMOUNT(false);
    struct lfs_info info;
    int             err = lfs_stat(&lfs, path, &info); // intentionally do not use LFS_API_CALL here, don't need error checking for this usage of the API
    return err >= 0 || err == LFS_ERR_EXIST;
}

static bool fs_delete_nolock(const char *path) {
    FS_AUTO_MOUNT_UNMOUNT(false);
    if (!fs_exists_nolock(path)) {
        return true;
    }
    int err = LFS_API_CALL(lfs_remove, &lfs, path);
    return err >= 0 || err == LFS_ERR_NOENT; // Allow for already deleted files to count as success
}

static fs_fd_t fs_open_nolock(const char *filename, fs_mode_t mode) {
    FIND_FD_GET_HANDLE(INVALID_FILESYSTEM_FD, FD_TYPE_EMPTY, {
        FS_AUTO_MOUNT_UNMOUNT(INVALID_FILESYSTEM_FD);

        int flags = 0;
        if (mode & FS_READ) {
            flags |= LFS_O_RDONLY;
        }
        if (mode & FS_WRITE) {
            flags |= LFS_O_WRONLY | LFS_O_CREAT;
        }
        if (mode & FS_TRUNCATE) {
            flags |= LFS_O_TRUNC;
        }

        extern void *fs_device_filebuf(int file_idx);
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

static fs_offset_t fs_seek_nolock(fs_fd_t fd, fs_offset_t offset, fs_whence_t whence) {
    FIND_FD_GET_HANDLE(fd, FD_TYPE_FILE, {
        if (!fs_is_mounted_nolock()) {
            return -1;
        }

        int lfs_whence = 0;
        switch (whence) {
            case FS_SEEK_SET:
                lfs_whence = LFS_SEEK_SET;
                break;
            case FS_SEEK_CUR:
                lfs_whence = LFS_SEEK_CUR;
                break;
            case FS_SEEK_END:
                lfs_whence = LFS_SEEK_END;
                break;
        }

        if (LFS_API_CALL(lfs_file_seek, &lfs, &handle->file.file_handle, (lfs_soff_t)offset, lfs_whence) < 0) {
            return -1;
        }

        fs_offset_t offset = (fs_offset_t)LFS_API_CALL(lfs_file_tell, &lfs, &handle->file.file_handle);
        return (offset < 0) ? -1 : offset;
    });
    return -1;
}

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

        if (orig_offset != end_offset) {
            LFS_API_CALL(lfs_file_seek, &lfs, &handle->file.file_handle, orig_offset, LFS_SEEK_SET);
            return true;
        }

        return false;
    });
    return true;
}

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
// Public APIs
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
    int before = mount_count;
    (void)before;
    bool ret;
    {
        FS_AUTO_LOCK_UNLOCK(false);
        ret = fs_mount_nolock();
    }
    fs_dprintf("%d -> %d\n", before, mount_count);
    return ret;
}

void fs_unmount(void) {
    int before = mount_count;
    (void)before;
    {
        FS_AUTO_LOCK_UNLOCK();
        fs_unmount_nolock();
    }
    fs_dprintf("%d -> %d\n", before, mount_count);
}

bool fs_is_mounted(void) {
    FS_AUTO_LOCK_UNLOCK(false);
    return fs_is_mounted_nolock();
}

fs_fd_t fs_opendir(const char *path) {
    fs_fd_t fd;
    {
        FS_AUTO_LOCK_UNLOCK(INVALID_FILESYSTEM_FD);
        fd = fs_opendir_nolock(path);
    }
    fs_dprintf("%s, fd=%d\n", path, (int)fd);
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

bool fs_mkdir(const char *path) {
    fs_dprintf("%s\n", path);
    FS_AUTO_LOCK_UNLOCK(false);
    return fs_mkdir_nolock(path);
}

bool fs_rmdir(const char *path, bool recursive) {
    fs_dprintf("%s - %s\n", path, recursive ? "recursive" : "non-recursive");
    FS_AUTO_LOCK_UNLOCK(false);
    return fs_rmdir_nolock(path, recursive);
}

bool fs_exists(const char *path) {
    fs_dprintf("%s\n", path);
    FS_AUTO_LOCK_UNLOCK(false);
    return fs_exists_nolock(path);
}

bool fs_delete(const char *path) {
    fs_dprintf("%s\n", path);
    FS_AUTO_LOCK_UNLOCK(false);
    return fs_delete_nolock(path);
}

fs_fd_t fs_open(const char *filename, fs_mode_t mode) {
    fs_fd_t fd;
    {
        FS_AUTO_LOCK_UNLOCK(INVALID_FILESYSTEM_FD);
        fd = fs_open_nolock(filename, mode);
    }
#ifdef CONSOLE_ENABLE
    char mode_str[4] = {0};
    if (mode & FS_READ) {
        strlcat(mode_str, "r", sizeof(mode_str));
    }
    if (mode & FS_WRITE) {
        strlcat(mode_str, "w", sizeof(mode_str));
    }
    if (mode & FS_TRUNCATE) {
        strlcat(mode_str, "t", sizeof(mode_str));
    }
    fs_dprintf("%s, mode=%s, fd=%d\n", filename, mode_str, (int)fd);
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
    fs_dprintf("LFS disk version: 0x%08x, block size: %d, block count: %d, allocated blocks: %d, name_max: %d, file_max: %d, attr_max: %d\n", //
               (int)fs_info.disk_version, (int)fs_info.block_size, (int)fs_info.block_count, (int)size,                                       //
               (int)fs_info.name_max, (int)fs_info.file_max, (int)fs_info.attr_max);

#endif
}
