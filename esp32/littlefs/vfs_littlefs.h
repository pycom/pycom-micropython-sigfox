#ifndef MICROPY_INCLUDED_VFS_LITTLEFS_H
#define MICROPY_INCLUDED_VFS_LITTLEFS_H

#include "py/obj.h"
#include "freertos/semphr.h"
#include "ff.h" //Needed for FatFs types
#include "lfs.h"

typedef struct vfs_lfs_struct_s
{
    lfs_t lfs;
    char* cwd; // Needs to be initialized to point to: "/\0"
    SemaphoreHandle_t sem_cwd; // Needs to be created
}vfs_lfs_struct_t;

extern bool isLittleFs(const TCHAR *path);
extern byte littleFsErrorToErrno(enum lfs_error littleFsError);
extern FRESULT lfsErrorToFatFsError(int lfs_error);
extern int fatFsModetoLittleFsMode(int FatFsMode);
const char* concat_with_cwd(vfs_lfs_struct_t* littlefs, const char* path);

extern const mp_obj_type_t mp_littlefs_vfs_type;
MP_DECLARE_CONST_FUN_OBJ_3(littlefs_vfs_open_obj);


#endif // MICROPY_INCLUDED_VFS_LITTLEFS_H
