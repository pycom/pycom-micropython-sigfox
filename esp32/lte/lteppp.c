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

#include "lteppp.h"

/******************************************************************************
 DEFINE CONSTANTS
 ******************************************************************************/

#define LTE_UART_BUFFER_SIZE                                            (2048)
#define LTE_OK_RSP                                                      "OK"
#define LTE_RX_TIMEOUT                                                  (5000 / portTICK_RATE_MS)
#define PPPOS_MUTEX_TIMEOUT                                             (5000 / portTICK_RATE_MS)
#define PPPOS_CLIENT_STACK_SIZE                                         (3072)

/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/
static uint8_t lteppp_rx_buffer[LTE_UART_BUFFER_SIZE];

/******************************************************************************
 DEFINE PUBLIC FUNCTIONS
 ******************************************************************************/
void lteppp_init(void) {
    // configure the UART pins
    pin_config(pin, MICROPY_LTE_TX_PIN, -1, GPIO_MODE_OUTPUT, MACHPIN_PULL_NONE, 1);
    pin_config(pin, MICROPY_LTE_RX_PIN, -1, GPIO_MODE_INPUT, MACHPIN_PULL_NONE, 1);
    pin_config(pin, MICROPY_LTE_RTS_PIN, -1, GPIO_MODE_OUTPUT, MACHPIN_PULL_NONE, 1);
    pin_config(pin, MICROPY_LTE_CTS_PIN, -1, GPIO_MODE_INPUT, MACHPIN_PULL_NONE, 1);

    // initialize the UART interface
    uart_config_t config;
    config.baud_rate = baudrate;
    config.data_bits = data_bits;
    config.parity = parity;
    config.stop_bits = stop_bits;
    config.flow_ctrl = flowcontrol;
    config.rx_flow_ctrl_thresh = 64;
    uart_param_config(MICROPY_LTE_UART_ID, &config);

    // install the UART driver
    uart_driver_install(MICROPY_LTE_UART_ID, LTE_UART_BUFFER_SIZE, LTE_UART_BUFFER_SIZE, 0, NULL, 0, NULL);
}

bool lte_send_at_cmd(const char *cmd, uint32_t cmd_len, char *rsp, uint32_t max_rsp_len, uint32_t timeout) {
    uart_write_bytes(MICROPY_LTE_UART_ID, cmd, cmd_len);
    vTaskDelay(1 / portTICK_RATE_MS);
    uart_wait_tx_done(MICROPY_LTE_UART_ID, 50 / portTICK_RATE_MS);

    uint32_t rx_len = uart_read_bytes(MICROPY_LTE_UART_ID, (uint8_t *)rsp, max_rsp_len - 1, timeout / portTICK_RATE_MS);
    if (rx_len > 0) {
        // NULL terminate the string
        rsp[rx_len] = '\0';
        printf("LTE rsp: %s\n", rsp);
        if (strstr(rsp, LTE_OK_RSP) != NULL) {
            return true;
        }
        return false;
    }
    printf("LTE timeout\n");
    return false;
}

