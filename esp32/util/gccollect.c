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

#include <stdio.h>

#include "py/mpconfig.h"
#include "py/mpstate.h"
#include "py/gc.h"
#include "py/mpthread.h"
#include "gccollect.h"
#include "soc/cpu.h"
#include "xtensa/hal.h"

/******************************************************************************
DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/
static void gc_collect_inner(int level) {
    if (level < XCHAL_NUM_AREGS / 8) {
        gc_collect_inner(level + 1);
        if (level != 0) {
            return;
        }
    }

    if (level == XCHAL_NUM_AREGS / 8) {
        // get the sp
        volatile uint32_t sp = (uint32_t)get_sp();
        gc_collect_root((void**)sp, ((mp_uint_t)MP_STATE_THREAD(stack_top) - sp) / sizeof(uint32_t));
        return;
    }

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
    gc_collect_inner(0);
    gc_collect_end();
}
