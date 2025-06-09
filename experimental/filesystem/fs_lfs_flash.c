// Copyright 2022-2025 Nick Brassel (@tzarc)
// SPDX-License-Identifier: GPL-2.0-or-later
#include <stdbool.h>
#include <ch.h>
#include "filesystem.h"
#include "lfs.h"
#include "flash_spi.h"

// Configurable LittleFS flash parameters (normally should not need to be overridden):
// - LFS_BLOCK_SIZE: defaults to EXTERNAL_FLASH_BLOCK_SIZE
// - LFS_BLOCK_COUNT: defaults to EXTERNAL_FLASH_BLOCK_COUNT
// - LFS_CACHE_SIZE: defaults to EXTERNAL_FLASH_PAGE_SIZE
// - LFS_BLOCK_CYCLES: defaults to 100 erase cycles

/** @brief Size of each filesystem block in bytes */
#ifndef LFS_BLOCK_SIZE
#    define LFS_BLOCK_SIZE (EXTERNAL_FLASH_BLOCK_SIZE)
#endif // LFS_BLOCK_SIZE

/** @brief Total number of blocks used by the filesystem */
#ifndef LFS_BLOCK_COUNT
#    define LFS_BLOCK_COUNT (EXTERNAL_FLASH_BLOCK_COUNT)
#endif // LFS_BLOCK_COUNT

/** @brief Size of cache buffers in bytes */
#ifndef LFS_CACHE_SIZE
#    define LFS_CACHE_SIZE (EXTERNAL_FLASH_PAGE_SIZE)
#endif // LFS_CACHE_SIZE

/** @brief Number of erase cycles before wear leveling kicks in */
#ifndef LFS_BLOCK_CYCLES
#    define LFS_BLOCK_CYCLES 100
#endif // LFS_BLOCK_CYCLES

// Compile-time validation of filesystem parameters
_Static_assert((LFS_BLOCK_SIZE) >= 128, "LFS_BLOCK_SIZE must be >= 128 bytes");
_Static_assert((LFS_CACHE_SIZE) % 8 == 0, "LFS_CACHE_SIZE must be a multiple of 8 bytes");
_Static_assert((LFS_BLOCK_SIZE) % (LFS_CACHE_SIZE) == 0, "LFS_BLOCK_SIZE must be a multiple of LFS_CACHE_SIZE");

/**
 * @brief LittleFS buffer storage
 *
 * Contains all the buffers required by LittleFS operations:
 * - Read cache buffer for block reads (LFS_CACHE_SIZE bytes)
 * - Program cache buffer for block writes (LFS_CACHE_SIZE bytes)
 * - Lookahead buffer for block allocation (LFS_CACHE_SIZE bytes)
 * - Per-file buffers for open file operations (LFS_CACHE_SIZE bytes each)
 */
static struct {
    uint8_t lfs_read_buf[LFS_CACHE_SIZE] __attribute__((aligned(4)));
    uint8_t lfs_prog_buf[LFS_CACHE_SIZE] __attribute__((aligned(4)));
    uint8_t lfs_lookahead_buf[LFS_CACHE_SIZE] __attribute__((aligned(4)));
    uint8_t lfs_file_bufs[FS_MAX_NUM_OPEN_FDS][LFS_CACHE_SIZE] __attribute__((aligned(4)));
} fs_lfs_buffers;

/**
 * @brief Initialize the filesystem device
 *
 * Clears all LittleFS buffers and initializes the underlying flash hardware.
 *
 * @return true on successful initialization
 */
bool fs_device_init(void) {
    memset(&fs_lfs_buffers, 0, sizeof(fs_lfs_buffers));
    flash_init();
    return true;
}

/**
 * @brief Get a file buffer for the specified file index
 *
 * @param file_idx Index of the file (0 to FS_MAX_NUM_OPEN_FDS-1)
 * @return Pointer to LFS_CACHE_SIZE byte file buffer, or NULL if index is invalid
 */
void *fs_device_filebuf(int file_idx) {
    if (file_idx < 0 || file_idx >= FS_MAX_NUM_OPEN_FDS) {
        return NULL;
    }
    return fs_lfs_buffers.lfs_file_bufs[file_idx];
}

/**
 * @brief Validate block parameters and calculate flash address safely
 *
 * Performs validation of block operations including:
 * - Block number bounds checking
 * - Negative offset validation
 * - Integer overflow protection in address calculation
 * - Size overflow checking
 *
 * @param c LittleFS configuration
 * @param block Block number to access
 * @param off Offset within the block in bytes
 * @param size Size of the operation in bytes (0 for erase operations)
 * @param addr_out Output parameter for calculated flash address in bytes
 * @return 0 on success, negative LFS error code on failure
 */
static int fs_validate_block_address(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, lfs_size_t size, uint32_t *addr_out) {
    if (!c || block >= c->block_count) {
        return LFS_ERR_INVAL;
    }

    // Check for negative offset
    if (off < 0) {
        return LFS_ERR_INVAL;
    }

    // Check for integer overflow in address calculation
    if (block > UINT32_MAX / c->block_size) {
        return LFS_ERR_INVAL;
    }

    uint32_t base_addr = block * c->block_size;
    uint32_t addr      = base_addr + (uint32_t)off;

    // Check for overflow in final address
    if (addr < base_addr) {
        return LFS_ERR_INVAL;
    }

    // Check for size overflow
    if (size > 0 && addr + size < addr) {
        return LFS_ERR_INVAL;
    }

    if (addr_out) {
        *addr_out = addr;
    }
    return 0;
}

/**
 * @brief Convert flash driver status codes to LittleFS error codes
 *
 * @param status Flash operation status code
 * @return Corresponding LFS error code (0 for success, negative for errors)
 */
static int fs_flash_status_to_lfs_error(flash_status_t status) {
    switch (status) {
        case FLASH_STATUS_SUCCESS:
            return 0;
        case FLASH_STATUS_BAD_ADDRESS:
            return LFS_ERR_INVAL;
        case FLASH_STATUS_TIMEOUT:
        case FLASH_STATUS_BUSY:
        case FLASH_STATUS_ERROR:
        default:
            return LFS_ERR_IO;
    }
}

/**
 * @brief Read data from flash memory
 *
 * LittleFS callback function to read data from the flash device.
 * Validates parameters and translates block/offset to flash address.
 *
 * @param c LittleFS configuration
 * @param block Block number to read from
 * @param off Offset within the block in bytes
 * @param buffer Buffer to store read data
 * @param size Number of bytes to read
 * @return 0 on success, negative LFS error code on failure
 */
int fs_device_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size) {
    if (!buffer || size == 0) {
        return LFS_ERR_INVAL;
    }

    uint32_t addr;
    int      ret = fs_validate_block_address(c, block, off, size, &addr);
    if (ret < 0) {
        return ret;
    }

    flash_status_t status = flash_read_range(addr, buffer, size);
    return fs_flash_status_to_lfs_error(status);
}

/**
 * @brief Program (write) data to flash memory
 *
 * LittleFS callback function to write data to the flash device.
 * Can program any size from 1 byte up to the block size, but the target
 * flash region must have been previously erased.
 *
 * @param c LittleFS configuration
 * @param block Block number to write to
 * @param off Offset within the block in bytes
 * @param buffer Buffer containing data to write
 * @param size Number of bytes to write
 * @return 0 on success, negative LFS error code on failure
 */
int fs_device_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size) {
    if (!buffer || size == 0) {
        return LFS_ERR_INVAL;
    }

    uint32_t addr;
    int      ret = fs_validate_block_address(c, block, off, size, &addr);
    if (ret < 0) {
        return ret;
    }

    flash_status_t status = flash_write_range(addr, buffer, size);
    return fs_flash_status_to_lfs_error(status);
}

/**
 * @brief Erase a flash block
 *
 * LittleFS callback function to erase an entire block of flash memory.
 * After erasing, any location within the block can be programmed.
 *
 * @param c LittleFS configuration
 * @param block Block number to erase (entire block will be erased)
 * @return 0 on success, negative LFS error code on failure
 */
int fs_device_erase(const struct lfs_config *c, lfs_block_t block) {
    uint32_t addr;
    int      ret = fs_validate_block_address(c, block, 0, 0, &addr);
    if (ret < 0) {
        return ret;
    }

    flash_status_t status = flash_erase_sector(addr);
    return fs_flash_status_to_lfs_error(status);
}

/**
 * @brief Synchronize flash operations
 *
 * LittleFS callback function to ensure all pending flash operations
 * are completed. For SPI flash, operations are typically synchronous,
 * so this function returns immediately.
 *
 * @param c LittleFS configuration
 * @return 0 (always successful for SPI flash)
 */
int fs_device_sync(const struct lfs_config *c) {
    (void)c; // Unused parameter
    return 0;
}

/** @brief Mutex for thread-safe flash device access */
static MUTEX_DECL(fs_dev_mutex);

/**
 * @brief Lock the flash device for exclusive access
 *
 * LittleFS callback function to acquire exclusive access to the flash device.
 * Uses ChibiOS mutex for thread safety.
 *
 * @param c LittleFS configuration
 * @return 0 on success, LFS_ERR_INVAL if config is NULL
 */
int fs_device_lock(const struct lfs_config *c) {
    if (!c) {
        return LFS_ERR_INVAL;
    }
    chMtxLock(&fs_dev_mutex);
    return 0;
}

/**
 * @brief Unlock the flash device
 *
 * LittleFS callback function to release exclusive access to the flash device.
 * Must be called at some point after a successful lock operation.
 *
 * @param c LittleFS configuration
 * @return 0 on success, LFS_ERR_INVAL if config is NULL
 */
int fs_device_unlock(const struct lfs_config *c) {
    if (!c) {
        return LFS_ERR_INVAL;
    }
    chMtxUnlock(&fs_dev_mutex);
    return 0;
}

/**
 * @brief LittleFS configuration structure
 *
 * Defines the complete configuration for the LittleFS filesystem including:
 * - Block device operation callbacks
 * - Memory layout parameters (block size in bytes, block count, cache sizes in bytes)
 * - Thread safety mechanisms
 * - Pre-allocated buffer pointers
 * - Wear leveling parameters (block cycles)
 */
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
