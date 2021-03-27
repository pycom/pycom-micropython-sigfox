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

/*
 * This file is part of the Micro Python project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013, 2014 Damien P. George
 * Copyright (c) 2015 Daniel Campora
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef MODUSOCKET_H_
#define MODUSOCKET_H_

#include "py/stream.h"

#define AF_LORA                             (0xA0)
#define AF_SIGFOX                           (0xA1)

#define SOL_LORA                            (0xFFF05)
#define SOL_SIGFOX                          (0xFFF06)

#define SO_LORAWAN_CONFIRMED                (0xF0002)
#define SO_LORAWAN_DR                       (0xF0003)
#define SO_SIGFOX_RX                        (0xF0004)
#define SO_SIGFOX_TX_REPEAT                 (0xF0005)
#define SO_SIGFOX_OOB                       (0xF0006)
#define SO_SIGFOX_BIT                       (0xF0007)

/* chars for storing an IPv6 address 39 chars + zero end string
* ex: ABCD:ABCD:ABCD:ABCD:ABCD:ABCD:ABCD:ABCD 4*8+7=39 chars */
#define MOD_USOCKET_IPV6_CHARS_MAX                    40

extern const mp_obj_dict_t socket_locals_dict;
extern const mp_stream_p_t socket_stream_p;

extern const mp_obj_type_t socket_type;

extern void modusocket_pre_init (void);
extern void modusocket_socket_add (int32_t sd, bool user);
extern void modusocket_socket_delete (int32_t sd);
extern void modusocket_enter_sleep (void);
extern void modusocket_close_all_user_sockets (void);

#endif /* MODUSOCKET_H_ */
