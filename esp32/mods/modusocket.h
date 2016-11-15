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

#ifndef MODUSOCKET_H_
#define MODUSOCKET_H_

#define AF_LORA                             (0xA0)

#define PROTO_LORA                          (120)

#define SOL_LORA                            (0xFFF05)

#define SO_LORAWAN_CONFIRMED                (0xF0002)
#define SO_LORAWAN_DR                       (0xF0003)

extern const mp_obj_dict_t socket_locals_dict;
extern const mp_stream_p_t socket_stream_p;

extern void modusocket_pre_init (void);
extern void modusocket_socket_add (int32_t sd, bool user);
extern void modusocket_socket_delete (int32_t sd);
extern void modusocket_enter_sleep (void);
extern void modusocket_close_all_user_sockets (void);

#endif /* MODUSOCKET_H_ */
