/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#ifndef ESP32_UTIL_PYCOM_GENERAL_UTIL_H_
#define ESP32_UTIL_PYCOM_GENERAL_UTIL_H_

#include "py/obj.h"

char *pycom_util_read_file(const char *file_path, vstr_t *vstr);

#endif /* ESP32_UTIL_PYCOM_GENERAL_UTIL_H_ */
