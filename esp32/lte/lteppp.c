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
#include "mpsleep.h"
#include "esp32_mphal.h"

/******************************************************************************
 DEFINE CONSTANTS
 ******************************************************************************/

#define LTE_TRX_WAIT_MS(len)                                    (((len + 1) * 12 * 1000) / MICROPY_LTE_UART_BAUDRATE)
#define LTE_TASK_PERIOD_MS                                      (2)

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
static lte_state_t lteppp_lte_state;
static SemaphoreHandle_t xLTESem;
static ppp_pcb *lteppp_pcb;         // PPP control block
struct netif lteppp_netif;          // PPP net interface

static uint32_t lte_ipv4addr;
static uint32_t lte_gw;
static uint32_t lte_netmask;
static ip6_addr_t lte_ipv6addr;

static bool lteppp_init_complete = false;

/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/
static void TASK_LTE (void *pvParameters);
static bool lteppp_send_at_cmd_exp(const char *cmd, uint32_t timeout, const char *expected_rsp);
static bool lteppp_send_at_cmd(const char *cmd, uint32_t timeout);
static bool lteppp_check_sim_present(void);
static void lteppp_status_cb (ppp_pcb *pcb, int err_code, void *ctx);
static uint32_t lteppp_output_callback(ppp_pcb *pcb, u8_t *data, u32_t len, void *ctx);

/******************************************************************************
 DEFINE PUBLIC FUNCTIONS
 ******************************************************************************/
void lteppp_init(void) {
    lteppp_lte_state = E_LTE_INIT;

    xCmdQueue = xQueueCreate(LTE_CMD_QUEUE_SIZE_MAX, sizeof(lte_task_cmd_data_t));
    xRxQueue = xQueueCreate(LTE_RSP_QUEUE_SIZE_MAX, LTE_AT_RSP_SIZE_MAX);

    xLTESem = xSemaphoreCreateMutex();

    lteppp_pcb = pppapi_pppos_create(&lteppp_netif, lteppp_output_callback, lteppp_status_cb, NULL);

    xTaskCreatePinnedToCore(TASK_LTE, "LTE", LTE_TASK_STACK_SIZE / sizeof(StackType_t), NULL, LTE_TASK_PRIORITY, &xLTETaskHndl, 1);
}

void lteppp_start (void) {
    uart_set_hw_flow_ctrl(LTE_UART_ID, UART_HW_FLOWCTRL_CTS_RTS, 64);
    vTaskDelay(5);
}

void lteppp_set_state(lte_state_t state) {
    xSemaphoreTake(xLTESem, portMAX_DELAY);
    lteppp_lte_state = state;
    xSemaphoreGive(xLTESem);
}

void lteppp_connect (void) {
    uart_flush(LTE_UART_ID);
    vTaskDelay(25);
    pppapi_set_default(lteppp_pcb);
    pppapi_set_auth(lteppp_pcb, PPPAUTHTYPE_PAP, "", "");
    pppapi_connect(lteppp_pcb, 0);
}

void lteppp_disconnect(void) {
    pppapi_close(lteppp_pcb, 0);
    vTaskDelay(150);
}

void lteppp_send_at_command (lte_task_cmd_data_t *cmd, lte_task_rsp_data_t *rsp) {
    xQueueSend(xCmdQueue, (void *)cmd, (TickType_t)portMAX_DELAY);
    xQueueReceive(xRxQueue, rsp, (TickType_t)portMAX_DELAY);
}

bool lteppp_wait_at_rsp (const char *expected_rsp, uint32_t timeout, bool from_mp) {
    uint32_t rx_len = 0;

    // wait until characters start arriving
    do {
        // being called from the MicroPython interpreter
        if (from_mp) {
            mp_hal_delay_ms(1);
        }
        else {
            vTaskDelay(1 / portTICK_RATE_MS);
        }
        uart_get_buffered_data_len(LTE_UART_ID, &rx_len);
        if (timeout > 0) {
            timeout--;
        }
    } while (timeout > 0 && 0 == rx_len);

    memset(lteppp_trx_buffer, 0, sizeof(lteppp_trx_buffer));
    if (rx_len > 0) {
        // try to read up to the size of the buffer minus null terminator (minus 2 because we store the OK status in the last byte)
        rx_len = uart_read_bytes(LTE_UART_ID, (uint8_t *)lteppp_trx_buffer, sizeof(lteppp_trx_buffer) - 2, LTE_TRX_WAIT_MS(sizeof(lteppp_trx_buffer)) / portTICK_RATE_MS);
        if (rx_len > 0) {
            // NULL terminate the string
            lteppp_trx_buffer[rx_len] = '\0';
            // printf("%s\n", lteppp_trx_buffer);
            if (expected_rsp != NULL) {
                if (strstr(lteppp_trx_buffer, expected_rsp) != NULL) {
                    return true;
                }
            }
            return false;
        }
    }
    return false;
}

lte_state_t lteppp_get_state(void) {
    lte_state_t state;
    xSemaphoreTake(xLTESem, portMAX_DELAY);
    state = lteppp_lte_state;
    xSemaphoreGive(xLTESem);
    return state;
}

void lteppp_deinit (void) {
    uart_set_hw_flow_ctrl(LTE_UART_ID, UART_HW_FLOWCTRL_DISABLE, 0);
    uart_set_rts(LTE_UART_ID, false);
    xSemaphoreTake(xLTESem, portMAX_DELAY);
    lteppp_lte_state = E_LTE_INIT;
    xSemaphoreGive(xLTESem);
}

uint32_t lteppp_ipv4(void) {
    return lte_ipv4addr;
}

bool lteppp_task_ready(void) {
    return lteppp_init_complete;
}

/******************************************************************************
 DEFINE PRIVATE FUNCTIONS
 ******************************************************************************/
static void TASK_LTE (void *pvParameters) {
    bool sim_present;
    lte_task_cmd_data_t *lte_task_cmd = (lte_task_cmd_data_t *)lteppp_trx_buffer;
    lte_task_rsp_data_t *lte_task_rsp = (lte_task_rsp_data_t *)lteppp_trx_buffer;

    // initialize the UART interface
    uart_config_t config;
    config.baud_rate = MICROPY_LTE_UART_BAUDRATE;
    config.data_bits = UART_DATA_8_BITS;
    config.parity = UART_PARITY_DISABLE;
    config.stop_bits = UART_STOP_BITS_1;
    config.flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS;
    config.rx_flow_ctrl_thresh = 64;
    uart_param_config(LTE_UART_ID, &config);

    // configure the UART pins
    pin_config(MICROPY_LTE_TX_PIN, -1, U2TXD_OUT_IDX, GPIO_MODE_OUTPUT, MACHPIN_PULL_NONE, 1);
    pin_config(MICROPY_LTE_RX_PIN, U2RXD_IN_IDX, -1, GPIO_MODE_INPUT, MACHPIN_PULL_NONE, 1);
    pin_config(MICROPY_LTE_RTS_PIN, -1, U2RTS_OUT_IDX, GPIO_MODE_OUTPUT, MACHPIN_PULL_NONE, 1);
    pin_config(MICROPY_LTE_CTS_PIN, U2CTS_IN_IDX, -1, GPIO_MODE_INPUT, MACHPIN_PULL_NONE, 1);

    // install the UART driver
    uart_driver_install(LTE_UART_ID, LTE_UART_BUFFER_SIZE, LTE_UART_BUFFER_SIZE, 0, NULL, 0, NULL);
    lteppp_uart_reg = &UART2;

    // disable the delay between transfers
    lteppp_uart_reg->idle_conf.tx_idle_num = 0;

    // configure the rx timeout threshold
    lteppp_uart_reg->conf1.rx_tout_thrhd = 20 & UART_RX_TOUT_THRHD_V;

    uart_set_hw_flow_ctrl(LTE_UART_ID, UART_HW_FLOWCTRL_DISABLE, 0);
    uart_set_rts(LTE_UART_ID, false);
    vTaskDelay(5 / portTICK_RATE_MS);
    uart_set_hw_flow_ctrl(LTE_UART_ID, UART_HW_FLOWCTRL_CTS_RTS, 64);
    vTaskDelay(5 / portTICK_RATE_MS);
    if (lteppp_send_at_cmd("+++", LTE_PPP_BACK_OFF_TIME_MS)) {
        vTaskDelay(LTE_PPP_BACK_OFF_TIME_MS / portTICK_RATE_MS);
        while (true) {
            vTaskDelay(LTE_RX_TIMEOUT_MIN_MS / portTICK_RATE_MS);
            if (lteppp_send_at_cmd("AT", LTE_RX_TIMEOUT_MIN_MS)) {
                break;
            }
        }

        lteppp_send_at_cmd("ATH", LTE_RX_TIMEOUT_MIN_MS);
        while (true) {
            vTaskDelay(LTE_RX_TIMEOUT_MIN_MS);
            if (lteppp_send_at_cmd("AT", LTE_RX_TIMEOUT_MIN_MS)) {
                break;
            }
        }
    }

    lteppp_send_at_cmd("AT", LTE_RX_TIMEOUT_MIN_MS);
    if (!lteppp_send_at_cmd("AT", LTE_RX_TIMEOUT_MIN_MS)) {
        vTaskDelay(LTE_PPP_BACK_OFF_TIME_MS / portTICK_RATE_MS);
        if (lteppp_send_at_cmd("+++", LTE_PPP_BACK_OFF_TIME_MS)) {
            vTaskDelay(LTE_RX_TIMEOUT_MIN_MS / portTICK_RATE_MS);
            while (true) {
                vTaskDelay(LTE_RX_TIMEOUT_MIN_MS / portTICK_RATE_MS);
                if (lteppp_send_at_cmd("AT", LTE_RX_TIMEOUT_MIN_MS)) {
                    break;
                }
            }
        } else {
            while (true) {
                vTaskDelay(LTE_RX_TIMEOUT_MIN_MS / portTICK_RATE_MS);
                if (lteppp_send_at_cmd("AT", LTE_RX_TIMEOUT_MIN_MS)) {
                    break;
                }
            }
            lteppp_send_at_cmd("ATH", LTE_RX_TIMEOUT_MIN_MS);
            while (true) {
                vTaskDelay(LTE_RX_TIMEOUT_MIN_MS);
                if (lteppp_send_at_cmd("AT", LTE_RX_TIMEOUT_MIN_MS)) {
                    break;
                }
            }
        }
    }

    lteppp_send_at_cmd("AT", LTE_RX_TIMEOUT_MAX_MS);

    // at least enable access to the SIM
    lteppp_send_at_cmd("AT+CFUN?", LTE_RX_TIMEOUT_MAX_MS);
    char *pos = strstr(lteppp_trx_buffer, "+CFUN: ");
    if (pos && (pos[7] != '1') && (pos[7] != '4')) {
        lteppp_send_at_cmd("AT+CFUN=4", LTE_RX_TIMEOUT_MAX_MS);
        lteppp_send_at_cmd("AT+CFUN?", LTE_RX_TIMEOUT_MAX_MS);
    }
    // check for SIM card inserted
    sim_present = lteppp_check_sim_present();

    // if we are coming from a power on reset, disable the LTE radio
    if (mpsleep_get_reset_cause() < MPSLEEP_WDT_RESET) {
        lteppp_send_at_cmd("AT+CFUN?", LTE_RX_TIMEOUT_MAX_MS);
        if (!sim_present) {
            lteppp_send_at_cmd("AT+CFUN=0", LTE_RX_TIMEOUT_MAX_MS);
        }
    }

    // enable PSM if not already enabled
    lteppp_send_at_cmd("AT+CPSMS?", LTE_RX_TIMEOUT_MAX_MS);
    if (!strstr(lteppp_trx_buffer, "+CPSMS: 0")) {
        lteppp_send_at_cmd("AT+CPSMS=0", LTE_RX_TIMEOUT_MIN_MS);
    }
    // enable low power mode
    lteppp_send_at_cmd("AT!=\"setlpm airplane=1 enable=1\"", LTE_RX_TIMEOUT_MAX_MS);

    lteppp_init_complete = true;

    for (;;) {
        vTaskDelay(LTE_TASK_PERIOD_MS);
        if (xQueueReceive(xCmdQueue, lteppp_trx_buffer, 0)) {
            lteppp_send_at_cmd_exp(lte_task_cmd->data, lte_task_cmd->timeout, NULL);
            xQueueSend(xRxQueue, (void *)lte_task_rsp, (TickType_t)portMAX_DELAY);
        } else {
            lte_state_t state = lteppp_get_state();
            if (state == E_LTE_PPP) {
                uint32_t rx_len;
                // wait for characters received
                uart_get_buffered_data_len(LTE_UART_ID, &rx_len);
                if (rx_len > 0) {
                    // try to read up to the size of the buffer
                    rx_len = uart_read_bytes(LTE_UART_ID, (uint8_t *)lteppp_trx_buffer, sizeof(lteppp_trx_buffer), 
                                             LTE_TRX_WAIT_MS(sizeof(lteppp_trx_buffer)) / portTICK_RATE_MS);
                    if (rx_len > 0) {
                        pppos_input_tcpip(lteppp_pcb, (uint8_t *)lteppp_trx_buffer, rx_len);
                    }
                }
            }
        }
    }
}

static bool lteppp_send_at_cmd_exp (const char *cmd, uint32_t timeout, const char *expected_rsp) {
    uint32_t cmd_len = strlen(cmd);
    // char tmp_buf[128];

    // printf("cmd: %s\n", cmd);

    // flush the rx buffer first
    uart_flush(LTE_UART_ID);
    // uart_read_bytes(LTE_UART_ID, (uint8_t *)tmp_buf, sizeof(tmp_buf), 5 / portTICK_RATE_MS);
    // then send the command
    uart_write_bytes(LTE_UART_ID, cmd, cmd_len);
    if (strcmp(cmd, "+++")) {
        uart_write_bytes(LTE_UART_ID, "\r\n", 2);
    }
    uart_wait_tx_done(LTE_UART_ID, LTE_TRX_WAIT_MS(cmd_len) / portTICK_RATE_MS);
    vTaskDelay(2 / portTICK_RATE_MS);

    return lteppp_wait_at_rsp(expected_rsp, timeout, false);
}

static bool lteppp_send_at_cmd(const char *cmd, uint32_t timeout) {
    return lteppp_send_at_cmd_exp (cmd, timeout, LTE_OK_RSP);
}

static bool lteppp_check_sim_present(void) {
    lteppp_send_at_cmd("AT+CPIN?", LTE_RX_TIMEOUT_MAX_MS);
    if (strstr(lteppp_trx_buffer, "ERROR")) {
        return false;
    } else {
        return true;
    }
}

// PPP output callback
static uint32_t lteppp_output_callback(ppp_pcb *pcb, u8_t *data, u32_t len, void *ctx) {
    LWIP_UNUSED_ARG(ctx);
    uint32_t tx_bytes = uart_write_bytes(LTE_UART_ID, (const char*)data, len);
    uart_wait_tx_done(LTE_UART_ID, LTE_TRX_WAIT_MS(len) / portTICK_RATE_MS);
    return tx_bytes;
}

// PPP status callback
static void lteppp_status_cb (ppp_pcb *pcb, int err_code, void *ctx) {
    struct netif *pppif = ppp_netif(pcb);
    LWIP_UNUSED_ARG(ctx);

    switch (err_code) {
    case PPPERR_NONE:
        // printf("status_cb: Connected\n");
        #if PPP_IPV4_SUPPORT
        lte_gw = pppif->gw.u_addr.ip4.addr;
        lte_netmask = pppif->netmask.u_addr.ip4.addr;
        lte_ipv4addr = pppif->ip_addr.u_addr.ip4.addr;
        // printf("ipaddr    = %s\n", ipaddr_ntoa(&pppif->ip_addr));
        // printf("gateway   = %s\n", ipaddr_ntoa(&pppif->gw));
        // printf("netmask   = %s\n", ipaddr_ntoa(&pppif->netmask));
        #endif
        #if PPP_IPV6_SUPPORT
        memcpy(lte_ipv6addr.addr, netif_ip6_addr(pppif, 0), sizeof(lte_ipv4addr));
        // printf("ip6addr   = %s\n", ip6addr_ntoa(netif_ip6_addr(pppif, 0)));
        #endif
        break;
    case PPPERR_PARAM:
        // printf("status_cb: Invalid parameter\n");
        break;
    case PPPERR_OPEN:
        // printf("status_cb: Unable to open PPP session\n");
        break;
    case PPPERR_DEVICE:
        // printf("status_cb: Invalid I/O device for PPP\n");
        break;
    case PPPERR_ALLOC:
        // printf("status_cb: Unable to allocate resources\n");
        break;
    case PPPERR_USER:
        // printf("status_cb: User interrupt (disconnected)\n");
        lte_ipv4addr = 0;
        memset(lte_ipv6addr.addr, 0, sizeof(lte_ipv4addr));
        break;
    case PPPERR_CONNECT:
        // printf("status_cb: Connection lost\n");
        lte_ipv4addr = 0;
        memset(lte_ipv6addr.addr, 0, sizeof(lte_ipv4addr));
        break;
    case PPPERR_AUTHFAIL:
        // printf("status_cb: Failed authentication challenge\n");
        break;
    case PPPERR_PROTOCOL:
        // printf("status_cb: Failed to meet protocol\n");
        break;
    case PPPERR_PEERDEAD:
        // printf("status_cb: Connection timeout\n");
        break;
    case PPPERR_IDLETIMEOUT:
        // printf("status_cb: Idle Timeout\n");
        break;
    case PPPERR_CONNECTTIME:
        // printf("status_cb: Max connect time reached\n");
        break;
    case PPPERR_LOOPBACK:
        // printf("status_cb: Loopback detected\n");
        break;
    default:
        // printf("status_cb: Unknown error code %d\n", err_code);
        lte_ipv4addr = 0;
        memset(lte_ipv6addr.addr, 0, sizeof(lte_ipv4addr));
        break;
    }
}
