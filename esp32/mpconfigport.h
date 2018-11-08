/*
 * This file is derived from the MicroPython project, http://micropython.org/
 *
 * Copyright (c) 2018, Pycom Limited and its licensors.
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

#ifndef __INCLUDED_MPCONFIGPORT_H
#define __INCLUDED_MPCONFIGPORT_H

#include <stdint.h>

// options to control how Micro Python is built
#define MICROPY_OBJ_REPR                            (MICROPY_OBJ_REPR_A)
#define MICROPY_ALLOC_PATH_MAX                      (128)
#define MICROPY_EMIT_X64                            (0)
#define MICROPY_EMIT_THUMB                          (0)
#define MICROPY_EMIT_INLINE_THUMB                   (0)
#define MICROPY_MEM_STATS                           (0)
#define MICROPY_DEBUG_PRINTERS                      (1)
#define MICROPY_ENABLE_GC                           (1)
#define MICROPY_STACK_CHECK                         (1)
#define MICROPY_HELPER_REPL                         (1)
#define MICROPY_HELPER_LEXER_UNIX                   (0)
#define MICROPY_ENABLE_SOURCE_LINE                  (1)
#define MICROPY_MODULE_WEAK_LINKS                   (1)
#define MICROPY_CAN_OVERRIDE_BUILTINS               (1)
#define MICROPY_PY_BUILTINS_COMPLEX                 (1)
#define MICROPY_PY_BUILTINS_STR_UNICODE             (1)
#define MICROPY_PY_BUILTINS_BYTEARRAY               (1)
#define MICROPY_PY_BUILTINS_MEMORYVIEW              (1)
#define MICROPY_PY_BUILTINS_FROZENSET               (1)
#define MICROPY_PY_BUILTINS_SET                     (1)
#define MICROPY_PY_BUILTINS_SLICE                   (1)
#define MICROPY_PY_BUILTINS_PROPERTY                (1)
#define MICROPY_PY_BUILTINS_EXECFILE                (1)
#define MICROPY_PY_WEBSOCKET                        (1)
#define MICROPY_PY___FILE__                         (1)
#define MICROPY_PY_GC                               (1)
#define MICROPY_PY_ARRAY                            (1)
#define MICROPY_PY_ARRAY_SLICE_ASSIGN               (1)
#define MICROPY_PY_COLLECTIONS                      (1)
#define MICROPY_PY_COLLECTIONS_DEQUE                (1)
#define MICROPY_PY_MATH                             (1)
#define MICROPY_PY_CMATH                            (1)
#define MICROPY_PY_IO                               (1)
#define MICROPY_PY_IO_FILEIO                        (1)
#define MICROPY_PY_STRUCT                           (1)
#define MICROPY_PY_SYS                              (1)
#define MICROPY_PY_THREAD                           (1)
#define MICROPY_PY_THREAD_GIL                       (1)
#define MICROPY_PY_THREAD_GIL_VM_DIVISOR            (8)
#define MICROPY_PY_SYS_MAXSIZE                      (1)
#define MICROPY_PY_SYS_EXIT                         (1)
#define MICROPY_PY_SYS_STDFILES                     (1)
#define MICROPY_PY_UBINASCII                        (1)
#define MICROPY_PY_UERRNO                           (1)
#define MICROPY_PY_UCTYPES                          (1)
#define MICROPY_PY_UHASHLIB                         (0)
#define MICROPY_PY_UHASHLIB_SHA1                    (0)
#define MICROPY_PY_UJSON                            (1)
#define MICROPY_PY_URE                              (1)
#define MICROPY_PY_USELECT                          (1)
#define MICROPY_PY_MACHINE                          (1)
#define MICROPY_PY_MICROPYTHON_MEM_INFO             (1)
#define MICROPY_PY_UTIMEQ                           (1)
#define MICROPY_CPYTHON_COMPAT                      (1)
#define MICROPY_LONGINT_IMPL                        (MICROPY_LONGINT_IMPL_MPZ)
#define MICROPY_FLOAT_IMPL                          (MICROPY_FLOAT_IMPL_FLOAT)
#define MICROPY_ERROR_REPORTING                     (MICROPY_ERROR_REPORTING_NORMAL)
#define MICROPY_OPT_COMPUTED_GOTO                   (1)
#define MICROPY_OPT_CACHE_MAP_LOOKUP_IN_BYTECODE    (0)
#define MICROPY_REPL_AUTO_INDENT                    (1)
#define MICROPY_COMP_MODULE_CONST                   (1)
#define MICROPY_ENABLE_FINALISER                    (1)
#define MICROPY_COMP_TRIPLE_TUPLE_ASSIGN            (1)
#define MICROPY_USE_INTERNAL_PRINTF                 (0)
#define MICROPY_PY_SYS_EXC_INFO                     (1)
#define MICROPY_MODULE_FROZEN_STR                   (0)
#define MICROPY_MODULE_FROZEN_MPY                   (1)
#define MICROPY_PERSISTENT_CODE_LOAD                (1)
#define MICROPY_QSTR_EXTRA_POOL                     mp_qstr_frozen_const_pool
#define MICROPY_PY_FRAMEBUF                         (1)
#define MICROPY_PY_UZLIB                            (1)

#define MICROPY_STREAMS_NON_BLOCK                   (1)
#define MICROPY_PY_BUILTINS_TIMEOUTERROR            (1)
#define MICROPY_PY_ALL_SPECIAL_METHODS              (1)

#define MICROPY_ENABLE_EMERGENCY_EXCEPTION_BUF      (1)
#define MICROPY_EMERGENCY_EXCEPTION_BUF_SIZE        (0)
#define MICROPY_KBD_EXCEPTION                       (1)

#ifndef BOOTLOADER_BUILD
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#endif

// fatfs configuration used in ffconf.h
#define MICROPY_FATFS_ENABLE_LFN                    (2)
#define MICROPY_FATFS_MAX_LFN                       (MICROPY_ALLOC_PATH_MAX)
#define MICROPY_FATFS_LFN_CODE_PAGE                 (437) // 1=SFN/ANSI 437=LFN/U.S.(OEM)
#define MICROPY_FATFS_RPATH                         (2)
#define MICROPY_FATFS_REENTRANT                     (1)
#define MICROPY_FATFS_TIMEOUT                       (5000)
#define MICROPY_FATFS_SYNC_T                        SemaphoreHandle_t

#define MICROPY_VFS                                 (1)
#define MICROPY_VFS_FAT                             (1)

#define MICROPY_READER_VFS                          (1)
#define MICROPY_PY_BUILTINS_INPUT                   (1)

// TODO these should be generic, not bound to fatfs
#define mp_type_fileio fatfs_type_fileio
#define mp_type_textio fatfs_type_textio

// use vfs's functions for import stat and builtin open
#define mp_import_stat mp_vfs_import_stat
#define mp_builtin_open mp_vfs_open
#define mp_builtin_open_obj mp_vfs_open_obj


// type definitions for the specific machine
#define BYTES_PER_WORD                              (4)
#define MICROPY_MAKE_POINTER_CALLABLE(p)            ((void*)((mp_uint_t)(p)))
#define MP_SSIZE_MAX                                (0x7FFFFFFF)
#define UINT_FMT                                    "%u"
#define INT_FMT                                     "%d"

typedef int32_t mp_int_t;                           // must be pointer size
typedef uint32_t mp_uint_t;                         // must be pointer size
typedef void *machine_ptr_t;                        // must be of pointer size
typedef const void *machine_const_ptr_t;            // must be of pointer size
typedef long mp_off_t;

#define MP_PLAT_PRINT_STRN(str, len)                mp_hal_stdout_tx_strn_cooked(str, len)

// extra built in names to add to the global namespace
#define MICROPY_PORT_BUILTINS \
    { MP_OBJ_NEW_QSTR(MP_QSTR_help),  (mp_obj_t)&mp_builtin_help_obj },   \
    { MP_OBJ_NEW_QSTR(MP_QSTR_input), (mp_obj_t)&mp_builtin_input_obj },  \
    { MP_OBJ_NEW_QSTR(MP_QSTR_open),  (mp_obj_t)&mp_builtin_open_obj },

// extra built in modules to add to the list of known ones
extern const struct _mp_obj_module_t machine_module;
extern const struct _mp_obj_module_t mp_module_uos;
extern const struct _mp_obj_module_t mp_module_network;
extern const struct _mp_obj_module_t mp_module_usocket;
extern const struct _mp_obj_module_t mp_module_ure;
extern const struct _mp_obj_module_t mp_module_ujson;
extern const struct _mp_obj_module_t mp_module_uselect;
extern const struct _mp_obj_module_t utime_module;
extern const struct _mp_obj_module_t pycom_module;
extern const struct _mp_obj_module_t mp_module_uhashlib;
extern const struct _mp_obj_module_t module_ucrypto;
extern const struct _mp_obj_module_t mp_module_ussl;
extern const struct _mp_obj_module_t mp_module_uqueue;

#define MICROPY_PORT_BUILTIN_MODULES \
    { MP_OBJ_NEW_QSTR(MP_QSTR_umachine),        (mp_obj_t)&machine_module },      \
    { MP_OBJ_NEW_QSTR(MP_QSTR_uos),             (mp_obj_t)&mp_module_uos },       \
    { MP_OBJ_NEW_QSTR(MP_QSTR_network),         (mp_obj_t)&mp_module_network },   \
    { MP_OBJ_NEW_QSTR(MP_QSTR_usocket),         (mp_obj_t)&mp_module_usocket },   \
    { MP_OBJ_NEW_QSTR(MP_QSTR_uselect),         (mp_obj_t)&mp_module_uselect },   \
    { MP_OBJ_NEW_QSTR(MP_QSTR_utime),           (mp_obj_t)&utime_module },        \
    { MP_OBJ_NEW_QSTR(MP_QSTR_pycom),           (mp_obj_t)&pycom_module },        \
    { MP_OBJ_NEW_QSTR(MP_QSTR_uhashlib),        (mp_obj_t)&mp_module_uhashlib },  \
    { MP_OBJ_NEW_QSTR(MP_QSTR_ucrypto),         (mp_obj_t)&module_ucrypto },      \
    { MP_OBJ_NEW_QSTR(MP_QSTR_ubinascii),       (mp_obj_t)&mp_module_ubinascii }, \
    { MP_OBJ_NEW_QSTR(MP_QSTR_ussl),            (mp_obj_t)&mp_module_ussl },      \
    { MP_OBJ_NEW_QSTR(MP_QSTR_uerrno),          (mp_obj_t)&mp_module_uerrno },    \
    { MP_OBJ_NEW_QSTR(MP_QSTR_uqueue),          (mp_obj_t)&mp_module_uqueue },    \

#define MICROPY_PORT_BUILTIN_MODULE_WEAK_LINKS \
    { MP_OBJ_NEW_QSTR(MP_QSTR_machine),         (mp_obj_t)&machine_module },      \
    { MP_OBJ_NEW_QSTR(MP_QSTR_os),              (mp_obj_t)&mp_module_uos },       \
    { MP_OBJ_NEW_QSTR(MP_QSTR_socket),          (mp_obj_t)&mp_module_usocket },   \
    { MP_OBJ_NEW_QSTR(MP_QSTR_select),          (mp_obj_t)&mp_module_uselect },   \
    { MP_OBJ_NEW_QSTR(MP_QSTR_binascii),        (mp_obj_t)&mp_module_ubinascii }, \
    { MP_OBJ_NEW_QSTR(MP_QSTR_struct),          (mp_obj_t)&mp_module_ustruct },   \
    { MP_OBJ_NEW_QSTR(MP_QSTR_re),              (mp_obj_t)&mp_module_ure },       \
    { MP_OBJ_NEW_QSTR(MP_QSTR_json),            (mp_obj_t)&mp_module_ujson },     \
    { MP_OBJ_NEW_QSTR(MP_QSTR_time),            (mp_obj_t)&utime_module },        \
    { MP_OBJ_NEW_QSTR(MP_QSTR_hashlib),         (mp_obj_t)&mp_module_uhashlib },  \
    { MP_OBJ_NEW_QSTR(MP_QSTR_crypto),          (mp_obj_t)&module_ucrypto },      \
    { MP_OBJ_NEW_QSTR(MP_QSTR_ssl),             (mp_obj_t)&mp_module_ussl },      \
    { MP_OBJ_NEW_QSTR(MP_QSTR_errno),           (mp_obj_t)&mp_module_uerrno },    \
    { MP_OBJ_NEW_QSTR(MP_QSTR_queue),           (mp_obj_t)&mp_module_uqueue },    \

// extra constants
#define MICROPY_PORT_CONSTANTS \
    { MP_OBJ_NEW_QSTR(MP_QSTR_umachine),        (mp_obj_t)&machine_module },      \

#define MP_STATE_PORT                               MP_STATE_VM

#include "xtensa/xtruntime.h"                       // for the critical section routines

#define MICROPY_BEGIN_ATOMIC_SECTION()              portENTER_CRITICAL_NESTED()
#define MICROPY_END_ATOMIC_SECTION(state)           portEXIT_CRITICAL_NESTED(state)

#define MICROPY_EVENT_POLL_HOOK                     mp_hal_delay_ms(1);

#define MICROPY_PORT_ROOT_POINTERS \
    const char *readline_hist[8];                               \
    mp_obj_t machine_config_main;                               \
    mp_obj_t uart_buf[3];                                       \
    mp_obj_list_t mp_irq_obj_list;                              \
    mp_obj_list_t mod_network_nic_list;                         \
    mp_obj_t mp_os_stream_o;                                    \
    mp_obj_t mp_os_read[3];                                     \
    mp_obj_t mp_os_write[3];                                    \
    mp_obj_t mp_alarm_heap;                                     \
    mp_obj_t mach_pwm_timer_obj[4];                             \
    mp_obj_list_t btc_conn_list;                                \
    mp_obj_list_t bts_srv_list;                                 \
    mp_obj_list_t bts_attr_list;                                \
    char* lfs_cwd;                                              \

// we need to provide a declaration/definition of alloca()
#include <alloca.h>

// board specifics
#define MICROPY_MPHALPORT_H                                     "esp32_mphal.h"
#define MICROPY_HW_MCU_NAME                                     "ESP32"
#define MICROPY_PORT_SFLASH_BLOCK_COUNT_4MB                     127
#define MICROPY_PORT_SFLASH_BLOCK_COUNT_8MB                     1024

#define DEFAULT_AP_PASSWORD                                     "www.pycom.io"
#define DEFAULT_AP_CHANNEL                                      (6)

#define MICROPY_FIRST_GEN_ANT_SELECT_PIN_NUM                    (16)
#define MICROPY_SECOND_GEN_ANT_SELECT_PIN_NUM                   (21)

#define _assert(expr)   ((expr) ? (void)0 : __assert_func(__FILE__, __LINE__, __func__, #expr))

#include "mpconfigboard.h"

#endif // __INCLUDED_MPCONFIGPORT_H
