/*
 * Copyright (c) 2018, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include <string.h>
#include "py/mpconfig.h"
#include "py/obj.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

#include "driver/uart.h"
#include "driver/gpio.h"
#include "tcpip_adapter.h"
#include "netif/ppp/pppos.h"
#include "netif/ppp/ppp.h"
#include "lwip/pppapi.h"

#include "machpin.h"
#include "lteppp.h"
#include "pins.h"

/******************************************************************************
 DEFINE CONSTANTS
 ******************************************************************************/

#define LTE_UART_ID                                             2
#define LTE_TRX_WAIT_MS(len)                                    (((len + 1) * 12 * 1000) / MICROPY_LTE_UART_BAUDRATE)

/******************************************************************************
 DEFINE TYPES
 ******************************************************************************/

/******************************************************************************
 DECLARE EXPORTED DATA
 ******************************************************************************/
extern TaskHandle_t xLTETaskHndl;

/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/
static char lteppp_trx_buffer[LTE_UART_BUFFER_SIZE];
static uart_dev_t* lteppp_uart_reg;
static QueueHandle_t xCmdQueue;
static QueueHandle_t xRxQueue;
static lte_state_t lte_state;

/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/
static void TASK_LTE (void *pvParameters);
static bool lte_send_at_cmd(const char *cmd, uint32_t timeout);

/******************************************************************************
 DEFINE PUBLIC FUNCTIONS
 ******************************************************************************/
void lteppp_init(void) {
    // configure the UART pins
    pin_config(MICROPY_LTE_TX_PIN, -1, U2TXD_OUT_IDX, GPIO_MODE_OUTPUT, MACHPIN_PULL_NONE, 1);
    pin_config(MICROPY_LTE_RX_PIN, U2RXD_IN_IDX, -1, GPIO_MODE_INPUT, MACHPIN_PULL_NONE, 1);
    pin_config(MICROPY_LTE_RTS_PIN, -1, U2RTS_OUT_IDX, GPIO_MODE_OUTPUT, MACHPIN_PULL_NONE, 1);
    pin_config(MICROPY_LTE_CTS_PIN, U2CTS_IN_IDX, -1, GPIO_MODE_INPUT, MACHPIN_PULL_NONE, 1);

    // initialize the UART interface
    uart_config_t config;
    config.baud_rate = MICROPY_LTE_UART_BAUDRATE;
    config.data_bits = UART_DATA_8_BITS;
    config.parity = UART_PARITY_DISABLE;
    config.stop_bits = UART_STOP_BITS_1;
    config.flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS;
    config.rx_flow_ctrl_thresh = 64;
    uart_param_config(LTE_UART_ID, &config);

    // install the UART driver
    uart_driver_install(LTE_UART_ID, LTE_UART_BUFFER_SIZE, LTE_UART_BUFFER_SIZE, 0, NULL, 0, NULL);
    lteppp_uart_reg = &UART2;

    // disable the delay between transfers
    lteppp_uart_reg->idle_conf.tx_idle_num = 0;

    // configure the rx timeout threshold
    lteppp_uart_reg->conf1.rx_tout_thrhd = 10 & UART_RX_TOUT_THRHD_V;

    xCmdQueue = xQueueCreate(LTE_CMD_QUEUE_SIZE_MAX, sizeof(lte_task_cmd_data_t));
    xRxQueue = xQueueCreate(LTE_RSP_QUEUE_SIZE_MAX, LTE_AT_RSP_SIZE_MAX);

    xTaskCreatePinnedToCore(TASK_LTE, "LTE", LTE_TASK_STACK_SIZE / sizeof(StackType_t), NULL, LTE_TASK_PRIORITY, &xLTETaskHndl, 1);
}

bool lteppp_send_at_command (lte_task_cmd_data_t *cmd, lte_task_rsp_data_t *rsp) {
    xQueueSend(xCmdQueue, (void *)cmd, (TickType_t)portMAX_DELAY);
    xQueueReceive(xRxQueue, rsp, (TickType_t)portMAX_DELAY);
    if (rsp->ok) {
        return true;
    }
    return false;
}

lte_state_t lteppp_get_state(void) {
    return lte_state;
}

/******************************************************************************
 DEFINE PRIVATE FUNCTIONS
 ******************************************************************************/

static void TASK_LTE (void *pvParameters) {
    lte_task_cmd_data_t *lte_task_cmd = (lte_task_cmd_data_t *)lteppp_trx_buffer;
    lte_task_rsp_data_t *lte_task_rsp = (lte_task_rsp_data_t *)lteppp_trx_buffer;

    lte_state = E_LTE_IDLE;

    for (;;) {
        vTaskDelay(2);

        if (xQueueReceive(xCmdQueue, lteppp_trx_buffer, 0)) {
            switch (lte_task_cmd->cmd) {
            case E_LTE_CMD_AT:
                if (lte_send_at_cmd(lte_task_cmd->data, lte_task_cmd->timeout)) {
                    lte_task_rsp->ok = true;
                } else {
                    lte_task_rsp->ok = false;
                }
                xQueueSend(xRxQueue, (void *)lte_task_rsp, (TickType_t)portMAX_DELAY);
                break;
            case E_LTE_CMD_PPP_ENTER:
                break;
            case E_LTE_CMD_PPP_EXIT:
                break;
            default:
                break;
            }
        } else {
            lte_send_at_cmd("AT+CEREG?", LTE_RX_TIMEOUT_DEF_MS);
            char *pos;
            if ((pos = strstr(lteppp_trx_buffer, "+CEREG: 2,1,")) && (strlen(pos) >= 21)) { // FIXME
                lte_state = E_LTE_ATTACHED;
            } else {
                lte_state = E_LTE_IDLE;
            }
        }
    }
}

static bool lte_send_at_cmd(const char *cmd, uint32_t timeout) {
    uint32_t cmd_len = strlen(cmd);
    // flush the rx buffer first
    uart_flush(LTE_UART_ID);
    // then send the command
    uart_write_bytes(LTE_UART_ID, cmd, cmd_len);
    uart_write_bytes(LTE_UART_ID, "\r\n", 2);
    uart_wait_tx_done(LTE_UART_ID, LTE_TRX_WAIT_MS(cmd_len) / portTICK_RATE_MS);
    vTaskDelay(1 / portTICK_RATE_MS);

    uint32_t rx_len = 0;
    // wait until characters start arriving
    do {
        vTaskDelay(1 / portTICK_RATE_MS);
        uart_get_buffered_data_len(LTE_UART_ID, &rx_len);
        if (timeout > 0) {
            timeout--;
        }
    } while (timeout > 0 && 0 == rx_len);

    if (rx_len > 0) {
        // try to read up to the size of the buffer minus null terminator (minus 2 because we store the OK status in the last byte)
        rx_len = uart_read_bytes(LTE_UART_ID, (uint8_t *)lteppp_trx_buffer, sizeof(lteppp_trx_buffer) - 2, LTE_TRX_WAIT_MS(sizeof(lteppp_trx_buffer)) / portTICK_RATE_MS);
        if (rx_len > 0) {
            // NULL terminate the string
            lteppp_trx_buffer[rx_len] = '\0';
            if (strstr(lteppp_trx_buffer, LTE_OK_RSP) != NULL) {
                return true;
            }
            return false;
        }
    }
    return false;
}
