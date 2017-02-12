/*
 * Copyright (c) 2016, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>


#include "py/mpconfig.h"
#include "py/nlr.h"
#include "py/runtime.h"
#include "py/stream.h"
#include "py/mperrno.h"
#include "mpexception.h"
#include "pybioctl.h"

#include "modnetwork.h"
#include "modusocket.h"
#include "sigfox/sigfox_api.h"
#include "sigfox/timer.h"
#include "sigfox/radio.h"
#include "sigfox/manufacturer_api.h"
#include "sigfox/manuf_api.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "modsigfox.h"
#include "pycom_config.h"

#include "sigfox/cc112x_easy_link_reg_config.h"

Spi_t sigfox_spi = {};
sigfox_settings_t sigfox_settings = {};

sfx_u8 uplink_spectrum_access;

typedef enum {
    E_SIGFOX_STATE_NOINIT = 0,
    E_SIGFOX_STATE_IDLE,
    E_SIGFOX_STATE_RX,
    E_SIGFOX_STATE_TX,
    E_SIGFOX_STATE_TEST
} sigfox_state_t;

typedef enum {
    E_SIGFOX_RCZ1 = 0,
    E_SIGFOX_RCZ2,
    E_SIGFOX_RCZ3,
    E_SIGFOX_RCZ4,
} sigfox_rcz_t;

typedef enum {
    E_SIGFOX_MODE_SIGFOX = 0,
    E_SIGFOX_MODE_FSK
} sigfox_mode_t;

typedef struct {
  mp_obj_base_t     base;
  sigfox_mode_t     mode;
  sigfox_state_t    state;
} sigfox_obj_t;

typedef struct {
    uint32_t    index;
    uint32_t    size;
    uint8_t     data[SIGFOX_TX_PAYLOAD_SIZE_MAX];
} sigfox_partial_rx_packet_t;


static QueueHandle_t xCmdQueue;
static QueueHandle_t xRxQueue;
static EventGroupHandle_t sigfoxEvents;

static sigfox_obj_t sigfox_obj;
static sigfox_partial_rx_packet_t sigfox_partial_rx_packet;
static sfx_rcz_t all_rcz[] = {RCZ1, RCZ2, RCZ3, RCZ4};
static uint32_t sfx_rcz_id;
static uint8_t sigfox_id_rev[4];
static sfx_rcz_t sfx_rcz;

STATIC sfx_u32 rcz_config_words[4][3] = {
    {0},
    {RCZ2_SET_STD_CONFIG_WORD_0, RCZ2_SET_STD_CONFIG_WORD_1, RCZ2_SET_STD_CONFIG_WORD_2},
    {3, 5000, 0},
    {RCZ4_SET_STD_CONFIG_WORD_0, RCZ4_SET_STD_CONFIG_WORD_1, RCZ4_SET_STD_CONFIG_WORD_2},
};
STATIC sfx_u32 rcz_current_config_words[3];

STATIC sfx_u16 rcz_default_channel[4] = {0, RCZ2_SET_STD_DEFAULT_CHANNEL, 0, RCZ4_SET_STD_DEFAULT_CHANNEL};
STATIC sfx_u16 rcz_current_channel;

STATIC sfx_u32 rcz_frequencies[4][2] = {
    {868130000, 869525000},
    {902200000, 905200000},
    {921000000, 922200000},
    {920800000, 922300000},
};

static void TASK_Sigfox (void *pvParameters);
static int sigfox_socket_socket (mod_network_socket_obj_t *s, int *_errno);
static void sigfox_socket_close (mod_network_socket_obj_t *s);
static int sigfox_socket_send (mod_network_socket_obj_t *s, const byte *buf, mp_uint_t len, int *_errno);
static int sigfox_socket_recv (mod_network_socket_obj_t *s, byte *buf, mp_uint_t len, int *_errno);
static int sigfox_socket_settimeout (mod_network_socket_obj_t *s, mp_int_t timeout_ms, int *_errno);
static int sigfox_socket_setsockopt(mod_network_socket_obj_t *s, mp_uint_t level, mp_uint_t opt, const void *optval, mp_uint_t optlen, int *_errno);
static int sigfox_socket_ioctl (mod_network_socket_obj_t *s, mp_uint_t request, mp_uint_t arg, int *_errno);

static void fsk_register_config (void);
static void fsk_manual_calibration (void);
static void fsk_cc112x_tx (uint8_t *data, uint32_t len);
static void fsk_cc112x_rx (void);
static void fsk_tx_register_config(void);
static void fsk_rx_register_config(void);
static int32_t sigfox_recv (byte *buf, uint32_t len, int32_t timeout_ms);

/******************************************************************************
 DEFINE CONSTANTS
 ******************************************************************************/
#define RX_FIFO_ERROR                                 (0x11)

#define SIGFOX_CHECK_SOCKET(s)                        if (s->sock_base.sd < 0) {  \
                                                          *_errno = MP_EBADF;        \
                                                          return -1;              \
                                                      }

#define SIGFOX_SOCKET_GET_FD(sd)                      (sd & 0xFF)

#define SIGFOX_SOCKET_SET_RX(sd, rx)                  (sd &= 0xFFFF00FF); \
                                                      (sd |= (rx << 8))

#define SIGFOX_SOCKET_GET_RX(sd)                      ((sd >> 8) & 0xFF)

#define SIGFOX_SOCKET_SET_TX_REPEAT(sd, tx_repeat)    (sd &= 0xFF00FFFF); \
                                                      (sd |= (tx_repeat << 16))

#define SIGFOX_SOCKET_GET_TX_REPEAT(sd)               ((sd >> 16) & 0xFF)

#define SIGFOX_SOCKET_SET_OOB(sd, oob)                (sd &= 0xF0FFFFFF); \
                                                      (sd |= (oob << 24))

#define SIGFOX_SOCKET_GET_OOB(sd)                     ((sd >> 24) & 0x0F)

#define SIGFOX_SOCKET_SET_BIT(sd, bit)                (sd &= 0x0FFFFFFF); \
                                                      (sd |= (bit << 28))

#define SIGFOX_SOCKET_GET_BIT(sd)                     ((sd >> 28) & 0x0F)

enum {
    E_MODSIGOFX_TEST_MODE_START_CW = SFX_TEST_MODE_TX_SYNTH + 1,
    E_MODSIGOFX_TEST_MODE_STOP_CW,
    E_MODSIGOFX_TEST_MODE_MANUAL_TX
};

/******************************************************************************
 DEFINE PUBLIC FUNCTIONS
 ******************************************************************************/
void modsigfox_init0 (void) {
    xCmdQueue = xQueueCreate(SIGFOX_CMD_QUEUE_SIZE_MAX, sizeof(sigfox_cmd_data_t));
    xRxQueue = xQueueCreate(SIGFOX_DATA_QUEUE_SIZE_MAX, sizeof(sigfox_rx_data_t));
    sigfoxEvents = xEventGroupCreate();

    TIMER_downlinnk_timer_create();
    TIMER_carrier_sense_timer_create();
    TIMER_RxTx_done_timer_create();

    MANUF_API_nvs_open();

    // there is only One block of memory to allocate
    Table_200bytes.memory_ptr = (sfx_u8 *)(DynamicMemoryTable) ;
    Table_200bytes.allocated = SFX_FALSE;

    // setup the CC1125 control RESET pin
    gpio_config_t gpioconf = {.pin_bit_mask = 1 << 18,
                              .mode = GPIO_MODE_OUTPUT,
                              .pull_up_en = GPIO_PULLUP_ENABLE,
                              .pull_down_en = GPIO_PULLDOWN_DISABLE,
                              .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&gpioconf);
    GPIO_REG_WRITE(GPIO_OUT_W1TS_REG, 1 << 18);

    SpiInit( &sigfox_spi, RADIO_MOSI, RADIO_MISO, RADIO_SCLK, RADIO_NSS );

    xTaskCreate(TASK_Sigfox, "Sigfox", SIGFOX_STACK_SIZE, NULL, SIGFOX_TASK_PRIORITY, NULL);
}

/******************************************************************************
 DEFINE PRIVATE FUNCTIONS
 ******************************************************************************/

static uint32_t modsigfox_api_init (void) {
    if (sigfox_obj.state != E_SIGFOX_STATE_NOINIT) {
        SIGFOX_API_close();
        sigfox_obj.state = E_SIGFOX_STATE_NOINIT;
    }

    if (SIGFOX_API_open(&sfx_rcz, sigfox_id_rev) != SFX_ERR_NONE) {
        return SIGFOX_STATUS_ERR;
    }

    if (sfx_rcz_id != E_SIGFOX_RCZ1) {
        if (SFX_ERR_NONE != SIGFOX_API_set_std_config(rcz_config_words[sfx_rcz_id], rcz_default_channel[sfx_rcz_id])) {
            return SIGFOX_STATUS_ERR;
        }

        if (sfx_rcz_id == E_SIGFOX_RCZ2 || sfx_rcz_id == E_SIGFOX_RCZ4) {
            SIGFOX_API_reset();
        }
        memcpy(rcz_current_config_words, rcz_config_words[sfx_rcz_id], sizeof(rcz_current_config_words));
        rcz_current_channel = rcz_default_channel[sfx_rcz_id];
    }
    return SFX_ERR_NONE;
}

static sfx_error_t modsigfox_sfx_send(sigfox_cmd_rx_data_t *cmd_rx_data, sigfox_rx_data_t *rx_data) {
    if (cmd_rx_data->cmd_u.info.tx.oob) {
        return SIGFOX_API_send_outofband();
    } else {
        if (cmd_rx_data->cmd_u.info.tx.len > 0) {
            return SIGFOX_API_send_frame(cmd_rx_data->cmd_u.info.tx.data, cmd_rx_data->cmd_u.info.tx.len, rx_data->data,
                                        cmd_rx_data->cmd_u.info.tx.tx_repeat, cmd_rx_data->cmd_u.info.tx.receive);
        }
        return SIGFOX_API_send_bit(cmd_rx_data->cmd_u.info.tx.data[0] ? SFX_TRUE : SFX_FALSE,
                                  rx_data->data, cmd_rx_data->cmd_u.info.tx.tx_repeat,
                                  cmd_rx_data->cmd_u.info.tx.receive);
    }
}

static void TASK_Sigfox(void *pvParameters) {
    sigfox_cmd_rx_data_t cmd_rx_data;

    for ( ; ; ) {
        vTaskDelay(2 / portTICK_RATE_MS);

        switch (sigfox_obj.state) {
        case E_SIGFOX_STATE_NOINIT:
        case E_SIGFOX_STATE_IDLE:
        case E_SIGFOX_STATE_RX:
        case E_SIGFOX_STATE_TEST:
            if (xQueueReceive(xCmdQueue, &cmd_rx_data, 0)) {
                switch (cmd_rx_data.cmd_u.cmd) {
                case E_SIGFOX_CMD_INIT:
                    {
                        uint32_t status = SIGFOX_STATUS_COMPLETED;
                        TIMER_RxTx_done_stop();   // stop the RxTx timer while reconfiguring
                        if (cmd_rx_data.cmd_u.info.init.mode == E_SIGFOX_MODE_SIGFOX) {
                            sfx_rcz_id = cmd_rx_data.cmd_u.info.init.rcz;
                            memcpy(&sfx_rcz, &all_rcz[cmd_rx_data.cmd_u.info.init.rcz], sizeof(sfx_rcz));
                            uplink_spectrum_access = sfx_rcz.spectrum_access;

                            uint8_t sigfox_id[4];
                            config_get_sigfox_id(sigfox_id);
                            // the Sigfox ID must be passed to API_open in reverse order
                            for (int i = 0; i < sizeof(sigfox_id); i++) {
                                sigfox_id_rev[3 - i] = sigfox_id[i];
                            }

                            status |= modsigfox_api_init();
                        } else {
                            // write radio registers
                            fsk_register_config();
                            // calibrate radio according to errata
                            fsk_manual_calibration();
                        }
                        sigfox_obj.mode = cmd_rx_data.cmd_u.info.init.mode;
                        sigfox_obj.state = E_SIGFOX_STATE_IDLE;
                        xEventGroupSetBits(sigfoxEvents, status);
                    }
                    break;
                case E_SIGFOX_CMD_TX:
                    {
                        if (sigfox_obj.mode == E_SIGFOX_MODE_SIGFOX) {
                            sigfox_rx_data_t rx_data = {.len = SIGFOX_RX_PAYLOAD_SIZE_MAX};
                            uint32_t status = SIGFOX_STATUS_COMPLETED;

                            sfx_error_t err = modsigfox_sfx_send(&cmd_rx_data, &rx_data);

                            if (err == SFX_ERR_NONE) {
                                if (cmd_rx_data.cmd_u.info.tx.receive) {
                                    xQueueSend(xRxQueue, (void *)&rx_data, 0);
                                }
                            } else {
                                if (err == SFX_ERR_SEND_FRAME_INVALID_FH_CHAN
                                    || err == SFX_ERR_SEND_BIT_INVALID_FH_CHAN
                                    || err == SFX_ERR_SEND_FRAME_STATE
                                    || err == SFX_ERR_SEND_BIT_STATE
                                    || err == SFX_ERR_SEND_OOB_STATE) {
                                    // try again...
                                    modsigfox_api_init();
                                    err = modsigfox_sfx_send(&cmd_rx_data, &rx_data);
                                    if (SFX_ERR_NONE == err) {
                                        if (cmd_rx_data.cmd_u.info.tx.receive) {
                                            xQueueSend(xRxQueue, (void *)&rx_data, 0);
                                        }
                                    } else {
                                        status |= SIGFOX_STATUS_ERR;
                                    }
                                } else {
                                    status |= SIGFOX_STATUS_ERR;
                                }
                            }
                            sigfox_obj.state = E_SIGFOX_STATE_IDLE;
                            xEventGroupSetBits(sigfoxEvents, status);
                        } else {
                            // stop the TxRx timer before reconfiguring for Tx
                            TIMER_RxTx_done_stop();
                            trxSpiCmdStrobe(CC112X_SIDLE);
                            fsk_cc112x_tx (cmd_rx_data.cmd_u.info.tx.data, cmd_rx_data.cmd_u.info.tx.len);
                            sigfox_obj.state = E_SIGFOX_STATE_TX;
                            TIMER_RxTx_done_start();
                        }
                    }
                    break;
                case E_SIGFOX_CMD_TEST:
                    if (cmd_rx_data.cmd_u.info.test.mode <= SFX_TEST_MODE_TX_SYNTH) {
                        SIGFOX_API_test_mode(cmd_rx_data.cmd_u.info.test.mode, cmd_rx_data.cmd_u.info.test.config);
                    } else { // start or stop CW
                        if (cmd_rx_data.cmd_u.info.test.mode == E_MODSIGOFX_TEST_MODE_START_CW) {
                            RADIO_start_unmodulated_cw (cmd_rx_data.cmd_u.info.test.config);
                        } else if (cmd_rx_data.cmd_u.info.test.mode == E_MODSIGOFX_TEST_MODE_STOP_CW) {
                            RADIO_stop_unmodulated_cw (cmd_rx_data.cmd_u.info.test.config);
                        } else {
                            for (int i = 0; i < 20; i++) {
                                uint8_t tx_frame[18] = { 0xAA, 0xAA, 0xA3, 0x5F, 0x8D, 0xC3, 0x74, 0x7D, 0x18,
                                                         0x00, 0xAA, 0xAA, 0xB8, 0x01, 0x5A, 0x5F, 0x53, 0x4C };
                                MANUF_API_rf_init(SFX_RF_MODE_TX);
                                MANUF_API_change_frequency(cmd_rx_data.cmd_u.info.test.config);
                                MANUF_API_rf_send(tx_frame, sizeof(tx_frame));
                                vTaskDelay(50 / portTICK_RATE_MS);
                            }
                        }
                    }
                    sigfox_obj.state = E_SIGFOX_STATE_TEST;
                    xEventGroupSetBits(sigfoxEvents, SIGFOX_STATUS_COMPLETED);
                    break;
                default:
                    break;
                }
            } else {
                if (sigfox_obj.state == E_SIGFOX_STATE_IDLE) {
                    // set radio in RX
                    fsk_rx_register_config();
                    trxSpiCmdStrobe(CC112X_SRX);
                    sigfox_obj.state = E_SIGFOX_STATE_RX;
                    TIMER_RxTx_done_start();
                } else if (sigfox_obj.state == E_SIGFOX_STATE_RX) {
                    fsk_cc112x_rx();
                }
            }
            break;
        case E_SIGFOX_STATE_TX:
            // wait for interrupt that packet has been sent (assumes the GPIO
            // connected to the radioRxTxISR function is set to GPIOx_CFG = 0x06)
            if (packetSemaphore == ISR_IDLE) {
                uint8 volatile readByte;
                // read the datarate from the registers
                cc112xSpiReadReg(CC112X_MARCSTATE, (uint8 *)&readByte, 1);
            } else {
                trxSpiCmdStrobe(CC112X_SIDLE);
                sigfox_obj.state = E_SIGFOX_STATE_IDLE;
                xEventGroupSetBits(sigfoxEvents, SIGFOX_STATUS_COMPLETED);
            }
            break;
        default:
            break;
        }
    }
}

IRAM_ATTR static void fsk_cc112x_tx (uint8_t *data, uint32_t len) {
    // initialize packet buffer of size PKTLEN + 4
    uint8 packet[SIGFOX_TX_PAYLOAD_SIZE_MAX + 4];

    // clear the semaphore flag
    packetSemaphore = ISR_IDLE;

    fsk_tx_register_config();

    // Write packet to tx fifo
    packet[0] = len;
    memcpy(&packet[1], data, len);

    cc112xSpiWriteTxFifo(packet, len + 1);

    // strobe TX to send packet
    trxSpiCmdStrobe(CC112X_STX);
}

IRAM_ATTR static void fsk_cc112x_rx (void) {
    sigfox_rx_data_t rx_data;
    uint8 payload[SIGFOX_TX_PAYLOAD_SIZE_MAX + 4];
    uint8 size, status;

    // wait for the packet received interrupt
    if (packetSemaphore == ISR_ACTION_REQUIRED) {
          // read the number of bytes in rx fifo
          cc112xSpiReadReg(CC112X_NUM_RXBYTES, &size, 1);

          // check that we have bytes in fifo
          if (size > 0) {
            // read marcstate to check for RX FIFO error
            cc112xSpiReadReg(CC112X_MARCSTATE, &status, 1);

            // mask out marcstate bits and check if we have a RX FIFO error
            if ((status & 0x1F) == RX_FIFO_ERROR) {
                // flush the RX Fifo
                trxSpiCmdStrobe(CC112X_SFRX);
            } else {
                // read n bytes from rx fifo
                cc112xSpiReadRxFifo(payload, size);

                // check CRC ok (CRC_OK: bit7 in second status byte)
                // this assumes status bytes are appended in RX_FIFO
                // (PKT_CFG1.APPEND_STATUS = 1.)
                // if CRC is disabled the CRC_OK field will read 1
                if(payload[size - 1] & 0x80){
                    memcpy(rx_data.data, &payload[1], payload[0]);
                    rx_data.len = payload[0];
                    xQueueSend(xRxQueue, (void *)&rx_data, 0);
                }
            }
        }

        // reset the packet semaphore
        trxSpiCmdStrobe(CC112X_SIDLE);
        packetSemaphore = ISR_IDLE;
        sigfox_obj.state = E_SIGFOX_STATE_IDLE;
    }
}

IRAM_ATTR static void fsk_register_config(void) {
    // reset the radio
    trxSpiCmdStrobe(CC112X_SRES);

    // write registers to radio
    for(uint16 i = 0; i < sizeof(cc1125PreferredSettings) / sizeof(registerSetting_t); i++) {
        uint8 writeByte = cc1125PreferredSettings[i].data;
        cc112xSpiWriteReg( cc1125PreferredSettings[i].addr, &writeByte, 1);
    }
}

IRAM_ATTR static void fsk_tx_register_config(void) {
    // write registers to radio
    for(uint16 i = 0; i < sizeof(cc1125TxSettings) / sizeof(registerSetting_t); i++) {
        uint8 writeByte = cc1125TxSettings[i].data;
        cc112xSpiWriteReg( cc1125TxSettings[i].addr, &writeByte, 1);
    }
}

IRAM_ATTR static void fsk_rx_register_config(void) {
    // write registers to radio
    for(uint16 i = 0; i < sizeof(cc1125RxSettings) / sizeof(registerSetting_t); i++) {
        uint8 writeByte = cc1125RxSettings[i].data;
        cc112xSpiWriteReg( cc1125RxSettings[i].addr, &writeByte, 1);
    }
}

IRAM_ATTR static void fsk_manual_calibration(void) {
#define VCDAC_START_OFFSET                  (2)
#define FS_VCO2_INDEX                       (0)
#define FS_VCO4_INDEX                       (1)
#define FS_CHP_INDEX                        (2)

    uint8 original_fs_cal2;
    uint8 calResults_for_vcdac_start_high[3];
    uint8 calResults_for_vcdac_start_mid[3];
    uint8 marcstate;
    uint8 writeByte;

    // set VCO cap-array to 0 (FS_VCO2 = 0x00)
    writeByte = 0x00;
    cc112xSpiWriteReg(CC112X_FS_VCO2, &writeByte, 1);

    // start with high VCDAC (original VCDAC_START + 2):
    cc112xSpiReadReg(CC112X_FS_CAL2, &original_fs_cal2, 1);
    writeByte = original_fs_cal2 + VCDAC_START_OFFSET;
    cc112xSpiWriteReg(CC112X_FS_CAL2, &writeByte, 1);

    // calibrate and wait for calibration to be done (radio back in IDLE state)
    trxSpiCmdStrobe(CC112X_SCAL);

    do {
        cc112xSpiReadReg(CC112X_MARCSTATE, &marcstate, 1);
    } while (marcstate != 0x41);

    // read FS_VCO2, FS_VCO4 and FS_CHP register obtained with high VCDAC_START value
    cc112xSpiReadReg(CC112X_FS_VCO2, &calResults_for_vcdac_start_high[FS_VCO2_INDEX], 1);
    cc112xSpiReadReg(CC112X_FS_VCO4, &calResults_for_vcdac_start_high[FS_VCO4_INDEX], 1);
    cc112xSpiReadReg(CC112X_FS_CHP, &calResults_for_vcdac_start_high[FS_CHP_INDEX], 1);

    // set VCO cap-array to 0 (FS_VCO2 = 0x00)
    writeByte = 0x00;
    cc112xSpiWriteReg(CC112X_FS_VCO2, &writeByte, 1);

    // continue with mid VCDAC (original VCDAC_START):
    writeByte = original_fs_cal2;
    cc112xSpiWriteReg(CC112X_FS_CAL2, &writeByte, 1);

    // calibrate and wait for calibration to be done (radio back in IDLE state)
    trxSpiCmdStrobe(CC112X_SCAL);

    do {
        cc112xSpiReadReg(CC112X_MARCSTATE, &marcstate, 1);
    } while (marcstate != 0x41);

    // read FS_VCO2, FS_VCO4 and FS_CHP register obtained with mid VCDAC_START value
    cc112xSpiReadReg(CC112X_FS_VCO2, &calResults_for_vcdac_start_mid[FS_VCO2_INDEX], 1);
    cc112xSpiReadReg(CC112X_FS_VCO4, &calResults_for_vcdac_start_mid[FS_VCO4_INDEX], 1);
    cc112xSpiReadReg(CC112X_FS_CHP, &calResults_for_vcdac_start_mid[FS_CHP_INDEX], 1);

    // write back highest FS_VCO2 and corresponding FS_VCO and FS_CHP result
    if (calResults_for_vcdac_start_high[FS_VCO2_INDEX] > calResults_for_vcdac_start_mid[FS_VCO2_INDEX]) {
        writeByte = calResults_for_vcdac_start_high[FS_VCO2_INDEX];
        cc112xSpiWriteReg(CC112X_FS_VCO2, &writeByte, 1);
        writeByte = calResults_for_vcdac_start_high[FS_VCO4_INDEX];
        cc112xSpiWriteReg(CC112X_FS_VCO4, &writeByte, 1);
        writeByte = calResults_for_vcdac_start_high[FS_CHP_INDEX];
        cc112xSpiWriteReg(CC112X_FS_CHP, &writeByte, 1);
    } else {
        writeByte = calResults_for_vcdac_start_mid[FS_VCO2_INDEX];
        cc112xSpiWriteReg(CC112X_FS_VCO2, &writeByte, 1);
        writeByte = calResults_for_vcdac_start_mid[FS_VCO4_INDEX];
        cc112xSpiWriteReg(CC112X_FS_VCO4, &writeByte, 1);
        writeByte = calResults_for_vcdac_start_mid[FS_CHP_INDEX];
        cc112xSpiWriteReg(CC112X_FS_CHP, &writeByte, 1);
    }
}

static void sigfox_send_cmd (sigfox_cmd_rx_data_t *cmd_rx_data) {
    xQueueSend(xCmdQueue, (void *)cmd_rx_data, (TickType_t)portMAX_DELAY);
    uint32_t result = xEventGroupWaitBits(sigfoxEvents,
                                          SIGFOX_STATUS_COMPLETED | SIGFOX_STATUS_ERR,
                                          pdTRUE,   // clear on exit
                                          pdFALSE,  // do not wait for all bits
                                          (TickType_t)portMAX_DELAY);

    if (result & SIGFOX_STATUS_ERR) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
    }
}

static int32_t sigfox_recv (byte *buf, uint32_t len, int32_t timeout_ms) {
    sigfox_rx_data_t rx_data;

    if (timeout_ms < 0) {
        // blocking mode
        timeout_ms = portMAX_DELAY;
    }

    // if there's a partial packet pending
    if (sigfox_partial_rx_packet.size > 0) {
        // adjust the len
        uint32_t available_len = sigfox_partial_rx_packet.size - sigfox_partial_rx_packet.index;
        if (available_len < len) {
            len = available_len;
        }

        // get the available data
        memcpy(buf, &sigfox_partial_rx_packet.data[sigfox_partial_rx_packet.index], len);

        // update the index and size values
        sigfox_partial_rx_packet.index += len;
        if (sigfox_partial_rx_packet.index == sigfox_partial_rx_packet.size) {
            // there's no more data left
            sigfox_partial_rx_packet.size = 0;
        }
        // return the number of bytes received
        return len;
    } else if (xQueueReceive(xRxQueue, &rx_data, (TickType_t)(timeout_ms / portTICK_PERIOD_MS))) {
        // adjust the len
        if (rx_data.len < len) {
            len = rx_data.len;
        }

        // get the available data
        memcpy(buf, rx_data.data, len);

        // copy the remainder to the partial data buffer
        int32_t r_len = rx_data.len - len;
        if (r_len > 0) {
            memcpy(sigfox_partial_rx_packet.data, &rx_data.data[len], r_len);
            sigfox_partial_rx_packet.size = r_len;
            sigfox_partial_rx_packet.index = 0;
        }
        // return the number of bytes received
        return len;
    }
    // non-blocking sockects do not thrown timeout error
    if (timeout_ms == 0) {
        return 0;
    }
    // there's no data available
    return -1;
}

static bool sigfox_rx_any (void) {
    sigfox_rx_data_t rx_data;
    if (sigfox_partial_rx_packet.size > 0) {
        return true;
    } else if (xQueuePeek(xRxQueue, &rx_data, (TickType_t)0)) {
        return true;
    }
    return false;
}

static bool sigfox_tx_space (void) {
    if (uxQueueSpacesAvailable(xCmdQueue) > 0) {
        return true;
    }
    return false;
}

/******************************************************************************/
// Micro Python bindings; Sigfox class

STATIC mp_obj_t sigfox_init_helper(sigfox_obj_t *self, const mp_arg_val_t *args) {
    sigfox_cmd_rx_data_t cmd_rx_data;
    uint8_t rcz = args[1].u_int;
    uint8_t mode = args[0].u_int;

    if (mode > E_SIGFOX_MODE_FSK) {
        mp_raise_ValueError("invalid mode");
    } else if (mode == E_SIGFOX_MODE_SIGFOX) {
        if (rcz > E_SIGFOX_RCZ4) {
            mp_raise_ValueError("invalid RCZ");
        }
    }

    cmd_rx_data.cmd_u.cmd = E_SIGFOX_CMD_INIT;
    cmd_rx_data.cmd_u.info.init.mode = mode;
    cmd_rx_data.cmd_u.info.init.rcz = rcz;
    sigfox_send_cmd (&cmd_rx_data);
    sigfox_obj.state = E_SIGFOX_STATE_IDLE;

    return mp_const_none;
}

STATIC const mp_arg_t sigfox_init_args[] = {
    { MP_QSTR_id,                             MP_ARG_INT,   {.u_int  = 0} },
    { MP_QSTR_mode,                           MP_ARG_INT,   {.u_int  = E_SIGFOX_MODE_SIGFOX} },
    { MP_QSTR_rcz,                            MP_ARG_INT,   {.u_int  = E_SIGFOX_RCZ1} },
};

STATIC mp_obj_t sigfox_make_new(const mp_obj_type_t *type, mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *all_args) {
    // parse args
    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, all_args + n_args);
    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(sigfox_init_args)];
    mp_arg_parse_all(n_args, all_args, &kw_args, MP_ARRAY_SIZE(args), sigfox_init_args, args);

    // setup the object
    sigfox_obj_t *self = &sigfox_obj;
    self->base.type = (mp_obj_t)&mod_network_nic_type_sigfox;

    // check the peripheral id
    if (args[0].u_int != 0) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_resource_not_avaliable));
    }

    // run the constructor if the peripehral is not initialized or extra parameters are given
    if (n_kw > 0 || self->state == E_SIGFOX_STATE_NOINIT) {
        // start the peripheral
        sigfox_init_helper(self, &args[1]);
        // register it as a network card
        mod_network_register_nic(self);
    }

    return (mp_obj_t)self;
}

STATIC mp_obj_t sigfox_init(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(sigfox_init_args) - 1];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), &sigfox_init_args[1], args);
    return sigfox_init_helper(pos_args[0], args);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(sigfox_init_obj, 1, sigfox_init);

STATIC mp_obj_t sigfox_mac(mp_obj_t self_in) {
    uint8_t mac[8];
    config_get_lpwan_mac(mac);
    return mp_obj_new_bytes((const byte *)mac, sizeof(mac));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(sigfox_mac_obj, sigfox_mac);

STATIC mp_obj_t sigfox_id(mp_obj_t self_in) {
    uint8_t id[4];
    config_get_sigfox_id(id);
    return mp_obj_new_bytes((const byte *)id, sizeof(id));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(sigfox_id_obj, sigfox_id);

STATIC mp_obj_t sigfox_pac(mp_obj_t self_in) {
    uint8_t pac[8];
    config_get_sigfox_pac(pac);
    return mp_obj_new_bytes((const byte *)pac, sizeof(pac));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(sigfox_pac_obj, sigfox_pac);

STATIC mp_obj_t sigfox_test_mode(mp_obj_t self_in, mp_obj_t mode, mp_obj_t config) {
    sigfox_cmd_rx_data_t cmd_rx_data;
    uint32_t _mode = mp_obj_get_int(mode);
    uint32_t _config = mp_obj_get_int(config);

    if (_mode > SFX_TEST_MODE_TX_SYNTH && _mode != E_MODSIGOFX_TEST_MODE_MANUAL_TX) {
        mp_raise_ValueError("invalid test mode");
    }

    cmd_rx_data.cmd_u.cmd = E_SIGFOX_CMD_TEST;
    cmd_rx_data.cmd_u.info.test.mode = _mode;
    cmd_rx_data.cmd_u.info.test.config = _config;
    sigfox_send_cmd (&cmd_rx_data);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(sigfox_test_mode_obj, sigfox_test_mode);

STATIC mp_obj_t sigfox_cw(mp_obj_t self_in, mp_obj_t frequency, mp_obj_t start) {
    sigfox_cmd_rx_data_t cmd_rx_data;
    uint32_t _frequency = mp_obj_get_int(frequency);

    if (mp_obj_is_true(start)) {
        cmd_rx_data.cmd_u.info.test.mode = E_MODSIGOFX_TEST_MODE_START_CW;
    } else {
        cmd_rx_data.cmd_u.info.test.mode = E_MODSIGOFX_TEST_MODE_STOP_CW;
    }

    cmd_rx_data.cmd_u.cmd = E_SIGFOX_CMD_TEST;
    cmd_rx_data.cmd_u.info.test.config = _frequency;
    sigfox_send_cmd (&cmd_rx_data);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(sigfox_cw_obj, sigfox_cw);

STATIC mp_obj_t sigfox_frequencies(mp_obj_t self_in) {
    mp_obj_t tuple[2];
    tuple[0] = mp_obj_new_int(rcz_frequencies[sfx_rcz_id][0]);
    tuple[1] = mp_obj_new_int(rcz_frequencies[sfx_rcz_id][1]);

    return mp_obj_new_tuple(2, tuple);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(sigfox_frequencies_obj, sigfox_frequencies);

STATIC mp_obj_t sigfox_config(mp_uint_t n_args, const mp_obj_t *args) {
    if (n_args == 1) {
        if (sfx_rcz_id > 0) {
            mp_obj_t tuple[4];
            tuple[0] = mp_obj_new_int(rcz_current_config_words[0]);
            tuple[1] = mp_obj_new_int(rcz_current_config_words[1]);
            tuple[2] = mp_obj_new_int(rcz_current_config_words[2]);
            tuple[3] = mp_obj_new_int(rcz_current_channel);
            return mp_obj_new_tuple(4, tuple);
        } else {
            return mp_const_none;
        }
    } else {
        if (sfx_rcz_id > 0) {
            mp_obj_t *config;
            mp_obj_get_array_fixed_n(args[1], 4, &config);
            sfx_u32 config_words[3];
            config_words[0] = mp_obj_get_int(config[0]);
            config_words[1] = mp_obj_get_int(config[1]);
            config_words[2] = mp_obj_get_int(config[2]);
            sfx_u16 channel = mp_obj_get_int(config[3]);
            if (SFX_ERR_NONE != SIGFOX_API_set_std_config(config_words, channel)) {
                nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
            } else {
                memcpy(rcz_current_config_words, config_words, sizeof(rcz_current_config_words));
                rcz_current_channel = channel;
            }
            return mp_const_none;
        } else {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError,"configuration not possible for RCZ1"));
        }
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(sigfox_config_obj, 1, 2, sigfox_config);

STATIC mp_obj_t sigfox_public_key(mp_uint_t n_args, const mp_obj_t *args) {
    if (n_args == 1) {
        if (usePublicKey == SFX_TRUE) {
            return mp_const_true;
        }
        return mp_const_false;
    } else {
        if (mp_obj_is_true(args[1])) {
            usePublicKey = SFX_TRUE;
        } else {
            usePublicKey = SFX_FALSE;
        }
        return mp_const_none;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(sigfox_public_key_obj, 1, 2, sigfox_public_key);

STATIC mp_obj_t sigfox_rssi_offset(mp_uint_t n_args, const mp_obj_t *args) {
    if (n_args == 1) {
        int16_t rssi_offset;
        if (SFX_ERR_MANUF_NONE != MANUF_API_get_nv_mem(SFX_NVMEM_RSSI, (sfx_u16 *)&rssi_offset)) {
            rssi_offset = 0;
        }
        return mp_obj_new_int(rssi_offset);
    } else {
        if (SFX_ERR_MANUF_NONE != MANUF_API_set_nv_mem(SFX_NVMEM_RSSI, mp_obj_get_int(args[1]))) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
        }
        return mp_const_none;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(sigfox_rssi_offset_obj, 1, 2, sigfox_rssi_offset);

STATIC mp_obj_t sigfox_freq_offset(mp_uint_t n_args, const mp_obj_t *args) {
    if (n_args == 1) {
        int16_t freq_offset;
        if (SFX_ERR_MANUF_NONE != MANUF_API_get_nv_mem(SFX_NVMEM_FREQ, (sfx_u16 *)&freq_offset)) {
            freq_offset = 0;
        }
        return mp_obj_new_int(freq_offset);
    } else {
        if (SFX_ERR_MANUF_NONE != MANUF_API_set_nv_mem(SFX_NVMEM_FREQ, mp_obj_get_int(args[1]))) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
        }
        return mp_const_none;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(sigfox_freq_offset_obj, 1, 2, sigfox_freq_offset);

STATIC mp_obj_t sigfox_version(mp_obj_t self_in) {
    sfx_u8 *ptr;
    sfx_u8 size;

    if (SFX_ERR_NONE == SIGFOX_API_get_version(&ptr, &size)) {
        return mp_obj_new_str((char *)ptr, size - 1, false);
    }
    nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(sigfox_version_obj, sigfox_version);

STATIC mp_obj_t sigfox_info(mp_obj_t self_in) {
    sfx_u8 info;

    if (sfx_rcz_id > 0) {
        if (SFX_ERR_NONE == SIGFOX_API_get_info(&info)) {
            mp_obj_t tuple[2];
            if (sfx_rcz_id == 2) {  // RCZ3 - LBT
                tuple[0] = mp_obj_new_int(info & 0b00000111);   // Frames sent
                tuple[1] = mp_obj_new_int(info & 0b11111000);   // Carrier sense attempts
            } else {
                tuple[0] = mp_obj_new_int(info & 0b00001111);   // FCC channel not default
                tuple[1] = mp_obj_new_int(info & 0b11110000);   // Number of free micro channel
            }
            return mp_obj_new_tuple(2, tuple);
        }
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(sigfox_info_obj, sigfox_info);

STATIC mp_obj_t sigfox_reset(mp_obj_t self_in) {
    sfx_u8 info;
    // only for RCZ2 and RCZ4
    if (sfx_rcz_id == 1 || sfx_rcz_id == 3) {
        SIGFOX_API_reset();
        if (SFX_ERR_NONE == SIGFOX_API_get_info(&info) && (info & 0b11110000)) {
            // we still have free channels, so there's no need to wait
        } else {
            // we must force wait 20s in order to comply with FCC regulations
            vTaskDelay(20000 / portTICK_RATE_MS);
        }
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(sigfox_reset_obj, sigfox_reset);


STATIC const mp_map_elem_t sigfox_locals_dict_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR_init),                (mp_obj_t)&sigfox_init_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_mac),                 (mp_obj_t)&sigfox_mac_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_id),                  (mp_obj_t)&sigfox_id_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_pac),                 (mp_obj_t)&sigfox_pac_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_test_mode),           (mp_obj_t)&sigfox_test_mode_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_cw),                  (mp_obj_t)&sigfox_cw_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_frequencies),         (mp_obj_t)&sigfox_frequencies_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_config),              (mp_obj_t)&sigfox_config_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_public_key),          (mp_obj_t)&sigfox_public_key_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_version),             (mp_obj_t)&sigfox_version_obj },
    // { MP_OBJ_NEW_QSTR(MP_QSTR_rssi),                (mp_obj_t)&sigfox_rssi_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_rssi_offset),         (mp_obj_t)&sigfox_rssi_offset_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_freq_offset),         (mp_obj_t)&sigfox_freq_offset_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_info),                (mp_obj_t)&sigfox_info_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_reset),               (mp_obj_t)&sigfox_reset_obj },

    { MP_OBJ_NEW_QSTR(MP_QSTR_SIGFOX),              MP_OBJ_NEW_SMALL_INT(E_SIGFOX_MODE_SIGFOX) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_FSK),                 MP_OBJ_NEW_SMALL_INT(E_SIGFOX_MODE_FSK) },

    { MP_OBJ_NEW_QSTR(MP_QSTR_RCZ1),                MP_OBJ_NEW_SMALL_INT(E_SIGFOX_RCZ1) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_RCZ2),                MP_OBJ_NEW_SMALL_INT(E_SIGFOX_RCZ2) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_RCZ3),                MP_OBJ_NEW_SMALL_INT(E_SIGFOX_RCZ3) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_RCZ4),                MP_OBJ_NEW_SMALL_INT(E_SIGFOX_RCZ4) },
};

STATIC MP_DEFINE_CONST_DICT(sigfox_locals_dict, sigfox_locals_dict_table);

const mod_network_nic_type_t mod_network_nic_type_sigfox = {
    .base = {
        { &mp_type_type },
        .name = MP_QSTR_Sigfox,
        .make_new = sigfox_make_new,
        .locals_dict = (mp_obj_t)&sigfox_locals_dict,
     },

    .n_socket = sigfox_socket_socket,
    .n_close = sigfox_socket_close,
    .n_send = sigfox_socket_send,
    .n_recv = sigfox_socket_recv,
    .n_settimeout = sigfox_socket_settimeout,
    .n_setsockopt = sigfox_socket_setsockopt,
    .n_ioctl = sigfox_socket_ioctl,
};


///******************************************************************************/
//// Micro Python bindings; Sigfox socket

static int sigfox_socket_socket (mod_network_socket_obj_t *s, int *_errno) {
    if (sigfox_obj.state == E_SIGFOX_STATE_NOINIT) {
        *_errno = MP_ENETDOWN;
        return -1;
    }
    s->sock_base.sd = 1;
    SIGFOX_SOCKET_SET_RX(s->sock_base.sd, false);
    SIGFOX_SOCKET_SET_TX_REPEAT(s->sock_base.sd, 2);
    SIGFOX_SOCKET_SET_OOB(s->sock_base.sd, false);
    return 0;
}

static void sigfox_socket_close (mod_network_socket_obj_t *s) {
    // this is to prevent the finalizer to close a socket that failed during creation
    if (s->sock_base.sd > 0) {
        s->sock_base.sd = -1;
    }
}

static int sigfox_socket_send(mod_network_socket_obj_t *s, const byte *buf, mp_uint_t len, int *_errno) {
    sigfox_cmd_rx_data_t cmd_rx_data;
    SIGFOX_CHECK_SOCKET(s);

    if (len > SIGFOX_TX_PAYLOAD_SIZE_MAX) {
        *_errno = MP_EMSGSIZE;
        return -1;
    }

    cmd_rx_data.cmd_u.cmd = E_SIGFOX_CMD_TX;
    cmd_rx_data.cmd_u.info.tx.len = len;
    cmd_rx_data.cmd_u.info.tx.tx_repeat = SIGFOX_SOCKET_GET_TX_REPEAT(s->sock_base.sd);
    cmd_rx_data.cmd_u.info.tx.receive = SIGFOX_SOCKET_GET_RX(s->sock_base.sd) ? SFX_TRUE : SFX_FALSE;
    cmd_rx_data.cmd_u.info.tx.oob = SIGFOX_SOCKET_GET_OOB(s->sock_base.sd);
    if (len > 0) {
        memcpy(cmd_rx_data.cmd_u.info.tx.data, buf, len);
    } else {
        if (SIGFOX_SOCKET_GET_BIT(s->sock_base.sd)) {
            cmd_rx_data.cmd_u.info.tx.data[0] = 1;
        } else {
            cmd_rx_data.cmd_u.info.tx.data[0] = 0;
        }
    }

    int32_t timeout_ms = s->sock_base.timeout;
    if (timeout_ms < 0) {
        // blocking mode
        timeout_ms = portMAX_DELAY;
    }

    // just pass it to the Sigfox queue
    if (!xQueueSend(xCmdQueue, (void *)&cmd_rx_data, (TickType_t)(timeout_ms / portTICK_PERIOD_MS))) {
        *_errno = MP_EAGAIN;
        return -1;
    }

    if (timeout_ms != 0) {
        uint32_t result = xEventGroupWaitBits(sigfoxEvents,
                                              SIGFOX_STATUS_COMPLETED | SIGFOX_STATUS_ERR,
                                              pdTRUE,   // clear on exit
                                              pdFALSE,  // do not wait for all bits
                                             (TickType_t)portMAX_DELAY);
        if (result & SIGFOX_STATUS_ERR) {
            *_errno = MP_ENETDOWN;
            return -1;
        }
    }

    return len;
}

static int sigfox_socket_recv (mod_network_socket_obj_t *s, byte *buf, mp_uint_t len, int *_errno) {
    SIGFOX_CHECK_SOCKET(s);
    int ret = sigfox_recv (buf, len, s->sock_base.timeout);
    if (ret < 0) {
        *_errno = MP_EAGAIN;
        return -1;
    }
    return ret;
}

static int sigfox_socket_setsockopt(mod_network_socket_obj_t *s, mp_uint_t level, mp_uint_t opt, const void *optval, mp_uint_t optlen, int *_errno) {
    SIGFOX_CHECK_SOCKET(s);
    if (level != SOL_SIGFOX) {
        *_errno = MP_EOPNOTSUPP;
        return -1;
    }

    if (opt == SO_SIGFOX_RX) {
        SIGFOX_SOCKET_SET_RX(s->sock_base.sd, *(uint8_t *)optval);
    } else if (opt == SO_SIGFOX_TX_REPEAT) {
        uint8_t tx_repeat = *(uint8_t *)optval;
        if (tx_repeat > 2) {
            *_errno = MP_EOPNOTSUPP;
            return -1;
        }
        SIGFOX_SOCKET_SET_TX_REPEAT(s->sock_base.sd, tx_repeat);
    } else if (opt == SO_SIGFOX_OOB) {
        SIGFOX_SOCKET_SET_OOB(s->sock_base.sd, *(uint8_t *)optval);
    } else if (opt == SO_SIGFOX_BIT) {
        SIGFOX_SOCKET_SET_BIT(s->sock_base.sd, *(uint8_t *)optval);
    } else {
        *_errno = MP_EOPNOTSUPP;
        return -1;
    }
    return 0;
}

static int sigfox_socket_settimeout (mod_network_socket_obj_t *s, mp_int_t timeout_ms, int *_errno) {
    SIGFOX_CHECK_SOCKET(s);
    s->sock_base.timeout = timeout_ms;
    return 0;
}

static int sigfox_socket_ioctl (mod_network_socket_obj_t *s, mp_uint_t request, mp_uint_t arg, int *_errno) {
    mp_int_t ret = 0;

    SIGFOX_CHECK_SOCKET(s);
    if (request == MP_IOCTL_POLL) {
        mp_uint_t flags = arg;
        if ((flags & MP_IOCTL_POLL_RD) && sigfox_rx_any()) {
            ret |= MP_IOCTL_POLL_RD;
        }
        if ((flags & MP_IOCTL_POLL_WR) && sigfox_tx_space()) {
            ret |= MP_IOCTL_POLL_WR;
        }
    } else {
        *_errno = MP_EINVAL;
        ret = MP_STREAM_ERROR;
    }
    return ret;
}
