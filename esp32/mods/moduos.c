/*
 * This file is derived from the MicroPython project, http://micropython.org/
 *
 * Copyright (c) 2021, Pycom Limited and its licensors.
 *
 * This software is licensed under the GNU GPL version 3 or any later version,
 * with permitted additional terms. For more information see the Pycom Licence
 * v1.0 document supplied with this file, or available at:
 * https://www.pycom.io/opensource/licensing
 */

/*
 * This file is part of the Micro Python project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013, 2014 Damien P. George
 * Copyright (c) 2015 Daniel Campora
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdint.h>
#include <string.h>

#include "py/mpstate.h"
#include "py/nlr.h"
#include "py/objtuple.h"
#include "py/objstr.h"
#include "py/runtime.h"
#include "timeutils.h"
#include "lib/oofatfs/ff.h"
#include "lib/oofatfs/diskio.h"
#include "genhdr/mpversion.h"
#include "moduos.h"
#include "sflash_diskio.h"
#include "extmod/vfs.h"
#include "extmod/vfs_fat.h"
#include "vfs_littlefs.h"
#include "random.h"
#include "mpexception.h"
#include "pybsd.h"
#include "machuart.h"
#include "pycom_version.h"
#include "mptask.h"

/// \module os - basic "operating system" services
///
/// The `os` module contains functions for filesystem access and `urandom`.
///
/// The filesystem has `/` as the root directory, and the available physical
/// drives are accessible from here.  They are currently:
///
///     /flash      -- the serial flash filesystem
///
/// On boot up, the current directory is `/flash`.

/******************************************************************************/
// MicroPython bindings
//

STATIC const qstr os_uname_info_fields[] = {
    MP_QSTR_sysname, MP_QSTR_nodename,
    MP_QSTR_release, MP_QSTR_version, MP_QSTR_machine
#if defined(LOPY) || defined(LOPY4) || defined(FIPY)
    ,MP_QSTR_lorawan
#endif
#ifdef MOD_SIGFOX_ENABLED
    ,MP_QSTR_sigfox
#endif
#if (VARIANT == PYBYTES)
    ,MP_QSTR_pybytes
#endif
#ifdef PYGATE_ENABLED
    ,MP_QSTR_pygate
#endif

};
STATIC const MP_DEFINE_STR_OBJ(os_uname_info_sysname_obj, MICROPY_PY_SYS_PLATFORM);
STATIC const MP_DEFINE_STR_OBJ(os_uname_info_nodename_obj, MICROPY_PY_SYS_PLATFORM);
STATIC const MP_DEFINE_STR_OBJ(os_uname_info_release_obj, SW_VERSION_NUMBER);
STATIC const MP_DEFINE_STR_OBJ(os_uname_info_version_obj, MICROPY_GIT_TAG " on " MICROPY_BUILD_DATE);
STATIC const MP_DEFINE_STR_OBJ(os_uname_info_machine_obj, MICROPY_HW_BOARD_NAME " with " MICROPY_HW_MCU_NAME);
#if defined(LOPY) || defined(LOPY4) || defined(FIPY)
STATIC const MP_DEFINE_STR_OBJ(os_uname_info_lorawan_obj, LORAWAN_VERSION_NUMBER);
#endif
#ifdef MOD_SIGFOX_ENABLED
STATIC const MP_DEFINE_STR_OBJ(os_uname_info_sigfox_obj, SIGFOX_VERSION_NUMBER);
#endif
#if (VARIANT == PYBYTES)
STATIC const MP_DEFINE_STR_OBJ(os_uname_info_pybytes_obj, PYBYTES_VERSION_NUMBER);
#endif
#ifdef PYGATE_ENABLED
STATIC const MP_DEFINE_STR_OBJ(os_uname_info_pygate_obj, PYGATE_VERSION_NUMBER);
#endif

STATIC MP_DEFINE_ATTRTUPLE(
    os_uname_info_obj
    ,os_uname_info_fields
    , 5
#if defined(LOPY) || defined(LOPY4) || defined(FIPY)
    +1
#endif
#ifdef MOD_SIGFOX_ENABLED
    +1
#endif
#if (VARIANT == PYBYTES)
    +1
#endif
#ifdef PYGATE_ENABLED
    +1
#endif
    ,(mp_obj_t)&os_uname_info_sysname_obj
    ,(mp_obj_t)&os_uname_info_nodename_obj
    ,(mp_obj_t)&os_uname_info_release_obj
    ,(mp_obj_t)&os_uname_info_version_obj
    ,(mp_obj_t)&os_uname_info_machine_obj
#if defined(LOPY) || defined(LOPY4) || defined(FIPY)
    ,(mp_obj_t)&os_uname_info_lorawan_obj
#endif
#ifdef MOD_SIGFOX_ENABLED
    ,(mp_obj_t)&os_uname_info_sigfox_obj
#endif
#if (VARIANT == PYBYTES)
    ,(mp_obj_t)&os_uname_info_pybytes_obj
#endif
#ifdef PYGATE_ENABLED
    ,(mp_obj_t)&os_uname_info_pygate_obj
#endif
);

STATIC mp_obj_t os_uname(void) {
    return (mp_obj_t)&os_uname_info_obj;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(os_uname_obj, os_uname);

STATIC mp_obj_t os_sync(void) {
    for (mp_vfs_mount_t *vfs = MP_STATE_VM(vfs_mount_table); vfs != NULL; vfs = vfs->next) {
        // this assumes that vfs->obj is fs_user_mount_t with block device functions
        disk_ioctl(MP_OBJ_TO_PTR(vfs->obj), CTRL_SYNC, NULL);
    }
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_0(mod_os_sync_obj, os_sync);

STATIC mp_obj_t os_urandom(mp_obj_t num) {
    mp_int_t n = mp_obj_get_int(num);
    vstr_t vstr;
    vstr_init_len(&vstr, n);
    for (int i = 0; i < n; i++) {
        vstr.buf[i] = rng_get();
    }
    return mp_obj_new_str_from_vstr(&mp_type_bytes, &vstr);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(os_urandom_obj, os_urandom);

STATIC mp_obj_t os_dupterm(uint n_args, const mp_obj_t *args) {
    if (n_args == 0) {
        if (MP_STATE_PORT(mp_os_stream_o) == MP_OBJ_NULL) {
            return mp_const_none;
        } else {
            return MP_STATE_PORT(mp_os_stream_o);
        }
    } else {
        mp_obj_t stream_o = args[0];
        if (stream_o == mp_const_none) {
            MP_STATE_PORT(mp_os_stream_o) = MP_OBJ_NULL;
        } else {
            if (!MP_OBJ_IS_TYPE(stream_o, &mach_uart_type)) {
                // must be a stream-like object providing at least read and write methods
                mp_load_method(stream_o, MP_QSTR_read, MP_STATE_PORT(mp_os_read));
                mp_load_method(stream_o, MP_QSTR_write, MP_STATE_PORT(mp_os_write));
            }
            MP_STATE_PORT(mp_os_stream_o) = stream_o;
        }
        return mp_const_none;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(os_dupterm_obj, 0, 1, os_dupterm);

STATIC const mp_rom_map_elem_t os_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),        MP_ROM_QSTR(MP_QSTR_uos) },

    { MP_ROM_QSTR(MP_QSTR_uname),           MP_ROM_PTR(&os_uname_obj) },

    { MP_ROM_QSTR(MP_QSTR_chdir),           MP_ROM_PTR(&mp_vfs_chdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_getcwd),          MP_ROM_PTR(&mp_vfs_getcwd_obj) },
    { MP_ROM_QSTR(MP_QSTR_ilistdir),        MP_ROM_PTR(&mp_vfs_ilistdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_listdir),         MP_ROM_PTR(&mp_vfs_listdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_mkdir),           MP_ROM_PTR(&mp_vfs_mkdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_rename),          MP_ROM_PTR(&mp_vfs_rename_obj) },
    { MP_ROM_QSTR(MP_QSTR_remove),          MP_ROM_PTR(&mp_vfs_remove_obj) },
    { MP_ROM_QSTR(MP_QSTR_rmdir),           MP_ROM_PTR(&mp_vfs_rmdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_stat),            MP_ROM_PTR(&mp_vfs_stat_obj) },
    { MP_ROM_QSTR(MP_QSTR_statvfs),         MP_ROM_PTR(&mp_vfs_statvfs_obj) },
    { MP_ROM_QSTR(MP_QSTR_getfree),         MP_ROM_PTR(&mp_vfs_getfree_obj) },
    { MP_ROM_QSTR(MP_QSTR_fsformat),        MP_ROM_PTR(&mp_vfs_fsformat_obj) },
    { MP_ROM_QSTR(MP_QSTR_unlink),          MP_ROM_PTR(&mp_vfs_remove_obj) },

    { MP_ROM_QSTR(MP_QSTR_sync),            MP_ROM_PTR(&mod_os_sync_obj) },
    { MP_ROM_QSTR(MP_QSTR_urandom),         MP_ROM_PTR(&os_urandom_obj) },

    // MicroPython additions
    // removed: mkfs
    // renamed: unmount -> umount VfsFat -> mkfat
    { MP_ROM_QSTR(MP_QSTR_mount),           MP_ROM_PTR(&mp_vfs_mount_obj) },
    { MP_ROM_QSTR(MP_QSTR_umount),          MP_ROM_PTR(&mp_vfs_umount_obj) },
    { MP_ROM_QSTR(MP_QSTR_mkfat),          MP_ROM_PTR(&mp_fat_vfs_type) },
    { MP_ROM_QSTR(MP_QSTR_dupterm),         MP_ROM_PTR(&os_dupterm_obj) }
};

STATIC MP_DEFINE_CONST_DICT(os_module_globals, os_module_globals_table);

const mp_obj_module_t mp_module_uos = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&os_module_globals,
};
