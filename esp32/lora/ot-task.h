/*
 * Copyright (c) 2018, Pycom Limited.
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
#include "py/misc.h"

#include <openthread/instance.h>

extern void openthread_task_init(void);

extern otInstance* openthread_init(void);

extern int mesh_socket_open(int *_errno);

extern void mesh_socket_close(void);

extern int mesh_socket_recvfrom(byte *buf, mp_uint_t len, byte *ip,
        mp_uint_t *port, int *_errno);

extern int mesh_socket_bind(byte *ip, mp_uint_t port, int *_errno);

extern int mesh_socket_sendto(const byte *buf, mp_uint_t len, byte *ip,
        mp_uint_t port, int *_errno);

#endif /* LORA_MESH_TASK_H_ */
