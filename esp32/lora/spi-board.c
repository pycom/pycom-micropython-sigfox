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

#include "py/mpconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "esp_heap_caps.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"
#include "esp_attr.h"

#include "gpio.h"
#include "spi.h"

#include "board.h"
#include "soc/gpio_sig_map.h"
#include "soc/dport_reg.h"
#include "gpio-board.h"


#define SPIDEV      SpiNum_SPI3

/*!
 * \brief Initializes the SPI object and MCU peripheral
 *
 * \remark When NSS pin is software controlled set the pin name to NC otherwise
 *         set the pin name to be used.
 *
 * \param [IN] obj  SPI object
 * \param [IN] mosi SPI MOSI pin name to be used
 * \param [IN] miso SPI MISO pin name to be used
 * \param [IN] sclk SPI SCLK pin name to be used
 * \param [IN] nss  SPI NSS pin name to be used
 */

// ESP32 notes:
//              SPI  => SPI_1
//              HSPI => SPI_2
//              VSPI => SPI_3

void SpiInit( Spi_t *obj, PinNames mosi, PinNames miso, PinNames sclk, PinNames nss ) {
    // assign the pins
    obj->Miso.pin_obj = gpio_board_map[miso];
    obj->Mosi.pin_obj = gpio_board_map[mosi];
    obj->Sclk.pin_obj = gpio_board_map[sclk];
    obj->Spi = (void *)SPIDEV;

    // this is SpiNum_SPI3
    DPORT_SET_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG,DPORT_SPI_CLK_EN_2);
    DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG,DPORT_SPI_RST_2);

#if defined (SIPY)
    // configure the SPI port
    spi_attr_t spi_attr = {.mode = SpiMode_Master, .subMode = SpiSubMode_0, .speed = SpiSpeed_8MHz,
                           .bitOrder = SpiBitOrder_MSBFirst, .halfMode = SpiWorkMode_Full};
#else
    // configure the SPI port
    spi_attr_t spi_attr = {.mode = SpiMode_Master, .subMode = SpiSubMode_0, .speed = SpiSpeed_10MHz,
                           .bitOrder = SpiBitOrder_MSBFirst, .halfMode = SpiWorkMode_Full};
#endif
    spi_init((uint32_t)obj->Spi, &spi_attr);
    while (READ_PERI_REG(SPI_CMD_REG((uint32_t)obj->Spi)) & SPI_USR);  // wait for SPI not busy

    // set a NULL command
    CLEAR_PERI_REG_MASK(SPI_USER_REG((uint32_t)obj->Spi), SPI_USR_COMMAND);
    SET_PERI_REG_BITS(SPI_USER2_REG((uint32_t)obj->Spi), SPI_USR_COMMAND_BITLEN, 0, SPI_USR_COMMAND_BITLEN_S);

    // set a NULL address
    CLEAR_PERI_REG_MASK(SPI_USER_REG((uint32_t)obj->Spi), SPI_USR_ADDR);
    SET_PERI_REG_BITS(SPI_USER1_REG((uint32_t)obj->Spi), SPI_USR_ADDR_BITLEN,0, SPI_USR_ADDR_BITLEN_S);

    // enable MOSI
    SET_PERI_REG_MASK(SPI_USER_REG((uint32_t)obj->Spi), SPI_USR_MOSI);

    // set the data send buffer length. The max data length 64 bytes.
    SET_PERI_REG_BITS(SPI_MOSI_DLEN_REG((uint32_t)obj->Spi), SPI_USR_MOSI_DBITLEN, 7, SPI_USR_MOSI_DBITLEN_S);
    SET_PERI_REG_BITS(SPI_MISO_DLEN_REG((uint32_t)obj->Spi), SPI_USR_MISO_DBITLEN, 7, SPI_USR_MISO_DBITLEN_S);

    // assign the SPI pins to the GPIO matrix and configure the AF
    pin_config(obj->Miso.pin_obj, VSPIQ_IN_IDX, -1, GPIO_MODE_INPUT, PIN_NO_PULL, 0);
    pin_config(obj->Mosi.pin_obj, -1, VSPID_OUT_IDX, GPIO_MODE_OUTPUT, PIN_NO_PULL, 0);
    pin_config(obj->Sclk.pin_obj, -1, VSPICLK_OUT_IDX, GPIO_MODE_OUTPUT, PIN_NO_PULL, 0);

#if defined (SIPY)
    // configure the chip select pin
    obj->Nss.pin_obj = gpio_board_map[nss];
    pin_config(obj->Nss.pin_obj, -1, -1, GPIO_MODE_OUTPUT, PIN_PULL_UP, 1);
#endif
}

/*!
 * \brief De-initializes the SPI object and MCU peripheral
 *
 * \param [IN] obj SPI object
 */
void SpiDeInit( Spi_t *obj ) {
    // disable the peripheral
}

/*!
 * \brief Configures the SPI peripheral
 *
 * \remark Slave mode isn't currently handled
 *
 * \param [IN] obj   SPI object
 * \param [IN] bits  Number of bits to be used. [8 or 16]
 * \param [IN] cpol  Clock polarity
 * \param [IN] cpha  Clock phase
 * \param [IN] slave When set the peripheral acts in slave mode
 */
void SpiFormat( Spi_t *obj, int8_t bits, int8_t cpol, int8_t cpha, int8_t slave ) {
    // configure the interface (only master mode supported)
}

/*!
 * \brief Sets the SPI speed
 *
 * \param [IN] obj SPI object
 * \param [IN] hz  SPI clock frequency in hz
 */
void SpiFrequency( Spi_t *obj, uint32_t hz ) {
    // configure the interface (only master mode supported)
}

/*!
 * \brief Sends outData and receives inData
 *
 * \param [IN] obj     SPI object
 * \param [IN] outData Byte to be sent
 * \retval inData      Received byte.
 */
#if defined(LOPY) || defined(LOPY4) || defined(FIPY)
IRAM_ATTR uint16_t SpiInOut(Spi_t *obj, uint16_t outData) {
    uint32_t spiNum = (uint32_t)obj->Spi;

#if defined(FIPY) || defined(LOPY4)
    // set data send buffer length (1 byte)
    SET_PERI_REG_BITS(SPI_MOSI_DLEN_REG(spiNum), SPI_USR_MOSI_DBITLEN, 7, SPI_USR_MOSI_DBITLEN_S);
    SET_PERI_REG_BITS(SPI_MISO_DLEN_REG(spiNum), SPI_USR_MISO_DBITLEN, 7, SPI_USR_MISO_DBITLEN_S);
#endif

    // load the send buffer
    WRITE_PERI_REG(SPI_W0_REG(spiNum), outData);
    // start to send data
    SET_PERI_REG_MASK(SPI_CMD_REG(spiNum), SPI_USR);
    while (READ_PERI_REG(SPI_CMD_REG(spiNum)) & SPI_USR);
    // read data out
    return READ_PERI_REG(SPI_W0_REG(spiNum));
}
#elif defined(SIPY)
IRAM_ATTR uint8_t SpiInOut(uint32_t spiNum, uint32_t outData) {
    // set data send buffer length (1 byte)
    SET_PERI_REG_BITS(SPI_MOSI_DLEN_REG(spiNum), SPI_USR_MOSI_DBITLEN, 7, SPI_USR_MOSI_DBITLEN_S);
    SET_PERI_REG_BITS(SPI_MISO_DLEN_REG(spiNum), SPI_USR_MISO_DBITLEN, 7, SPI_USR_MISO_DBITLEN_S);

    // load the send buffer
    WRITE_PERI_REG(SPI_W0_REG(spiNum), outData);

    // start send data
    SET_PERI_REG_MASK(SPI_CMD_REG(spiNum), SPI_USR);
    while (READ_PERI_REG(SPI_CMD_REG(spiNum)) & SPI_USR);

    // read data out
    return READ_PERI_REG(SPI_W0_REG(spiNum));
}
#endif

#if defined(SIPY) || defined(LOPY4) || defined(FIPY)
/*!
 * \brief Sends outData
 *
 * \param [IN] obj     SPI object
 * \param [IN] outData Byte to be sent
 * \retval void
 */
IRAM_ATTR void SpiOut(uint32_t spiNum, uint32_t outData) {
    // set data send buffer length (2 bytes)
    SET_PERI_REG_BITS(SPI_MOSI_DLEN_REG(spiNum), SPI_USR_MOSI_DBITLEN, 15, SPI_USR_MOSI_DBITLEN_S);
    SET_PERI_REG_BITS(SPI_MISO_DLEN_REG(spiNum), SPI_USR_MISO_DBITLEN, 15, SPI_USR_MISO_DBITLEN_S);

    // load send buffer
    WRITE_PERI_REG(SPI_W0_REG(spiNum), outData);
    WRITE_PERI_REG(SPI_W0_REG(spiNum) + 4, outData >> 8);  // the SPI FIFO is 4-byte wide

    // start send data
    SET_PERI_REG_MASK(SPI_CMD_REG(spiNum), SPI_USR);
    while (READ_PERI_REG(SPI_CMD_REG(spiNum)) & SPI_USR);
}
#endif
