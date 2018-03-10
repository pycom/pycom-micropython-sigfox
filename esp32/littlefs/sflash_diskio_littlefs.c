#include "ff.h" /* Needed by diskio.h */
#include "diskio.h"
#include "sflash_diskio.h"
#include "sflash_diskio_littlefs.h"

//TODO: figure out a proper value here
#define CONTEXT (void*)1234


int littlefs_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size)
{
    return sflash_disk_read(buffer, block, (size+off)/SFLASH_FS_SECTOR_SIZE);
}


int littlefs_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size)
{
    return sflash_disk_write(buffer, block, (size+off)/SFLASH_FS_SECTOR_SIZE);
}


int littlefs_erase(const struct lfs_config *c, lfs_block_t block)
{
    // sflash_disk_write erases before write
    return RES_OK;
}

int littlefs_sync(const struct lfs_config *c)
{
    //TODO: probably this is not needed as it will do nothing
    return sflash_disk_flush();
}

const struct lfs_config lfscfg =
{
    .context = CONTEXT,
    .read = &littlefs_read,
    .prog = &littlefs_prog,
    .erase = &littlefs_erase,
    .sync = &littlefs_sync,
    .read_size = SFLASH_FS_SECTOR_SIZE,
    .prog_size = SFLASH_FS_SECTOR_SIZE,
    .block_size = SFLASH_FS_SECTOR_SIZE,
    .block_count = SFLASH_SECTORS_PER_BLOCK*SFLASH_BLOCK_COUNT_4MB,
    .lookahead = 128
};
