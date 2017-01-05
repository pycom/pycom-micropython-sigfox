/*
 * This file is derived from the MicroPython project, http://micropython.org/
 *
 * Copyright (c) 2016, Pycom Limited and its licensors.
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

#include "lora/system/gpio.h"
#include "lora/system/timer.h"
#include "lora/system/spi.h"
#include "lora/system/delay.h"
#include "radio.h"
#include "sx1272/sx1272.h"
#include "timer-board.h"
#include "sx1272-board.h"
#include "utilities.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#define USE_MODEM_LORA

/*!
 * Board MCU pins definitions
 */

#define RADIO_RESET                                 GPIO18

#define RADIO_MOSI                                  GPIO27
#define RADIO_MISO                                  GPIO19
#define RADIO_SCLK                                  GPIO5
#define RADIO_NSS                                   GPIO17

#define RADIO_DIO                                   GPIO23

void BoardInitPeriph( void );

void BoardInitMcu( void );

void BoardDeInitMcu( void );

uint32_t BoardGetRandomSeed( void );

void BoardGetUniqueId( uint8_t *id );

uint8_t BoardGetBatteryLevel( void );

#endif // LORA_BOARD_H_
