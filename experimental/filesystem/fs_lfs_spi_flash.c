// Copyright 2022-2025 Nick Brassel (@tzarc)
// SPDX-License-Identifier: GPL-2.0-or-later
#include <ch.h>
#include "filesystem.h"
#include "flash/flash_spi.h"
#include "lfs.h"
#include "flash_spi.h"

#ifndef LFS_BLOCK_SIZE
#    define LFS_BLOCK_SIZE (EXTERNAL_FLASH_BLOCK_SIZE)
#endif // LFS_BLOCK_SIZE

#ifndef LFS_BLOCK_COUNT
#    define LFS_BLOCK_COUNT (EXTERNAL_FLASH_BLOCK_COUNT)
#endif // LFS_BLOCK_COUNT

#ifndef LFS_CACHE_SIZE
#    define LFS_CACHE_SIZE (EXTERNAL_FLASH_PAGE_SIZE)
#endif // LFS_CACHE_SIZE

#ifndef LFS_BLOCK_CYCLES
#    define LFS_BLOCK_CYCLES 100
#endif // LFS_BLOCK_CYCLES

_Static_assert((LFS_BLOCK_SIZE) >= 128, "LFS_BLOCK_SIZE must be >= 128");
_Static_assert((LFS_CACHE_SIZE) % 8 == 0, "LFS_CACHE_SIZE must be a multiple of 8");
_Static_assert((LFS_BLOCK_SIZE) % (LFS_CACHE_SIZE) == 0, "LFS_BLOCK_SIZE must be a multiple of LFS_CACHE_SIZE");

static struct {
    uint8_t lfs_read_buf[LFS_CACHE_SIZE] __attribute__((aligned(4)));
    uint8_t lfs_prog_buf[LFS_CACHE_SIZE] __attribute__((aligned(4)));
    uint8_t lfs_lookahead_buf[LFS_CACHE_SIZE] __attribute__((aligned(4)));
    uint8_t lfs_file_bufs[MAX_NUM_OPEN_FDS][LFS_CACHE_SIZE] __attribute__((aligned(4)));
} fs_lfs_buffers;

bool fs_device_init(void) {
    memset(&fs_lfs_buffers, 0, sizeof(fs_lfs_buffers));
    flash_init();
    return true;
}

void *fs_device_filebuf(int file_idx) {
    return fs_lfs_buffers.lfs_file_bufs[file_idx];
}

int fs_device_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size) {
    // fs_dprintf("read\n");
    flash_read_range(block * c->block_size + off, buffer, size);
    return 0;
}

int fs_device_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size) {
    // fs_dprintf("prog\n");
    flash_write_range(block * c->block_size + off, buffer, size);
    return 0;
}

int fs_device_erase(const struct lfs_config *c, lfs_block_t block) {
    // fs_dprintf("erase\n");
    flash_erase_sector(block * c->block_size);
    return 0;
}

int fs_device_sync(const struct lfs_config *c) {
    // fs_dprintf("sync\n");
    return 0;
}

static MUTEX_DECL(fs_dev_mutex);
int fs_device_lock(const struct lfs_config *c) {
    chMtxLock(&fs_dev_mutex);
    return 0;
}

int fs_device_unlock(const struct lfs_config *c) {
    chMtxUnlock(&fs_dev_mutex);
    return 0;
}

// configuration of the filesystem is provided by this struct
const struct lfs_config lfs_cfg = {
    // thread safety
    .lock   = fs_device_lock,
    .unlock = fs_device_unlock,

    // block device operations
    .read  = fs_device_read,
    .prog  = fs_device_prog,
    .erase = fs_device_erase,
    .sync  = fs_device_sync,

    // block device configuration
    .read_size      = (LFS_CACHE_SIZE),
    .prog_size      = (LFS_CACHE_SIZE),
    .block_size     = (LFS_BLOCK_SIZE),
    .block_count    = (LFS_BLOCK_COUNT),
    .block_cycles   = (LFS_BLOCK_CYCLES),
    .cache_size     = (LFS_CACHE_SIZE),
    .lookahead_size = (LFS_CACHE_SIZE),

    .read_buffer      = fs_lfs_buffers.lfs_read_buf,
    .prog_buffer      = fs_lfs_buffers.lfs_prog_buf,
    .lookahead_buffer = fs_lfs_buffers.lfs_lookahead_buf,
};
