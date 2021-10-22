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
#include "sx1308-config.h"
#include "sx1308-spi.h"
#include "loragw_reg_esp.h"

typedef struct
{
    Spi_sx1308_t Spi;
    pin_obj_t *Reset;
    pin_obj_t *TxOn;
    pin_obj_t *RxOn;
    pin_obj_t *RadioAEn;
    pin_obj_t *RFPowerEn;
    uint8_t waittxend;
    uint8_t txongoing;
    uint32_t offtmstp;
    uint32_t offtmstpref;
    bool firsttx;
} SX1308_t;

extern volatile SX1308_t SX1308;

bool sx1308_init(void);

void sx1308_deinit(void);

void sx1308_dig_reset(void);

void sx1308_spiWrite(uint8_t reg, uint8_t val);

void sx1308_spiWriteBurstF(uint8_t reg, uint8_t * val, int size);

void sx1308_spiWriteBurstM(uint8_t reg, uint8_t * val, int size);

void sx1308_spiWriteBurstE(uint8_t reg, uint8_t * val, int size);

void sx1308_spiWriteBurst(uint8_t reg, uint8_t * val, int size);

uint8_t sx1308_spiRead(uint8_t reg);

uint8_t sx1308_spiReadBurstF(uint8_t reg, uint8_t *data, int size);

uint8_t sx1308_spiReadBurstM(uint8_t reg, uint8_t *data, int size);

uint8_t sx1308_spiReadBurstE(uint8_t reg, uint8_t *data, int size);

uint8_t sx1308_spiReadBurst(uint8_t reg, uint8_t *data, int size);

uint32_t sx1308_timer_read_us(void);
