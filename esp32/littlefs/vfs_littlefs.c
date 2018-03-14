
#include "py/mpconfig.h"

#include <string.h>
#include "py/runtime.h"
#include "py/mperrno.h"
#include "lib/oofatfs/ff.h"
#include "extmod/vfs.h"
#include "vfs_littlefs.h"
#include "lib/timeutils/timeutils.h"


STATIC mp_obj_t littlefs_vfs_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    return mp_const_none;
}

typedef struct _mp_vfs_littlefs_ilistdir_it_t {
    mp_obj_base_t base;
    mp_fun_1_t iternext;
    bool is_str;
    lfs_dir_t dir;
    lfs_t* lfs;
} mp_vfs_littlefs_ilistdir_it_t;

STATIC mp_obj_t mp_vfs_littlefs_ilistdir_it_iternext(mp_obj_t self_in) {
    mp_vfs_littlefs_ilistdir_it_t *self = MP_OBJ_TO_PTR(self_in);

    //TODO: check whether this for loop is needed here, seems no sense to have it
    for (;;) {
        struct lfs_info fno;
        int res = lfs_dir_read(self->lfs, &self->dir, &fno);
        char *fn = fno.name;
        if (res < LFS_ERR_OK || fn[0] == 0) {
            // stop on error or end of dir
            break;
        }

        // TODO: filter . and ..

        // make 3-tuple with info about this entry
        mp_obj_tuple_t *t = MP_OBJ_TO_PTR(mp_obj_new_tuple(3, NULL));
        if (self->is_str) {
            t->items[0] = mp_obj_new_str(fn, strlen(fn));
        } else {
            t->items[0] = mp_obj_new_bytes((const byte*)fn, strlen(fn));
        }
        if (fno.type == LFS_TYPE_DIR) {
            // dir
            t->items[1] = MP_OBJ_NEW_SMALL_INT(MP_S_IFDIR);
        } else {
            // file
            t->items[1] = MP_OBJ_NEW_SMALL_INT(MP_S_IFREG);
        }
        t->items[2] = MP_OBJ_NEW_SMALL_INT(0); // no inode number

        return MP_OBJ_FROM_PTR(t);
    }

    // ignore error because we may be closing a second time
    lfs_dir_close(self->lfs, &self->dir);

    return MP_OBJ_STOP_ITERATION;
}

STATIC mp_obj_t littlefs_vfs_ilistdir_func(size_t n_args, const mp_obj_t *args) {

    fs_user_mount_t *self = MP_OBJ_TO_PTR(args[0]);
    bool is_str_type = true;
    const char *path;
    if (n_args == 2) {
        if (mp_obj_get_type(args[1]) == &mp_type_bytes) {
            is_str_type = false;
        }
        path = mp_obj_str_get_str(args[1]);
    } else {
        path = "";
    }

    // Create a new iterator object to list the dir
    mp_vfs_littlefs_ilistdir_it_t *iter = m_new_obj(mp_vfs_littlefs_ilistdir_it_t);
    iter->lfs = &self->fs.littlefs;
    iter->base.type = &mp_type_polymorph_iter;
    iter->iternext = mp_vfs_littlefs_ilistdir_it_iternext;
    iter->is_str = is_str_type;
    int res = lfs_dir_open(&self->fs.littlefs, &iter->dir, path);
    if (res != LFS_ERR_OK) {
        //TODO: return with proper error code
        mp_raise_OSError(res);
    }

    return MP_OBJ_FROM_PTR(iter);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(littlefs_vfs_ilistdir_obj, 1, 2, littlefs_vfs_ilistdir_func);

STATIC mp_obj_t littlefs_vfs_mkdir(mp_obj_t vfs_in, mp_obj_t path_o) {
    fs_user_mount_t *self = MP_OBJ_TO_PTR(vfs_in);
    const char *path = mp_obj_str_get_str(path_o);
    int res = lfs_mkdir(&self->fs.littlefs, path);
    if (res == LFS_ERR_OK) {
        return mp_const_none;
    } else {
        //TODO: return with proper error code
        mp_raise_OSError(res);
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(littlefs_vfs_mkdir_obj, littlefs_vfs_mkdir);


STATIC mp_obj_t littlefs_vfs_remove(mp_obj_t vfs_in, mp_obj_t path_in) {
    fs_user_mount_t *self = MP_OBJ_TO_PTR(vfs_in);
    const char *path = mp_obj_str_get_str(path_in);

    int res = lfs_remove(&self->fs.littlefs, path);

    if (res != LFS_ERR_OK) {
        //TODO: return with proper error code
        mp_raise_OSError(res);
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(littlefs_vfs_remove_obj, littlefs_vfs_remove);
STATIC MP_DEFINE_CONST_FUN_OBJ_2(littlefs_vfs_rmdir_obj, littlefs_vfs_remove);

STATIC mp_obj_t littlefs_vfs_rename(mp_obj_t vfs_in, mp_obj_t path_in, mp_obj_t path_out) {
    fs_user_mount_t *self = MP_OBJ_TO_PTR(vfs_in);
    const char *old_path = mp_obj_str_get_str(path_in);
    const char *new_path = mp_obj_str_get_str(path_out);
    int res = lfs_rename(&self->fs.littlefs, old_path, new_path);
    if (res != LFS_ERR_OK) {
        //TODO: return with proper error code
        mp_raise_OSError(res);
    }

    return mp_const_none;

}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(littlefs_vfs_rename_obj, littlefs_vfs_rename);

///// Change current directory.
//STATIC mp_obj_t littlefs_vfs_chdir(mp_obj_t vfs_in, mp_obj_t path_in) {
//    fs_user_mount_t *self = MP_OBJ_TO_PTR(vfs_in);
//    const char *path;
//    path = mp_obj_str_get_str(path_in);
//
//    int res = lfs_dir_open(&self->fs.littlefs, path);
//
//    if (res != FR_OK) {
//        mp_raise_OSError(fresult_to_errno_table[res]);
//    }
//
//    return mp_const_none;
//}
//STATIC MP_DEFINE_CONST_FUN_OBJ_2(littlefs_vfs_chdir_obj, littlefs_vfs_chdir);

/// \function stat(path)
/// Get the status of a file or directory.
STATIC mp_obj_t littlefs_vfs_stat(mp_obj_t vfs_in, mp_obj_t path_in) {
    fs_user_mount_t *self = MP_OBJ_TO_PTR(vfs_in);
    const char *path = mp_obj_str_get_str(path_in);

    struct lfs_info fno;
    //TODO: is this needed ?
//    if (path[0] == 0 || (path[0] == '/' && path[1] == 0)) {
//        // stat root directory
//        fno.fsize = 0;
//        fno.fdate = 0x2821; // Jan 1, 2000
//        fno.ftime = 0;
//        fno.fattrib = AM_DIR;
//    } else {
        int res = lfs_stat(&self->fs.littlefs, path, &fno);
        if (res < LFS_ERR_OK) {
            //TODO: return correct error code
            mp_raise_OSError(res);
        }
//    }

    mp_obj_tuple_t *t = MP_OBJ_TO_PTR(mp_obj_new_tuple(10, NULL));
    mp_int_t mode = 0;
    if (fno.type == LFS_TYPE_DIR) {
        mode |= MP_S_IFDIR;
    } else {
        mode |= MP_S_IFREG;
    }
    //TODO: LittleFS does not store the time
//    mp_int_t seconds = timeutils_seconds_since_2000(
//        1980 + ((fno.fdate >> 9) & 0x7f),
//        (fno.fdate >> 5) & 0x0f,
//        fno.fdate & 0x1f,
//        (fno.ftime >> 11) & 0x1f,
//        (fno.ftime >> 5) & 0x3f,
//        2 * (fno.ftime & 0x1f)
//    );
    t->items[0] = MP_OBJ_NEW_SMALL_INT(mode); // st_mode
    t->items[1] = MP_OBJ_NEW_SMALL_INT(0); // st_ino
    t->items[2] = MP_OBJ_NEW_SMALL_INT(0); // st_dev
    t->items[3] = MP_OBJ_NEW_SMALL_INT(0); // st_nlink
    t->items[4] = MP_OBJ_NEW_SMALL_INT(0); // st_uid
    t->items[5] = MP_OBJ_NEW_SMALL_INT(0); // st_gid
    t->items[6] = mp_obj_new_int_from_uint(fno.size); // st_size
    //TODO: get time
    t->items[7] = MP_OBJ_NEW_SMALL_INT(0); // st_atime
    t->items[8] = MP_OBJ_NEW_SMALL_INT(0); // st_mtime
    t->items[9] = MP_OBJ_NEW_SMALL_INT(0); // st_ctime

    return MP_OBJ_FROM_PTR(t);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(littlefs_vfs_stat_obj, littlefs_vfs_stat);


STATIC const mp_rom_map_elem_t littlefs_vfs_locals_dict_table[] = {
    #if _FS_REENTRANT
//    { MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&littlefs_vfs_del_obj) },
    #endif
    //{ MP_ROM_QSTR(MP_QSTR_mkfs), MP_ROM_PTR(&littlefs_vfs_mkfs_obj) },
    { MP_ROM_QSTR(MP_QSTR_open), MP_ROM_PTR(&littlefs_vfs_open_obj) },
    { MP_ROM_QSTR(MP_QSTR_ilistdir), MP_ROM_PTR(&littlefs_vfs_ilistdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_mkdir), MP_ROM_PTR(&littlefs_vfs_mkdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_rmdir), MP_ROM_PTR(&littlefs_vfs_rmdir_obj) },
//    { MP_ROM_QSTR(MP_QSTR_chdir), MP_ROM_PTR(&littlefs_vfs_chdir_obj) },
//    { MP_ROM_QSTR(MP_QSTR_getcwd), MP_ROM_PTR(&littlefs_vfs_getcwd_obj) },
    { MP_ROM_QSTR(MP_QSTR_remove), MP_ROM_PTR(&littlefs_vfs_remove_obj) },
    { MP_ROM_QSTR(MP_QSTR_rename), MP_ROM_PTR(&littlefs_vfs_rename_obj) },
    { MP_ROM_QSTR(MP_QSTR_stat), MP_ROM_PTR(&littlefs_vfs_stat_obj) },
//    { MP_ROM_QSTR(MP_QSTR_mount), MP_ROM_PTR(&vfs_littlefs_mount_obj) },
//    { MP_ROM_QSTR(MP_QSTR_umount), MP_ROM_PTR(&littlefs_vfs_umount_obj) },

};
STATIC MP_DEFINE_CONST_DICT(littlefs_vfs_locals_dict, littlefs_vfs_locals_dict_table);

const mp_obj_type_t mp_littlefs_vfs_type = {
    { &mp_type_type },
    .name = MP_QSTR_VfsLittleFs,
    //TODO: do not allow to instantiate it
    .make_new = littlefs_vfs_make_new,
    .locals_dict = (mp_obj_dict_t*)&littlefs_vfs_locals_dict,
};

