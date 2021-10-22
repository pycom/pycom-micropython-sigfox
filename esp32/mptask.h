/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#ifndef MPTASK_H_
#define MPTASK_H_

#include "mpthreadport.h"
#include "ff.h"
#include "extmod/vfs_fat.h"

/******************************************************************************
 DEFINE CONSTANTS
 ******************************************************************************/
#define MICROPY_TASK_PRIORITY                   MP_THREAD_PRIORITY
#define MICROPY_TASK_STACK_SIZE                 (8 * 1024)
#define MICROPY_TASK_STACK_SIZE_PSRAM           (12 * 1024)


/******************************************************************************
 DECLARE PUBLIC VARIABLES
 ******************************************************************************/
extern fs_user_mount_t sflash_vfs_flash;

/******************************************************************************
 DECLARE PUBLIC FUNCTIONS
 ******************************************************************************/
extern void TASK_Micropython (void *pvParameters);
extern bool isLittleFs(const TCHAR *path);
extern void mptask_config_wifi(bool force_start);
#endif /* MPTASK_H_ */
