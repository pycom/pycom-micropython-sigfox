/*
 * Copyright (c) 2021, Pycom Limited.
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

/******************************************************************************
DECLARE EXPORTED DATA
 ******************************************************************************/
const char mpexception_os_resource_not_avaliable[]  = "resource not available";
const char mpexception_os_operation_failed[]        = "the requested operation failed";
const char mpexception_os_request_not_possible[]    = "the requested operation is not possible";
const char mpexception_value_invalid_arguments[]    = "invalid argument(s) value";
const char mpexception_num_type_invalid_arguments[] = "invalid argument(s) num/type";
