/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#ifndef LORA_MESH_TASK_H_
#define LORA_MESH_TASK_H_

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "py/mpconfig.h"
#include "py/mpstate.h"
#include "py/obj.h"
#include "py/nlr.h"
#include "py/runtime.h"
#include "py/mphal.h"
#include "py/stream.h"
#include "py/mperrno.h"
#include "py/misc.h"

#include <openthread/instance.h>
#include <openthread/thread.h>
#include <modnetwork.h>

/******************************************************************************
 PUBLIC VARS
 ******************************************************************************/

extern const mp_obj_type_t lora_mesh_type;

/******************************************************************************
 PUBLIC FUNCTIONS
 ******************************************************************************/

extern bool lora_mesh_ready(void);

/******************************************************************************
 * socket functions used in modlora.c
 */

extern int mesh_socket_open(mod_network_socket_obj_t *s, int *_errno);

extern void mesh_socket_close(mod_network_socket_obj_t *s);

extern int mesh_socket_recvfrom(mod_network_socket_obj_t *s, byte *buf, mp_uint_t len, byte *ip,
        mp_uint_t *port, int *_errno);

extern int mesh_socket_bind(mod_network_socket_obj_t *s, byte *ip, mp_uint_t port, int *_errno);

extern int mesh_socket_sendto(mod_network_socket_obj_t *s, const byte *buf, mp_uint_t len, byte *ip,
        mp_uint_t port, int *_errno);
/******************************************************************************/

#endif /* LORA_MESH_TASK_H_ */
