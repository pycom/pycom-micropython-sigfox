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
#include "py/stream.h"
#include "esp32_mphal.h"

#include "modnetwork.h"
#include "modusocket.h"
#include "sigfox/sigfox_api.h"
#include "sigfox/timer.h"
#if defined(FIPY) || defined(LOPY4)
#include "sigfox/radio_sx127x.h"
#else
#include "sigfox/radio.h"
#endif
#include "sigfox/manufacturer_api.h"
#include "sigfox/manuf_api.h"

#include "ff.h"
#include "lfs.h"
#include "diskio.h"
#include "sflash_diskio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "mptask.h"

#include "modsigfox.h"
#include "pycom_config.h"

#include "sigfox/cc112x_easy_link_reg_config.h"


#define FSK_FREQUENCY_MIN                            863000000   // Hz
#define FSK_FREQUENCY_MAX                            928000000   // Hz

#define SFX_RESET_FCC_MIN_DELAY_S                    23

extern TaskHandle_t xSigfoxTaskHndl;
#if defined(FIPY) || defined(LOPY4)
extern SemaphoreHandle_t xLoRaSigfoxSem;
#endif

Spi_t sigfox_spi = {};
sigfox_settings_t sigfox_settings = {};

sfx_u8 uplink_spectrum_access;

sigfox_obj_t sigfox_obj;

static QueueHandle_t xCmdQueue;
static QueueHandle_t xRxQueue;
static EventGroupHandle_t sigfoxEvents;

static sigfox_partial_rx_packet_t sigfox_partial_rx_packet;
static sfx_rc_t all_rcz[] = {RC1, RC2, RC3, RC4};
static uint32_t sfx_rcz_id;
static uint8_t sigfox_id_rev[4];
static sfx_rc_t sfx_rcz;

static RTC_DATA_ATTR uint32_t tx_timestamp;

STATIC sfx_u32 rcz_config_words[4][3] = {
    {0},
    RC2_SM_CONFIG,
    RC3_CONFIG,
    RC4_SM_CONFIG,
};
STATIC sfx_u32 rcz_current_config_words[3];

STATIC sfx_u32 rcz_frequencies[4][2] = {
    {868130000, 869525000},
    {902200000, 905200000},
    {923200000, 922200000},
    {920800000, 922300000},
};

static void TASK_Sigfox (void *pvParameters);

#if !defined(FIPY) && !defined(LOPY4)
static void fsk_register_config (void);
static void fsk_manual_calibration (void);
static void fsk_cc112x_tx (uint8_t *data, uint32_t len);
static void fsk_cc112x_rx (void);
static void fsk_tx_register_config(void);
static void fsk_rx_register_config(void);
#endif
static int32_t sigfox_recv (byte *buf, uint32_t len, int32_t timeout_ms);

/******************************************************************************
 DEFINE CONSTANTS
 ******************************************************************************/
#define RX_FIFO_ERROR                                 (0x11)

#define SIGFOX_CHECK_SOCKET(s)                        if (s->sock_base.u.sd < 0) {  \
                                                          *_errno = MP_EBADF;       \
                                                          return -1;                \
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

    MANUF_API_nvs_open();

    // there is only One block of memory to allocate
    Table_200bytes.memory_ptr = (sfx_u8 *)(DynamicMemoryTable) ;
    Table_200bytes.allocated = SFX_FALSE;

#if !defined(FIPY) && !defined(LOPY4)
    // setup the CC1125 control RESET pin
    gpio_config_t gpioconf = {.pin_bit_mask = 1 << 18,
                              .mode = GPIO_MODE_OUTPUT,
                              .pull_up_en = GPIO_PULLUP_DISABLE,
                              .pull_down_en = GPIO_PULLDOWN_DISABLE,
                              .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&gpioconf);
    GPIO_REG_WRITE(GPIO_OUT_W1TS_REG, 1 << 18);
    SpiInit( &sigfox_spi, RADIO_MOSI, RADIO_MISO, RADIO_SCLK, RADIO_NSS );
#endif

    xTaskCreatePinnedToCore(TASK_Sigfox, "Sigfox", SIGFOX_STACK_SIZE, NULL, SIGFOX_TASK_PRIORITY, &xSigfoxTaskHndl, 0);
}

void sigfox_update_id (void) {
    #define SFX_ID_PATH          "/sys/sfx.id"

    lfs_file_t fp;

    xSemaphoreTake(sflash_vfs_littlefs.fs.littlefs.mutex, portMAX_DELAY);

    if(LFS_ERR_OK == lfs_file_open(&sflash_vfs_littlefs.fs.littlefs.lfs, &fp, SFX_ID_PATH, LFS_O_RDONLY)){
        uint8_t id[4];
        int sz_out = lfs_file_read(&sflash_vfs_littlefs.fs.littlefs.lfs, &fp, id, sizeof(id));
        if (sz_out > LFS_ERR_OK) {
            if (config_set_sigfox_id(id)) {
                mp_hal_delay_ms(250);
                ets_printf("SFX ID write OK\n");
            }
        }
        lfs_file_close(&sflash_vfs_littlefs.fs.littlefs.lfs, &fp);

        if (sz_out > LFS_ERR_OK) {
            // delete the mac address file
            lfs_remove(&sflash_vfs_littlefs.fs.littlefs.lfs, SFX_ID_PATH);
        }
    }

    xSemaphoreGive(sflash_vfs_littlefs.fs.littlefs.mutex);
}

void sigfox_update_pac (void) {
    #define SFX_PAC_PATH          "/sys/sfx.pac"

    lfs_file_t fp;

    xSemaphoreTake(sflash_vfs_littlefs.fs.littlefs.mutex, portMAX_DELAY);

    if(LFS_ERR_OK == lfs_file_open(&sflash_vfs_littlefs.fs.littlefs.lfs, &fp, SFX_PAC_PATH, LFS_O_RDONLY)){
        uint8_t pac[8];
        int sz_out = lfs_file_read(&sflash_vfs_littlefs.fs.littlefs.lfs, &fp, pac, sizeof(pac));
        if (sz_out > LFS_ERR_OK) {
            if (config_set_sigfox_pac(pac)) {
                mp_hal_delay_ms(250);
                ets_printf("SFX PAC write OK\n");
            }
        }
        lfs_file_close(&sflash_vfs_littlefs.fs.littlefs.lfs, &fp);

        if (sz_out > LFS_ERR_OK) {
            // delete the mac address file
            lfs_remove(&sflash_vfs_littlefs.fs.littlefs.lfs, SFX_PAC_PATH);
        }
    }

    xSemaphoreGive(sflash_vfs_littlefs.fs.littlefs.mutex);
}

void sigfox_update_private_key (void) {
    #define SFX_PRIVATE_KEY_PATH          "/sys/sfx_private.key"

    lfs_file_t fp;

    xSemaphoreTake(sflash_vfs_littlefs.fs.littlefs.mutex, portMAX_DELAY);

    if(LFS_ERR_OK == lfs_file_open(&sflash_vfs_littlefs.fs.littlefs.lfs, &fp, SFX_PRIVATE_KEY_PATH, LFS_O_RDONLY)){
       uint8_t key[16];
       int sz_out = lfs_file_read(&sflash_vfs_littlefs.fs.littlefs.lfs, &fp, key, sizeof(key));
       if (sz_out > LFS_ERR_OK) {
           if (config_set_sigfox_private_key(key)) {
               mp_hal_delay_ms(250);
               ets_printf("SFX private key write OK\n");
           }
       }
       lfs_file_close(&sflash_vfs_littlefs.fs.littlefs.lfs, &fp);

       if (sz_out > LFS_ERR_OK) {
           // delete the mac address file
           lfs_remove(&sflash_vfs_littlefs.fs.littlefs.lfs, SFX_PRIVATE_KEY_PATH);
       }
   }

    xSemaphoreGive(sflash_vfs_littlefs.fs.littlefs.mutex);
}

void sigfox_update_public_key (void) {
    #define SFX_PUBLIC_KEY_PATH          "/sys/sfx_public.key"

    lfs_file_t fp;

    xSemaphoreTake(sflash_vfs_littlefs.fs.littlefs.mutex, portMAX_DELAY);

    if(LFS_ERR_OK == lfs_file_open(&sflash_vfs_littlefs.fs.littlefs.lfs, &fp, SFX_PUBLIC_KEY_PATH, LFS_O_RDONLY)){
       uint8_t key[16];
       int sz_out = lfs_file_read(&sflash_vfs_littlefs.fs.littlefs.lfs, &fp, key, sizeof(key));
       if (sz_out > LFS_ERR_OK) {
           if (config_set_sigfox_public_key(key)) {
               mp_hal_delay_ms(250);
               ets_printf("SFX public key write OK\n");
           }
       }
       lfs_file_close(&sflash_vfs_littlefs.fs.littlefs.lfs, &fp);

       if (sz_out > LFS_ERR_OK) {
           // delete the mac address file
           lfs_remove(&sflash_vfs_littlefs.fs.littlefs.lfs, SFX_PUBLIC_KEY_PATH);
       }
   }

    xSemaphoreGive(sflash_vfs_littlefs.fs.littlefs.mutex);
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
        if (SFX_ERR_NONE != SIGFOX_API_set_std_config(rcz_config_words[sfx_rcz_id], false)) {
            return SIGFOX_STATUS_ERR;
        }

        if (sfx_rcz_id == E_SIGFOX_RCZ2 || sfx_rcz_id == E_SIGFOX_RCZ4) {
            SIGFOX_API_reset();
        }
        memcpy(rcz_current_config_words, rcz_config_words[sfx_rcz_id], sizeof(rcz_current_config_words));
    }
    return SFX_ERR_NONE;
}

static sfx_error_t modsigfox_sfx_send(sigfox_cmd_rx_data_t *cmd_rx_data, sigfox_rx_data_t *rx_data) {
#if defined(FIPY) || defined(LOPY4)
    xSemaphoreTake(xLoRaSigfoxSem, portMAX_DELAY);
#endif
    uint32_t now = mp_hal_ticks_s();
    sfx_error_t sfx_error;

    if (sfx_rcz_id == E_SIGFOX_RCZ2 || sfx_rcz_id == E_SIGFOX_RCZ4) {
        sfx_u8 info;
        if (SFX_ERR_NONE == SIGFOX_API_get_info(&info)) {
            if ((info & 0b00001111) || !(info & 0b11110000)) {   // FCC channel not default or no free micro-channels on the default one
                SIGFOX_API_reset();    // Need to reset the Sigfox API in order to return to the default channel
                int32_t tx_delay = now - tx_timestamp;
                if (tx_timestamp > 0) {
                    // We must wait in order to respect FCC regulations
                    if (tx_delay >= 0 && tx_delay < SFX_RESET_FCC_MIN_DELAY_S) {
                        mp_hal_delay_ms((SFX_RESET_FCC_MIN_DELAY_S - tx_delay) * 1000);
                    }
                }
            }
        } else {
            // reset anyway in order to return to the default channel
            SIGFOX_API_reset();
        }
    }

    tx_timestamp = now;     // save the current timestamp

#if defined(FIPY)
    RADIO_warm_up_crystal(rcz_frequencies[sfx_rcz_id][0]);
#endif

    if (cmd_rx_data->cmd_u.info.tx.oob) {
        sfx_error =  SIGFOX_API_send_outofband();
        goto end_send;
    } else {
        if (cmd_rx_data->cmd_u.info.tx.len > 0) {
            sfx_error =  SIGFOX_API_send_frame(cmd_rx_data->cmd_u.info.tx.data, cmd_rx_data->cmd_u.info.tx.len, rx_data->data,
                                         cmd_rx_data->cmd_u.info.tx.tx_repeat, cmd_rx_data->cmd_u.info.tx.receive);
            goto end_send;
        }
        sfx_error = SIGFOX_API_send_bit(cmd_rx_data->cmd_u.info.tx.data[0] ? SFX_TRUE : SFX_FALSE,
                                   rx_data->data, cmd_rx_data->cmd_u.info.tx.tx_repeat,
                                   cmd_rx_data->cmd_u.info.tx.receive);
        goto end_send;
    }
end_send:
#if defined(FIPY) || defined(LOPY4)
    RADIO_reset_registers();
    xSemaphoreGive(xLoRaSigfoxSem);
#endif
    return sfx_error;
}

static void TASK_Sigfox(void *pvParameters) {
    sigfox_cmd_rx_data_t cmd_rx_data;

    TIMER_bitrate_create();
    TIMER_downlinnk_timer_create();
    TIMER_carrier_sense_timer_create();
#if !defined(FIPY) && !defined(LOPY4)
    TIMER_RxTx_done_timer_create();
#endif

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
                     #if !defined(FIPY) && !defined(LOPY4)
                        TIMER_RxTx_done_stop();   // stop the RxTx timer while reconfiguring
                     #endif
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
                        #if !defined(FIPY) && !defined(LOPY4)
                            // write radio registers
                            fsk_register_config();
                            // calibrate radio according to errata
                            fsk_manual_calibration();
                        #endif
                        }
                        sigfox_obj.mode = cmd_rx_data.cmd_u.info.init.mode;
                        sigfox_obj.frequency = cmd_rx_data.cmd_u.info.init.frequency;
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
                        #if !defined(FIPY) && !defined(LOPY4)
                            // stop the TxRx timer before reconfiguring for Tx
                            TIMER_RxTx_done_stop();
                            trxSpiCmdStrobe(CC112X_SIDLE);
                            // we must start the timer before calling fsk_cc112x_tx()
                            TIMER_RxTx_done_start();
                            fsk_cc112x_tx (cmd_rx_data.cmd_u.info.tx.data, cmd_rx_data.cmd_u.info.tx.len);
                            sigfox_obj.state = E_SIGFOX_STATE_TX;
                        #endif
                        }
                    }
                    break;
                case E_SIGFOX_CMD_TEST:
                #if defined(FIPY)
                    xSemaphoreTake(xLoRaSigfoxSem, portMAX_DELAY);
                #endif
                if (cmd_rx_data.cmd_u.info.test.mode <= SFX_TEST_MODE_TX_SYNTH) {
                        SIGFOX_API_test_mode(cmd_rx_data.cmd_u.info.test.mode, cmd_rx_data.cmd_u.info.test.config);
                    } else { // start or stop CW
                        if (cmd_rx_data.cmd_u.info.test.mode == E_MODSIGOFX_TEST_MODE_START_CW) {
                            RADIO_start_unmodulated_cw (cmd_rx_data.cmd_u.info.test.config);
                        } else if (cmd_rx_data.cmd_u.info.test.mode == E_MODSIGOFX_TEST_MODE_STOP_CW) {
                            RADIO_stop_unmodulated_cw (cmd_rx_data.cmd_u.info.test.config);
                        #if defined(FIPY) || defined(LOPY4)
                            RADIO_reset_registers();
                        #endif
                        } else {
                        #if defined(FIPY)
                            RADIO_warm_up_crystal(rcz_frequencies[sfx_rcz_id][0]);
                        #endif
                            for (int i = 0; i < 20; i++) {
                                uint8_t tx_frame[18] = { 0xAA, 0xAA, 0xA3, 0x5F, 0x8D, 0xC3, 0x74, 0x7D, 0x18,
                                                         0x00, 0xAA, 0xAA, 0xB8, 0x01, 0x5A, 0x5F, 0x53, 0x4C };
                                MANUF_API_rf_init(SFX_RF_MODE_TX);
                                MANUF_API_change_frequency(cmd_rx_data.cmd_u.info.test.config);
                                MANUF_API_rf_send(tx_frame, sizeof(tx_frame));
                                vTaskDelay(50 / portTICK_RATE_MS);
                            }
                        #if defined(FIPY) || defined(LOPY4)
                            RADIO_reset_registers();
                        #endif
                        }
                    }
                    sigfox_obj.state = E_SIGFOX_STATE_TEST;
                    xEventGroupSetBits(sigfoxEvents, SIGFOX_STATUS_COMPLETED);
                #if defined(FIPY) || defined(LOPY4)
                    xSemaphoreGive(xLoRaSigfoxSem);
                #endif
                    break;
                default:
                    break;
                }
            }
        #if !defined(FIPY) && !defined(LOPY4)
            else {
                if (sigfox_obj.state == E_SIGFOX_STATE_IDLE) {
                    // set radio in RX
                    fsk_rx_register_config();
                    RADIO_change_frequency(sigfox_obj.frequency);
                    trxSpiCmdStrobe(CC112X_SRX);
                    sigfox_obj.state = E_SIGFOX_STATE_RX;
                    TIMER_RxTx_done_start();
                } else if (sigfox_obj.state == E_SIGFOX_STATE_RX) {
                    fsk_cc112x_rx();
                }
            }
        #endif
            break;
    #if !defined(FIPY) && !defined(LOPY4)
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
    #endif
        default:
            break;
        }
    }
}

#if !defined(FIPY) && !defined(LOPY4)
static void fsk_cc112x_tx (uint8_t *data, uint32_t len) {
    uint8 packet[FSK_TX_PAYLOAD_SIZE_MAX + 4];

    // clear the semaphore flag
    packetSemaphore = ISR_IDLE;

    fsk_tx_register_config();

    RADIO_change_frequency(sigfox_obj.frequency);

    // Write packet to tx fifo
    packet[0] = len;
    memcpy(&packet[1], data, len);
    packet[len + 1] = 0;

    cc112xSpiWriteTxFifo(packet, len + 1);

    // strobe TX to send packet
    trxSpiCmdStrobe(CC112X_STX);
}

static void fsk_cc112x_rx (void) {
    sigfox_rx_data_t rx_data;
    uint8_t rx_last, status;
    uint8_t rxlastindex;

    // wait for the packet received interrupt
    if (packetSemaphore == ISR_ACTION_REQUIRED) {

        /* IMPORTANT : using the register CC112X_NUM_RXBYTES gives wrong values concerning the packet length
         * DO NOT USE THIS REGISTER, use RXLAST instead which give the last index in the RX FIFO
         * and flush the FIFO after reading it   */
        rxlastindex = 0;

        // Read 10 times to get around a bug
        for (int i = 0; i < 10; i++) {
            cc112xSpiReadReg(CC112X_RXLAST, &rx_last, 1);
            rxlastindex |= rx_last;
        }

        /* Check that we have bytes in the fifo */
        if (rxlastindex != 0) {
            // read marcstate to check for RX FIFO error
            cc112xSpiReadReg(CC112X_MARCSTATE, &status, 1);

            // mask out marcstate bits and check if we have a RX FIFO error
            if ((status & 0x1F) == RX_FIFO_ERROR && (rxlastindex + 1 < sizeof(rx_data.data))) {
                // flush the RX Fifo
                trxSpiCmdStrobe(CC112X_SFRX);
            } else {
                // read n bytes from rx fifo
                cc112xSpiReadRxFifo(rx_data.data, rxlastindex + 1);

                // Once read, Flush RX Fifo
                trxSpiCmdStrobe(CC112X_SFRX);

                // check CRC ok (CRC_OK: bit7 in second status byte)
                // this assumes status bytes are appended in RX_FIFO
                // (PKT_CFG1.APPEND_STATUS = 1.)
                // if CRC is disabled the CRC_OK field will read 1
                if (rx_data.data[rxlastindex] & 0x80) {
                    rx_data.len = rx_data.data[0];
                    memmove(rx_data.data, &rx_data.data[1], rx_data.data[0]);
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

static void fsk_register_config(void) {
    // reset the radio
    trxSpiCmdStrobe(CC112X_SRES);

    // write registers to radio
    for(uint16 i = 0; i < sizeof(cc1125PreferredSettings) / sizeof(registerSetting_t); i++) {
        uint8 writeByte = cc1125PreferredSettings[i].data;
        cc112xSpiWriteReg( cc1125PreferredSettings[i].addr, &writeByte, 1);
    }
}

static void fsk_tx_register_config(void) {
    // write registers to radio
    for(uint16 i = 0; i < sizeof(cc1125TxSettings) / sizeof(registerSetting_t); i++) {
        uint8 writeByte = cc1125TxSettings[i].data;
        cc112xSpiWriteReg( cc1125TxSettings[i].addr, &writeByte, 1);
    }
}

static void fsk_rx_register_config(void) {
    // write registers to radio
    for(uint16 i = 0; i < sizeof(cc1125RxSettings) / sizeof(registerSetting_t); i++) {
        uint8 writeByte = cc1125RxSettings[i].data;
        cc112xSpiWriteReg( cc1125RxSettings[i].addr, &writeByte, 1);
    }
}

static void fsk_manual_calibration(void) {

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
#endif

static void sigfox_send_cmd (sigfox_cmd_rx_data_t *cmd_rx_data) {
    xEventGroupClearBits(sigfoxEvents, SIGFOX_STATUS_COMPLETED | SIGFOX_STATUS_ERR);

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

#if !defined(FIPY) && !defined(LOPY4)
static void sigfox_validate_frequency (uint32_t frequency) {
    if (frequency < FSK_FREQUENCY_MIN || frequency > FSK_FREQUENCY_MAX) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "frequency %d out of range", frequency));
    }
}
#endif

/******************************************************************************/
// Micro Python bindings; Sigfox class

mp_obj_t sigfox_init_helper(sigfox_obj_t *self, const mp_arg_val_t *args) {
    sigfox_cmd_rx_data_t cmd_rx_data;
    uint8_t mode = args[0].u_int;
    uint8_t rcz = args[1].u_int;
    uint32_t frequency = 0;

#if !defined(FIPY) && !defined(LOPY4)
    if (mode > E_SIGFOX_MODE_FSK) {
#else
    if (mode != E_SIGFOX_MODE_SIGFOX) {
#endif
        mp_raise_ValueError("invalid mode");
    } else if (mode == E_SIGFOX_MODE_SIGFOX) {
        if (rcz > E_SIGFOX_RCZ4) {
            mp_raise_ValueError("invalid RCZ");
    #if !defined(FIPY) && !defined(LOPY4)
        } else if (args[2].u_obj != mp_const_none) {
            mp_raise_ValueError("frequency is only valid in FSK mode");
    #endif
        }
#if !defined(FIPY) && !defined(LOPY4)
    } else {
        frequency = mp_obj_get_int(args[2].u_obj);
        sigfox_validate_frequency(frequency);
#endif
    }

    cmd_rx_data.cmd_u.cmd = E_SIGFOX_CMD_INIT;
    cmd_rx_data.cmd_u.info.init.frequency = frequency;
    cmd_rx_data.cmd_u.info.init.mode = mode;
    cmd_rx_data.cmd_u.info.init.rcz = rcz;
    sigfox_send_cmd (&cmd_rx_data);
    self->state = E_SIGFOX_STATE_IDLE;

    return mp_const_none;
}

mp_obj_t sigfox_mac(mp_obj_t self_in) {
    uint8_t mac[8];
    config_get_lpwan_mac(mac);
    return mp_obj_new_bytes((const byte *)mac, sizeof(mac));
}

mp_obj_t sigfox_id(mp_obj_t self_in) {
    uint8_t id[4];
    config_get_sigfox_id(id);
    return mp_obj_new_bytes((const byte *)id, sizeof(id));
}

mp_obj_t sigfox_pac(mp_obj_t self_in) {
    uint8_t pac[8];
    config_get_sigfox_pac(pac);
    return mp_obj_new_bytes((const byte *)pac, sizeof(pac));
}

mp_obj_t sigfox_test_mode(mp_obj_t self_in, mp_obj_t mode, mp_obj_t config) {
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

mp_obj_t sigfox_cw(mp_obj_t self_in, mp_obj_t frequency, mp_obj_t start) {
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

mp_obj_t sigfox_frequencies(mp_obj_t self_in) {
    mp_obj_t tuple[2];
    tuple[0] = mp_obj_new_int(rcz_frequencies[sfx_rcz_id][0]);
    tuple[1] = mp_obj_new_int(rcz_frequencies[sfx_rcz_id][1]);

    return mp_obj_new_tuple(2, tuple);
}

mp_obj_t sigfox_config(mp_uint_t n_args, const mp_obj_t *args) {
    if (n_args == 1) {
        if (sfx_rcz_id != E_SIGFOX_RCZ1) {
            mp_obj_t tuple[3];
            tuple[0] = mp_obj_new_int_from_uint(rcz_current_config_words[0]);
            tuple[1] = mp_obj_new_int_from_uint(rcz_current_config_words[1]);
            tuple[2] = mp_obj_new_int_from_uint(rcz_current_config_words[2]);
            return mp_obj_new_tuple(3, tuple);
        } else {
            return mp_const_none;
        }
    } else {
        if (sfx_rcz_id != E_SIGFOX_RCZ1) {
            mp_obj_t *config;
            mp_obj_get_array_fixed_n(args[1], 3, &config);
            sfx_u32 config_words[3];
            config_words[0] = mp_obj_get_int_truncated(config[0]);
            config_words[1] = mp_obj_get_int_truncated(config[1]);
            config_words[2] = mp_obj_get_int_truncated(config[2]);
            if (SFX_ERR_NONE != SIGFOX_API_set_std_config(config_words, false)) {
                nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
            } else {
                memcpy(rcz_current_config_words, config_words, sizeof(rcz_current_config_words));
            }
            return mp_const_none;
        } else {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError,"configuration not possible for RCZ1"));
        }
    }
}

mp_obj_t sigfox_public_key(mp_uint_t n_args, const mp_obj_t *args) {
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

mp_obj_t sigfox_rssi(mp_obj_t self_in) {
    int8_t rssi;
    int16_t f_rssi;
    MANUF_API_get_rssi(&rssi);
    f_rssi = rssi - 100;   // Sigfox backend compatibility
    return MP_OBJ_NEW_SMALL_INT(f_rssi);
}

mp_obj_t sigfox_rssi_offset(mp_uint_t n_args, const mp_obj_t *args) {
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

mp_obj_t sigfox_freq_offset(mp_uint_t n_args, const mp_obj_t *args) {
    if (n_args == 1) {
        int16_t freq_offset;
        if (SFX_ERR_MANUF_NONE != MANUF_API_get_nv_mem(SFX_NVMEM_FREQ, (sfx_u16 *)&freq_offset)) {
            freq_offset = 0;
        }
        return mp_obj_new_int(freq_offset);
    } else {
        if (mp_obj_get_int(args[1]) > INT16_MAX || mp_obj_get_int(args[1]) < INT16_MIN) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "the freq offset is out of range (-32768 < freq_offset < 32767)"));
        }

        if (SFX_ERR_MANUF_NONE != MANUF_API_set_nv_mem(SFX_NVMEM_FREQ, mp_obj_get_int(args[1]))) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
        }
        return mp_const_none;
    }
}

mp_obj_t sigfox_version(mp_obj_t self_in) {
    sfx_u8 *ptr;
    sfx_u8 size;

    if (SFX_ERR_NONE == SIGFOX_API_get_version(&ptr, &size)) {
        return mp_obj_new_str((char *)ptr, size - 1);
    }
    nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
}

mp_obj_t sigfox_info(mp_obj_t self_in) {
    sfx_u8 info;

    if (sfx_rcz_id != E_SIGFOX_RCZ1) {
        if (SFX_ERR_NONE == SIGFOX_API_get_info(&info)) {
            mp_obj_t tuple[2];
            if (sfx_rcz_id == E_SIGFOX_RCZ3) {  // RCZ3 - LBT
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

mp_obj_t sigfox_reset(mp_obj_t self_in) {
    // only for RCZ2 and RCZ4
    if (sfx_rcz_id == E_SIGFOX_RCZ2 || sfx_rcz_id == E_SIGFOX_RCZ4) {
        SIGFOX_API_reset();
    }
    return mp_const_none;
}

///******************************************************************************/
//// Micro Python bindings; Sigfox socket

int sigfox_socket_socket (mod_network_socket_obj_t *s, int *_errno) {
    if (sigfox_obj.state == E_SIGFOX_STATE_NOINIT) {
        *_errno = MP_ENETDOWN;
        return -1;
    }
    s->sock_base.u.sd = 1;
    SIGFOX_SOCKET_SET_RX(s->sock_base.u.sd, false);
    SIGFOX_SOCKET_SET_TX_REPEAT(s->sock_base.u.sd, 2);
    SIGFOX_SOCKET_SET_OOB(s->sock_base.u.sd, false);
    return 0;
}

void sigfox_socket_close (mod_network_socket_obj_t *s) {
}

int sigfox_socket_send(mod_network_socket_obj_t *s, const byte *buf, mp_uint_t len, int *_errno) {
    sigfox_cmd_rx_data_t cmd_rx_data;
    SIGFOX_CHECK_SOCKET(s);

    uint8_t maxlen = (sigfox_obj.mode == E_SIGFOX_MODE_SIGFOX) ? SIGFOX_TX_PAYLOAD_SIZE_MAX : FSK_TX_PAYLOAD_SIZE_MAX;

    if (len > maxlen) {
        *_errno = MP_EMSGSIZE;
        return -1;
    }

    cmd_rx_data.cmd_u.cmd = E_SIGFOX_CMD_TX;
    cmd_rx_data.cmd_u.info.tx.len = len;
    cmd_rx_data.cmd_u.info.tx.tx_repeat = SIGFOX_SOCKET_GET_TX_REPEAT(s->sock_base.u.sd);
    cmd_rx_data.cmd_u.info.tx.receive = SIGFOX_SOCKET_GET_RX(s->sock_base.u.sd) ? SFX_TRUE : SFX_FALSE;
    cmd_rx_data.cmd_u.info.tx.oob = SIGFOX_SOCKET_GET_OOB(s->sock_base.u.sd);
    if (len > 0) {
        memcpy(cmd_rx_data.cmd_u.info.tx.data, buf, len);
    } else {
        if (SIGFOX_SOCKET_GET_BIT(s->sock_base.u.sd)) {
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

    xEventGroupClearBits(sigfoxEvents, SIGFOX_STATUS_COMPLETED | SIGFOX_STATUS_ERR);

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

int sigfox_socket_recv (mod_network_socket_obj_t *s, byte *buf, mp_uint_t len, int *_errno) {
    SIGFOX_CHECK_SOCKET(s);
    int ret = sigfox_recv (buf, len, s->sock_base.timeout);
    if (ret < 0) {
        *_errno = MP_EAGAIN;
        return -1;
    }
    return ret;
}

int sigfox_socket_setsockopt(mod_network_socket_obj_t *s, mp_uint_t level, mp_uint_t opt, const void *optval, mp_uint_t optlen, int *_errno) {
    SIGFOX_CHECK_SOCKET(s);
    if (level != SOL_SIGFOX) {
        *_errno = MP_EOPNOTSUPP;
        return -1;
    }

    if (opt == SO_SIGFOX_RX) {
        SIGFOX_SOCKET_SET_RX(s->sock_base.u.sd, *(uint8_t *)optval);
    } else if (opt == SO_SIGFOX_TX_REPEAT) {
        uint8_t tx_repeat = *(uint8_t *)optval;
        if (tx_repeat > 2) {
            *_errno = MP_EOPNOTSUPP;
            return -1;
        }
        SIGFOX_SOCKET_SET_TX_REPEAT(s->sock_base.u.sd, tx_repeat);
    } else if (opt == SO_SIGFOX_OOB) {
        SIGFOX_SOCKET_SET_OOB(s->sock_base.u.sd, *(uint8_t *)optval);
    } else if (opt == SO_SIGFOX_BIT) {
        SIGFOX_SOCKET_SET_BIT(s->sock_base.u.sd, *(uint8_t *)optval);
    } else {
        *_errno = MP_EOPNOTSUPP;
        return -1;
    }
    return 0;
}

int sigfox_socket_settimeout (mod_network_socket_obj_t *s, mp_int_t timeout_ms, int *_errno) {
    SIGFOX_CHECK_SOCKET(s);
    s->sock_base.timeout = timeout_ms;
    return 0;
}

int sigfox_socket_ioctl (mod_network_socket_obj_t *s, mp_uint_t request, mp_uint_t arg, int *_errno) {
    mp_int_t ret = 0;

    SIGFOX_CHECK_SOCKET(s);
    if (request == MP_STREAM_POLL) {
        mp_uint_t flags = arg;
        if ((flags & MP_STREAM_POLL_RD) && sigfox_rx_any()) {
            ret |= MP_STREAM_POLL_RD;
        }
        if ((flags & MP_STREAM_POLL_WR) && sigfox_tx_space()) {
            ret |= MP_STREAM_POLL_WR;
        }
    } else {
        *_errno = MP_EINVAL;
        ret = MP_STREAM_ERROR;
    }
    return ret;
}
