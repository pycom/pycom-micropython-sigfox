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

Description: Bleeper STM32L151RD microcontroller pins definition

License: Revised BSD License, see LICENSE.TXT file include in the project

Maintainer: Miguel Luis and Gregory Cristian
*/

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "heap_alloc_caps.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_attr.h"

#include "gpio.h"
#include "spi.h"

#include "board.h"
#include "soc/gpio_sig_map.h"
#include "soc/dport_reg.h"
#include "gpio-board.h"


#define SPIDEV      SpiNum_SPI3

//static uint8_t spi_transfer(Spi_t *obj, uint8_t outdata);

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
    SET_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG,DPORT_SPI_CLK_EN_2);
    CLEAR_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG,DPORT_SPI_RST_2);

    // configure the SPI port
    spi_attr_t spi_attr = {.mode = SpiMode_Master, .subMode = SpiSubMode_0, .speed = SpiSpeed_10MHz,
                           .bitOrder = SpiBitOrder_MSBFirst, .halfMode = SpiWorkMode_Full};
    spi_init((uint32_t)obj->Spi, &spi_attr);

    // assign the SPI pins to the GPIO matrix and configure the AF
    pin_config(obj->Miso.pin_obj, VSPIQ_IN_IDX, -1, GPIO_MODE_INPUT, PIN_NO_PULL, 0, 0);
    pin_config(obj->Mosi.pin_obj, -1, VSPID_OUT_IDX, GPIO_MODE_OUTPUT, PIN_NO_PULL, 0, 0);
    pin_config(obj->Sclk.pin_obj, -1, VSPICLK_OUT_MUX_IDX, GPIO_MODE_OUTPUT, PIN_NO_PULL, 0, 0);
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


static IRAM_ATTR int spi_master_send_recv_data(spi_num_e spiNum, spi_data_t* pData) {
    char idx = 0;
    if ((spiNum > SpiNum_Max)
        || (NULL == pData)) {
        return -1;
    }
    uint32_t *value;// = pData->rx_data;
    while (READ_PERI_REG(SPI_CMD_REG(spiNum))&SPI_USR);
    // Set command by user.
    if (pData->cmdLen != 0) {
        // Max command length 16 bits.
        SET_PERI_REG_BITS(SPI_USER2_REG(spiNum), SPI_USR_COMMAND_BITLEN,((pData->cmdLen << 3) - 1), SPI_USR_COMMAND_BITLEN_S);
        // Enable command
        SET_PERI_REG_MASK(SPI_USER_REG(spiNum), SPI_USR_COMMAND);
        // Load command
        spi_master_cfg_cmd(spiNum, pData->cmd);
    } else {
        CLEAR_PERI_REG_MASK(SPI_USER_REG(spiNum), SPI_USR_COMMAND);
        SET_PERI_REG_BITS(SPI_USER2_REG(spiNum), SPI_USR_COMMAND_BITLEN,0, SPI_USR_COMMAND_BITLEN_S);
    }
    // Set Address by user.
    if (pData->addrLen == 0) {
        CLEAR_PERI_REG_MASK(SPI_USER_REG(spiNum), SPI_USR_ADDR);
        SET_PERI_REG_BITS(SPI_USER1_REG(spiNum), SPI_USR_ADDR_BITLEN,0, SPI_USR_ADDR_BITLEN_S);
    } else {
        if (NULL == pData->addr) {
            return -1;
        }
        SET_PERI_REG_BITS(SPI_USER1_REG(spiNum), SPI_USR_ADDR_BITLEN,((pData->addrLen << 3) - 1), SPI_USR_ADDR_BITLEN_S);
        // Enable address
        SET_PERI_REG_MASK(SPI_USER_REG(spiNum), SPI_USR_ADDR);
        // Load address
        spi_master_cfg_addr(spiNum, *pData->addr);
    }
    value = pData->txData;
    // Set data by user.
    if(pData->txDataLen != 0) {
        if(NULL == value) {
            return -1;
        }
        //CLEAR_PERI_REG_MASK(SPI_USER_REG(spiNum), SPI_USR_MISO);
        // Enable MOSI
        SET_PERI_REG_MASK(SPI_USER_REG(spiNum), SPI_USR_MOSI);
        // Load send buffer
        do {
            WRITE_PERI_REG((SPI_W0_REG(spiNum) + (idx << 2)), *value++);
        } while(++idx < ((pData->txDataLen / 4) + ((pData->txDataLen % 4) ? 1 : 0)));
        // Set data send buffer length.Max data length 64 bytes.
        SET_PERI_REG_BITS(SPI_MOSI_DLEN_REG(spiNum), SPI_USR_MOSI_DBITLEN, ((pData->txDataLen << 3) - 1),SPI_USR_MOSI_DBITLEN_S);
        SET_PERI_REG_BITS(SPI_MISO_DLEN_REG(spiNum), SPI_USR_MISO_DBITLEN, ((pData->rxDataLen << 3) - 1),SPI_USR_MISO_DBITLEN_S);
    } else {
        CLEAR_PERI_REG_MASK(SPI_USER_REG(spiNum), SPI_USR_MOSI);
        CLEAR_PERI_REG_MASK(SPI_USER_REG(spiNum), SPI_USR_MISO);
        SET_PERI_REG_BITS(SPI_MOSI_DLEN_REG(spiNum), SPI_USR_MOSI_DBITLEN,0, SPI_USR_MOSI_DBITLEN_S);
    }
    // Start send data
    SET_PERI_REG_MASK(SPI_CMD_REG(spiNum), SPI_USR);
    while (READ_PERI_REG(SPI_CMD_REG(spiNum))&SPI_USR);
    value = pData->rxData;
    // Read data out
    idx = 0;
    do {
        *value++ =  READ_PERI_REG(SPI_W0_REG(spiNum) + (idx << 2));
    } while (++idx < ((pData->rxDataLen / 4) + ((pData->rxDataLen % 4) ? 1 : 0)));
    return 0;
}

/*!
 * \brief Sends outData and receives inData
 *
 * \param [IN] obj     SPI object
 * \param [IN] outData Byte to be sent
 * \retval inData      Received byte.
 */
IRAM_ATTR uint16_t SpiInOut( Spi_t *obj, uint16_t outData ) {
   uint32_t _rxdata = 0;
   uint32_t _txdata = outData;
   spi_data_t spidata = {.cmd = 0, .cmdLen = 0, .addr = NULL, .addrLen = 0,
                         .txData = &_txdata, .txDataLen = 1, .rxData = &_rxdata, .rxDataLen = 1};

   spi_master_send_recv_data((uint32_t)obj->Spi, &spidata);

   return _rxdata;
}
