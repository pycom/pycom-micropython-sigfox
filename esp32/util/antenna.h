/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#ifndef _ANTENNA_H_
#define _ANTENNA_H_

typedef enum {
    ANTENNA_TYPE_INTERNAL = 0,
    ANTENNA_TYPE_EXTERNAL,
    ANTENNA_TYPE_MANUAL,
} antenna_type_t;

extern void antenna_init0 (void);
extern void antenna_select (antenna_type_t antenna_type);
extern void antenna_validate_antenna (uint8_t antenna);

#endif /* _ANTENNA_H_ */
