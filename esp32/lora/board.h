/*
 * This file is derived from the MicroPython project, http://micropython.org/
 *
 * Copyright (c) 2021, Pycom Limited and its licensors.
 *
 * This software is licensed under the GNU GPL version 3 or any later version,
 * with permitted additional terms. For more information see the Pycom Licence
 * v1.0 document supplied with this file, or available at:
 * https://www.pycom.io/opensource/licensing
 *
 * This file contains code under the following copyright and licensing notices.
 * The code has been changed but otherwise retained.
 */

/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
    (C)2013 Semtech

Description: Target board general functions implementation

License: Revised BSD License, see LICENSE.TXT file include in the project

Maintainer: Miguel Luis and Gregory Cristian
*/

#ifndef LORA_BOARD_H_
#define LORA_BOARD_H_

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "py/mpconfig.h"

#include "lora/system/gpio.h"
#include "lora/system/timer.h"
#include "lora/system/spi.h"
#include "lora/system/delay.h"
#include "radio.h"
#if defined(LOPY) || defined (FIPY)
#include "sx1272/sx1272.h"
#include "sx1272-board.h"
#elif defined(LOPY4)
#include "sx1276/sx1276.h"
#include "sx1276-board.h"
#endif
#include "timer-board.h"
#include "utilities.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#define USE_MODEM_LORA

/*!
 * Board MCU pins definitions
 */
#if defined (LOPY) || defined (LOPY4) || defined (FIPY)
#define RADIO_RESET                                 micropy_lpwan_reset_pin_index

#define RADIO_MOSI                                  GPIO27
#define RADIO_MISO                                  GPIO19
#define RADIO_SCLK                                  GPIO5
#define RADIO_NSS                                   micropy_lpwan_ncs_pin_index

#define RADIO_DIO                                   micropy_lpwan_dio_pin_index
#endif

void BoardInitPeriph( void );

void BoardInitMcu( void );

void BoardDeInitMcu( void );

uint32_t BoardGetRandomSeed( void );

void BoardGetUniqueId( uint8_t *id );

uint8_t BoardGetBatteryLevel( void );

void BoardSetBatteryLevel( uint8_t level );

#endif // LORA_BOARD_H_
