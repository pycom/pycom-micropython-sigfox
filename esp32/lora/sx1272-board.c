/*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
    (C)2013 Semtech

Description: SX1272 driver specific target board functions implementation

License: Revised BSD License, see LICENSE.TXT file include in the project

Maintainer: Miguel Luis and Gregory Cristian
*/

#include "board.h"
#include "esp_attr.h"

#if defined(LOPY) || defined (FIPY)

#include "radio.h"
#include "sx1272/sx1272.h"
#include "sx1272-board.h"

static uint8_t SX1272GetPaSelect( uint32_t channel )
{
    return RF_PACONFIG_PASELECT_PABOOST;
}

/*!
 * Radio driver structure initialization
 */
DRAM_ATTR const struct Radio_s Radio =
{
    SX1272Init,
    SX1272GetStatus,
    SX1272SetModem,
    SX1272SetChannel,
    SX1272GetChannel,
    SX1272IsChannelFree,
    SX1272Random,
    SX1272SetRxConfig,
    SX1272SetTxConfig,
    SX1272CheckRfFrequency,
    SX1272GetTimeOnAir,
    SX1272Send,
    SX1272SetSleep,
    SX1272SetStby,
    SX1272SetRx,
    SX1272StartCad,
    SX1272SetTxContinuousWave,
    SX1272ReadRssi,
    SX1272Write,
    SX1272Read,
    SX1272WriteBuffer,
    SX1272ReadBuffer,
    SX1272SetMaxPayloadLength,
    SX1272SetPublicNetwork,
    SX1272Reset,
    NULL, // uint32_t ( *GetWakeupTime )( void )
    NULL, // void     ( *IrqProcess )( void )
    NULL, // void     ( *RxBoosted )( uint32_t timeout )
    NULL  // void     ( *SetRxDutyCycle ) ( uint32_t rxTime, uint32_t sleepTime )
};

/*!
 * Antenna switch GPIO pins objects
 */
void SX1272IoInit( void )
{
    GpioInit( &SX1272.Spi.Nss, RADIO_NSS, PIN_OUTPUT, PIN_PUSH_PULL, PIN_PULL_UP, 1 );
    GpioInit( &SX1272.DIO, RADIO_DIO, PIN_INPUT, PIN_PUSH_PULL, PIN_NO_PULL, 0 );
}

void SX1272IoIrqInit( DioIrqHandler **irqHandlers )
{
    GpioSetInterrupt( &SX1272.DIO, IRQ_RISING_EDGE, IRQ_HIGH_PRIORITY, irqHandlers[0] );
}

void SX1272IoDeInit( void )
{
    GpioInit( &SX1272.Spi.Nss, RADIO_NSS, PIN_OUTPUT, PIN_PUSH_PULL, PIN_PULL_UP, 1 );
    GpioInit( &SX1272.DIO, RADIO_DIO, PIN_INPUT, PIN_PUSH_PULL, PIN_NO_PULL, 0 );
}

void SX1272IoTcxoInit( void )
{
    // Not supported
}

void SX1272IoDbgInit( void )
{
    // Not supported
}

void SX1272Reset( void )
{
    if (micropy_lpwan_use_reset_pin) {
        // Set RESET pin to 1
        GpioInit( &SX1272.Reset, RADIO_RESET, PIN_OUTPUT, PIN_PUSH_PULL, PIN_NO_PULL, 1 );

        // Wait 2 ms
        DelayMs( 2 );

        // Configure RESET as input
        GpioInit( &SX1272.Reset, RADIO_RESET, PIN_INPUT, PIN_PUSH_PULL, PIN_NO_PULL, 1 );

        // Wait 6 ms
        DelayMs( 6 );
    } else {
        DelayMs( 2 );
    }
}

void SX1272SetRfTxPower( int8_t power )
{
    uint8_t paConfig = 0;
    uint8_t paDac = 0;

    paConfig = SX1272Read( REG_PACONFIG );
    paDac = SX1272Read( REG_PADAC );

    paConfig = ( paConfig & RF_PACONFIG_PASELECT_MASK ) | SX1272GetPaSelect( SX1272.Settings.Channel );

    if( ( paConfig & RF_PACONFIG_PASELECT_PABOOST ) == RF_PACONFIG_PASELECT_PABOOST )
    {
        if( power > 17 )
        {
            paDac = ( paDac & RF_PADAC_20DBM_MASK ) | RF_PADAC_20DBM_ON;
        }
        else
        {
            paDac = ( paDac & RF_PADAC_20DBM_MASK ) | RF_PADAC_20DBM_OFF;
        }
        if( ( paDac & RF_PADAC_20DBM_ON ) == RF_PADAC_20DBM_ON )
        {
            if( power < 5 )
            {
                power = 5;
            }
            if( power > 20 )
            {
                power = 20;
            }
            paConfig = ( paConfig & RFLR_PACONFIG_OUTPUTPOWER_MASK ) | ( uint8_t )( ( uint16_t )( power - 5 ) & 0x0F );
        }
        else
        {
            if( power < 2 )
            {
                power = 2;
            }
            if( power > 17 )
            {
                power = 17;
            }
            paConfig = ( paConfig & RFLR_PACONFIG_OUTPUTPOWER_MASK ) | ( uint8_t )( ( uint16_t )( power - 2 ) & 0x0F );
        }
    }
    else
    {
        if( power < -1 )
        {
            power = -1;
        }
        if( power > 14 )
        {
            power = 14;
        }
        paConfig = ( paConfig & RFLR_PACONFIG_OUTPUTPOWER_MASK ) | ( uint8_t )( ( uint16_t )( power + 1 ) & 0x0F );
    }
    SX1272Write( REG_PACONFIG, paConfig );
    SX1272Write( REG_PADAC, paDac );
}

void SX1272SetAntSwLowPower( bool status )
{
    // No antenna switch available.
}

void SX1272AntSwInit( void )
{
    // No antenna switch available.
}

void SX1272AntSwDeInit( void )
{
    // No antenna switch available.
}

void SX1272SetAntSw( uint8_t opMode )
{
    // No antenna switch available.
}

bool SX1272CheckRfFrequency( uint32_t frequency )
{
    // Implement check. Currently all frequencies are supportted
    return true;
}

void SX1272SetBoardTcxo( uint8_t state )
{
    // Not supported
}

uint32_t SX1272GetBoardTcxoWakeupTime( void )
{
    // Returning a random value for now.
    return 5;
}

void SX1272DbgPinTxWrite( uint8_t state )
{
    // Not supported
}

void SX1272DbgPinRxWrite( uint8_t state )
{
    // Not supported
}

#endif
