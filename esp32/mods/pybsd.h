/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#ifndef PYBSD_H_
#define PYBSD_H_

/******************************************************************************
 DEFINE PUBLIC TYPES
 ******************************************************************************/
typedef struct {
    mp_obj_base_t       base;
    bool                enabled;
} pybsd_obj_t;

/******************************************************************************
 DECLARE EXPORTED DATA
 ******************************************************************************/
extern pybsd_obj_t pybsd_obj;
extern const mp_obj_type_t pyb_sd_type;

#endif // PYBSD_H_
