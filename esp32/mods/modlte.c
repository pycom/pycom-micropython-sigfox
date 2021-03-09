/*
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


#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "py/mpconfig.h"
#include "py/obj.h"
#include "py/objstr.h"
#include "py/runtime.h"
#include "py/stream.h"
#include "py/mphal.h"
#include "py/mperrno.h"
#include "py/misc.h"

#include "esp_heap_caps.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"
#include "esp_event_loop.h"
#include "ff.h"

#include "machpin.h"
#include "pins.h"

//#include "timeutils.h"
#include "netutils.h"
#include "modnetwork.h"
#include "modusocket.h"
#include "py/stream.h"
//#include "pybrtc.h"
#include "serverstask.h"
#include "mpexception.h"
#include "modussl.h"

#include "lteppp.h"
#include "modlte.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "lwipsocket.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_log.h"

#include "driver/uart.h"
#include "driver/gpio.h"

#include "pycom_config.h"
#include "modmachine.h"
#include "mpirq.h"

#include "str_utils.h"

/******************************************************************************
 DEFINE TYPES
 ******************************************************************************/

/******************************************************************************
 DEFINE CONSTANTS
 ******************************************************************************/
#define LTE_NUM_UARTS               2
#define UART_TRANSFER_MAX_LEN       1

#define  DEFAULT_PROTO_TYPE          (const char*)"IP"
#define  DEFAULT_APN                 (const char*)""

#define SQNS_SW_FULL_BAND_SUPPORT   41000
#define SQNS_SW_5_8_BAND_SUPPORT    39000

// PSM Power saving mode
// PERIOD, aka Requested Periodic TAU (T3412), in GPRS Timer 3 format
#define PSM_PERIOD_2S         0b011
#define PSM_PERIOD_30S        0b100
#define PSM_PERIOD_1M         0b101
#define PSM_PERIOD_10M        0b000
#define PSM_PERIOD_1H         0b001
#define PSM_PERIOD_10H        0b010
#define PSM_PERIOD_320H       0b110
#define PSM_PERIOD_DISABLED   0b111
// ACTIVE, aka Requested Active Time (T3324), in GPRS Timer 2 format
#define PSM_ACTIVE_2S         0b000
#define PSM_ACTIVE_1M         0b001
#define PSM_ACTIVE_6M         0b010
#define PSM_ACTIVE_DISABLED   0b111

/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/
static lte_obj_t lte_obj = {.init = false, .trigger = LTE_EVENT_NONE, .events = 0, .handler = NULL, .handler_arg = NULL};
static lte_task_rsp_data_t modlte_rsp;
uart_dev_t* uart_driver_0 = &UART0;
uart_dev_t* uart_driver_lte = &UART2;

uart_config_t lte_uart_config0;
uart_config_t lte_uart_config1;

static bool lte_legacyattach_flag = true;
static bool lte_debug = false;

static bool lte_ue_is_out_of_coverage = false;

extern TaskHandle_t xLTEUpgradeTaskHndl;
extern TaskHandle_t mpTaskHandle;
extern TaskHandle_t svTaskHandle;
#if defined(FIPY)
extern TaskHandle_t xLoRaTaskHndl;
#ifdef LORA_OPENTHREAD_ENABLED
extern TaskHandle_t xMeshTaskHndl;
#endif
extern TaskHandle_t xSigfoxTaskHndl;
#endif
extern TaskHandle_t xLTETaskHndl;

/******************************************************************************
 DECLARE PUBLIC DATA
 ******************************************************************************/
/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/
static bool lte_push_at_command_ext_cont (char *cmd_str, uint32_t timeout, const char *expected_rsp, size_t len, bool continuation);
static bool lte_push_at_command_ext (char *cmd_str, uint32_t timeout, const char *expected_rsp, size_t len);
static bool lte_push_at_command (char *cmd_str, uint32_t timeout);
static void lte_pause_ppp(void);
static bool lte_check_attached(bool legacy);
static void lte_check_init(void);
static bool lte_check_sim_present(void);
static int lte_get_modem_version(void);
STATIC mp_obj_t lte_suspend(mp_obj_t self_in);
STATIC mp_obj_t lte_connect(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args);

STATIC mp_obj_t lte_deinit(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args);
STATIC mp_obj_t lte_disconnect(mp_obj_t self_in);
static void lte_set_default_inf(void);
static void lte_callback_handler(void* arg);

//#define MSG(fmt, ...) printf("[%u] modlte: " fmt, mp_hal_ticks_ms(), ##__VA_ARGS__)
#define MSG(fmt, ...) (void)0

/******************************************************************************
 DEFINE PUBLIC FUNCTIONS
 ******************************************************************************/

void modlte_init0(void) {
    lteppp_init();
}
void modlte_start_modem(void)
{
    xTaskNotifyGive(xLTETaskHndl);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    portYIELD();
}

void modlte_urc_events(lte_events_t events)
{
    // set the events to report to the user, the clearing is done upon reading via lte_events()
    if ( (events & LTE_EVENT_COVERAGE_LOST)
        && (lte_obj.trigger & LTE_EVENT_COVERAGE_LOST) )
    {
        lte_obj.events |= (uint32_t)LTE_EVENT_COVERAGE_LOST;
    }
    if ( (events & LTE_EVENT_BREAK)
        && (lte_obj.trigger & LTE_EVENT_BREAK) )
    {
        lte_obj.events |= (uint32_t)LTE_EVENT_BREAK;
    }

    //MSG("urc(%u) l.trig=%u l.eve=%d\n", events, lte_obj.trigger, lte_obj.events);
    mp_irq_queue_interrupt(lte_callback_handler, &lte_obj);
}

//*****************************************************************************
// DEFINE STATIC FUNCTIONS
//*****************************************************************************

static void lte_callback_handler(void* arg)
{
    lte_obj_t *self = arg;

    if (self->handler && self->handler != mp_const_none) {
        //MSG("call callback(handler=%p, arg=%p)\n", self->handler_arg, self->handler);
        mp_call_function_1(self->handler, self->handler_arg);
    }else{
        //MSG("no callback\n");
    }

}

static bool lte_push_at_command_ext_cont (char *cmd_str, uint32_t timeout, const char *expected_rsp, size_t len, bool continuation)
{
    lte_task_cmd_data_t cmd = { .timeout = timeout, .dataLen = len, .expect_continuation = continuation};
    memcpy(cmd.data, cmd_str, len);
    uint32_t start = mp_hal_ticks_ms();
    if (lte_debug)
        printf("[AT] %u %s\n", start, cmd_str);
    lteppp_send_at_command(&cmd, &modlte_rsp);
    if (continuation || (expected_rsp == NULL) || (strstr(modlte_rsp.data, expected_rsp) != NULL)) {
        if (lte_debug)
            printf("[AT-OK] +%u %s\n", mp_hal_ticks_ms()-start, modlte_rsp.data);
        return true;
    }
    if (lte_debug)
        printf("[AT-FAIL] +%u %s\n", mp_hal_ticks_ms()-start, modlte_rsp.data);
    return false;
}

static bool lte_push_at_command_ext(char *cmd_str, uint32_t timeout, const char *expected_rsp, size_t len) {
    return lte_push_at_command_ext_cont(cmd_str, timeout, expected_rsp, len, false);
}

static bool lte_push_at_command (char *cmd_str, uint32_t timeout) {
    return lte_push_at_command_ext(cmd_str, timeout, LTE_OK_RSP, strlen(cmd_str));
}

static void lte_pause_ppp(void) {
    mp_hal_delay_ms(LTE_PPP_BACK_OFF_TIME_MS);
    if (!lte_push_at_command("+++", LTE_PPP_BACK_OFF_TIME_MS)) {
        mp_hal_delay_ms(LTE_PPP_BACK_OFF_TIME_MS);
        if (!lte_push_at_command("AT", LTE_RX_TIMEOUT_MIN_MS)) {
            mp_hal_delay_ms(LTE_PPP_BACK_OFF_TIME_MS);
            if (!lte_push_at_command("AT", LTE_RX_TIMEOUT_MIN_MS)) {
                mp_hal_delay_ms(LTE_PPP_BACK_OFF_TIME_MS);
                if (!lte_push_at_command("+++", LTE_PPP_BACK_OFF_TIME_MS)) {
                    mp_hal_delay_ms(LTE_PPP_BACK_OFF_TIME_MS);
                    if (!lte_push_at_command("AT", LTE_RX_TIMEOUT_MIN_MS)) {
                        mp_hal_delay_ms(LTE_PPP_BACK_OFF_TIME_MS);
                        if (!lte_push_at_command("AT", LTE_RX_TIMEOUT_MIN_MS)) {
                            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "Pause PPP failed"));
                        }
                    }
                }
            }
        }
    }
}

static bool lte_check_attached(bool legacy) {
    char *pos;
    bool attached = false;
    bool cgatt = false;
    if (lteppp_get_state() == E_LTE_PPP  && lteppp_ipv4() > 0) {
        attached = true;
    } else {
        if (lteppp_get_state() == E_LTE_PPP) {
            lte_pause_ppp();
            while (true) {
                mp_hal_delay_ms(LTE_RX_TIMEOUT_MIN_MS);
                if (lte_push_at_command("AT", LTE_RX_TIMEOUT_MAX_MS)) {
                    break;
                }
            }
        }
        lte_push_at_command("AT", LTE_RX_TIMEOUT_MIN_MS);
        lte_push_at_command("AT+CGATT?", LTE_RX_TIMEOUT_MIN_MS);
        if (((pos = strstr(modlte_rsp.data, "+CGATT")) && (strlen(pos) >= 7) && (pos[7] == '1' || pos[8] == '1'))) {
            cgatt = true;
        }
        if (legacy) {
            lte_push_at_command("AT+CEREG?", LTE_RX_TIMEOUT_MIN_MS);
            if (!cgatt) {
                if (strstr(modlte_rsp.data, "ERROR")) {
                    mp_hal_delay_ms(LTE_RX_TIMEOUT_MIN_MS);
                    lte_push_at_command("AT+CEREG?", LTE_RX_TIMEOUT_MIN_MS);
                }
                if (((pos = strstr(modlte_rsp.data, "+CEREG: 1,1")) || (pos = strstr(modlte_rsp.data, "+CEREG: 1,5")))
                        && (strlen(pos) >= 31) && (pos[30] == '7' || pos[30] == '9')) {
                    attached = true;
                }
            } else {
                if ((pos = strstr(modlte_rsp.data, "+CEREG: 1,1")) || (pos = strstr(modlte_rsp.data, "+CEREG: 1,5"))) {
                    attached = true;
                } else {
                    if((pos = strstr(modlte_rsp.data, "+CEREG: 1,4")))
                    {
                        lte_ue_is_out_of_coverage = true;
                    }
                    else
                    {
                        lte_ue_is_out_of_coverage = false;
                    }
                    attached = false;
                }
            }
        }
        else
        {
            attached = cgatt;
        }
        lte_push_at_command("AT+CFUN?", LTE_RX_TIMEOUT_MIN_MS);
        if (lteppp_get_state() >= E_LTE_ATTACHING) {
            if (!strstr(modlte_rsp.data, "+CFUN: 1")) {
                //for some reason the modem has crashed, enabled the radios again...
                lte_push_at_command("AT+CFUN=1", LTE_RX_TIMEOUT_MIN_MS);
            }
        } else {
            if (strstr(modlte_rsp.data, "+CFUN: 1")) {
                lteppp_set_state(E_LTE_ATTACHING);
            } else {
                lteppp_set_state(E_LTE_IDLE);
            }
        }
    }
    if (attached && lteppp_get_state() < E_LTE_PPP) {
        lteppp_set_state(E_LTE_ATTACHED);
    }
    if (!attached && lteppp_get_state() > E_LTE_IDLE) {
        lteppp_set_state(E_LTE_ATTACHING);
    }
    return attached;
}

static bool lte_check_legacy_version(void) {
    if (lte_push_at_command("ATI1", LTE_RX_TIMEOUT_MAX_MS)) {
        return strstr(modlte_rsp.data, "LR5.1.1.0-33080");
    } else {
        if (lte_push_at_command("ATI1", LTE_RX_TIMEOUT_MAX_MS)) {
            return strstr(modlte_rsp.data, "LR5.1.1.0-33080");
        } else {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "LTE modem version not read"));
        }
    }
    return true;
}

static int lte_get_modem_version(void)
{
    /* Get modem version */
    char * ver = NULL;

    lte_push_at_command_ext("AT!=\"showver\"", LTE_RX_TIMEOUT_MAX_MS, NULL, strlen("AT!=\"showver\""));
    ver = strstr(modlte_rsp.data, "Software     :");

    if(ver != NULL )
    {
        ver = strstr(ver, "[");
        char * ver_close =  strstr(ver, "]");
        int v = 0;
        if (ver != NULL && ver_close != NULL && ver_close > ver) {
            ver[ver_close - ver] = '\0';
            ver++;
            v = atoi(ver);
        }
        return v;
    }
    return 0;
}

static void lte_check_init(void) {
    if (!lte_obj.init) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "LTE modem not initialized"));
    }
}

static void lte_set_default_inf(void)
{
    lteppp_set_default_inf();
}

static void lte_check_inppp(void) {
    if (lteppp_get_state() == E_LTE_PPP) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "LTE modem is in data state, cannot send AT commands"));
    }
}

static bool lte_check_sim_present(void) {
    lte_push_at_command("AT+CFUN?", LTE_RX_TIMEOUT_MIN_MS);
    if (strstr(modlte_rsp.data, "+CFUN: 0")) {
    	lte_push_at_command("AT+CFUN=4", LTE_RX_TIMEOUT_MAX_MS);
    	mp_hal_delay_ms(LTE_RX_TIMEOUT_MIN_MS);
    }
    lte_push_at_command("AT+CPIN?", LTE_RX_TIMEOUT_MAX_MS);
    if (strstr(modlte_rsp.data, "READY")) {
    	return true;
    } else {
    	for (int n=0; n < 4; n++) {
    		mp_hal_delay_ms(1000);
    		lte_push_at_command("AT+CPIN?", LTE_RX_TIMEOUT_MAX_MS);
    		if (strstr(modlte_rsp.data, "READY")) {
    			return true;
    		}
    	}
    	return false;
    }
}

static void TASK_LTE_UPGRADE(void *pvParameters){

    size_t len = 0;
    uint8_t rx_buff[UART_TRANSFER_MAX_LEN];
    // Suspend All tasks
    vTaskSuspend(mpTaskHandle);
    vTaskSuspend(svTaskHandle);
#if defined(FIPY)
    vTaskSuspend(xLoRaTaskHndl);
    vTaskSuspend(xSigfoxTaskHndl);
#ifdef LORA_OPENTHREAD_ENABLED
    vTaskSuspend(xMeshTaskHndl);
#endif
#endif
    vTaskSuspend(xLTETaskHndl);

    for (;;) {
        uart_get_buffered_data_len(0, &len);

        if (len) {
            if (len > UART_TRANSFER_MAX_LEN) {
                len = UART_TRANSFER_MAX_LEN;
            }
            uart_read_bytes(0, rx_buff, len, 0);
            uart_write_bytes(1, (char*) (rx_buff), len);
        }

        len = 0;
        uart_get_buffered_data_len(1, &len);

        if (len) {
            if (len > UART_TRANSFER_MAX_LEN) {
                len = UART_TRANSFER_MAX_LEN;
            }
            uart_read_bytes(1, rx_buff, len, 0);
            uart_write_bytes(0, (char*) (rx_buff), len);
        }
    }
}

/******************************************************************************/
// Micro Python bindings; LTE class

static mp_obj_t lte_init_helper(lte_obj_t *self, const mp_arg_val_t *args) {
    lteppp_init();
    const size_t at_cmd_len = LTE_AT_CMD_SIZE_MAX - 4;
    char at_cmd[at_cmd_len];
    lte_modem_conn_state_t modem_state;

    if (lte_obj.init) {
        //printf("All done since we were already initialised.\n");
        return mp_const_none;
    }
    modem_state  = lteppp_get_modem_conn_state();
    switch(modem_state)
    {
    case E_LTE_MODEM_DISCONNECTED:
        // Notify the LTE thread to start
        modlte_start_modem();
        MP_THREAD_GIL_EXIT();
        xSemaphoreTake(xLTE_modem_Conn_Sem, portMAX_DELAY);
        MP_THREAD_GIL_ENTER();
        lte_modem_conn_state_t modem_state = lteppp_get_modem_conn_state();
        if (E_LTE_MODEM_DISCONNECTED == modem_state) {
            xSemaphoreGive(xLTE_modem_Conn_Sem);
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "Couldn't start connection to Modem (modem_state=disconnected)"));
        } else if (E_LTE_MODEM_RECOVERY == modem_state){
            xSemaphoreGive(xLTE_modem_Conn_Sem);
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "Couldn't start connection to Modem (modem_state=recovery). Perform a modem firmware update."));
        }
        break;
    case E_LTE_MODEM_CONNECTING:
        // Block till modem is connected
        xSemaphoreTake(xLTE_modem_Conn_Sem, portMAX_DELAY);
        if (E_LTE_MODEM_DISCONNECTED == lteppp_get_modem_conn_state()) {
            xSemaphoreGive(xLTE_modem_Conn_Sem);
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "Couldn't connect to Modem (modem_state=connecting)"));
        }
        break;
    case E_LTE_MODEM_CONNECTED:
        //continue
        break;
    case E_LTE_MODEM_RECOVERY:
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "Couldn't connect to Modem (modem_state=recovery). Perform a modem firmware update."));
        break;
    default:
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "Couldn't connect to Modem (modem_state - default)"));
        break;
    }
    lte_obj.cid = args[1].u_int;

    lte_push_at_command("AT+CFUN?", LTE_RX_TIMEOUT_MIN_MS);
    if (strstr(modlte_rsp.data, "+CFUN: 0") || strstr(modlte_rsp.data, "+CFUN: 4")) {
        const char *carrier = "standard";
        lte_obj.carrier = false;
        if (args[0].u_obj != mp_const_none) {
            carrier = mp_obj_str_get_str(args[0].u_obj);
            lte_push_at_command("AT+SQNCTM=?", LTE_RX_TIMEOUT_MIN_MS);
            if (!strstr(modlte_rsp.data, carrier)) {
                xSemaphoreGive(xLTE_modem_Conn_Sem);
                nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "invalid carrier %s", carrier));
            } else if (!strstr(carrier, "standard")) {
                lte_obj.carrier = true;
            }
        }

        // set legacy flag
        lte_legacyattach_flag = args[2].u_bool;

        // configure the carrier
        lte_push_at_command("AT+SQNCTM?", LTE_RX_TIMEOUT_MAX_MS);
        const char* detected_carrier = modlte_rsp.data;
        if (!strstr(detected_carrier, carrier) && (args[0].u_obj != mp_const_none)) {
            sprintf(at_cmd, "AT+SQNCTM=\"%s\"", carrier);
            lte_push_at_command(at_cmd, LTE_RX_TIMEOUT_MAX_MS);
            lteppp_wait_at_rsp("+S", LTE_RX_TIMEOUT_MAX_MS, true, NULL);
            mp_hal_delay_ms(LTE_RX_TIMEOUT_MIN_MS);
            if (!lteppp_wait_at_rsp("+SYSSTART", LTE_RX_TIMEOUT_MAX_MS, true, NULL)) {
                lteppp_wait_at_rsp("+SYSSTART", LTE_RX_TIMEOUT_MAX_MS, true, NULL);
            }
            lte_push_at_command("AT", LTE_RX_TIMEOUT_MAX_MS);
            lte_push_at_command("AT", LTE_RX_TIMEOUT_MAX_MS);
        }
        if (!strstr(detected_carrier, carrier) && (!strstr(detected_carrier, "standard"))) {
            lte_obj.carrier = true;
        }
        lte_push_at_command("AT+CFUN=4", LTE_RX_TIMEOUT_MAX_MS);
        lte_push_at_command("AT", LTE_RX_TIMEOUT_MAX_MS);
        lte_push_at_command("AT", LTE_RX_TIMEOUT_MAX_MS);
    }

    lteppp_set_state(E_LTE_IDLE);
    mod_network_register_nic(&lte_obj);
    lte_obj.init = true;

    // configure PSM
    u8_t psm_period_value = args[4].u_int;
    u8_t psm_period_unit = args[5].u_int;
    u8_t psm_active_value = args[6].u_int;
    u8_t psm_active_unit = args[7].u_int;
    if ( psm_period_unit != PSM_PERIOD_DISABLED && psm_active_unit != PSM_ACTIVE_DISABLED ) {
        u8_t psm_period = ( psm_period_unit << 5 ) | psm_period_value;
        u8_t psm_active = ( psm_active_unit << 5 ) | psm_active_value;
        char p[9];
        char a[9];
        sprint_binary_u8(p, psm_period);
        sprint_binary_u8(a, psm_active);
        snprintf(at_cmd, at_cmd_len, "AT+CPSMS=1,,,\"%s\",\"%s\"", p, a);
        lte_push_at_command(at_cmd, LTE_RX_TIMEOUT_MAX_MS);
    }

    xSemaphoreGive(xLTE_modem_Conn_Sem);
    return mp_const_none;
}

STATIC mp_obj_t lte_psm(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    lte_check_init();
    lte_check_inppp();
    mp_obj_t tuple[5];
    static const qstr psm_info_fields[] = {
        MP_QSTR_enabled,
        MP_QSTR_period_value,
        MP_QSTR_period_unit,
        MP_QSTR_active_value,
        MP_QSTR_active_unit,
    };

    lte_push_at_command("AT+CPSMS?", LTE_RX_TIMEOUT_MAX_MS);
    const char *resp = modlte_rsp.data;
    char *pos;
    if ( ( pos = strstr(resp, "+CPSMS: ") ) ) {
        // decode the resp:
        // +CPSMS: <mode>,[<Requested_Periodic-RAU>],[<Requested_GPRS-READYtimer>],[<Requested_Periodic-TAU>],[<Requested_Active-Time>]

        // go to <mode>
        pos += strlen_const("+CPSMS: ");
        tuple[0] = mp_obj_new_bool(*pos == '1');

        // go to <Requested_Periodic-RAU>
        pos += strlen_const("1,");

        // find <Requested_GPRS-READYtimer>
        pos = strstr(pos, ",");
        pos++;

        // find <Requested_Periodic-TAU>
        pos = strstr(pos, ",");
        pos++; // ,
        pos++; // "

        // get three digit TAU unit
        char* oldpos = pos;
        tuple[2] = mp_obj_new_int_from_str_len( (const char**) &pos, 3, false, 2);
        assert( pos == oldpos + 3); // mp_obj_new_int_from_str_len is supposed to consume exactly 3 characters

        // get five digit TAU value
        tuple[1] = mp_obj_new_int_from_str_len( (const char**) &pos, 5, false, 2);

        // find <Requested_Active-Time>
        pos = strstr(pos, ",");
        pos++; // ,
        pos++; // "

        // get three digit ActiveTime unit
        oldpos = pos;
        tuple[4] = mp_obj_new_int_from_str_len( (const char**) &pos, 3, false, 2);
        assert( pos == oldpos + 3); // mp_obj_new_int_from_str_len is supposed to consume exactly 3 characters

        // get five digit ActiveTime value
        tuple[3] = mp_obj_new_int_from_str_len( (const char**) &pos, 5, false, 2);

    } else {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "Failed to read PSM setting"));
    }

    return mp_obj_new_attrtuple(psm_info_fields, 5, tuple);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(lte_psm_obj, 1, lte_psm);


static const mp_arg_t lte_init_args[] = {
    { MP_QSTR_id,                                   MP_ARG_INT, {.u_int = 0} },
    { MP_QSTR_carrier,                              MP_ARG_KW_ONLY  | MP_ARG_OBJ,  {.u_obj = mp_const_none} },
    { MP_QSTR_cid,                                  MP_ARG_KW_ONLY  | MP_ARG_INT,  {.u_int = 1} },
    { MP_QSTR_legacyattach,                         MP_ARG_KW_ONLY  | MP_ARG_BOOL, {.u_bool = true} },
    { MP_QSTR_debug,                                MP_ARG_KW_ONLY  | MP_ARG_BOOL, {.u_bool = false} },
    { MP_QSTR_psm_period_value,                     MP_ARG_KW_ONLY  | MP_ARG_INT,  {.u_int = 0 } },
    { MP_QSTR_psm_period_unit,                      MP_ARG_KW_ONLY  | MP_ARG_INT,  {.u_int = PSM_PERIOD_DISABLED } },
    { MP_QSTR_psm_active_value,                     MP_ARG_KW_ONLY  | MP_ARG_INT,  {.u_int = 0 } },
    { MP_QSTR_psm_active_unit,                      MP_ARG_KW_ONLY  | MP_ARG_INT,  {.u_int = PSM_ACTIVE_DISABLED } },
};

static mp_obj_t lte_make_new(const mp_obj_type_t *type, mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *all_args) {
    // parse args
    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, all_args + n_args);
    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(lte_init_args)];
    mp_arg_parse_all(n_args, all_args, &kw_args, MP_ARRAY_SIZE(args), lte_init_args, args);
    if (args[4].u_bool)
        lte_debug = true;

    // setup the object
    lte_obj_t *self = &lte_obj;
    self->base.type = (mp_obj_t)&mod_network_nic_type_lte;

    if (n_kw > 0) {
        // check the peripheral id
        if (args[0].u_int != 0) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_resource_not_avaliable));
        }
    }

    // check psm args
    u8_t psm_period_value = args[5].u_int;
    u8_t psm_period_unit = args[6].u_int;
    u8_t psm_active_value = args[7].u_int;
    u8_t psm_active_unit = args[8].u_int;
    if (psm_period_unit > 7)
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "Invalid psm_period_unit"));
    if (psm_period_value > 31)
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "Invalid psm_period_value"));
    switch(psm_active_unit){
        case PSM_ACTIVE_2S:
        case PSM_ACTIVE_1M:
        case PSM_ACTIVE_6M:
        case PSM_ACTIVE_DISABLED:
            // ok, nothing to do
            break;
        default:
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "Invalid psm_active_unit"));
            break;
    }
    if (psm_active_value > 31)
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "Invalid psm_active_value"));

    // start the peripheral
    lte_init_helper(self, &args[1]);
    return (mp_obj_t)self;
}

STATIC mp_obj_t lte_init(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(lte_init_args) - 1];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), &lte_init_args[1], args);
    lte_debug = args[3].u_bool;
    return lte_init_helper(pos_args[0], args);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(lte_init_obj, 1, lte_init);

STATIC mp_obj_t lte_deinit(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    STATIC const mp_arg_t allowed_args[] = {
        { MP_QSTR_detach,            MP_ARG_KW_ONLY  | MP_ARG_BOOL,  {.u_bool = true}},
        { MP_QSTR_reset,             MP_ARG_KW_ONLY  | MP_ARG_BOOL,  {.u_bool = false}},
        { MP_QSTR_dettach,           MP_ARG_KW_ONLY  | MP_ARG_BOOL,  {.u_bool = true}}, /* backward compatibility for dettach method FIXME */
    };

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    if (lte_obj.init) {
        lte_obj_t *self = (lte_obj_t*)pos_args[0];
        if (lteppp_get_state() == E_LTE_PPP && (lteppp_get_legacy() == E_LTE_LEGACY || args[0].u_bool)) {
            lte_disconnect(self);
        } else {
            if (lteppp_get_state() == E_LTE_PPP) {
                lte_suspend(self);
            }
            if (!args[0].u_bool || !args[2].u_bool) { /* backward compatibility for dettach method FIXME */
                vTaskDelay(100);
                lte_push_at_command("AT!=\"setlpm airplane=1 enable=1\"", LTE_RX_TIMEOUT_MAX_MS);
                lteppp_deinit();
                lte_obj.init = false;
                return mp_const_none;
            }
        }

        if (lte_check_sim_present()) {
            if (args[1].u_bool) {
                if (lte_push_at_command("AT+CFUN=4,1", LTE_RX_TIMEOUT_MAX_MS)) {
                    lteppp_wait_at_rsp("+S", LTE_RX_TIMEOUT_MAX_MS, true, NULL);
                    mp_hal_delay_ms(LTE_RX_TIMEOUT_MIN_MS);
                    if (!lteppp_wait_at_rsp("+SYSSTART", LTE_RX_TIMEOUT_MAX_MS, true, NULL)) {
                        lteppp_wait_at_rsp("+SYSSTART", LTE_RX_TIMEOUT_MAX_MS, true, NULL);
                    }
                    lte_push_at_command("AT", LTE_RX_TIMEOUT_MAX_MS);
                    if (!lte_push_at_command("AT", LTE_RX_TIMEOUT_MAX_MS)) {
                        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
                    } else {
                        lte_push_at_command("AT!=\"setlpm airplane=1 enable=1\"", LTE_RX_TIMEOUT_MAX_MS);
                    }
                }
            } else {
                if (!lte_push_at_command("AT+CFUN=4", LTE_RX_TIMEOUT_MAX_MS)) {
                    goto error;
                }
            }
        } else {
            if (args[1].u_bool) {
                if (lte_push_at_command("AT+CFUN=0,1", LTE_RX_TIMEOUT_MAX_MS)) {
                    lteppp_wait_at_rsp("+S", LTE_RX_TIMEOUT_MAX_MS, true, NULL);
                    mp_hal_delay_ms(LTE_RX_TIMEOUT_MIN_MS);
                    if (!lteppp_wait_at_rsp("+SYSSTART", LTE_RX_TIMEOUT_MAX_MS, true, NULL)) {
                        lteppp_wait_at_rsp("+SYSSTART", LTE_RX_TIMEOUT_MAX_MS, true, NULL);
                    }
                    lte_push_at_command("AT", LTE_RX_TIMEOUT_MAX_MS);
                    if (!lte_push_at_command("AT", LTE_RX_TIMEOUT_MAX_MS)) {
                        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
                    } else {
                        lte_push_at_command("AT!=\"setlpm airplane=1 enable=1\"", LTE_RX_TIMEOUT_MAX_MS);
                    }
                }
            } else {
                if (!lte_push_at_command("AT+CFUN=0", LTE_RX_TIMEOUT_MAX_MS)) {
                    goto error;
                }
            }
        }
        lte_obj.init = false;
    }
    lteppp_deinit();
    mod_network_deregister_nic(&lte_obj);
    return mp_const_none;

error:
    lteppp_deinit();
    lte_obj.init = false;
    nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(lte_deinit_obj, 1, lte_deinit);

STATIC void lte_add_band(uint32_t band, bool is_hw_new_band_support, bool is_sw_new_band_support, int version)
{
    /* Check band support */
    switch(band)
    {
        case 5:
        case 8:
            if (!is_hw_new_band_support)
            {
                nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "band %d not supported by this board hardware!", band));
            }
            else if(version < SQNS_SW_5_8_BAND_SUPPORT)
            {
                nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "band %d not supported by current modem Firmware [%d], please upgrade!", band, version));
            }
            break;
        case 1:
        case 2:
        case 14:
        case 17:
        case 18:
        case 19:
        case 25:
        case 26:
        case 66:
            if(!is_sw_new_band_support)
            {
                nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "band %d not supported by current modem Firmware [%d], please upgrade!", band, version));
            }
            if (!is_hw_new_band_support)
            {
                nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "band %d not supported by this board hardware!", band));
            }
            break;
        default:
            break;
    }

    if (band == 1) {
        lte_push_at_command("AT!=\"RRC::addScanBand band=1\"", LTE_RX_TIMEOUT_MIN_MS);
    } else if (band == 2) {
        lte_push_at_command("AT!=\"RRC::addScanBand band=2\"", LTE_RX_TIMEOUT_MIN_MS);
    } else if (band == 3) {
        lte_push_at_command("AT!=\"RRC::addScanBand band=3\"", LTE_RX_TIMEOUT_MIN_MS);
    } else if (band == 4) {
        lte_push_at_command("AT!=\"RRC::addScanBand band=4\"", LTE_RX_TIMEOUT_MIN_MS);
    } else if (band == 5) {
        lte_push_at_command("AT!=\"RRC::addScanBand band=5\"", LTE_RX_TIMEOUT_MIN_MS);
    } else if (band == 8) {
        lte_push_at_command("AT!=\"RRC::addScanBand band=8\"", LTE_RX_TIMEOUT_MIN_MS);
    } else if (band == 12) {
        lte_push_at_command("AT!=\"RRC::addScanBand band=12\"", LTE_RX_TIMEOUT_MIN_MS);
    } else if (band == 13) {
        lte_push_at_command("AT!=\"RRC::addScanBand band=13\"", LTE_RX_TIMEOUT_MIN_MS);
    } else if (band == 14) {
        lte_push_at_command("AT!=\"RRC::addScanBand band=14\"", LTE_RX_TIMEOUT_MIN_MS);
    } else if (band == 17) {
        lte_push_at_command("AT!=\"RRC::addScanBand band=17\"", LTE_RX_TIMEOUT_MIN_MS);
    } else if (band == 18) {
        lte_push_at_command("AT!=\"RRC::addScanBand band=18\"", LTE_RX_TIMEOUT_MIN_MS);
    } else if (band == 19) {
        lte_push_at_command("AT!=\"RRC::addScanBand band=19\"", LTE_RX_TIMEOUT_MIN_MS);
    } else if (band == 20) {
        lte_push_at_command("AT!=\"RRC::addScanBand band=20\"", LTE_RX_TIMEOUT_MIN_MS);
    } else if (band == 25) {
        lte_push_at_command("AT!=\"RRC::addScanBand band=25\"", LTE_RX_TIMEOUT_MIN_MS);
    } else if (band == 26) {
        lte_push_at_command("AT!=\"RRC::addScanBand band=26\"", LTE_RX_TIMEOUT_MIN_MS);
    } else if (band == 28) {
        lte_push_at_command("AT!=\"RRC::addScanBand band=28\"", LTE_RX_TIMEOUT_MIN_MS);
    } else if (band == 66) {
        lte_push_at_command("AT!=\"RRC::addScanBand band=66\"", LTE_RX_TIMEOUT_MIN_MS);
    } else {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "band %d not supported", band));
    }
}


STATIC mp_obj_t lte_attach(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    lte_check_init();
    bool is_hw_new_band_support = false;
    bool is_sw_new_band_support = false;
    STATIC const mp_arg_t allowed_args[] = {
        { MP_QSTR_band,             MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_apn,              MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_log,              MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_false} },
        { MP_QSTR_cid,              MP_ARG_KW_ONLY  | MP_ARG_INT, {.u_obj = mp_const_none} },
        { MP_QSTR_type,             MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_legacyattach,     MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_bands,            MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} },
    };

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    if (args[5].u_obj != mp_const_none) {
        lte_legacyattach_flag = mp_obj_is_true(args[5].u_obj);
    }

    lte_check_attached(lte_legacyattach_flag);

    if (lteppp_get_state() < E_LTE_ATTACHING) {

        if ( ! lte_check_sim_present() ) {
                nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_RuntimeError, "Sim card not present"));
        }

        const char *carrier = "standard";
        if (!lte_push_at_command("AT+SQNCTM?", LTE_RX_TIMEOUT_MAX_MS)) {
            if (!lte_push_at_command("AT+SQNCTM?", LTE_RX_TIMEOUT_MAX_MS)) {
                nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "Modem did not respond!\n"));
            }
        }
        if (strstr(modlte_rsp.data, carrier)) {
            lte_obj.carrier = false;
            /* Get configured bands in modem */
            lte_push_at_command_ext("AT+SMDD", LTE_RX_TIMEOUT_MAX_MS, NULL, strlen("AT+SMDD"));
            /* Dummy command for command response > Uart buff size */
            MP_THREAD_GIL_EXIT();
            if(strstr(modlte_rsp.data, "17 bands") != NULL)
            {
                is_hw_new_band_support = true;
            }
            while(modlte_rsp.data_remaining)
            {
                if (!is_hw_new_band_support) {
                    if(strstr(modlte_rsp.data, "17 bands") != NULL)
                    {
                        is_hw_new_band_support = true;
                    }
                }
                lte_push_at_command_ext("Pycom_Dummy", LTE_RX_TIMEOUT_MAX_MS, NULL, strlen("Pycom_Dummy"));
            }
            MP_THREAD_GIL_ENTER();
            int version = lte_get_modem_version();

            if(version > 0 && version > SQNS_SW_FULL_BAND_SUPPORT)
            {
                is_sw_new_band_support = true;
            }
            // configuring scanning in all bands
            lte_push_at_command("AT!=\"clearscanconfig\"", LTE_RX_TIMEOUT_MIN_MS);
            // Delay to ensure next addScan command is not discarded
            vTaskDelay(1000);

            if (args[0].u_obj == mp_const_none  && args[6].u_obj == mp_const_none) {
                // neither the argument 'band', nor 'bands' was supplied
                lte_push_at_command("AT!=\"RRC::addScanBand band=3\"", LTE_RX_TIMEOUT_MIN_MS);
                lte_push_at_command("AT!=\"RRC::addScanBand band=4\"", LTE_RX_TIMEOUT_MIN_MS);
                if (is_hw_new_band_support && version > SQNS_SW_5_8_BAND_SUPPORT) {
                    lte_push_at_command("AT!=\"RRC::addScanBand band=5\"", LTE_RX_TIMEOUT_MIN_MS);
                    lte_push_at_command("AT!=\"RRC::addScanBand band=8\"", LTE_RX_TIMEOUT_MIN_MS);
                }
                lte_push_at_command("AT!=\"RRC::addScanBand band=12\"", LTE_RX_TIMEOUT_MIN_MS);
                lte_push_at_command("AT!=\"RRC::addScanBand band=13\"", LTE_RX_TIMEOUT_MIN_MS);
                lte_push_at_command("AT!=\"RRC::addScanBand band=20\"", LTE_RX_TIMEOUT_MIN_MS);
                lte_push_at_command("AT!=\"RRC::addScanBand band=28\"", LTE_RX_TIMEOUT_MIN_MS);
            }
            else
            {
                // argument 'band'
                if (args[0].u_obj != mp_const_none) {
                    lte_add_band(mp_obj_get_int(args[0].u_obj), is_hw_new_band_support, is_sw_new_band_support, version);
                }

                // argument 'bands'
                if (args[6].u_obj != mp_const_none){
                    mp_obj_t *bands;
                    size_t n_bands=0;
                    mp_obj_get_array(args[6].u_obj, &n_bands, &bands);

                    for (size_t b = 0 ; b < n_bands ; ++b )
                    {
                        lte_add_band(mp_obj_get_int(bands[b]), is_hw_new_band_support, is_sw_new_band_support, version);
                    }
                }
            }
        } else {
            lte_obj.carrier = true;
        }

        // argument 'cid'
        if (args[3].u_obj != mp_const_none) {
            lte_obj.cid = args[3].u_int;
        }

        // argument 'apn'
        if (args[1].u_obj != mp_const_none || args[4].u_obj != mp_const_none) {
            const char* strapn;
            const char* strtype;

            if (args[1].u_obj == mp_const_none) {
                strapn = DEFAULT_APN;
            }
            else
            {
                strapn = mp_obj_str_get_str(args[1].u_obj);
            }

            if (args[4].u_obj == mp_const_none) {
                strtype = DEFAULT_PROTO_TYPE;
            }
            else
            {
                strtype = mp_obj_str_get_str(args[4].u_obj);
            }

            char at_cmd[LTE_AT_CMD_SIZE_MAX - 4];
            sprintf(at_cmd, "AT+CGDCONT=%d,\"%s\",\"%s\"", lte_obj.cid, strtype, strapn);
            if (!lte_push_at_command(at_cmd, LTE_RX_TIMEOUT_MAX_MS)) {
                nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
            }
        }

        // argument 'log'
        if (args[2].u_obj == mp_const_false) {
            lte_push_at_command("AT!=\"disablelog 1\"", LTE_RX_TIMEOUT_MAX_MS);
        } else {
            lte_push_at_command("AT!=\"disablelog 0\"", LTE_RX_TIMEOUT_MAX_MS);
        }

        lteppp_set_state(E_LTE_ATTACHING);
        if (!lte_push_at_command("AT+CFUN=1", LTE_RX_TIMEOUT_MAX_MS)) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
        }
        else
        {
            lte_push_at_command("AT!=\"setlpm airplane=1 enable=1\"", LTE_RX_TIMEOUT_MAX_MS);
        }
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(lte_attach_obj, 1, lte_attach);

mp_obj_t lte_detach(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    lte_check_init();

    STATIC const mp_arg_t allowed_args[] = {
        { MP_QSTR_reset,              MP_ARG_KW_ONLY  | MP_ARG_BOOL,  {.u_bool = false}},
    };

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    lte_obj_t *self = (lte_obj_t*)pos_args[0];

    if (lteppp_get_state() == E_LTE_PPP) {
        lte_disconnect(self);
    }
    if (lte_check_sim_present()) {
        if (args[0].u_bool) {
            if (lte_push_at_command("AT+CFUN=4,1", LTE_RX_TIMEOUT_MAX_MS)) {
                lteppp_wait_at_rsp("+S", LTE_RX_TIMEOUT_MAX_MS, true, NULL);
                mp_hal_delay_ms(LTE_RX_TIMEOUT_MIN_MS);
                if (!lteppp_wait_at_rsp("+SYSSTART", LTE_RX_TIMEOUT_MAX_MS, true, NULL)) {
                    lteppp_wait_at_rsp("+SYSSTART", LTE_RX_TIMEOUT_MAX_MS, true, NULL);
                }
                lte_push_at_command("AT", LTE_RX_TIMEOUT_MAX_MS);
                if (!lte_push_at_command("AT", LTE_RX_TIMEOUT_MAX_MS)) {
                    nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
                } else {
                    lte_push_at_command("AT!=\"setlpm airplane=1 enable=1\"", LTE_RX_TIMEOUT_MAX_MS);
                }
            }
        } else {
            if (!lte_push_at_command("AT+CFUN=4", LTE_RX_TIMEOUT_MAX_MS)) {
                goto error;
            }
        }
    } else {
        if (args[0].u_bool) {
            if (lte_push_at_command("AT+CFUN=0,1", LTE_RX_TIMEOUT_MAX_MS)) {
                lteppp_wait_at_rsp("+S", LTE_RX_TIMEOUT_MAX_MS, true, NULL);
                mp_hal_delay_ms(LTE_RX_TIMEOUT_MIN_MS);
                if (!lteppp_wait_at_rsp("+SYSSTART", LTE_RX_TIMEOUT_MAX_MS, true, NULL)) {
                    lteppp_wait_at_rsp("+SYSSTART", LTE_RX_TIMEOUT_MAX_MS, true, NULL);
                }
                lte_push_at_command("AT", LTE_RX_TIMEOUT_MAX_MS);
                if (!lte_push_at_command("AT", LTE_RX_TIMEOUT_MAX_MS)) {
                    nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
                }
            }
        } else {
            if (!lte_push_at_command("AT+CFUN=0", LTE_RX_TIMEOUT_MAX_MS)) {
                goto error;
            }
        }
    }
    lteppp_set_state(E_LTE_IDLE);
    return mp_const_none;

error:
    nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(lte_detach_obj, 1, lte_detach);

STATIC mp_obj_t lte_suspend(mp_obj_t self_in) {

    if (lteppp_get_legacy() == E_LTE_LEGACY) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "modem firmware not supported"));
    }
    lte_check_init();
    if (lteppp_get_state() == E_LTE_PPP) {
        lteppp_suspend();
        MSG("Pausing ppp...\n");
        lte_pause_ppp();
        MSG("Pausing ppp done...\n");
        lteppp_set_state(E_LTE_SUSPENDED);
        while (true) {
            mp_hal_delay_ms(LTE_RX_TIMEOUT_MIN_MS);
            MSG("Sending AT...\n");
            if (lte_push_at_command("AT", LTE_RX_TIMEOUT_MAX_MS)) {
                MSG("OK\n");
                break;
            }
        }
        lte_check_attached(lte_legacyattach_flag);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lte_suspend_obj, lte_suspend);



STATIC mp_obj_t lte_isattached(mp_obj_t self_in) {
    lte_check_init();
    if (lte_check_attached(lte_legacyattach_flag)) {
        return mp_const_true;
    }
    return mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lte_isattached_obj, lte_isattached);

STATIC mp_obj_t lte_connect(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    lte_check_init();
    if (lteppp_get_state() == E_LTE_PPP  && lteppp_ipv4() > 0) {
        return mp_const_none;
    }
    STATIC const mp_arg_t allowed_args[] = {
        { MP_QSTR_cid,      MP_ARG_KW_ONLY  | MP_ARG_INT, {.u_obj = mp_const_none} },
        { MP_QSTR_legacy,   MP_ARG_KW_ONLY  | MP_ARG_BOOL, {.u_obj = mp_const_none} },

    };

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    if (args[1].u_obj == mp_const_none) {
        args[1].u_bool = lte_check_legacy_version();
    }

    lte_check_attached(lte_legacyattach_flag);
    lteppp_set_legacy(args[1].u_bool);

    if (args[1].u_bool) {
        lte_push_at_command("ATH", LTE_RX_TIMEOUT_MIN_MS);
        while (true) {
            mp_hal_delay_ms(LTE_RX_TIMEOUT_MIN_MS);
            if (lte_push_at_command("AT", LTE_RX_TIMEOUT_MAX_MS)) {
                break;
            }
        }
    }

    if (lteppp_get_state() == E_LTE_ATTACHED || (args[1].u_bool && lteppp_get_state() == E_LTE_SUSPENDED)) {
        if (args[1].u_bool || !lte_push_at_command_ext("ATO", LTE_RX_TIMEOUT_MAX_MS, LTE_CONNECT_RSP, strlen("ATO") )) {
            char at_cmd[LTE_AT_CMD_SIZE_MAX - 4];
            if (args[0].u_obj != mp_const_none) {
                lte_obj.cid = args[0].u_int;
            }
            sprintf(at_cmd, "AT+CGDATA=\"PPP\",%d", lte_obj.cid);
            // set the PPP state in advance, to avoid CEREG? to be sent right after PPP is entered
            if (!lte_push_at_command_ext(at_cmd, LTE_RX_TIMEOUT_MAX_MS, LTE_CONNECT_RSP, strlen(at_cmd) )) {
                nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
            }
        }
        mod_network_register_nic(&lte_obj);
        lteppp_connect();
        lteppp_set_state(E_LTE_PPP);
        vTaskDelay(1000);
    } else if (lteppp_get_state() == E_LTE_PPP) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "modem already connected"));
    } else if (lteppp_get_state() == E_LTE_SUSPENDED) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "modem is suspended"));
    } else {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "modem not attached"));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(lte_connect_obj, 1, lte_connect);

STATIC mp_obj_t lte_resume(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    lte_check_init();
    STATIC const mp_arg_t allowed_args[] = {
        { MP_QSTR_cid,      MP_ARG_KW_ONLY  | MP_ARG_INT, {.u_obj = mp_const_none} },
    };

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    if (lteppp_get_state() == E_LTE_PPP) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "Modem is already connected"));
    }
    lte_check_attached(lte_legacyattach_flag);

    if (lteppp_get_state() == E_LTE_SUSPENDED || lteppp_get_state() == E_LTE_ATTACHED) {
        if (lteppp_get_state() == E_LTE_ATTACHED && lteppp_get_legacy() == E_LTE_LEGACY) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "Operation failed (attached and legacy)"));
        }
//        char at_cmd[LTE_AT_CMD_SIZE_MAX - 4];
        if (args[0].u_obj != mp_const_none) {
            lte_obj.cid = args[0].u_int;
        }

        if (lte_push_at_command_ext("ATO", LTE_RX_TIMEOUT_MAX_MS, LTE_CONNECT_RSP, strlen("ATO") )) {
            MSG("resume ATO OK\n");
            lteppp_connect();
            lteppp_resume();
            lteppp_set_state(E_LTE_PPP);
            vTaskDelay(1500);
        } else {
            MSG("resume ATO failed -> reconnect\n");
            lteppp_disconnect();
            lteppp_set_state(E_LTE_ATTACHED);
            lte_check_attached(lte_legacyattach_flag);
            return lte_connect(n_args, pos_args, kw_args);
        }
    } else if (lteppp_get_state() == E_LTE_PPP) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "modem already connected"));
    } else {
        MSG("resume do nothing\n");
        //Do Nothing
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(lte_resume_obj, 1, lte_resume);

STATIC mp_obj_t lte_disconnect(mp_obj_t self_in) {
    lte_check_init();
    if (lteppp_get_state() == E_LTE_PPP || lteppp_get_state() == E_LTE_SUSPENDED) {
        lteppp_disconnect();
        if (lteppp_get_state() == E_LTE_PPP) {
            lte_pause_ppp();
        }
        lteppp_set_state(E_LTE_ATTACHED);
        lte_push_at_command("ATH", LTE_RX_TIMEOUT_MAX_MS);
        while (true) {
            if (lte_push_at_command("AT", LTE_RX_TIMEOUT_MAX_MS)) {
                break;
            }
            mp_hal_delay_ms(LTE_RX_TIMEOUT_MIN_MS);
        }
        lte_check_attached(lte_legacyattach_flag);
        mod_network_deregister_nic(&lte_obj);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lte_disconnect_obj, lte_disconnect);

STATIC mp_obj_t lte_isconnected(mp_obj_t self_in) {
    lte_check_init();
    if (ltepp_is_ppp_conn_up()) {
        return mp_const_true;
    }
    else
    {
        return mp_const_false;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lte_isconnected_obj, lte_isconnected);

STATIC mp_obj_t lte_send_raw_at(mp_obj_t self_in, mp_obj_t cmd_o) {
    lte_check_init();
    lte_check_inppp();
    const char *cmd = mp_obj_str_get_str(cmd_o);
    lte_push_at_command((char *)cmd, LTE_RX_TIMEOUT_MAX_MS);
    vstr_t vstr;
    vstr_init_len(&vstr, strlen(modlte_rsp.data));
    strcpy(vstr.buf, modlte_rsp.data);
    return mp_obj_new_str_from_vstr(&mp_type_str, &vstr);
}
//STATIC MP_DEFINE_CONST_FUN_OBJ_2(lte_send_raw_at_obj, lte_send_raw_at);

STATIC mp_obj_t lte_send_at_cmd(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    lte_check_init();
    lte_check_inppp();
    STATIC const mp_arg_t allowed_args[] = {
        { MP_QSTR_cmd,        MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_timeout,    MP_ARG_KW_ONLY  | MP_ARG_INT, {.u_int = LTE_RX_TIMEOUT_MAX_MS} },
    };
    // parse args
    uint32_t argLength = MP_ARRAY_SIZE(allowed_args);
    mp_arg_val_t args[argLength];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);
    if (args[0].u_obj == mp_const_none) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "the command must be specified!"));
    }
    if (MP_OBJ_IS_STR_OR_BYTES(args[0].u_obj))
    {
        size_t len;
        lte_push_at_command_ext((char *)(mp_obj_str_get_data(args[0].u_obj, &len)), args[1].u_int, NULL, len);
    }
    else
    {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_TypeError, mpexception_num_type_invalid_arguments));
    }

    vstr_t vstr;
    vstr_init(&vstr, 0);
    vstr_add_str(&vstr, modlte_rsp.data);
    while(modlte_rsp.data_remaining)
    {
        lte_push_at_command_ext("Pycom_Dummy", args[1].u_int, NULL, strlen("Pycom_Dummy") );
        vstr_add_str(&vstr, modlte_rsp.data);
    }
    return mp_obj_new_str_from_vstr(&mp_type_str, &vstr);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(lte_send_at_cmd_obj, 1, lte_send_at_cmd);


STATIC mp_obj_t lte_imei(mp_obj_t self_in) {
    lte_check_init();
    lte_check_inppp();
    char *pos;
    vstr_t vstr;
    char *resp;
    vstr_init_len(&vstr, strlen("AT+CGSN"));
    strcpy(vstr.buf, "AT+CGSN");
    lte_send_raw_at(MP_OBJ_NULL, mp_obj_new_str_from_vstr(&mp_type_str, &vstr));
    resp = modlte_rsp.data;
    if (strcmp(resp, "000000000000000") == 0) {
        vstr_init_len(&vstr, 5);
        memcpy(vstr.buf, "UNSET", 5);
        return mp_obj_new_str_from_vstr(&mp_type_str, &vstr);
    }
    if ((pos = strstr(resp, "35434")) && (strlen(pos) > 20)) {
        vstr_init_len(&vstr, 15);
        memcpy(vstr.buf, pos, 15);
        return mp_obj_new_str_from_vstr(&mp_type_str, &vstr);
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lte_imei_obj, lte_imei);

STATIC mp_obj_t lte_time(mp_obj_t self_in) {
    lte_check_init();
    lte_check_inppp();
    char *pos;
    vstr_t vstr;
    char *resp;
    vstr_init_len(&vstr, strlen("AT+CCLK?"));
    strcpy(vstr.buf, "AT+CCLK?");
    lte_send_raw_at(MP_OBJ_NULL, mp_obj_new_str_from_vstr(&mp_type_str, &vstr));
    resp = modlte_rsp.data;
    if ((pos = strstr(resp, "+CCLK:")) && (strlen(pos) > 20)) {
        char *start_pos;
        char *end_pos;
        start_pos = strstr(pos,"\"");
        end_pos = strstr(pos,"\r\n");
        vstr_init_len(&vstr, strlen(start_pos) - strlen(end_pos) - 2);
        memcpy(vstr.buf, &start_pos[1], strlen(pos) - strlen(end_pos) - 2);
        return mp_obj_new_str_from_vstr(&mp_type_str, &vstr);
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lte_time_obj, lte_time);

STATIC mp_obj_t lte_iccid(mp_obj_t self_in) {
	lte_check_init();
	lte_check_inppp();
	if (lte_check_sim_present()) {
		char *pos, *iccid;
		vstr_t vstr;
		vstr_init_len(&vstr, strlen("AT+SQNCCID?"));
		strcpy(vstr.buf, "AT+SQNCCID?");
		lte_send_raw_at(MP_OBJ_NULL, mp_obj_new_str_from_vstr(&mp_type_str, &vstr));
		if ((pos = strstr(modlte_rsp.data, "SQNCCID:")) && (strlen(pos) > 25)) {
			iccid = strchr(pos, '"')+1;
			pos = strchr(iccid, '"');
			vstr_init_len(&vstr, strlen(iccid)-strlen(pos));
			memcpy(vstr.buf, iccid, strlen(iccid)-strlen(pos));
			return mp_obj_new_str_from_vstr(&mp_type_str, &vstr);
		}
	} else {
		nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "SIM card not found!"));
	}
	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lte_iccid_obj, lte_iccid);

STATIC mp_obj_t lte_ue_coverage(mp_obj_t self_in) {
    lte_check_init();
    lte_check_inppp();
    if(lte_check_attached(lte_legacyattach_flag))
    {
        return lte_ue_is_out_of_coverage?mp_const_false:mp_const_true;
    }
    else
    {
        return mp_const_false;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lte_ue_coverage_obj, lte_ue_coverage);

STATIC mp_obj_t lte_reset(mp_obj_t self_in) {
    lte_check_init();
    lte_disconnect(self_in);
    if (!lte_push_at_command("AT+CFUN=0", LTE_RX_TIMEOUT_MAX_MS)) {
        mp_hal_delay_ms(LTE_RX_TIMEOUT_MIN_MS);
        lte_push_at_command("AT+CFUN=0", LTE_RX_TIMEOUT_MAX_MS);
    }
    mp_hal_delay_ms(LTE_RX_TIMEOUT_MIN_MS);
    lte_push_at_command("AT^RESET", LTE_RX_TIMEOUT_MAX_MS);
    lteppp_set_state(E_LTE_IDLE);
    mp_hal_delay_ms(LTE_RX_TIMEOUT_MIN_MS);
    if (!lteppp_wait_at_rsp("+SYSSTART", LTE_RX_TIMEOUT_MAX_MS, true, NULL)) {
        lteppp_wait_at_rsp("+SYSSTART", LTE_RX_TIMEOUT_MAX_MS, true, NULL);
    }
    lte_push_at_command("AT", LTE_RX_TIMEOUT_MAX_MS);
    if (!lte_push_at_command("AT", LTE_RX_TIMEOUT_MAX_MS)) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lte_reset_obj, lte_reset);

STATIC mp_obj_t lte_factory_reset(mp_obj_t self_in) {
    lte_check_init();
    lte_disconnect(self_in);
    if (!lte_push_at_command("AT&F", LTE_RX_TIMEOUT_MAX_MS * 2)) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
    }
    lte_obj.init = false;
    lte_push_at_command("AT^RESET", LTE_RX_TIMEOUT_MAX_MS);
    lteppp_set_state(E_LTE_IDLE);
    mp_hal_delay_ms(LTE_RX_TIMEOUT_MIN_MS);
    uint8_t timeout = 0;
    while (!lteppp_wait_at_rsp("+SYSSTART", LTE_RX_TIMEOUT_MAX_MS, true, NULL)) {
        if(timeout > 3)
        {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
        }
        timeout++;
    }
    lte_push_at_command("AT", LTE_RX_TIMEOUT_MAX_MS);
    if (!lte_push_at_command("AT", LTE_RX_TIMEOUT_MAX_MS)) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
    }
    machine_reset();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lte_factory_reset_obj, lte_factory_reset);

STATIC mp_obj_t lte_upgrade_mode(void) {

    if(lte_obj.init)
    {
        //nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "Modem not disabled"));
        lte_obj.init = false;
    }

    // uninstall the driver
    uart_driver_delete(0);
    uart_driver_delete(1);
    uart_driver_delete(2);

    // initialize the UART interface
    lte_uart_config1.baud_rate = MICROPY_LTE_UART_BAUDRATE;
    lte_uart_config1.data_bits = UART_DATA_8_BITS;
    lte_uart_config1.parity = UART_PARITY_DISABLE;
    lte_uart_config1.stop_bits = UART_STOP_BITS_1;
    lte_uart_config1.flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS;
    lte_uart_config1.rx_flow_ctrl_thresh = 64;
    uart_param_config(1, &lte_uart_config1);

    //deassign LTE Uart pins
    pin_deassign(MICROPY_LTE_TX_PIN);
    gpio_pullup_dis(MICROPY_LTE_TX_PIN->pin_number);

    pin_deassign(MICROPY_LTE_RX_PIN);
    gpio_pullup_dis(MICROPY_LTE_RX_PIN->pin_number);

    pin_deassign(MICROPY_LTE_CTS_PIN);
    gpio_pullup_dis(MICROPY_LTE_CTS_PIN->pin_number);

    pin_deassign(MICROPY_LTE_RTS_PIN);
    gpio_pullup_dis(MICROPY_LTE_RTS_PIN->pin_number);

    // configure the UART pins
    pin_config(MICROPY_LTE_TX_PIN, -1, U1TXD_OUT_IDX, GPIO_MODE_OUTPUT, MACHPIN_PULL_NONE, 1);
    pin_config(MICROPY_LTE_RX_PIN, U1RXD_IN_IDX, -1, GPIO_MODE_INPUT, MACHPIN_PULL_NONE, 1);
    pin_config(MICROPY_LTE_RTS_PIN, -1, U1RTS_OUT_IDX, GPIO_MODE_OUTPUT, MACHPIN_PULL_NONE, 1);
    pin_config(MICROPY_LTE_CTS_PIN, U1CTS_IN_IDX, -1, GPIO_MODE_INPUT, MACHPIN_PULL_NONE, 1);

    // install the UART driver
    uart_driver_install(1, 8192, 8192, 0, NULL, 0, NULL);
    uart_driver_lte = &UART1;

    // disable the delay between transfers
    uart_driver_lte->idle_conf.tx_idle_num = 0;

    // configure the rx timeout threshold
    uart_driver_lte->conf1.rx_tout_thrhd = 10 & UART_RX_TOUT_THRHD_V;


    lte_uart_config0.baud_rate = MICROPY_LTE_UART_BAUDRATE;
    lte_uart_config0.data_bits = UART_DATA_8_BITS;
    lte_uart_config0.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    lte_uart_config0.parity = UART_PARITY_DISABLE;
    lte_uart_config0.rx_flow_ctrl_thresh = 64;
    lte_uart_config0.stop_bits = 1.0;

    uart_param_config(0, &lte_uart_config0);

    pin_config(&PIN_MODULE_P1, -1, U0TXD_OUT_IDX, GPIO_MODE_OUTPUT, MACHPIN_PULL_NONE, 1);
    pin_config(&PIN_MODULE_P0, U0RXD_IN_IDX, -1, GPIO_MODE_INPUT, MACHPIN_PULL_NONE, 1);

    // install the UART driver
    uart_driver_install(0, 8192, 8192, 0, NULL, 0, NULL);

    // disable the delay between transfers
    uart_driver_0->idle_conf.tx_idle_num = 0;

    // configure the rx timeout threshold
    uart_driver_0->conf1.rx_tout_thrhd = 10 & UART_RX_TOUT_THRHD_V;

    xTaskCreatePinnedToCore(TASK_LTE_UPGRADE, "LTE_UPGRADE", 3072 / sizeof(StackType_t), NULL, 7, &xLTEUpgradeTaskHndl, 1);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lte_upgrade_mode_obj, lte_upgrade_mode);
#ifdef LTE_DEBUG_BUFF
STATIC mp_obj_t lte_debug_buff(void) {
    vstr_t vstr;
    char* str_log = lteppp_get_log_buff();
    vstr_init_len(&vstr, strlen(str_log));
    strcpy(vstr.buf, str_log);
    return mp_obj_new_str_from_vstr(&mp_type_str, &vstr);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lte_debug_buff_obj, lte_debug_buff);
#endif
STATIC mp_obj_t lte_reconnect_uart (void) {
    connect_lte_uart();
    lteppp_disconnect();
    lte_obj.init = false;
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lte_reconnect_uart_obj, lte_reconnect_uart);

STATIC mp_obj_t lte_callback(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    STATIC const mp_arg_t allowed_args[] = {
        { MP_QSTR_trigger,      MP_ARG_REQUIRED | MP_ARG_OBJ,   },
        { MP_QSTR_handler,      MP_ARG_OBJ,                     {.u_obj = mp_const_none} },
        { MP_QSTR_arg,          MP_ARG_OBJ,                     {.u_obj = mp_const_none} },
    };

    // parse arguments
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), allowed_args, args);
    lte_obj_t *self = pos_args[0];

    // enable the callback
    if (args[0].u_obj != mp_const_none && args[1].u_obj != mp_const_none)
    {
        self->trigger = mp_obj_get_int(args[0].u_obj);

        self->handler = args[1].u_obj;

        if (args[2].u_obj == mp_const_none) {
            self->handler_arg = self;
        } else {
            self->handler_arg = args[2].u_obj;
        }
    }
    else
    {   // disable the callback
        self->trigger = 0;
        mp_irq_remove(self);
        INTERRUPT_OBJ_CLEAN(self);
    }

    mp_irq_add(self, args[1].u_obj);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(lte_callback_obj, 1, lte_callback);

STATIC mp_obj_t lte_events(mp_obj_t self_in) {
    lte_obj_t *self = self_in;

    int32_t events = self->events;
    self->events = 0;
    return mp_obj_new_int(events);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lte_events_obj, lte_events);

STATIC const mp_map_elem_t lte_locals_dict_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR_init),                (mp_obj_t)&lte_init_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_deinit),              (mp_obj_t)&lte_deinit_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_psm),                 (mp_obj_t)&lte_psm_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_attach),              (mp_obj_t)&lte_attach_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_dettach),             (mp_obj_t)&lte_detach_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_detach),              (mp_obj_t)&lte_detach_obj }, /* backward compatibility for dettach method FIXME */
    { MP_OBJ_NEW_QSTR(MP_QSTR_isattached),          (mp_obj_t)&lte_isattached_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_connect),             (mp_obj_t)&lte_connect_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_disconnect),          (mp_obj_t)&lte_disconnect_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_pppsuspend),          (mp_obj_t)&lte_suspend_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_pppresume),           (mp_obj_t)&lte_resume_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_time),                (mp_obj_t)&lte_time_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_isconnected),         (mp_obj_t)&lte_isconnected_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_imei),                (mp_obj_t)&lte_imei_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_iccid),               (mp_obj_t)&lte_iccid_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_send_at_cmd),         (mp_obj_t)&lte_send_at_cmd_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_reset),               (mp_obj_t)&lte_reset_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_factory_reset),       (mp_obj_t)&lte_factory_reset_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_modem_upgrade_mode),  (mp_obj_t)&lte_upgrade_mode_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_reconnect_uart),      (mp_obj_t)&lte_reconnect_uart_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ue_coverage),         (mp_obj_t)&lte_ue_coverage_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_lte_callback),         (mp_obj_t)&lte_callback_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_events),              (mp_obj_t)&lte_events_obj },
#ifdef LTE_DEBUG_BUFF
    { MP_OBJ_NEW_QSTR(MP_QSTR_debug_buff),          (mp_obj_t)&lte_debug_buff_obj },
#endif

    // class constants
    { MP_OBJ_NEW_QSTR(MP_QSTR_IP),                   MP_OBJ_NEW_QSTR(MP_QSTR_IP) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_IPV4V6),               MP_OBJ_NEW_QSTR(MP_QSTR_IPV4V6) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_EVENT_COVERAGE_LOSS),  MP_OBJ_NEW_SMALL_INT(LTE_EVENT_COVERAGE_LOST) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_EVENT_BREAK),          MP_OBJ_NEW_SMALL_INT(LTE_EVENT_BREAK) },
    // PSM Power Saving Mode
    { MP_OBJ_NEW_QSTR(MP_QSTR_PSM_PERIOD_2S),        MP_OBJ_NEW_SMALL_INT(PSM_PERIOD_2S) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PSM_PERIOD_30S),       MP_OBJ_NEW_SMALL_INT(PSM_PERIOD_30S) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PSM_PERIOD_1M),        MP_OBJ_NEW_SMALL_INT(PSM_PERIOD_1M) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PSM_PERIOD_10M),       MP_OBJ_NEW_SMALL_INT(PSM_PERIOD_10M) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PSM_PERIOD_1H),        MP_OBJ_NEW_SMALL_INT(PSM_PERIOD_1H) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PSM_PERIOD_10H),       MP_OBJ_NEW_SMALL_INT(PSM_PERIOD_10H) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PSM_PERIOD_320H),      MP_OBJ_NEW_SMALL_INT(PSM_PERIOD_320H) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PSM_PERIOD_DISABLED),  MP_OBJ_NEW_SMALL_INT(PSM_PERIOD_DISABLED) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PSM_ACTIVE_2S),        MP_OBJ_NEW_SMALL_INT(PSM_ACTIVE_2S) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PSM_ACTIVE_1M),        MP_OBJ_NEW_SMALL_INT(PSM_ACTIVE_1M) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PSM_ACTIVE_6M),        MP_OBJ_NEW_SMALL_INT(PSM_ACTIVE_6M) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PSM_ACTIVE_DISABLED),  MP_OBJ_NEW_SMALL_INT(PSM_ACTIVE_DISABLED) },

};
STATIC MP_DEFINE_CONST_DICT(lte_locals_dict, lte_locals_dict_table);

const mod_network_nic_type_t mod_network_nic_type_lte = {
    .base = {
        { &mp_type_type },
        .name = MP_QSTR_LTE,
        .make_new = lte_make_new,
        .locals_dict = (mp_obj_t)&lte_locals_dict,
     },

    .n_gethostbyname = lwipsocket_gethostbyname,
    .n_listen = lwipsocket_socket_listen,
    .n_accept = lwipsocket_socket_accept,
    .n_socket = lwipsocket_socket_socket,
    .n_close = lwipsocket_socket_close,
    .n_connect = lwipsocket_socket_connect,
    .n_sendto =  lwipsocket_socket_sendto,
    .n_send = lwipsocket_socket_send,
    .n_recv = lwipsocket_socket_recv,
    .n_recvfrom = lwipsocket_socket_recvfrom,
    .n_settimeout = lwipsocket_socket_settimeout,
    .n_setsockopt = lwipsocket_socket_setsockopt,
    .n_bind = lwipsocket_socket_bind,
    .n_ioctl = lwipsocket_socket_ioctl,
    .n_setupssl = lwipsocket_socket_setup_ssl,
    .inf_up = ltepp_is_ppp_conn_up,
    .set_default_inf = lte_set_default_inf
};
