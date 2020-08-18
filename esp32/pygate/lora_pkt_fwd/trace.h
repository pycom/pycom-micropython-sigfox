/*
 * Copyright (c) 2020, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */
/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
  (C)2013 Semtech-Cycleo

Description:
    LoRa concentrator : Packet Forwarder trace helpers

License: Revised BSD License, see LICENSE.TXT file include in the project
Maintainer: Michael Coracin
*/


#ifndef _LORA_PKTFWD_TRACE_H
#define _LORA_PKTFWD_TRACE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp32_mphal.h"

#include "py/mpprint.h"

// debug levels
#define DEBUG     4
#define INFO_     3
#define WARN_     2
#define ERROR     1

// run time debug level
extern int debug_level;
// compile time debug level
#define DEBUG_LEVEL INFO_

#define MSG_DX(LEVEL, fmt, ...)                                                               \
            do  {                                                                             \
                if (debug_level >= LEVEL)                                                     \
                    mp_printf(&mp_plat_print, "hello world");                                 \
                    mp_printf(&mp_plat_print, "[%u] lorapf: " #LEVEL " " fmt, mp_hal_ticks_ms(), ##__VA_ARGS__); \
            } while (0)

#if DEBUG_LEVEL >= DEBUG
#define MSG_DEBUG(...) MSG_DX(DEBUG, __VA_ARGS__)
#else
#define MSG_DEBUG(...) (void)0
#endif

#if DEBUG_LEVEL >= INFO_
#define MSG_INFO(...) MSG_DX(INFO_, __VA_ARGS__)
#else
#define MSG_INFO(...) (void)0
#endif

#if DEBUG_LEVEL >= WARN_
#define MSG_WARN(...) MSG_DX(WARN_, __VA_ARGS__)
#else
#define MSG_WARN(...) (void)0
#endif

#if DEBUG_LEVEL >= ERROR
#define MSG_ERROR(...) MSG_DX(ERROR, __VA_ARGS__)
#else
#define MSG_ERROR(...) (void)0
#endif


#endif
/* --- EOF ------------------------------------------------------------------ */
