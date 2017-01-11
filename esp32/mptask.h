/*
 * Copyright (c) 2016, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#ifndef MPTASK_H_
#define MPTASK_H_

#include "mpthreadport.h"

/******************************************************************************
 DEFINE CONSTANTS
 ******************************************************************************/
#define MICROPY_TASK_PRIORITY                   MP_THREAD_PRIORITY
#define MICROPY_TASK_STACK_SIZE                 (8 * 1024)
#define MICROPY_TASK_STACK_LEN                  (MICROPY_TASK_STACK_SIZE / sizeof(StackType_t))

/******************************************************************************
 EXPORTED DATA
 ******************************************************************************/
extern StackType_t *mpTaskStack;

/******************************************************************************
 DECLARE PUBLIC FUNCTIONS
 ******************************************************************************/
extern void TASK_Micropython (void *pvParameters);

#endif /* MPTASK_H_ */
