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
  (C)2017 Semtech
*/

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "py/mpconfig.h"
#include "py/obj.h"
#include "py/runtime.h"
#include "esp32_mphal.h"

#include "esp_heap_caps.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"
#include "esp_attr.h"

#include "gpio.h"
#include "spi.h"

// #include "loragw_hal.h"
#include "esp_attr.h"
#include "machpin.h"
#include "pins.h"
#include "sx1308.h"
#include "machtimer.h"


static IRAM_ATTR void GpioWrite( pin_obj_t *pin_obj, uint32_t value );
static IRAM_ATTR void SX1308_Tx_Off_Isr(void);

volatile SX1308_t SX1308;

static uint64_t timer_start_value = 0;

void sx1308_deinit(void)
{
    // Disable power to RF
    // Enable power to all RF systems
    GpioWrite( SX1308.RadioAEn, 0 );
    GpioWrite( SX1308.RFPowerEn, 0 );
    mp_hal_delay_ms(50);
}

bool sx1308_init(void) {
    // Init the SX1308 structure
    SX1308.Spi.Miso = SX1308_MISO_PIN;
    SX1308.Spi.Mosi = SX1308_MOSI_PIN;
    SX1308.Spi.Sclk = SX1308_SCLK_PIN;
    SX1308.Spi.Nss = SX1308_NSS_PIN;
    SX1308.Spi.Spi = SX1308_SPI_NUM;
    sx1308_SpiInit((Spi_sx1308_t *)&SX1308.Spi);
    SX1308.Reset = SX1308_RST_PIN;
    SX1308.TxOn = SX1308_TX_ON_PIN;
    SX1308.RxOn = SX1308_RX_ON_PIN;
    SX1308.RadioAEn = PYGATE_RADIO_A_EN_PIN;
    SX1308.RFPowerEn = PYGATE_RF_POWER_EN_PIN;

    // Initialize the non SPI pins
    pin_config(SX1308.Reset, -1, -1, GPIO_MODE_OUTPUT, MACHPIN_PULL_NONE, 0);
    pin_config(SX1308.TxOn, -1, -1, GPIO_MODE_INPUT, MACHPIN_PULL_NONE, 0);
    pin_config(SX1308.RxOn, -1, -1, GPIO_MODE_INPUT, MACHPIN_PULL_NONE, 0);

    pin_config(SX1308.RadioAEn, -1, -1, GPIO_MODE_OUTPUT, MACHPIN_PULL_NONE, 0);
    pin_config(SX1308.RFPowerEn, -1, -1, GPIO_MODE_OUTPUT, MACHPIN_PULL_NONE, 0);

    // Enable power to all RF systems
    GpioWrite( SX1308.RadioAEn, 1 );
    GpioWrite( SX1308.RFPowerEn, 1 );
    mp_hal_delay_ms(50);

    sx1308_dig_reset();

    // configure the falling edge interrupt for TxOn
    pin_irq_disable(SX1308.TxOn);
    pin_extint_register(SX1308.TxOn, GPIO_INTR_NEGEDGE, 0);
    machpin_register_irq_c_handler(SX1308.TxOn, (void *)SX1308_Tx_Off_Isr);
    pin_irq_enable(SX1308.TxOn);

    timer_start_value = machtimer_get_timer_counter_value()/(CLK_FREQ / 1000000);
    SX1308.firsttx = true;
    SX1308.txongoing = 0;
    SX1308.offtmstp = 0;

    return true;
}

void sx1308_dig_reset(void) { //init modem for s2lp
    GpioWrite( SX1308.Reset, 1 );
    mp_hal_delay_us(50);
    GpioWrite( SX1308.Reset, 0 );
    mp_hal_delay_us(50);
}

void sx1308_spiWrite(uint8_t reg, uint8_t val) {
    //NSS = 0;
    GpioWrite( SX1308.Spi.Nss, 0 );
    sx1308_SpiInOut( (Spi_sx1308_t *)&SX1308.Spi, 0x80 | (reg & 0x7F) );
    sx1308_SpiInOut( (Spi_sx1308_t *)&SX1308.Spi, val );
    //NSS = 1;
    GpioWrite( SX1308.Spi.Nss, 1 );
}

void sx1308_spiWriteBurstF(uint8_t reg, uint8_t * val, int size) {
    int i = 0;

    //NSS = 0;
    GpioWrite( SX1308.Spi.Nss, 0 );

    sx1308_SpiInOut( (Spi_sx1308_t *)&SX1308.Spi, 0x80 | (reg & 0x7F) );
    for( i = 0; i < size; i++ )
    {
        sx1308_SpiInOut( (Spi_sx1308_t *)&SX1308.Spi, val[i] );
    }
}

void sx1308_spiWriteBurstM(uint8_t reg, uint8_t * val, int size) {
    int i = 0;

    for( i = 0; i < size; i++ )
    {
        sx1308_SpiInOut( (Spi_sx1308_t *)&SX1308.Spi, val[i] );
    }
}

void sx1308_spiWriteBurstE(uint8_t reg, uint8_t * val, int size) {
    int i = 0;

    for( i = 0; i < size; i++ )
    {
        sx1308_SpiInOut( (Spi_sx1308_t *)&SX1308.Spi, val[i] );
    }

    //NSS = 1;
    GpioWrite( SX1308.Spi.Nss, 1 );
}

void sx1308_spiWriteBurst(uint8_t reg, uint8_t * val, int size) {
    int i = 0;

    //NSS = 0;
    GpioWrite( SX1308.Spi.Nss, 0 );

    sx1308_SpiInOut( (Spi_sx1308_t *)&SX1308.Spi, 0x80 | (reg & 0x7F) );
    for( i = 0; i < size; i++ )
    {
        sx1308_SpiInOut( (Spi_sx1308_t *)&SX1308.Spi, val[i] );
    }

    //NSS = 1;
    GpioWrite( SX1308.Spi.Nss, 1 );
}

uint8_t sx1308_spiRead(uint8_t reg) {
    uint8_t val = 0;

    //NSS = 0;
    GpioWrite( SX1308.Spi.Nss, 0 );
    sx1308_SpiInOut( (Spi_sx1308_t *)&SX1308.Spi, reg & 0x7F );
    val = sx1308_SpiInOut( (Spi_sx1308_t *)&SX1308.Spi, 0 );
    //NSS = 1;
    GpioWrite( SX1308.Spi.Nss, 1 );

    return val;
}

uint8_t sx1308_spiReadBurstF(uint8_t reg, uint8_t *data, int size) {
    int i;

    //NSS = 0;
    GpioWrite( SX1308.Spi.Nss, 0 );

    sx1308_SpiInOut( (Spi_sx1308_t *)&SX1308.Spi, reg & 0x7F );

    for( i = 0; i < size; i++ )
    {
        data[i] = sx1308_SpiInOut( (Spi_sx1308_t *)&SX1308.Spi, 0 );
    }

    return 0;
}

uint8_t sx1308_spiReadBurstM(uint8_t reg, uint8_t *data, int size) {
    int i;

    for( i = 0; i < size; i++ )
    {
        data[i] = sx1308_SpiInOut( (Spi_sx1308_t *)&SX1308.Spi, 0 );
    }

    return 0;
}

uint8_t sx1308_spiReadBurstE(uint8_t reg, uint8_t *data, int size) {
    int i;

    for( i = 0; i < size; i++ )
    {
        data[i] = sx1308_SpiInOut( (Spi_sx1308_t *)&SX1308.Spi, 0 );
    }

    //NSS = 1;
    GpioWrite( SX1308.Spi.Nss, 1 );

    return 0;
}

uint8_t sx1308_spiReadBurst(uint8_t reg, uint8_t *data, int size) {
    int i;

    //NSS = 0;
    GpioWrite( SX1308.Spi.Nss, 0 );

    sx1308_SpiInOut( (Spi_sx1308_t *)&SX1308.Spi, reg & 0x7F );

    for( i = 0; i < size; i++ )
    {
        data[i] = sx1308_SpiInOut( (Spi_sx1308_t *)&SX1308.Spi, 0 );
    }

    //NSS = 1;
    GpioWrite( SX1308.Spi.Nss, 1 );

    return 0;
}

uint32_t sx1308_timer_read_us(void) {
     return ((machtimer_get_timer_counter_value()/(CLK_FREQ / 1000000) - timer_start_value) & 0xFFFFFFFF);
}


static IRAM_ATTR void GpioWrite( pin_obj_t *pin_obj, uint32_t value ) {
    // set the pin value
    if (value) {
        pin_obj->value = 1;
    } else {
        pin_obj->value = 0;
    }
    pin_set_value(pin_obj);
}

static IRAM_ATTR void SX1308_Tx_Off_Isr (void) {
    if (SX1308.txongoing == 1) {
        SX1308.waittxend = 0;
    }
}
