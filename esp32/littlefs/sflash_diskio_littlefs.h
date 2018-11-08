
#ifndef SFLASH_DISKIO_LITTLEFS_H
#define SFLASH_DISKIO_LITTLEFS_H

#include "lfs.h"

extern int littlefs_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size);
extern int littlefs_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size);
extern int littlefs_erase(const struct lfs_config *c, lfs_block_t block);
extern int littlefs_sync(const struct lfs_config *c);
extern struct lfs_config lfscfg;

#endif
