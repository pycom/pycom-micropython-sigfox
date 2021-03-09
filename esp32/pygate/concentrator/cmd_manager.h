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

#ifndef CMDMANAGER_H
#define CMDMANAGER_H

#include "sx1308.h"

#define ATOMICTX                900
#define ATOMICRX                600

#define CMD_HEADER_RX_SIZE      4 /* id + len_msb + len_lsb + address */
#define CMD_HEADER_TX_SIZE      4 /* id + len_msb + len_lsb + status */

#define CMD_DATA_RX_SIZE        ATOMICRX
#define CMD_DATA_TX_SIZE        (1024 + 16 * 44) /* MAX_FIFO + 16 * METADATA_SIZE_ALIGNED */
#define CMD_LENGTH_MSB          1
#define CMD_LENGTH_LSB          2

#define CMD_ERROR               0
#define CMD_OK                  1
#define CMD_K0                  0
#define ACK_OK                  1
#define ACK_K0                  0
#define FWVERSION               0x010a0006
#define ISUARTINTERFACE         1
#define ISUSBINTERFACE          0

#define BAUDRATE                115200

typedef struct {
    char id;
    uint8_t len_msb;
    uint8_t len_lsb;
    uint8_t address;
    uint8_t cmd_data[CMD_DATA_RX_SIZE];
} CmdSettings_t;

extern int cmd_manager_DecodeCmd(uint8_t *BufFromHost);

extern void cmd_manager_GetCmdToHost (uint8_t **bufToHost);


#endif // CMDMANAGER_H
