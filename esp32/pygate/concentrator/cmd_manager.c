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

#include "cmd_manager.h"
#include "esp32_mphal.h"
#include "loragw_hal_esp.h"
#include "loragw_reg_esp.h"
#include <string.h>


#define DELAY_COM_INIT                          (1000)
#define DELAY_RESET                             (200)

static bool cmd_manager_CheckCmd(char id);


static uint8_t BufToHost[CMD_DATA_TX_SIZE + CMD_HEADER_TX_SIZE];
static CmdSettings_t cmdSettings_FromHost;


/********************************************************/
/*   cmd name   |      description                      */
/*------------------------------------------------------*/
/*  r           |Read register                          */
/*  s           |Read long burst First packet           */
/*  t           |Read long burst Middle packet          */
/*  u           |Read long burst End packet             */
/*  p           |Read atomic burst packet               */
/*  w           |Write register                         */
/*  x           |Write long burst First packet          */
/*  y           |Write long burst Middle packet         */
/*  z           |Write long burst End packet            */
/*  a           |Write atomic burst packet              */
/*------------------------------------------------------*/
/*  b           |lgw_receive cmd                        */
/*  c           |lgw_rxrf_setconf cmd                   */
/*  d           |int lgw_rxif_setconf_cmd               */
/*  f           |int lgw_send cmd                       */
/*  h           |lgw_txgain_setconf                     */
/*  q           |lgw_get_trigcnt                        */
/*  i           |lgw_board_setconf                      */
/*  j           |lgw_mcu_commit_radio_calibration       */
/*  l           |lgw_check_fw_version                   */
/*  m           |Reset SX1308 and STM32                 */
/*  n           |Jump to bootloader                     */
/********************************************************/
int cmd_manager_DecodeCmd(uint8_t *BufFromHost) {
    int i = 0;
    int adressreg;
    int val;
    int size;
    int x;

    if (BufFromHost[0] == 0) {
        return (CMD_ERROR);
    }

    cmdSettings_FromHost.id = BufFromHost[0];

    if (cmd_manager_CheckCmd(cmdSettings_FromHost.id) == false) {
        BufToHost[0] = 'k';
        BufToHost[1] = 0;
        BufToHost[2] = 0;
        BufToHost[3] = ACK_K0;
        return(CMD_K0);
    }

    switch (cmdSettings_FromHost.id) {

        case 'r': { // cmd Read register
                cmdSettings_FromHost.len_msb = BufFromHost[1];
                cmdSettings_FromHost.len_lsb = BufFromHost[2];
                adressreg = BufFromHost[3];
                for (i = 0; i < cmdSettings_FromHost.len_lsb + (cmdSettings_FromHost.len_msb << 8); i++) {
                    cmdSettings_FromHost.cmd_data[i] = BufFromHost[4 + i];
                }
                val = sx1308_spiRead(adressreg);
                BufToHost[0] = 'r';
                BufToHost[1] = 0;
                BufToHost[2] = 1;
                BufToHost[3] = ACK_OK;
                BufToHost[4] = val;
                return(CMD_OK);
            }
        case 's': { // cmd Read burst register first
                cmdSettings_FromHost.len_msb = BufFromHost[1];
                cmdSettings_FromHost.len_lsb = BufFromHost[2];
                adressreg = BufFromHost[3];
                for (i = 0; i < cmdSettings_FromHost.len_lsb + (cmdSettings_FromHost.len_msb << 8); i++) {
                    cmdSettings_FromHost.cmd_data[i] = BufFromHost[4 + i];
                }
                size = (cmdSettings_FromHost.cmd_data[0] << 8) + cmdSettings_FromHost.cmd_data[1];
                BufToHost[0] = 's';
                BufToHost[1] = cmdSettings_FromHost.cmd_data[0];
                BufToHost[2] = cmdSettings_FromHost.cmd_data[1];
                BufToHost[3] = ACK_OK;
                sx1308_spiReadBurstF(adressreg, &BufToHost[4 + 0], size);
                return(CMD_OK);
            }
        case 't': { // cmd Read burst register middle
                cmdSettings_FromHost.len_msb = BufFromHost[1];
                cmdSettings_FromHost.len_lsb = BufFromHost[2];
                adressreg = BufFromHost[3];

                for (i = 0; i < cmdSettings_FromHost.len_lsb + (cmdSettings_FromHost.len_msb << 8); i++) {
                    cmdSettings_FromHost.cmd_data[i] = BufFromHost[4 + i];
                }
                size = (cmdSettings_FromHost.cmd_data[0] << 8) + cmdSettings_FromHost.cmd_data[1];

                BufToHost[0] = 't';
                BufToHost[1] = cmdSettings_FromHost.cmd_data[0];
                BufToHost[2] = cmdSettings_FromHost.cmd_data[1];
                BufToHost[3] = ACK_OK;
                sx1308_spiReadBurstM(adressreg, &BufToHost[4 + 0], size);
                return(CMD_OK);
            }
        case 'u': { // cmd Read burst register end
                cmdSettings_FromHost.len_msb = BufFromHost[1];
                cmdSettings_FromHost.len_lsb = BufFromHost[2];
                adressreg = BufFromHost[3];
                for (i = 0; i < cmdSettings_FromHost.len_lsb + (cmdSettings_FromHost.len_msb << 8); i++) {
                    cmdSettings_FromHost.cmd_data[i] = BufFromHost[4 + i];
                }
                size = (cmdSettings_FromHost.cmd_data[0] << 8) + cmdSettings_FromHost.cmd_data[1];
                BufToHost[0] = 'u';
                BufToHost[1] = cmdSettings_FromHost.cmd_data[0];
                BufToHost[2] = cmdSettings_FromHost.cmd_data[1];
                BufToHost[3] = ACK_OK;
                sx1308_spiReadBurstE(adressreg, &BufToHost[4 + 0], size);
                return(CMD_OK);
            }
        case 'p': { // cmd Read burst register atomic
                cmdSettings_FromHost.len_msb = BufFromHost[1];
                cmdSettings_FromHost.len_lsb = BufFromHost[2];
                adressreg = BufFromHost[3];
                for (i = 0; i < cmdSettings_FromHost.len_lsb + (cmdSettings_FromHost.len_msb << 8); i++) {
                    cmdSettings_FromHost.cmd_data[i] = BufFromHost[4 + i];
                }
                size = (cmdSettings_FromHost.cmd_data[0] << 8) + cmdSettings_FromHost.cmd_data[1];
                BufToHost[0] = 'p';
                BufToHost[1] = cmdSettings_FromHost.cmd_data[0];
                BufToHost[2] = cmdSettings_FromHost.cmd_data[1];
                BufToHost[3] = ACK_OK;
                sx1308_spiReadBurst(adressreg, &BufToHost[4 + 0], size);
                return(CMD_OK);
            }
        case 'w': { // cmd write register
                cmdSettings_FromHost.len_msb = BufFromHost[1];
                cmdSettings_FromHost.len_lsb = BufFromHost[2];
                adressreg = BufFromHost[3];
                for (i = 0; i < cmdSettings_FromHost.len_lsb + (cmdSettings_FromHost.len_msb << 8); i++) {
                    cmdSettings_FromHost.cmd_data[i] = BufFromHost[4 + i];
                }
                val = cmdSettings_FromHost.cmd_data[0];
                sx1308_spiWrite(adressreg, val);
                BufToHost[0] = 'w';
                BufToHost[1] = 0;
                BufToHost[2] = 0;
                BufToHost[3] = ACK_OK;
                return(CMD_OK);
            }
        case 'x': { // cmd write burst register
                cmdSettings_FromHost.len_msb = BufFromHost[1];
                cmdSettings_FromHost.len_lsb = BufFromHost[2];
                adressreg = BufFromHost[3];
                size = cmdSettings_FromHost.len_lsb + (cmdSettings_FromHost.len_msb << 8);
                // printf("Entering burst register write, size=%d\n", size);
                for (i = 0; i < cmdSettings_FromHost.len_lsb + (cmdSettings_FromHost.len_msb << 8); i++) {
                    cmdSettings_FromHost.cmd_data[i] = BufFromHost[4 + i];
                }
                // printf("About to perform a burst register write on address=%x\n", adressreg);
                sx1308_spiWriteBurstF(adressreg, &cmdSettings_FromHost.cmd_data[0], size);
                BufToHost[0] = 'x';
                BufToHost[1] = 0;
                BufToHost[2] = 0;
                BufToHost[3] = ACK_OK;
                return(CMD_OK);
            }
        case 'y': { // cmd write burst register
                cmdSettings_FromHost.len_msb = BufFromHost[1];
                cmdSettings_FromHost.len_lsb = BufFromHost[2];
                adressreg = BufFromHost[3];
                size = cmdSettings_FromHost.len_lsb + (cmdSettings_FromHost.len_msb << 8);
                for (i = 0; i < cmdSettings_FromHost.len_lsb + (cmdSettings_FromHost.len_msb << 8); i++) {
                    cmdSettings_FromHost.cmd_data[i] = BufFromHost[4 + i];
                }
                sx1308_spiWriteBurstM(adressreg, &cmdSettings_FromHost.cmd_data[0], size);
                BufToHost[0] = 'y';
                BufToHost[1] = 0;
                BufToHost[2] = 0;
                BufToHost[3] = ACK_OK;
                return(CMD_OK);
            }
        case 'z': { // cmd write burst register
                cmdSettings_FromHost.len_msb = BufFromHost[1];
                cmdSettings_FromHost.len_lsb = BufFromHost[2];
                adressreg = BufFromHost[3];
                size = cmdSettings_FromHost.len_lsb + (cmdSettings_FromHost.len_msb << 8);
                for (i = 0; i < cmdSettings_FromHost.len_lsb + (cmdSettings_FromHost.len_msb << 8); i++) {
                    cmdSettings_FromHost.cmd_data[i] = BufFromHost[4 + i];
                }
                sx1308_spiWriteBurstE(adressreg, &cmdSettings_FromHost.cmd_data[0], size);
                BufToHost[0] = 'z';
                BufToHost[1] = 0;
                BufToHost[2] = 0;
                BufToHost[3] = ACK_OK;
                return(CMD_OK);
            }
        case 'a': { // cmd write burst atomic register
                cmdSettings_FromHost.len_msb = BufFromHost[1];
                cmdSettings_FromHost.len_lsb = BufFromHost[2];
                adressreg = BufFromHost[3];
                size = cmdSettings_FromHost.len_lsb + (cmdSettings_FromHost.len_msb << 8);
                for (i = 0; i < cmdSettings_FromHost.len_lsb + (cmdSettings_FromHost.len_msb << 8); i++) {
                    cmdSettings_FromHost.cmd_data[i] = BufFromHost[4 + i];
                }
                sx1308_spiWriteBurst(adressreg, &cmdSettings_FromHost.cmd_data[0], size);
                BufToHost[0] = 'a';
                BufToHost[1] = 0;
                BufToHost[2] = 0;
                BufToHost[3] = ACK_OK;
                return(CMD_OK);
            }
        case 'b': { // lgw_receive
                static struct lgw_pkt_rx_s pkt_data[16]; //16 max packets TBU
                int nbpacket = 0;
                int j = 0;
                int sizeatomic = sizeof(struct lgw_pkt_rx_s) / sizeof(uint8_t);
                int cptalc = 0;
                int pt = 0;
                cmdSettings_FromHost.len_msb = BufFromHost[1];
                cmdSettings_FromHost.len_lsb = BufFromHost[2];
                adressreg = BufFromHost[3];
                for (i = 0; i < cmdSettings_FromHost.len_lsb + (cmdSettings_FromHost.len_msb << 8); i++) {
                    cmdSettings_FromHost.cmd_data[i] = BufFromHost[4 + i];
                }
                nbpacket = esp_lgw_receive(cmdSettings_FromHost.cmd_data[0], &pkt_data[0]);
                BufToHost[0] = 'b';
                BufToHost[3] = ((nbpacket >= 0) ? ACK_OK : ACK_K0); 
                BufToHost[4] = nbpacket;
                for (j = 0; j < nbpacket; j++) {
                    for (i = 0; i < (pkt_data[j].size + (sizeatomic - 256)); i++) {
                        BufToHost[5 + i + pt] = *((uint8_t *)(&pkt_data[j]) + i);
                        cptalc++;
                    }
                    pt = cptalc;
                }
                cptalc = cptalc + 1; // + 1 for nbpacket
                BufToHost[1] = (uint8_t)((cptalc >> 8) & 0xFF);
                BufToHost[2] = (uint8_t)((cptalc >> 0) & 0xFF);
                return(CMD_OK);
            }
        case 'c': { // lgw_rxrf_setconf
                uint8_t rf_chain;
                uint8_t conf[20];
                cmdSettings_FromHost.len_msb = BufFromHost[1];
                cmdSettings_FromHost.len_lsb = BufFromHost[2];
                rf_chain = BufFromHost[3];
                for (i = 0; i < cmdSettings_FromHost.len_lsb + (cmdSettings_FromHost.len_msb << 8); i++) {
                    conf[i] = BufFromHost[4 + i];
                }
                x = esp_lgw_rxrf_setconf(rf_chain, (struct lgw_conf_rxrf_s *)conf);
                BufToHost[0] = 'c';
                BufToHost[1] = 0;
                BufToHost[2] = 0;
                BufToHost[3] = ((x == 0) ? ACK_OK : ACK_K0);
                return(CMD_OK);
            }
        case 'h': { // lgw_txgain_setconf
                uint8_t conf[(LGW_MULTI_NB * TX_GAIN_LUT_SIZE_MAX) + 4];
                cmdSettings_FromHost.len_msb = BufFromHost[1];
                cmdSettings_FromHost.len_lsb = BufFromHost[2];
                for (i = 0; i < cmdSettings_FromHost.len_lsb + (cmdSettings_FromHost.len_msb << 8); i++) {
                    conf[i] = BufFromHost[4 + i];
                }
                x = esp_lgw_txgain_setconf((struct lgw_tx_gain_lut_s *)conf);
                BufToHost[0] = 'h';
                BufToHost[1] = 0;
                BufToHost[2] = 0;
                BufToHost[3] = ((x == 0) ? ACK_OK : ACK_K0);
                return(CMD_OK);
            }
        case 'd': { // lgw_rxif_setconf
                uint8_t if_chain;
                uint8_t conf[(sizeof(struct lgw_conf_rxif_s) / sizeof(uint8_t))];
                cmdSettings_FromHost.len_msb = BufFromHost[1];
                cmdSettings_FromHost.len_lsb = BufFromHost[2];
                if_chain = BufFromHost[3];
                for (i = 0; i < cmdSettings_FromHost.len_lsb + (cmdSettings_FromHost.len_msb << 8); i++) {
                    conf[i] = BufFromHost[4 + i];
                }
                x = esp_lgw_rxif_setconf(if_chain, (struct lgw_conf_rxif_s *)conf);
                BufToHost[0] = 'd';
                BufToHost[1] = 0;
                BufToHost[2] = 0;
                BufToHost[3] = ((x == 0) ? ACK_OK : ACK_K0);
                return(CMD_OK);
            }
        case 'f': { // lgw_send
                uint32_t count_us;
                int32_t txcontinuous;
                uint8_t conf[(sizeof(struct lgw_pkt_tx_s) / sizeof(uint8_t))];

                cmdSettings_FromHost.len_msb = BufFromHost[1];
                cmdSettings_FromHost.len_lsb = BufFromHost[2];
                for (i = 0; i < cmdSettings_FromHost.len_lsb + (cmdSettings_FromHost.len_msb << 8); i++) {
                    conf[i] = BufFromHost[4 + i];
                }

                /* Switch off SX1308 correlators to reduce power consumption during transmit */
                esp_lgw_reg_w(LGW_CLKHS_EN, 0);

                /* Send packet */
                SX1308.txongoing = 1;
                SX1308.waittxend = 1;
                x = esp_lgw_send((struct lgw_pkt_tx_s *)conf);
                if (x < 0) {
                    //pc.printf("lgw_send() failed\n");
                }
                
                /* In case of TX continuous, return the answer immediatly */
                esp_lgw_reg_r(LGW_TX_MODE, &txcontinuous); // to switch off the timeout in case of tx continuous
                if (txcontinuous == 1) {
                    BufToHost[0] = 'f';
                    BufToHost[1] = 0;
                    BufToHost[2] = 0;
                    BufToHost[3] = ACK_OK;
                    return(CMD_OK);
                }

                /* Wait for TX_DONE interrupt, or 10 seconds timeout */
                int32_t tx_timeout_ms = 10000;
                while (SX1308.waittxend && tx_timeout_ms > 0) {
                    ets_delay_us(1000);
                    tx_timeout_ms -= 1;
                }

                /* Align SX1308 internal counter and STM32 counter */
                if (SX1308.firsttx == true) {
                    esp_lgw_get_trigcnt(&count_us);
                    SX1308.offtmstpref = (sx1308_timer_read_us() - count_us) + 60;
                    SX1308.firsttx = false;
                }

                /* reset Sx1308 */
                sx1308_dig_reset();

                /* Switch SX1308 correlators back on  */
                esp_lgw_reg_w(LGW_CLKHS_EN, 1);

                /* restart SX1308 */
                x = esp_lgw_start();
                if (x < 0) {
                    //pc.printf("lgw_start() failed\n");
                }

                /* Send command answer */
                BufToHost[0] = 'f';
                BufToHost[1] = 0;
                BufToHost[2] = 0;
                BufToHost[3] = ((x == 0) ? ACK_OK : ACK_K0);

                return(CMD_OK);
            }
        case 'q': { // lgw_get_trigcnt
                uint32_t timestamp;
                x = esp_lgw_get_trigcnt(&timestamp);
                timestamp += SX1308.offtmstp;
                BufToHost[0] = 'q';
                BufToHost[1] = 0;
                BufToHost[2] = 4;
                BufToHost[3] = ((x == 0) ? ACK_OK : ACK_K0);
                BufToHost[4] = (uint8_t)(timestamp >> 24);
                BufToHost[5] = (uint8_t)((timestamp & 0x00FF0000) >> 16);
                BufToHost[6] = (uint8_t)((timestamp & 0x0000FF00) >> 8);
                BufToHost[7] = (uint8_t)((timestamp & 0x000000FF));
                return(CMD_OK);
            }
        case 'i': { // lgw_board_setconf
                uint8_t  conf[(sizeof(struct lgw_conf_board_s) / sizeof(uint8_t))];
                cmdSettings_FromHost.len_msb = BufFromHost[1];
                cmdSettings_FromHost.len_lsb = BufFromHost[2];
                for (i = 0; i < cmdSettings_FromHost.len_lsb + (cmdSettings_FromHost.len_msb << 8); i++) {
                    conf[i] = BufFromHost[4 + i];
                }

                x = esp_lgw_board_setconf((struct lgw_conf_board_s *)conf);
                BufToHost[0] = 'i';
                BufToHost[1] = 0;
                BufToHost[2] = 0;
                BufToHost[3] = ((x == 0) ? ACK_OK : ACK_K0);
                return(CMD_OK);
            }
        case 'j': { // lgw_calibration_snapshot
                esp_lgw_calibration_offset_transfer(BufFromHost[4], BufFromHost[5]);
                BufToHost[0] = 'j';
                BufToHost[1] = 0;
                BufToHost[2] = 0;
                BufToHost[3] = ACK_OK;
                return(CMD_OK);
            }
        case 'l': { // lgw_mcu_commit_radio_calibration
                int fwfromhost;
                cmdSettings_FromHost.len_msb = BufFromHost[1];
                cmdSettings_FromHost.len_lsb = BufFromHost[2];
                for (i = 0; i < cmdSettings_FromHost.len_lsb + (cmdSettings_FromHost.len_msb << 8); i++) {
                    cmdSettings_FromHost.cmd_data[i] = BufFromHost[4 + i];
                }
                fwfromhost = (BufFromHost[4] << 24) + (BufFromHost[5] << 16) + (BufFromHost[6] << 8) + (BufFromHost[7]);
                BufToHost[0] = 'l';
                BufToHost[1] = 0;
                BufToHost[2] = 8;
                if (fwfromhost == FWVERSION) {
                    BufToHost[3] = ACK_OK;
                } else {
                    BufToHost[3] = ACK_K0;
                }
                BufToHost[4] = '1';
                BufToHost[5] = '2';
                BufToHost[6] = '3';
                BufToHost[7] = '4';
                BufToHost[8] = '5';
                BufToHost[9] = '6';
                BufToHost[10] = '7';
                BufToHost[11] = '8';
                return(CMD_OK);
            }
        case 'm': { // Reset SX1308 and STM32
                /* reset SX1308 */
                esp_lgw_soft_reset();
                /* Prepare command answer */
                BufToHost[0] = 'm';
                BufToHost[1] = 0;
                BufToHost[2] = 0;
                BufToHost[3] = ACK_OK;
                /* Indicate that STM32 reset has to be triggered */
                // TODO: Reset the ESP32
                //sx1308_deinit();
                return(CMD_OK);
            }
        default:
            BufToHost[0] = 'k';
            BufToHost[1] = 0;
            BufToHost[2] = 0;
            BufToHost[3] = ACK_K0;
            return(CMD_K0);
    }
}

void cmd_manager_GetCmdToHost (uint8_t **bufToHost) {
    *bufToHost = BufToHost;
}

size_t cmd_manager_GetCmdToHost_byte (uint32_t index, uint8_t *bufToHost, size_t len) {
    if(index < (CMD_DATA_TX_SIZE + CMD_HEADER_TX_SIZE))
    {
        memcpy(bufToHost, &BufToHost[index], len);
        return len;
    }
    else
    {
        return -1;
    }
}

static bool cmd_manager_CheckCmd(char id) {
    switch (id) {
        case 'r': /* read register */
        case 's': /* read burst - first chunk */
        case 't': /* read burst - middle chunk */
        case 'u': /* read burst - end chunk */
        case 'p': /* read burst - atomic */
        case 'w': /* write register */
        case 'x': /* write burst - first chunk */
        case 'y': /* write burst - middle chunk */
        case 'z': /* write burst - end chunk */
        case 'a': /* write burst - atomic */
        case 'b': /* lgw_receive */
        case 'c': /* lgw_rxrf_setconf */
        case 'd': /* lgw_rxif_setconf */
        case 'f': /* lgw_send */
        case 'h': /* lgw_txgain_setconf */
        case 'q': /* lgw_get_trigcnt */
        case 'i': /* lgw_board_setconf */
        case 'j': /* lgw_mcu_commit_radio_calibration */
        case 'l': /* lgw_check_fw_version */
        case 'm': /* reset MCU */
            return true;
        default:
            return false;
    }
}

