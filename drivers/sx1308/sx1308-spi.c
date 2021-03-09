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

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

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

#include "spi.h"
#include "gpio.h"

#include "soc/gpio_sig_map.h"
#include "soc/dport_reg.h"

#include "sx1308-spi.h"


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
void sx1308_SpiInit( Spi_sx1308_t *obj) {
    // this is SpiNum_SPI2
    DPORT_SET_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG, DPORT_SPI_CLK_EN);
    DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG, DPORT_SPI_RST);

    // configure the SPI port
    spi_attr_t spi_attr = {.mode = SpiMode_Master, .subMode = SpiSubMode_0, .speed = SpiSpeed_8MHz,
                           .bitOrder = SpiBitOrder_MSBFirst, .halfMode = SpiWorkMode_Full};

    spi_init(obj->Spi, &spi_attr);
    while (READ_PERI_REG(SPI_CMD_REG(obj->Spi)) & SPI_USR);  // wait for SPI not busy

    // set a NULL command
    CLEAR_PERI_REG_MASK(SPI_USER_REG(obj->Spi), SPI_USR_COMMAND);
    SET_PERI_REG_BITS(SPI_USER2_REG(obj->Spi), SPI_USR_COMMAND_BITLEN, 0, SPI_USR_COMMAND_BITLEN_S);

    // set a NULL address
    CLEAR_PERI_REG_MASK(SPI_USER_REG(obj->Spi), SPI_USR_ADDR);
    SET_PERI_REG_BITS(SPI_USER1_REG(obj->Spi), SPI_USR_ADDR_BITLEN,0, SPI_USR_ADDR_BITLEN_S);

    // enable MOSI
    SET_PERI_REG_MASK(SPI_USER_REG(obj->Spi), SPI_USR_MOSI);

    // set the data send buffer length. The max data length 64 bytes.
    SET_PERI_REG_BITS(SPI_MOSI_DLEN_REG(obj->Spi), SPI_USR_MOSI_DBITLEN, 7, SPI_USR_MOSI_DBITLEN_S);
    SET_PERI_REG_BITS(SPI_MISO_DLEN_REG(obj->Spi), SPI_USR_MISO_DBITLEN, 7, SPI_USR_MISO_DBITLEN_S);

    // assign the SPI pins to the GPIO matrix and configure the AF
    pin_config(obj->Miso, HSPIQ_IN_IDX, -1, GPIO_MODE_INPUT, MACHPIN_PULL_NONE, 0);
    pin_config(obj->Mosi, -1, HSPID_OUT_IDX, GPIO_MODE_OUTPUT, MACHPIN_PULL_NONE, 0);
    pin_config(obj->Sclk, -1, HSPICLK_OUT_IDX, GPIO_MODE_OUTPUT, MACHPIN_PULL_NONE, 0);
    pin_config(obj->Nss, -1, -1, GPIO_MODE_OUTPUT, MACHPIN_PULL_UP, 1);
} 

/*!
 * \brief Sends outData and receives inData
 *
 * \param [IN] obj     SPI object
 * \param [IN] outData Byte to be sent
 * \retval inData      Received byte.
 */
IRAM_ATTR uint16_t sx1308_SpiInOut(Spi_sx1308_t *obj, uint16_t outData) {
    uint32_t spiNum = obj->Spi;

    // set data send buffer length (1 byte)
    SET_PERI_REG_BITS(SPI_MOSI_DLEN_REG(spiNum), SPI_USR_MOSI_DBITLEN, 7, SPI_USR_MOSI_DBITLEN_S);
    SET_PERI_REG_BITS(SPI_MISO_DLEN_REG(spiNum), SPI_USR_MISO_DBITLEN, 7, SPI_USR_MISO_DBITLEN_S);

    // load the send buffer
    WRITE_PERI_REG(SPI_W0_REG(spiNum), outData);
    // start to send data
    SET_PERI_REG_MASK(SPI_CMD_REG(spiNum), SPI_USR);

    while (READ_PERI_REG(SPI_CMD_REG(spiNum)) & SPI_USR);

    // read data out
    return READ_PERI_REG(SPI_W0_REG(spiNum));
}
