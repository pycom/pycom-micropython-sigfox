
#include "py/mpconfig.h"

#include <string.h>
#include "py/runtime.h"
#include "py/mperrno.h"
#include "py/gc.h"
#include "lib/oofatfs/ff.h"
#include "extmod/vfs.h"
#include "vfs_littlefs.h"
#include "lib/timeutils/timeutils.h"


int lfs_statvfs_count(void *p, lfs_block_t b)
{
    *(lfs_size_t *)p += 1;
    return 0;
}

// After this function m_free() must be called on the returned address after usage!!
const char* concat_with_cwd(vfs_lfs_struct_t* littlefs, const char* path)
{
    char* path_out = NULL;

    if (path[0] == '/') /* Absolute path */
    {
        path_out = (char*)gc_alloc(strlen(path) + 1, false); // Count the \0 too
        if(path_out != NULL)
        {
            strcpy(path_out, path);
        }
    }
    else
    {
        path_out = (char*)gc_alloc(strlen(littlefs->cwd) + 1 + strlen(path) + 1, false);
        if(path_out != NULL)
        {
            strcpy(path_out, littlefs->cwd);
            if (strlen(path_out) > 1) strcat(path_out, "/"); //if not root append / to the end
            strcat(path_out, path);
            uint len = strlen(path_out);
            if ((len > 1) && (path_out[len - 1] == '/')) //Remove trailing "/" from the end if any
            {
                path_out[len - 1] = '\0';
            }
        }

    }

    return (const char*)path_out;
}

static int is_valid_directory(vfs_lfs_struct_t* littlefs, const char* path)
{
    struct lfs_info fi;

    int r = lfs_stat(&littlefs->lfs, path, &fi);

    if(LFS_ERR_OK == r)
    {
        if(fi.type == LFS_TYPE_DIR) return true;
        else return false;
    }
    else
    {
        return false;
    }
}

static int change_cwd(vfs_lfs_struct_t* littlefs, const char* path_in)
{
    int res;

    if(path_in[0] == '.' && path_in[1] == '.' && path_in[2] == '\0') // go back 1 level
    {
        if(strlen(littlefs->cwd) > 1) // if cwd = "/" do nothing, else go back to one level
        {
            uint len = strlen(littlefs->cwd);
            for(;; len--)
            {
                if(len == 0)
                {
                    littlefs->cwd[1] = '\0';
                    break;
                }
                else if(littlefs->cwd[len] == '/')
                {
                    littlefs->cwd[len] = '\0';
                    break;
                }
            }
        }
        res = LFS_ERR_OK;
    }
    else if(path_in[0] == '.' && path_in[1] == '\0') // "." means the current directory, do nothing
    {
        res = LFS_ERR_OK;
    }
    else // append the new relative path to the end
    {
        const char* new_path = concat_with_cwd(littlefs, path_in);

        if(new_path == NULL)
        {
            res = LFS_ERR_NOMEM;
        }
        else if(is_valid_directory(littlefs, new_path))
        {
            m_free(littlefs->cwd);
            littlefs->cwd = (char*)new_path;
            MP_STATE_PORT(lfs_cwd) = littlefs->cwd;

            res = LFS_ERR_OK;
        }
        else
        {
            res = LFS_ERR_NOENT;
        }
    }

    return res;
}

static int parse_and_append_to_cwd(vfs_lfs_struct_t* littlefs, const char* path_in)
{
    char tmp[LFS_NAME_MAX+1];
    int start = 0;
    int end = 1; // Skip the first / = absolute path is given
    while(end < strlen(path_in)+1) // Read until the end of the path
    {
        while(path_in[end] != '/' && path_in[end] != '\0') end++; //find the first / or end of the path
        memset(tmp,0,end-start); // Clear the buffer
        memcpy(tmp, &path_in[start], end-start); // Copy the element of the path
        tmp[end-start] = '\0'; //

        int retVal = change_cwd(littlefs, tmp);

        if(retVal != LFS_ERR_OK) return retVal;

        end++; //skip the found / in path_in
        start = end;
    }

    return LFS_ERR_OK;
}

bool isLittleFs(const TCHAR *path)
{
    const char *flash = "/flash";

    if(strncmp(flash, path, sizeof(flash)-1) == 0)
    {
        return true;
    }
    else
    {
        return false;
    }
}

// this table converts from FRESULT to POSIX errno
byte littleFsErrorToErrno(enum lfs_error littleFsError)
{
    switch(littleFsError)
    {
        case LFS_ERR_OK:
            return 0;
        break;
        case LFS_ERR_CORRUPT:
            return MP_ENOEXEC;
        break;
        case LFS_ERR_NOENT:
            return MP_ENOENT;
        break;
        case LFS_ERR_EXIST:
            return MP_EEXIST;
        break;
        case LFS_ERR_NOTDIR:
            return MP_ENOTDIR;
        break;
        case LFS_ERR_ISDIR:
            return MP_EISDIR;
        break;
        case LFS_ERR_NOTEMPTY:
            return MP_ENOTEMPTY;
        break;
        case LFS_ERR_BADF:
            return MP_EBADF;
        break;
        case LFS_ERR_INVAL:
            return MP_EINVAL;
        break;
        case LFS_ERR_NOSPC:
            return MP_ENOSPC;
        break;
        case LFS_ERR_NOMEM:
            return MP_ENOMEM;
        break;
        default:
            return 0;
        break;
    }
};

// convert LittleFs return values/error code to the FatFs version
FRESULT lfsErrorToFatFsError(int lfs_error)
{
    switch (lfs_error)
    {
        case LFS_ERR_OK:
            return FR_OK;
        break;
        case LFS_ERR_IO:
            return FR_DISK_ERR;
        break;
        case LFS_ERR_CORRUPT:
            return FR_NO_FILESYSTEM;
        break;
        case LFS_ERR_NOENT:
            return FR_NO_FILE;
        break;
        case LFS_ERR_NOTDIR:
            return FR_INT_ERR;
        break;
        case LFS_ERR_ISDIR:
            return FR_INT_ERR;
        break;
        case LFS_ERR_NOTEMPTY:
            return FR_INT_ERR;
        break;
        case LFS_ERR_BADF:
            return FR_INVALID_OBJECT;
        break;
        case LFS_ERR_INVAL:
            return FR_INVALID_PARAMETER;
        break;
        case LFS_ERR_NOSPC:
            return FR_DISK_ERR;
        break;
        case LFS_ERR_NOMEM:
            return FR_INT_ERR;
        break;
        default:
            if(lfs_error > 0) return FR_OK; // positive value means good thing happened, transform it to OK result
            else return FR_INT_ERR;
        break;
    }
}

int fatFsModetoLittleFsMode(int FatFsMode)
{
    int mode = 0;

    if(FatFsMode & FA_READ) mode |= LFS_O_RDONLY;

    if(FatFsMode & FA_WRITE) mode |= LFS_O_WRONLY;

    if(FatFsMode & FA_CREATE_NEW) mode |= LFS_O_CREAT | LFS_O_EXCL;

    if(FatFsMode & FA_CREATE_ALWAYS) mode |= LFS_O_CREAT | LFS_O_TRUNC;

    if(FatFsMode & FA_OPEN_ALWAYS) mode |= LFS_O_CREAT;

    if(FatFsMode & FA_OPEN_APPEND) mode |= LFS_O_CREAT | LFS_O_APPEND;

    return mode;
}


typedef struct _mp_vfs_littlefs_ilistdir_it_t {
    mp_obj_base_t base;
    mp_fun_1_t iternext;
    bool is_str;
    lfs_dir_t dir;
    vfs_lfs_struct_t* littlefs;
} mp_vfs_littlefs_ilistdir_it_t;

STATIC mp_obj_t mp_vfs_littlefs_ilistdir_it_iternext(mp_obj_t self_in) {
    mp_vfs_littlefs_ilistdir_it_t *self = MP_OBJ_TO_PTR(self_in);

    //cycle is needed to filter out "." and ".."
    for (;;) {
        struct lfs_info fno;

        xSemaphoreTake(self->littlefs->mutex, portMAX_DELAY);
            int res = lfs_dir_read(&self->littlefs->lfs, &self->dir, &fno);
        xSemaphoreGive(self->littlefs->mutex);

        char *fn = fno.name;
        if (res < LFS_ERR_OK || fn[0] == 0) {
            // stop on error or end of dir
            break;
        }

        //Filter . and ..
        if(fn[0] == '.' && fn[1] == '\0') continue;
        if(fn[0] == '.' && fn[1] == '.' && fn[2] == '\0') continue;

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
    xSemaphoreTake(self->littlefs->mutex, portMAX_DELAY);
        lfs_dir_close(&self->littlefs->lfs, &self->dir);
    xSemaphoreGive(self->littlefs->mutex);

    return MP_OBJ_STOP_ITERATION;
}

STATIC mp_obj_t littlefs_vfs_ilistdir_func(size_t n_args, const mp_obj_t *args) {

    fs_user_mount_t *self = MP_OBJ_TO_PTR(args[0]);
    bool is_str_type = true;
    const char *path_in;
    int res = LFS_ERR_OK;

    if (n_args == 2) {
        if (mp_obj_get_type(args[1]) == &mp_type_bytes) {
            is_str_type = false;
        }
        path_in = mp_obj_str_get_str(args[1]);
    } else {
        path_in = "";
    }

    // Create a new iterator object to list the dir
    mp_vfs_littlefs_ilistdir_it_t *iter = m_new_obj(mp_vfs_littlefs_ilistdir_it_t);
    iter->littlefs = &self->fs.littlefs;
    iter->base.type = &mp_type_polymorph_iter;
    iter->iternext = mp_vfs_littlefs_ilistdir_it_iternext;
    iter->is_str = is_str_type;

    xSemaphoreTake(self->fs.littlefs.mutex, portMAX_DELAY);
        const char* path = concat_with_cwd(&self->fs.littlefs, path_in);
        if(path == NULL)
        {
            res = LFS_ERR_NOMEM;
        }
        else
        {
            res = lfs_dir_open(&self->fs.littlefs.lfs, &iter->dir, path);
        }
    xSemaphoreGive(self->fs.littlefs.mutex);

    m_free((void*)path);

    if (res != LFS_ERR_OK) {
        mp_raise_OSError(littleFsErrorToErrno(res));
    }

    return MP_OBJ_FROM_PTR(iter);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(littlefs_vfs_ilistdir_obj, 1, 2, littlefs_vfs_ilistdir_func);

STATIC mp_obj_t littlefs_vfs_mkdir(mp_obj_t vfs_in, mp_obj_t path_param) {

    int res = LFS_ERR_OK;
    fs_user_mount_t *self = MP_OBJ_TO_PTR(vfs_in);
    const char *path_in = mp_obj_str_get_str(path_param);

    xSemaphoreTake(self->fs.littlefs.mutex, portMAX_DELAY);
        const char* path = concat_with_cwd(&self->fs.littlefs, path_in);
        if(path == NULL)
        {
            res = LFS_ERR_NOMEM;
        }
        else
        {
            res = lfs_mkdir(&self->fs.littlefs.lfs, path);
        }
    xSemaphoreGive(self->fs.littlefs.mutex);

    m_free((void*)path);

    if (res != LFS_ERR_OK) {
        mp_raise_OSError(littleFsErrorToErrno(res));
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(littlefs_vfs_mkdir_obj, littlefs_vfs_mkdir);


STATIC mp_obj_t littlefs_vfs_remove(mp_obj_t vfs_in, mp_obj_t path_param) {

    int res = LFS_ERR_OK;
    fs_user_mount_t *self = MP_OBJ_TO_PTR(vfs_in);
    const char *path_in = mp_obj_str_get_str(path_param);

    xSemaphoreTake(self->fs.littlefs.mutex, portMAX_DELAY);
        const char* path = concat_with_cwd(&self->fs.littlefs, path_in);
        if(path == NULL)
        {
            res = LFS_ERR_NOMEM;
        }
        else
        {
            res = lfs_remove(&self->fs.littlefs.lfs, path);
        }
    xSemaphoreGive(self->fs.littlefs.mutex);

    m_free((void*)path);

    if (res != LFS_ERR_OK) {
        mp_raise_OSError(littleFsErrorToErrno(res));
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(littlefs_vfs_remove_obj, littlefs_vfs_remove);
STATIC MP_DEFINE_CONST_FUN_OBJ_2(littlefs_vfs_rmdir_obj, littlefs_vfs_remove);

STATIC mp_obj_t littlefs_vfs_rename(mp_obj_t vfs_in, mp_obj_t path_param_in, mp_obj_t path_param_out) {

    int res = LFS_ERR_OK;
    fs_user_mount_t *self = MP_OBJ_TO_PTR(vfs_in);
    const char *path_in = mp_obj_str_get_str(path_param_in);
    const char *path_out = mp_obj_str_get_str(path_param_out);

    xSemaphoreTake(self->fs.littlefs.mutex, portMAX_DELAY);
        const char* old_path = concat_with_cwd(&self->fs.littlefs, path_in);
        const char* new_path = concat_with_cwd(&self->fs.littlefs, path_out);

        if(old_path == NULL || new_path == NULL)
        {
            res = LFS_ERR_NOMEM;
        }
        else
        {
            res = lfs_rename(&self->fs.littlefs.lfs, old_path, new_path);
        }
    xSemaphoreGive(self->fs.littlefs.mutex);

    m_free((void*)old_path);
    m_free((void*)new_path);

    if (res != LFS_ERR_OK) {
        mp_raise_OSError(littleFsErrorToErrno(res));
    }

    return mp_const_none;

}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(littlefs_vfs_rename_obj, littlefs_vfs_rename);

STATIC mp_obj_t littlefs_vfs_chdir(mp_obj_t vfs_in, mp_obj_t path_param) {

    int res = LFS_ERR_OK;
    fs_user_mount_t *self = MP_OBJ_TO_PTR(vfs_in);
    const char *path_in = mp_obj_str_get_str(path_param);

    xSemaphoreTake(self->fs.littlefs.mutex, portMAX_DELAY);
        res = parse_and_append_to_cwd(&self->fs.littlefs, path_in);
    xSemaphoreGive(self->fs.littlefs.mutex);

    if (res != LFS_ERR_OK) {
        mp_raise_OSError(littleFsErrorToErrno(res));
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(littlefs_vfs_chdir_obj, littlefs_vfs_chdir);

STATIC mp_obj_t littlefs_vfs_getcwd(mp_obj_t vfs_in) {

    fs_user_mount_t *self = MP_OBJ_TO_PTR(vfs_in);

    xSemaphoreTake(self->fs.littlefs.mutex, portMAX_DELAY);
        mp_obj_t ret = mp_obj_new_str(self->fs.littlefs.cwd, strlen(self->fs.littlefs.cwd));
    xSemaphoreGive(self->fs.littlefs.mutex);

    return ret;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(littlefs_vfs_getcwd_obj, littlefs_vfs_getcwd);

STATIC mp_obj_t littlefs_vfs_stat(mp_obj_t vfs_in, mp_obj_t path_param) {

    int res = LFS_ERR_OK;
    fs_user_mount_t *self = MP_OBJ_TO_PTR(vfs_in);
    const char* path_in = mp_obj_str_get_str(path_param);
    struct lfs_info fno;

    xSemaphoreTake(self->fs.littlefs.mutex, portMAX_DELAY);
        const char* path = concat_with_cwd(&self->fs.littlefs, path_in);
        if(path == NULL)
        {
            res = LFS_ERR_NOMEM;
        }
        else if (path[0] == 0 || (path[0] == '/' && path[1] == 0))
        {
            // stat root directory
            fno.size = 0;
            fno.type = LFS_TYPE_DIR;
        } else {
            res = lfs_stat(&self->fs.littlefs.lfs, path, &fno);
        }
    xSemaphoreGive(self->fs.littlefs.mutex);

    if (res < LFS_ERR_OK) {
        mp_raise_OSError(littleFsErrorToErrno(res));
    }


    m_free((void*)path);

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

// Get the status of a VFS.
STATIC mp_obj_t littlefs_vfs_statvfs(mp_obj_t vfs_in, mp_obj_t path_in) {

    fs_user_mount_t *self = MP_OBJ_TO_PTR(vfs_in);
    (void)path_in;

    lfs_t* lfs = &self->fs.littlefs.lfs;

    mp_obj_tuple_t *t = MP_OBJ_TO_PTR(mp_obj_new_tuple(10, NULL));

    lfs_size_t in_use = 0;

    xSemaphoreTake(self->fs.littlefs.mutex, portMAX_DELAY);
        int res = lfs_traverse(lfs, lfs_statvfs_count, &in_use);
    xSemaphoreGive(self->fs.littlefs.mutex);

    if (res != LFS_ERR_OK) {
        mp_raise_OSError(littleFsErrorToErrno(res));
    }

    t->items[0] = MP_OBJ_NEW_SMALL_INT(lfs->cfg->block_size); // f_bsize
    t->items[1] = t->items[0]; // f_frsize
    t->items[2] = MP_OBJ_NEW_SMALL_INT(lfs->cfg->block_count); // f_blocks
    t->items[3] = MP_OBJ_NEW_SMALL_INT(lfs->cfg->block_count - in_use); // f_bfree
    t->items[4] = t->items[3]; // f_bavail
    t->items[5] = MP_OBJ_NEW_SMALL_INT(0); // f_files
    t->items[6] = MP_OBJ_NEW_SMALL_INT(0); // f_ffree
    t->items[7] = MP_OBJ_NEW_SMALL_INT(0); // f_favail
    t->items[8] = MP_OBJ_NEW_SMALL_INT(0); // f_flags
    t->items[9] = MP_OBJ_NEW_SMALL_INT(LFS_NAME_MAX); // f_namemax

    return MP_OBJ_FROM_PTR(t);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(littlefs_vfs_statvfs_obj, littlefs_vfs_statvfs);


STATIC mp_obj_t littlefs_vfs_umount(mp_obj_t self_in) {
    (void)self_in;
    // keep the LittleFs filesystem mounted internally so the VFS methods can still be used
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(littlefs_vfs_umount_obj, littlefs_vfs_umount);

STATIC const mp_rom_map_elem_t littlefs_vfs_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_open), MP_ROM_PTR(&littlefs_vfs_open_obj) },
    { MP_ROM_QSTR(MP_QSTR_ilistdir), MP_ROM_PTR(&littlefs_vfs_ilistdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_mkdir), MP_ROM_PTR(&littlefs_vfs_mkdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_rmdir), MP_ROM_PTR(&littlefs_vfs_rmdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_chdir), MP_ROM_PTR(&littlefs_vfs_chdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_getcwd), MP_ROM_PTR(&littlefs_vfs_getcwd_obj) },
    { MP_ROM_QSTR(MP_QSTR_remove), MP_ROM_PTR(&littlefs_vfs_remove_obj) },
    { MP_ROM_QSTR(MP_QSTR_rename), MP_ROM_PTR(&littlefs_vfs_rename_obj) },
    { MP_ROM_QSTR(MP_QSTR_stat), MP_ROM_PTR(&littlefs_vfs_stat_obj) },
    { MP_ROM_QSTR(MP_QSTR_statvfs), MP_ROM_PTR(&littlefs_vfs_statvfs_obj) },
//    { MP_ROM_QSTR(MP_QSTR_mount), MP_ROM_PTR(&vfs_littlefs_mount_obj) },
    { MP_ROM_QSTR(MP_QSTR_umount), MP_ROM_PTR(&littlefs_vfs_umount_obj) },

};
STATIC MP_DEFINE_CONST_DICT(littlefs_vfs_locals_dict, littlefs_vfs_locals_dict_table);

const mp_obj_type_t mp_littlefs_vfs_type = {
    { &mp_type_type },
    .locals_dict = (mp_obj_dict_t*)&littlefs_vfs_locals_dict,
};

