/*
 * Copyright (c) 2016, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#ifndef MODUOS_H_
#define MODUOS_H_

#include "ff.h"

/******************************************************************************
 DEFINE PUBLIC TYPES
 ******************************************************************************/
typedef struct _os_fs_mount_t {
    mp_obj_t device;
    const char *path;
    mp_uint_t pathlen;
    mp_obj_t readblocks[4];
    mp_obj_t writeblocks[4];
    mp_obj_t sync[2];
    mp_obj_t count[2];
    FATFS fatfs;
    uint8_t vol;
} os_fs_mount_t;

typedef struct _os_term_dup_obj_t {
    mp_obj_t stream_o;
    mp_obj_t read[3];
    mp_obj_t write[3];
} os_term_dup_obj_t;

/******************************************************************************
 DECLARE PUBLIC FUNCTIONS
 ******************************************************************************/
void moduos_init0 (void);
os_fs_mount_t *osmount_find_by_path (const char *path);
os_fs_mount_t *osmount_find_by_volume (uint8_t vol);
void osmount_unmount_all (void);

#endif // MODUOS_H_
