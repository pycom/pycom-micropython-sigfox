/*
 * Copyright (c) 2021, Pycom Limited.
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
#define LORAPF_DEBUG     4
#define LORAPF_INFO_     3
#define LORAPF_WARN_     2
#define LORAPF_ERROR     1

// run time debug level
extern int debug_level;
// compile time debug level
#define LORAPF_DEBUG_LEVEL LORAPF_INFO_

#define MSG_DX(LEVEL, fmt, ...)                                                               \
            do  {                                                                             \
                if (debug_level >= LEVEL)                                                     \
                    mp_printf(&mp_plat_print, "[%u] " #LEVEL ":" fmt, mp_hal_ticks_ms(), ##__VA_ARGS__); \
            } while (0)

#if LORAPF_DEBUG_LEVEL >= LORAPF_DEBUG
#define MSG_DEBUG(...) MSG_DX(LORAPF_DEBUG, __VA_ARGS__)
#else
#define MSG_DEBUG(...) (void)0
#endif

#if LORAPF_DEBUG_LEVEL >= LORAPF_INFO_
#define MSG_INFO(...) MSG_DX(LORAPF_INFO_, __VA_ARGS__)
#else
#define MSG_INFO(...) (void)0
#endif

#if LORAPF_DEBUG_LEVEL >= LORAPF_WARN_
#define MSG_WARN(...) MSG_DX(LORAPF_WARN_, __VA_ARGS__)
#else
#define MSG_WARN(...) (void)0
#endif

#if LORAPF_DEBUG_LEVEL >= LORAPF_ERROR
#define MSG_ERROR(...) MSG_DX(LORAPF_ERROR, __VA_ARGS__)
#else
#define MSG_ERROR(...) (void)0
#endif


#endif
/* --- EOF ------------------------------------------------------------------ */
