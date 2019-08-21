#ifndef MICROPY_INCLUDED_VFS_LITTLEFS_H
#define MICROPY_INCLUDED_VFS_LITTLEFS_H

#include "py/obj.h"
#include "lib/oofatfs/ff.h" //Needed for FatFs types
#include "lfs.h"

#define LFS_ATTRIBUTE_TIMESTAMP     ((uint8_t)1)

typedef void* SemaphoreHandle_t;

typedef struct pycom_lfs_file_s {
    lfs_file_t fp;
    struct lfs_file_config cfg;  // Attributes of the file, e.g.: timestamp
    bool timestamp_update;  // For requesting timestamp update when closing the file
} pycom_lfs_file_t;

typedef struct vfs_lfs_struct_s
{
    lfs_t lfs;
    char* cwd; // Needs to be initialized to point to: "/\0"
    SemaphoreHandle_t mutex; // Needs to be created
}vfs_lfs_struct_t;

typedef struct lfs_timestamp_attribute_s
{
    WORD    fdate;
    WORD    ftime;
}lfs_timestamp_attribute_t;

typedef struct lfs_ftp_file_stat_s
{
    struct lfs_info info;
    lfs_timestamp_attribute_t timestamp;
}lfs_ftp_file_stat_t;

extern byte littleFsErrorToErrno(enum lfs_error littleFsError);
extern FRESULT lfsErrorToFatFsError(int lfs_error);
extern int fatFsModetoLittleFsMode(int FatFsMode);
extern const char* concat_with_cwd(vfs_lfs_struct_t* littlefs, const char* path);
extern int lfs_statvfs_count(void *p, lfs_block_t b);
extern void littlefs_prepare_attributes(struct lfs_file_config *cfg);
extern int littlefs_open_common_helper(lfs_t *lfs, const char* file_path, lfs_file_t *fp, int mode, struct lfs_file_config *cfg, bool *timestamp_ptr);
extern int littlefs_close_common_helper(lfs_t *lfs, lfs_file_t *fp, struct lfs_file_config *cfg, bool *timestamp_ptr);
extern int littlefs_stat_common_helper(lfs_t *lfs, const char* file_path, struct lfs_info *fno, lfs_timestamp_attribute_t *ts);
extern void littlefs_free_up_attributes(struct lfs_file_config *cfg);
extern int littlefs_update_timestamp(lfs_t* lfs, const char* file_relative_path);
extern void littlefs_update_timestamp_cfg(struct lfs_file_config *cfg);
extern lfs_timestamp_attribute_t littlefs_get_timestamp_fp(lfs_file_t* fp);


extern const mp_obj_type_t mp_littlefs_vfs_type;
MP_DECLARE_CONST_FUN_OBJ_3(littlefs_vfs_open_obj);


#endif // MICROPY_INCLUDED_VFS_LITTLEFS_H
