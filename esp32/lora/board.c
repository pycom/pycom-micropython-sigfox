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

#include "board.h"
#include "random.h"

/*!
 * Flag to indicate if the MCU is Initialized
 */
static bool McuInitialized = false;
static uint8_t BatteryLevel = 255;

void BoardInitPeriph( void )
{

}

void BoardInitMcu( void )
{
    if( McuInitialized == false )
    {
    #if defined(LOPY) || defined (FIPY)
        SpiInit( &SX1272.Spi, RADIO_MOSI, RADIO_MISO, RADIO_SCLK, NC );
        SX1272IoInit( );
    #elif defined(LOPY4)
        SpiInit( &SX1276.Spi, RADIO_MOSI, RADIO_MISO, RADIO_SCLK, NC );
        SX1276IoInit( );
    #endif

        TimerHwInit( );

        McuInitialized = true;
    }
}

void BoardDeInitMcu( void )
{
#if defined(LOPY) || defined (FIPY)
    SpiDeInit( &SX1272.Spi );
    SX1272IoDeInit( );
#elif defined(LOPY4)
    SpiDeInit( &SX1276.Spi );
    SX1276IoDeInit( );
#endif

    McuInitialized = false;
}

uint32_t BoardGetRandomSeed( void )
{
    return rng_get();
}

void BoardGetUniqueId( uint8_t *id )
{
    for (int i = 0; i < 8; i++) {
        id[i] = i;
    }
}

uint8_t BoardGetBatteryLevel( void )
{
    return BatteryLevel;
}

void BoardSetBatteryLevel( uint8_t level )
{
    BatteryLevel = level;
}
