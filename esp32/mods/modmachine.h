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

#ifndef MODMACHINE_H_
#define MODMACHINE_H_

typedef enum
{
    PYGATE_STOPPED = 0,
    PYGATE_STARTED,
    PYGATE_ERROR
}machine_pygate_states_t;

typedef void (*_sig_func_cb_ptr)(int);

extern mp_obj_t NORETURN machine_reset(void);
extern void machine_register_pygate_sig_handler(_sig_func_cb_ptr sig_handler);
extern void machine_pygate_set_status(machine_pygate_states_t status);

#endif
