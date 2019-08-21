#include "ff.h" /* Needed by diskio.h */
#include "diskio.h"
#include "sflash_diskio.h"
#include "sflash_diskio_littlefs.h"

//TODO: figure out a proper value here
#define PYCOM_CONTEXT ((void*)"pycom.io")


char prog_buffer[SFLASH_BLOCK_SIZE] = {0};
char read_buffer[SFLASH_BLOCK_SIZE] = {0};
// Must be on 64 bit aligned address, create it as array of 64 bit entries to achieve it
uint64_t lookahead_buffer[SFLASH_BLOCK_COUNT_8MB/(8*8)] = {0};

int littlefs_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size)
{
    return sflash_disk_read_littlefs(c, buffer, block, size);
}


int littlefs_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size)
{
    return sflash_disk_write_littlefs(c, buffer, block, size);
}


int littlefs_erase(const struct lfs_config *c, lfs_block_t block)
{
    return sflash_disk_erase_littlefs(c, block);
}

int littlefs_sync(const struct lfs_config *c)
{
    return LFS_ERR_OK;
}

struct lfs_config lfscfg =
{
    .context = PYCOM_CONTEXT,
    .read = &littlefs_read,
    .prog = &littlefs_prog,
    .erase = &littlefs_erase,
    .sync = &littlefs_sync,
    .read_size = SFLASH_BLOCK_SIZE,
    .prog_size = SFLASH_BLOCK_SIZE,
    .block_size = SFLASH_BLOCK_SIZE,
    .block_count = 0, // To be initialized according to the flash size of the chip
    .block_cycles = 0, // No block-level wear-leveling
    /* Current implementation of littlefs_read/prog/erase/sync() functions only operates with
     * full blocks, always the starting address of the block is passed to the driver.
     * This helps on the Power-loss resilient behavior of LittleFS, with this approach the File System will not be corrupted by corruption of a single file/block
     * E.g.: it is not possible to only read/write a middle of a block, inline files will not work, set cache_size equal to read/prog size.*/
    .cache_size = SFLASH_BLOCK_SIZE,
    .lookahead_size = 0, // To be initialized according to the flash size of the chip
    .prog_buffer = prog_buffer,
    .read_buffer = read_buffer,
    .lookahead_buffer = lookahead_buffer,
    .name_max = 0, // 0 means it is equal to LFS_NAME_MAX
    .file_max = 0, // 0 means it is equal to LFS_FILE_MAX
    .attr_max = 0 // 0 means it is equal to LFS_ATTR_MAX
};
