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

#include <stdio.h>

#include "py/mpconfig.h"
#include "py/mpstate.h"
#include "py/gc.h"
#include "py/mpthread.h"
#include "gccollect.h"

/******************************************************************************
DECLARE PRIVATE DATA
 ******************************************************************************/

/******************************************************************************
DECLARE PUBLIC FUNCTIONS
 ******************************************************************************/
static void gc_collect_inner(void) {
    uint32_t state = MICROPY_BEGIN_ATOMIC_SECTION();
    xthal_window_spill();
    MICROPY_END_ATOMIC_SECTION(state);

    volatile uint32_t sp;
    asm volatile("or %0, a1, a1" : "=r"(sp));
    gc_collect_root((void**)sp, ((mp_uint_t)MP_STATE_THREAD(stack_top) - sp) / sizeof(uint32_t));

    // trace root pointers from any threads
    #if MICROPY_PY_THREAD
    mp_thread_gc_others();
    #endif
}

void gc_collect(void) {
    gc_collect_start();
    gc_collect_inner();
    gc_collect_end();
}
