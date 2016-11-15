/*
 * Copyright (c) 2016, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include <stdint.h>
#include <string.h>

#include "py/mpstate.h"
#include "mpexception.h"


/******************************************************************************
DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/
STATIC void mpexception_set_user_interrupt (int chr, void *data);

/******************************************************************************
DECLARE EXPORTED DATA
 ******************************************************************************/
const char mpexception_os_resource_not_avaliable[]  = "resource not available";
const char mpexception_os_operation_failed[]        = "the requested operation failed";
const char mpexception_os_request_not_possible[]    = "the requested operation is not possible";
const char mpexception_value_invalid_arguments[]    = "invalid argument(s) value";
const char mpexception_num_type_invalid_arguments[] = "invalid argument(s) num/type";
const char mpexception_uncaught[]                   = "uncaught exception";

int   user_interrupt_char = -1;

/******************************************************************************
DECLARE PRIVATE DATA
 ******************************************************************************/
STATIC void *user_interrupt_data = NULL;

/******************************************************************************
DEFINE PUBLIC FUNCTIONS
 ******************************************************************************/

void mpexception_init0 (void) {
    // Create an exception object for interrupting through the stdin uart
    MP_STATE_PORT(mp_kbd_exception) = mp_obj_new_exception(&mp_type_KeyboardInterrupt);
    mpexception_set_user_interrupt (-1, MP_STATE_PORT(mp_kbd_exception));
}

void mpexception_set_interrupt_char (int c) {
    if (c != -1) {
        mp_obj_exception_clear_traceback(MP_STATE_PORT(mp_kbd_exception));
    }
    mpexception_set_user_interrupt(c, MP_STATE_PORT(mp_kbd_exception));
}

// Call this function to raise a pending exception during an interrupt.
// It will try to raise the exception "softly" by setting the
// mp_pending_exception variable hoping that the VM will notice it.
void mpexception_nlr_jump (void *o) {
    if (MP_STATE_PORT(mp_pending_exception) == MP_OBJ_NULL) {
        MP_STATE_PORT(mp_pending_exception) = o;
    }
}

void mpexception_keyboard_nlr_jump (void) {
    mpexception_nlr_jump (user_interrupt_data);
}

/******************************************************************************
DEFINE PRIVATE FUNCTIONS
 ******************************************************************************/

STATIC void mpexception_set_user_interrupt (int chr, void *data) {
    user_interrupt_char = chr;
    user_interrupt_data = data;
}
