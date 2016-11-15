/*
 * This file is derived from the MicroPython project, http://micropython.org/
 *
 * Copyright (c) 2016, Pycom Limited and its licensors.
 *
 * This software is licensed under the GNU GPL version 3 or any later version,
 * with permitted additional terms. For more information see the Pycom Licence
 * v1.0 document supplied with this file, or available at:
 * https://www.pycom.io/opensource/licensing
 */

#ifndef __MICROPY_INCLUDED_ESP32_MPTHREADPORT_H__
#define __MICROPY_INCLUDED_ESP32_MPTHREADPORT_H__

#ifndef BOOTLOADER
#include "freertos/FreeRTOS.h"
#endif

typedef struct _mp_thread_mutex_t {
    #ifndef BOOTLOADER
    SemaphoreHandle_t handle;
    StaticSemaphore_t buffer;
    #endif
} mp_thread_mutex_t;

void mp_thread_init(void);
void mp_thread_gc_others(void);

#endif // __MICROPY_INCLUDED_ESP32_MPTHREADPORT_H__
