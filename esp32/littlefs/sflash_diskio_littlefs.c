#include "ff.h" /* Needed by diskio.h */
#include "diskio.h"
#include "sflash_diskio.h"
#include "sflash_diskio_littlefs.h"

//TODO: figure out a proper value here
#define PYCOM_CONTEXT ((void*)"pycom.io")


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
    //TODO: probably this is not needed as it will do nothing
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
    .lookahead = 0 // To be initialized according to the flash size of the chip
};
