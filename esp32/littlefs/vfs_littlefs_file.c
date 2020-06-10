#include "py/mpconfig.h"
//#if MICROPY_VFS && MICROPY_VFS_FAT

#include <stdio.h>

#include "py/runtime.h"
#include "py/stream.h"
#include "py/mperrno.h"
#include "lfs.h"
#include "extmod/vfs.h"
#include "vfs_littlefs.h"

extern const mp_obj_type_t mp_type_vfs_lfs_fileio;
extern const mp_obj_type_t mp_type_vfs_lfs_textio;


typedef struct _pyb_file_obj_t {
    mp_obj_base_t base;
    lfs_file_t fp;
    vfs_lfs_struct_t* littlefs;
    struct lfs_file_config cfg;  // Attributes of the file, e.g.: timestamp
    bool timestamp_update;  // For requesting timestamp update when closing the file
} pyb_file_obj_t;

STATIC void file_obj_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    (void)kind;
    mp_printf(print, "<io.%s %p>", mp_obj_get_type_str(self_in), MP_OBJ_TO_PTR(self_in));
}

STATIC mp_uint_t file_obj_read(mp_obj_t self_in, void *buf, mp_uint_t size, int *errcode) {

    pyb_file_obj_t *self = MP_OBJ_TO_PTR(self_in);

    xSemaphoreTake(self->littlefs->mutex, portMAX_DELAY);
        lfs_ssize_t sz_out = lfs_file_read(&self->littlefs->lfs ,&self->fp, buf, size);
    xSemaphoreGive(self->littlefs->mutex);

    if (sz_out < 0) {
        *errcode = littleFsErrorToErrno(sz_out);
        return MP_STREAM_ERROR;
    }
    return sz_out;
}

STATIC mp_uint_t file_obj_write(mp_obj_t self_in, const void *buf, mp_uint_t size, int *errcode) {

    pyb_file_obj_t *self = MP_OBJ_TO_PTR(self_in);

    xSemaphoreTake(self->littlefs->mutex, portMAX_DELAY);
        lfs_ssize_t sz_out = lfs_file_write(&self->littlefs->lfs, &self->fp, buf, size);
        // Request timestamp update if file has been written successfully
        if(sz_out > 0) {
            self->timestamp_update = true;
        }
    xSemaphoreGive(self->littlefs->mutex);

    if (sz_out < 0) {
        *errcode = littleFsErrorToErrno(sz_out);
        return MP_STREAM_ERROR;
    }
    if (sz_out != size) {
        *errcode = MP_ENOSPC;
        return MP_STREAM_ERROR;
    }
    return sz_out;
}


STATIC mp_obj_t file_obj___exit__(size_t n_args, const mp_obj_t *args) {
    (void)n_args;
    return mp_stream_close(args[0]);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(file_obj___exit___obj, 4, 4, file_obj___exit__);

STATIC mp_uint_t file_obj_ioctl(mp_obj_t o_in, mp_uint_t request, uintptr_t arg, int *errcode) {

    pyb_file_obj_t *self = MP_OBJ_TO_PTR(o_in);

    if (request == MP_STREAM_SEEK) {

        struct mp_stream_seek_t *s = (struct mp_stream_seek_t*)(uintptr_t)arg;

        xSemaphoreTake(self->littlefs->mutex, portMAX_DELAY);
            lfs_file_seek(&self->littlefs->lfs, &self->fp, s->offset, s->whence);
            s->offset = lfs_file_tell(&self->littlefs->lfs, &self->fp);
        xSemaphoreGive(self->littlefs->mutex);

        return 0;

    } else if (request == MP_STREAM_FLUSH) {

        xSemaphoreTake(self->littlefs->mutex, portMAX_DELAY);
            int res = lfs_file_sync(&self->littlefs->lfs, &self->fp);
        xSemaphoreGive(self->littlefs->mutex);

        if (res < 0) {
            *errcode = littleFsErrorToErrno(res);
            return MP_STREAM_ERROR;
        }
        return 0;

    } else if (request == MP_STREAM_CLOSE) {

        xSemaphoreTake(self->littlefs->mutex, portMAX_DELAY);
            int res = littlefs_close_common_helper(&self->littlefs->lfs, &self->fp, &self->cfg, &self->timestamp_update);
        xSemaphoreGive(self->littlefs->mutex);
        if (res < 0) {
            *errcode = littleFsErrorToErrno(res);
            return MP_STREAM_ERROR;
        }
        // Free up the object so GC does not need to do that
        m_del_obj(pyb_file_obj_t, self);

        return 0;
    } else {
        *errcode = MP_EINVAL;
        return MP_STREAM_ERROR;
    }
}

// Note: encoding is ignored for now; it's also not a valid kwarg for CPython's FileIO,
// but by adding it here we can use one single mp_arg_t array for open() and FileIO's constructor
STATIC const mp_arg_t file_open_args[] = {
    { MP_QSTR_file, MP_ARG_OBJ | MP_ARG_REQUIRED, {.u_rom_obj = MP_ROM_PTR(&mp_const_none_obj)} },
    { MP_QSTR_mode, MP_ARG_OBJ, {.u_obj = MP_OBJ_NEW_QSTR(MP_QSTR_r)} },
    { MP_QSTR_encoding, MP_ARG_OBJ | MP_ARG_KW_ONLY, {.u_rom_obj = MP_ROM_PTR(&mp_const_none_obj)} },
};
#define FILE_OPEN_NUM_ARGS MP_ARRAY_SIZE(file_open_args)

STATIC mp_obj_t file_open(fs_user_mount_t *vfs, const mp_obj_type_t *type, mp_arg_val_t *args) {
    int mode = 0;
    const char *mode_s = mp_obj_str_get_str(args[1].u_obj);

    assert(vfs != NULL);

    // TODO make sure only one of r, w, x, a, and b, t are specified
    while (*mode_s) {
        switch (*mode_s++) {
            case 'r':
                mode |= LFS_O_RDONLY;
                break;
            case 'w':
                mode |= LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC;
                break;
            case 'x':
                mode |= LFS_O_WRONLY | LFS_O_CREAT | LFS_O_EXCL;
                break;
            case 'a':
                mode |= LFS_O_WRONLY | LFS_O_CREAT | LFS_O_APPEND;
                break;
            case '+':
                mode |= LFS_O_RDWR;
                break;
            #if MICROPY_PY_IO_FILEIO
            case 'b':
                type = &mp_type_vfs_lfs_fileio;
                break;
            #endif
            case 't':
                type = &mp_type_vfs_lfs_textio;
                break;
        }
    }

    pyb_file_obj_t *o = m_new_obj_with_finaliser(pyb_file_obj_t);
    o->base.type = type;
    o->timestamp_update = false;

    xSemaphoreTake(vfs->fs.littlefs.mutex, portMAX_DELAY);
        const char *fname = concat_with_cwd(&vfs->fs.littlefs, mp_obj_str_get_str(args[0].u_obj));
        int res = littlefs_open_common_helper(&vfs->fs.littlefs.lfs, fname, &o->fp, mode, &o->cfg, &o->timestamp_update);
    xSemaphoreGive(vfs->fs.littlefs.mutex);

    free((void*)fname);
    if (res < LFS_ERR_OK) {
        m_del_obj(pyb_file_obj_t, o);
        mp_raise_OSError(littleFsErrorToErrno(res));
    }

    o->littlefs = &vfs->fs.littlefs;

    return MP_OBJ_FROM_PTR(o);
}

STATIC mp_obj_t file_obj_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_val_t arg_vals[FILE_OPEN_NUM_ARGS];
    mp_arg_parse_all_kw_array(n_args, n_kw, args, FILE_OPEN_NUM_ARGS, file_open_args, arg_vals);
    return file_open(NULL, type, arg_vals);
}

// TODO gc hook to close the file if not already closed

STATIC const mp_rom_map_elem_t rawfile_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_read), MP_ROM_PTR(&mp_stream_read_obj) },
    { MP_ROM_QSTR(MP_QSTR_readinto), MP_ROM_PTR(&mp_stream_readinto_obj) },
    { MP_ROM_QSTR(MP_QSTR_readline), MP_ROM_PTR(&mp_stream_unbuffered_readline_obj) },
    { MP_ROM_QSTR(MP_QSTR_readlines), MP_ROM_PTR(&mp_stream_unbuffered_readlines_obj) },
    { MP_ROM_QSTR(MP_QSTR_write), MP_ROM_PTR(&mp_stream_write_obj) },
    { MP_ROM_QSTR(MP_QSTR_flush), MP_ROM_PTR(&mp_stream_flush_obj) },
    { MP_ROM_QSTR(MP_QSTR_close), MP_ROM_PTR(&mp_stream_close_obj) },
    { MP_ROM_QSTR(MP_QSTR_seek), MP_ROM_PTR(&mp_stream_seek_obj) },
    { MP_ROM_QSTR(MP_QSTR_tell), MP_ROM_PTR(&mp_stream_tell_obj) },
    { MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&mp_stream_close_obj) },
    { MP_ROM_QSTR(MP_QSTR___enter__), MP_ROM_PTR(&mp_identity_obj) },
    { MP_ROM_QSTR(MP_QSTR___exit__), MP_ROM_PTR(&file_obj___exit___obj) },
};

STATIC MP_DEFINE_CONST_DICT(rawfile_locals_dict, rawfile_locals_dict_table);

#if MICROPY_PY_IO_FILEIO
STATIC const mp_stream_p_t fileio_stream_p = {
    .read = file_obj_read,
    .write = file_obj_write,
    .ioctl = file_obj_ioctl,
};

const mp_obj_type_t mp_type_vfs_lfs_fileio = {
    { &mp_type_type },
    .name = MP_QSTR_FileIO,
    .print = file_obj_print,
    .make_new = file_obj_make_new,
    .getiter = mp_identity_getiter,
    .iternext = mp_stream_unbuffered_iter,
    .protocol = &fileio_stream_p,
    .locals_dict = (mp_obj_dict_t*)&rawfile_locals_dict,
};
#endif

STATIC const mp_stream_p_t textio_stream_p = {
    .read = file_obj_read,
    .write = file_obj_write,
    .ioctl = file_obj_ioctl,
    .is_text = true,
};

const mp_obj_type_t mp_type_vfs_lfs_textio = {
    { &mp_type_type },
    .name = MP_QSTR_TextIOWrapper,
    .print = file_obj_print,
    .make_new = file_obj_make_new,
    .getiter = mp_identity_getiter,
    .iternext = mp_stream_unbuffered_iter,
    .protocol = &textio_stream_p,
    .locals_dict = (mp_obj_dict_t*)&rawfile_locals_dict,
};

// Factory function for I/O stream classes
STATIC mp_obj_t littlefs_builtin_open_self(mp_obj_t self_in, mp_obj_t path, mp_obj_t mode) {
    // TODO: analyze buffering args and instantiate appropriate type
    fs_user_mount_t *self = MP_OBJ_TO_PTR(self_in);
    mp_arg_val_t arg_vals[FILE_OPEN_NUM_ARGS];
    arg_vals[0].u_obj = path;
    arg_vals[1].u_obj = mode;
    arg_vals[2].u_obj = mp_const_none;
    return file_open(self, &mp_type_vfs_lfs_textio, arg_vals);
}
MP_DEFINE_CONST_FUN_OBJ_3(littlefs_vfs_open_obj, littlefs_builtin_open_self);

//#endif // MICROPY_VFS && MICROPY_VFS_FAT
