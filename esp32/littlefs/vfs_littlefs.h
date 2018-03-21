#ifndef MICROPY_INCLUDED_EXTMOD_VFS_LITTLEFS_H
#define MICROPY_INCLUDED_EXTMOD_VFS_LITTLEFS_H

#include "py/obj.h"
#include "ff.h" //Needed for FatFs types

extern bool isLittleFs(const TCHAR *path);
extern byte littleFsErrorToErrno(enum lfs_error littleFsError);
extern FRESULT lfsErrorToFatFsError(int lfs_error);
extern int fatFsModetoLittleFsMode(int FatFsMode);

extern const mp_obj_type_t mp_littlefs_vfs_type;

MP_DECLARE_CONST_FUN_OBJ_3(littlefs_vfs_open_obj);

#endif // MICROPY_INCLUDED_EXTMOD_VFS_LITTLEFS_H
