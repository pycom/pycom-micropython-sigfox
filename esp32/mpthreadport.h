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
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Damien P. George on behalf of Pycom Ltd
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

#ifndef __MICROPY_INCLUDED_ESP32_MPTHREADPORT_H__
#define __MICROPY_INCLUDED_ESP32_MPTHREADPORT_H__

#include "py/mpconfig.h"
#include "py/mpstate.h"
#include "py/obj.h"
#include "freertos/FreeRTOS.h"

#define MP_THREAD_PRIORITY                  5

typedef struct _mp_thread_mutex_t {
    SemaphoreHandle_t handle;
    StaticSemaphore_t buffer;
} mp_thread_mutex_t;

typedef struct _mp_obj_thread_lock_t {
    mp_obj_base_t base;
    mp_thread_mutex_t *mutex;
    volatile bool locked;
} mp_obj_thread_lock_t;

void mp_thread_preinit(void *stack, uint32_t stack_len, uint8_t chip_revision);
void mp_thread_init(void);
void mp_thread_gc_others(void);
void mp_thread_deinit(void);
mp_obj_thread_lock_t *mp_thread_new_thread_lock(void);
void mp_thread_create_ex(void *(*entry)(void*), void *arg, size_t *stack_size, int priority, char *name);

#endif // __MICROPY_INCLUDED_ESP32_MPTHREADPORT_H__
