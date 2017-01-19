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
#include "soc/cpu.h"

/******************************************************************************
DECLARE PRIVATE DATA
 ******************************************************************************/

/******************************************************************************
DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/
uint32_t gc_helper_get_regs_and_sp(mp_uint_t *regs) {
    // FIXME
    return (mp_uint_t)regs;
}

static void gc_collect_inner(void) {
    // get the registers and the sp
    mp_uint_t regs[8];
    mp_uint_t sp = gc_helper_get_regs_and_sp(regs);

    gc_collect_root((void**)sp, ((mp_uint_t)MP_STATE_THREAD(stack_top) - sp) / sizeof(uint32_t));

    // trace root pointers from any threads
    #if MICROPY_PY_THREAD
    mp_thread_gc_others();
    #endif
}

/******************************************************************************
DECLARE PUBLIC FUNCTIONS
 ******************************************************************************/
void gc_collect(void) {
    gc_collect_start();
    gc_collect_inner();
    gc_collect_end();
}
