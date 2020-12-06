
#include "py/mpconfig.h"

#include <string.h>
#include "py/runtime.h"
#include "py/mperrno.h"
#include "lib/oofatfs/ff.h"
#include "extmod/vfs.h"
#include "vfs_littlefs.h"
#include "lib/timeutils/timeutils.h"
#include "sflash_diskio_littlefs.h"


int lfs_statvfs_count(void *p, lfs_block_t b)
{
    *(lfs_size_t *)p += 1;
    return 0;
}

// After this function free() must be called on the returned address after usage!!
const char* concat_with_cwd(vfs_lfs_struct_t* littlefs, const char* path)
{
    char* path_out = NULL;

    if (path[0] == '/') /* Absolute path */
    {
        path_out = (char*)malloc(strlen(path) + 1); // Count the \0 too
        if(path_out != NULL)
        {
            strcpy(path_out, path);
        }
    }
    else
    {
        path_out = (char*)malloc(strlen(littlefs->cwd) + 1 + strlen(path) + 1);
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

static mp_import_stat_t lfs_vfs_import_stat(void *self, const char *path)
{
    struct lfs_info lfs_info_stat;
    fs_user_mount_t *vfs = (fs_user_mount_t*) self;
    assert(vfs != NULL);
    /* check if path exists */
    if((int)LFS_ERR_OK == lfs_stat(&(vfs->fs.littlefs.lfs), path, &lfs_info_stat))
    {
        if(lfs_info_stat.type == LFS_TYPE_DIR)
        {
            return MP_IMPORT_STAT_DIR;
        }
        else
        {
            return MP_IMPORT_STAT_FILE;
        }
    }
    else
    {
        return MP_IMPORT_STAT_NO_EXIST;
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
            free(littlefs->cwd);
            littlefs->cwd = (char*)new_path;

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

int littlefs_open_common_helper(lfs_t *lfs, const char* file_path, lfs_file_t *fp, int mode, struct lfs_file_config *cfg, bool *timestamp_update_ptr){

    // Prepare the attributes
    littlefs_prepare_attributes(cfg);

    bool new_file = false;
    struct lfs_info info;
    if(LFS_ERR_NOENT == lfs_stat(lfs, file_path, &info)) {
        new_file = true;
    }

    // Open/create the file
    int res = lfs_file_opencfg(lfs, fp, file_path, mode, cfg);

    if(res == LFS_ERR_OK) {
        /* Request timestamp update, if open was successful and:
         * - The file is truncated when opened OR
         * - The file does not exist thus is created now
         */
        if((mode & LFS_O_TRUNC) || (new_file == true)) {
            *timestamp_update_ptr = true;
        }

        /* Fetch the timestamp if no read access given, it is only read automatically in lfs_file_opencfg if the file opened with read access */
        if(!(mode & LFS_O_RDONLY)){
            int res_getattr = lfs_getattr(lfs, file_path, LFS_ATTRIBUTE_TIMESTAMP, fp->cfg->attrs[0].buffer, sizeof(lfs_timestamp_attribute_t));
            // Request timestamp update if no timestamp is saved for this entry
            if(res_getattr < LFS_ERR_OK) {
                *timestamp_update_ptr = true;
            }
        }
    }
    else {
        // Problem happened, free up the attributes
        littlefs_free_up_attributes(cfg);
    }

    return res;
}

int littlefs_close_common_helper(lfs_t *lfs, lfs_file_t *fp, struct lfs_file_config *cfg, bool *timestamp_update_ptr){

    // Update timestamp if it has been requested
    if(*timestamp_update_ptr == true) {
        littlefs_update_timestamp_cfg(cfg);
        *timestamp_update_ptr = false;
    }

    int res = lfs_file_close(lfs, fp);
    if(res == LFS_ERR_OK) {
        littlefs_free_up_attributes(cfg);
    }

    return res;
}

int littlefs_stat_common_helper(lfs_t *lfs, const char* file_path, struct lfs_info *fno, lfs_timestamp_attribute_t *ts){

    int res = lfs_stat(lfs, file_path, fno);
    if(res == LFS_ERR_OK) {
        // Get the timestamp
        int lfs_getattr_ret = lfs_getattr(lfs, file_path, LFS_ATTRIBUTE_TIMESTAMP, ts, sizeof(lfs_timestamp_attribute_t));
        // If no timestamp is saved for this entry, fill it with 0
        if(lfs_getattr_ret < LFS_ERR_OK) {
            ts->fdate = 0;
            ts->ftime = 0;
        }
    }

    return res;
}


void littlefs_prepare_attributes(struct lfs_file_config *cfg)
{
    // Currently we only have 1 attribute
    cfg->attr_count = 1;
    cfg->attrs = malloc(cfg->attr_count * sizeof(struct lfs_attr));

    // Set attribute for storing the timestamp
    cfg->attrs[0].size = sizeof(lfs_timestamp_attribute_t);
    cfg->attrs[0].type = LFS_ATTRIBUTE_TIMESTAMP;
    cfg->attrs[0].buffer = malloc(sizeof(lfs_timestamp_attribute_t));

}

void littlefs_free_up_attributes(struct lfs_file_config *cfg)
{
    cfg->attr_count = 0;
    // Currently we only have 1 attribute for timestamp
    free(cfg->attrs[0].buffer);
    free(cfg->attrs);
}


int littlefs_update_timestamp(lfs_t* lfs, const char* file_relative_path)
{
    DWORD tm = get_fattime();
    lfs_timestamp_attribute_t ts;
    ts.fdate = (WORD)(tm >> 16);
    ts.ftime = (WORD)tm;
    // Write directly the timestamp value onto the flash
    return lfs_setattr(lfs, file_relative_path, LFS_ATTRIBUTE_TIMESTAMP, &ts, sizeof(lfs_timestamp_attribute_t));
}

void littlefs_update_timestamp_cfg(struct lfs_file_config *cfg)
{
    // Check is needed to prevent any accidental case when cfg->attrs[x].buffer is already freed up by someone else
    if(cfg->attr_count > 0) {
        DWORD tm = get_fattime();
        lfs_timestamp_attribute_t ts;
        ts.fdate = (WORD)(tm >> 16);
        ts.ftime = (WORD)tm;

        // Until we only have 1 attribute, it is good to write the 0th element
        // This will automatically written out to the flash when close is performed
        memcpy(cfg->attrs[0].buffer, &ts, sizeof(ts));
    }
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

        // make 4-tuple with info about this entry
        mp_obj_tuple_t *t = MP_OBJ_TO_PTR(mp_obj_new_tuple(4, NULL));
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
        t->items[3] = mp_obj_new_int_from_uint(fno.size);

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
        const char *path = concat_with_cwd(&self->fs.littlefs, path_in);
        if (path == NULL) {
            res = LFS_ERR_NOMEM;
        } else {
            res = lfs_dir_open(&self->fs.littlefs.lfs, &iter->dir, path);
        }
    xSemaphoreGive(self->fs.littlefs.mutex);

    free((void*)path);

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
        const char *path = concat_with_cwd(&self->fs.littlefs, path_in);
        if (path == NULL) {
            res = LFS_ERR_NOMEM;
        } else {
            res = lfs_mkdir(&self->fs.littlefs.lfs, path);
            if (res == LFS_ERR_OK) {
                littlefs_update_timestamp(&self->fs.littlefs.lfs, path);
            }
        }
    xSemaphoreGive(self->fs.littlefs.mutex);

    free((void*)path);

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
        const char *path = concat_with_cwd(&self->fs.littlefs, path_in);
        if (path == NULL) {
            res = LFS_ERR_NOMEM;
        } else {
            res = lfs_remove(&self->fs.littlefs.lfs, path);
        }
    xSemaphoreGive(self->fs.littlefs.mutex);

    free((void*)path);

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
        const char *old_path = concat_with_cwd(&self->fs.littlefs, path_in);
        const char *new_path = concat_with_cwd(&self->fs.littlefs, path_out);

        if (old_path == NULL || new_path == NULL) {
            res = LFS_ERR_NOMEM;
        } else {
            res = lfs_rename(&self->fs.littlefs.lfs, old_path, new_path);
        }
    xSemaphoreGive(self->fs.littlefs.mutex);

    free((void*)old_path);
    free((void*)new_path);

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
    lfs_timestamp_attribute_t ts;


    xSemaphoreTake(self->fs.littlefs.mutex, portMAX_DELAY);
        const char *path = concat_with_cwd(&self->fs.littlefs, path_in);
        if (path == NULL) {
            res = LFS_ERR_NOMEM;
        } else if (path[0] == 0 || (path[0] == '/' && path[1] == 0)) {
            // stat root directory
            fno.size = 0;
            fno.type = LFS_TYPE_DIR;
        } else {
            res = littlefs_stat_common_helper(&self->fs.littlefs.lfs, path, &fno, &ts);
        }

    xSemaphoreGive(self->fs.littlefs.mutex);

    free((void*)path);

    if (res < LFS_ERR_OK) {
        mp_raise_OSError(littleFsErrorToErrno(res));
    }

    mp_obj_tuple_t *t = MP_OBJ_TO_PTR(mp_obj_new_tuple(10, NULL));
    mp_int_t mode = 0;
    if (fno.type == LFS_TYPE_DIR) {
        mode |= MP_S_IFDIR;
    } else {
        mode |= MP_S_IFREG;
    }

    mp_int_t seconds = timeutils_seconds_since_2000(
        1980 + ((ts.fdate >> 9) & 0x7f),
        (ts.fdate >> 5) & 0x0f,
        ts.fdate & 0x1f,
        (ts.ftime >> 11) & 0x1f,
        (ts.ftime >> 5) & 0x3f,
        2 * (ts.ftime & 0x1f)
    );

    t->items[0] = MP_OBJ_NEW_SMALL_INT(mode); // st_mode
    t->items[1] = MP_OBJ_NEW_SMALL_INT(0); // st_ino
    t->items[2] = MP_OBJ_NEW_SMALL_INT(0); // st_dev
    t->items[3] = MP_OBJ_NEW_SMALL_INT(0); // st_nlink
    t->items[4] = MP_OBJ_NEW_SMALL_INT(0); // st_uid
    t->items[5] = MP_OBJ_NEW_SMALL_INT(0); // st_gid
    // Size only interpreted on files, not directories
    if(fno.type == LFS_TYPE_REG) {
        t->items[6] = mp_obj_new_int_from_uint(fno.size); // st_size
    }
    else {
        t->items[6] = mp_obj_new_int_from_uint(0); // st_size
    }
    t->items[7] = mp_obj_new_int_from_uint(seconds); // st_atime
    t->items[8] = mp_obj_new_int_from_uint(seconds); // st_mtime
    t->items[9] = mp_obj_new_int_from_uint(seconds); // st_ctime

    return MP_OBJ_FROM_PTR(t);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(littlefs_vfs_stat_obj, littlefs_vfs_stat);

// Get the status of a VFS.
STATIC mp_obj_t littlefs_vfs_statvfs(mp_obj_t vfs_in, mp_obj_t path_in) {

    fs_user_mount_t *self = MP_OBJ_TO_PTR(vfs_in);
    (void)path_in;

    lfs_t* lfs = &self->fs.littlefs.lfs;

    mp_obj_tuple_t *t = MP_OBJ_TO_PTR(mp_obj_new_tuple(10, NULL));

    xSemaphoreTake(self->fs.littlefs.mutex, portMAX_DELAY);
        lfs_ssize_t in_use = lfs_fs_size(lfs);
    xSemaphoreGive(self->fs.littlefs.mutex);

    if (in_use < 0) {
        mp_raise_OSError(littleFsErrorToErrno(in_use));
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

// Get the free space in KByte
STATIC mp_obj_t littlefs_vfs_getfree(mp_obj_t vfs_in) {

    fs_user_mount_t *self = MP_OBJ_TO_PTR(vfs_in);

    lfs_t* lfs = &self->fs.littlefs.lfs;

    xSemaphoreTake(self->fs.littlefs.mutex, portMAX_DELAY);
        lfs_ssize_t in_use = lfs_fs_size(lfs);
    xSemaphoreGive(self->fs.littlefs.mutex);

    if (in_use < 0) {
        mp_raise_OSError(littleFsErrorToErrno(in_use));
    }

    uint32_t free_space = (lfs->cfg->block_count - in_use) * lfs->cfg->block_size;

    return MP_OBJ_NEW_SMALL_INT(free_space / 1024);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(littlefs_vfs_getfree_obj, littlefs_vfs_getfree);

STATIC mp_obj_t littlefs_vfs_umount(mp_obj_t self_in) {
    (void)self_in;
    // keep the LittleFs filesystem mounted internally so the VFS methods can still be used
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(littlefs_vfs_umount_obj, littlefs_vfs_umount);

STATIC mp_obj_t littlefs_vfs_fsformat(mp_obj_t vfs_in)
{
    fs_user_mount_t * vfs = MP_OBJ_TO_PTR(vfs_in);

    lfs_format(&vfs->fs.littlefs.lfs, &lfscfg);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(littlefs_vfs_fsformat_obj, littlefs_vfs_fsformat);

STATIC const mp_rom_map_elem_t littlefs_vfs_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_open),        MP_ROM_PTR(&littlefs_vfs_open_obj) },
    { MP_ROM_QSTR(MP_QSTR_ilistdir),    MP_ROM_PTR(&littlefs_vfs_ilistdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_mkdir),       MP_ROM_PTR(&littlefs_vfs_mkdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_rmdir),       MP_ROM_PTR(&littlefs_vfs_rmdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_chdir),       MP_ROM_PTR(&littlefs_vfs_chdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_getcwd),      MP_ROM_PTR(&littlefs_vfs_getcwd_obj) },
    { MP_ROM_QSTR(MP_QSTR_remove),      MP_ROM_PTR(&littlefs_vfs_remove_obj) },
    { MP_ROM_QSTR(MP_QSTR_rename),      MP_ROM_PTR(&littlefs_vfs_rename_obj) },
    { MP_ROM_QSTR(MP_QSTR_stat),        MP_ROM_PTR(&littlefs_vfs_stat_obj) },
    { MP_ROM_QSTR(MP_QSTR_statvfs),     MP_ROM_PTR(&littlefs_vfs_statvfs_obj) },
    { MP_ROM_QSTR(MP_QSTR_getfree),     MP_ROM_PTR(&littlefs_vfs_getfree_obj) },
    { MP_ROM_QSTR(MP_QSTR_umount),      MP_ROM_PTR(&littlefs_vfs_umount_obj) },
    { MP_ROM_QSTR(MP_QSTR_fsformat),    MP_ROM_PTR(&littlefs_vfs_fsformat_obj) }

};
STATIC MP_DEFINE_CONST_DICT(littlefs_vfs_locals_dict, littlefs_vfs_locals_dict_table);

STATIC const mp_vfs_proto_t lfs_vfs_proto = {
    .import_stat = lfs_vfs_import_stat,
};

const mp_obj_type_t mp_littlefs_vfs_type = {
    { &mp_type_type },
    .protocol = &lfs_vfs_proto,
    .locals_dict = (mp_obj_dict_t*)&littlefs_vfs_locals_dict,
};

