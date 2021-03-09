/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */
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

Description: Bleeper STM32L151RD microcontroller pins definition

License: Revised BSD License, see LICENSE.TXT file include in the project

Maintainer: Miguel Luis and Gregory Cristian
*/

#include "py/mpstate.h"
#include "py/mpconfig.h"
#include "py/runtime.h"
#include "machpin.h"


struct Spi_sx1308_s
{
    uint32_t Spi;
    pin_obj_t *Mosi;
    pin_obj_t *Miso;
    pin_obj_t *Sclk;
    pin_obj_t *Nss;
};

typedef struct Spi_sx1308_s Spi_sx1308_t;


extern void sx1308_SpiInit( Spi_sx1308_t *obj);

extern uint16_t sx1308_SpiInOut(Spi_sx1308_t *obj, uint16_t outData);
