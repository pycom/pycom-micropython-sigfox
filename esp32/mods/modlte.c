/*
* Copyright (c) 2017, Pycom Limited and its licensors.
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
#include "esp_event.h"
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
/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/
static lte_obj_t lte_obj = {.init = false};
static lte_task_rsp_data_t modlte_rsp;
uart_dev_t* uart_driver_0 = &UART0;
uart_dev_t* uart_driver_lte = &UART2;

uart_config_t lte_uart_config0;
uart_config_t lte_uart_config1;

static bool lte_legacyattach_flag = true;

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
static bool lte_push_at_command_ext (char *cmd_str, uint32_t timeout, const char *expected_rsp);
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
//*****************************************************************************
// DEFINE STATIC FUNCTIONS
//*****************************************************************************

static bool lte_push_at_command_ext(char *cmd_str, uint32_t timeout, const char *expected_rsp) {
    lte_task_cmd_data_t cmd = { .timeout = timeout };
    memcpy(cmd.data, cmd_str, strlen(cmd_str));
    //printf("[CMD] %s\n", cmd_str);
    lteppp_send_at_command(&cmd, &modlte_rsp);
    if (strstr(modlte_rsp.data, expected_rsp) != NULL) {
        //printf("[OK] %s\n", modlte_rsp.data);
        return true;
    }
    //printf("[FAIL] %s\n", modlte_rsp.data);
    return false;
}

static bool lte_push_at_command_delay_ext (char *cmd_str, uint32_t timeout, const char *expected_rsp, TickType_t delay) {
    lte_task_cmd_data_t cmd = { .timeout = timeout };
    memcpy(cmd.data, cmd_str, strlen(cmd_str));
    //printf("[CMD] %s\n", cmd_str);
    lteppp_send_at_command_delay(&cmd, &modlte_rsp, delay);
    if (strstr(modlte_rsp.data, expected_rsp) != NULL) {
        //printf("[OK] %s\n", modlte_rsp.data);
        return true;
    }
    //printf("[FAIL] %s\n", modlte_rsp.data);
    return false;
}

static bool lte_push_at_command (char *cmd_str, uint32_t timeout) {
    return lte_push_at_command_ext(cmd_str, timeout, LTE_OK_RSP);
}

static bool lte_push_at_command_delay (char *cmd_str, uint32_t timeout, TickType_t delay) {
    return lte_push_at_command_delay_ext(cmd_str, timeout, LTE_OK_RSP, delay);
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
                            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
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
                if (((pos = strstr(modlte_rsp.data, "+CEREG: 2,1,")) || (pos = strstr(modlte_rsp.data, "+CEREG: 2,5,")))
                        && (strlen(pos) >= 31) && (pos[30] == '7' || pos[30] == '9')) {
                    attached = true;
                }
            } else {
                if ((pos = strstr(modlte_rsp.data, "+CEREG: 2,1,")) || (pos = strstr(modlte_rsp.data, "+CEREG: 2,5,")) || (pos = strstr(modlte_rsp.data, "+CEREG: 2,4"))) {
                    attached = true;
                    if((pos = strstr(modlte_rsp.data, "+CEREG: 2,4")))
                    {
                        lte_ue_is_out_of_coverage = true;
                    }
                    else
                    {
                        lte_ue_is_out_of_coverage = false;
                    }
                } else {
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
    lte_task_cmd_data_t cmd = { .timeout = LTE_RX_TIMEOUT_MAX_MS };

    /* Get modem version */
    memcpy(cmd.data, "AT!=\"showver\"", strlen("AT!=\"showver\""));
    char * ver = NULL;

    lteppp_send_at_command(&cmd, &modlte_rsp);
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

static void lte_check_inppp(void) {
    if (lteppp_get_state() == E_LTE_PPP) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "LTE modem is in data state, cannot send AT commands"));
    }
}

static bool lte_check_sim_present(void) {
    lte_push_at_command("AT+CPIN?", LTE_RX_TIMEOUT_MAX_MS);
    if (strstr(modlte_rsp.data, "ERROR")) {
        lte_push_at_command("AT+CPIN?", LTE_RX_TIMEOUT_MAX_MS);
        if (strstr(modlte_rsp.data, "ERROR")) {
            return false;
        } else {
            return true;
        }
    } else {
        return true;
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
    char at_cmd[LTE_AT_CMD_SIZE_MAX - 4];
    lte_modem_conn_state_t modem_state;

    if (lte_obj.init) {
        //printf("All done since we were already initialised.\n");
        return mp_const_none;
    }
    modem_state  = lteppp_modem_state();
    switch(modem_state)
    {
    case E_LTE_MODEM_DISCONNECTED:
        // Notify the LTE thread to start
        modlte_start_modem();
        MP_THREAD_GIL_EXIT();
        xSemaphoreTake(xLTE_modem_Conn_Sem, portMAX_DELAY);
        MP_THREAD_GIL_ENTER();
        if (E_LTE_MODEM_DISCONNECTED == lteppp_modem_state()) {
            xSemaphoreGive(xLTE_modem_Conn_Sem);
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "Couldn't connect to Modem!"));
        }
        break;
    case E_LTE_MODEM_CONNECTING:
        // Block till modem is connected
        xSemaphoreTake(xLTE_modem_Conn_Sem, portMAX_DELAY);
        if (E_LTE_MODEM_DISCONNECTED == lteppp_modem_state()) {
            xSemaphoreGive(xLTE_modem_Conn_Sem);
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "Couldn't connect to Modem!"));
        }
        break;
    case E_LTE_MODEM_CONNECTED:
        //continue
        break;
    default:
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "Couldn't connect to Modem!"));
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
        if (!strstr(modlte_rsp.data, carrier)) {
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

        // at least enable access to the SIM
        lte_push_at_command("AT+CFUN=4", LTE_RX_TIMEOUT_MAX_MS);
        lte_push_at_command("AT", LTE_RX_TIMEOUT_MAX_MS);
    }

    lteppp_set_state(E_LTE_IDLE);
    mod_network_register_nic(&lte_obj);
    lte_obj.init = true;
    xSemaphoreGive(xLTE_modem_Conn_Sem);
    return mp_const_none;
}

static const mp_arg_t lte_init_args[] = {
    { MP_QSTR_id,                                   MP_ARG_INT, {.u_int = 0} },
    { MP_QSTR_carrier,                              MP_ARG_KW_ONLY  | MP_ARG_OBJ,  {.u_obj = mp_const_none} },
    { MP_QSTR_cid,                                  MP_ARG_KW_ONLY  | MP_ARG_INT,  {.u_int = 1} },
    { MP_QSTR_legacyattach,                         MP_ARG_KW_ONLY  | MP_ARG_BOOL, {.u_bool = true} }
};

static mp_obj_t lte_make_new(const mp_obj_type_t *type, mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *all_args) {
    // parse args
    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, all_args + n_args);
    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(lte_init_args)];
    mp_arg_parse_all(n_args, all_args, &kw_args, MP_ARRAY_SIZE(args), lte_init_args, args);

    // setup the object
    lte_obj_t *self = &lte_obj;
    self->base.type = (mp_obj_t)&mod_network_nic_type_lte;

    if (n_kw > 0) {
        // check the peripheral id
        if (args[0].u_int != 0) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_resource_not_avaliable));
        }
    }
    // start the peripheral
    lte_init_helper(self, &args[1]);
    return (mp_obj_t)self;
}

STATIC mp_obj_t lte_init(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(lte_init_args) - 1];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), &lte_init_args[1], args);
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

    };

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    if (args[5].u_obj != mp_const_none) {
        lte_legacyattach_flag = mp_obj_is_true(args[5].u_obj);
    }

    lte_check_attached(lte_legacyattach_flag);

    if (lteppp_get_state() < E_LTE_ATTACHING) {

        if (!lte_obj.carrier) {
            /* Get configured bands in modem */
            lte_task_cmd_data_t cmd = { .timeout = LTE_RX_TIMEOUT_MAX_MS };
            memcpy(cmd.data, "AT+SMDD", strlen("AT+SMDD"));
            lteppp_send_at_command(&cmd, &modlte_rsp);
            /* Dummy command for command response > Uart buff size */
            memcpy(cmd.data, "Pycom_Dummy", strlen("Pycom_Dummy"));
            MP_THREAD_GIL_EXIT();
            while(modlte_rsp.data_remaining)
            {
                if((strstr(modlte_rsp.data, "17 bands") != NULL) && !is_hw_new_band_support)
                {
                    is_hw_new_band_support = true;
                }
                lteppp_send_at_command(&cmd, &modlte_rsp);
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
            if (args[0].u_obj == mp_const_none) {
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
                uint32_t band = mp_obj_get_int(args[0].u_obj);
                /* Check band support */
                switch(band)
                {
                    case 5:
                    case 8:
                        if(!is_hw_new_band_support)
                        {
                            nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "band %d not supported by this board hardware!", band));
                        }
                        else if(version < SQNS_SW_5_8_BAND_SUPPORT)
                        {
                            nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "band %d not supported by current modem Firmware, please upgrade!", band));
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
                            nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "band %d not supported by current modem Firmware, please upgrade!", band));
                        }
                        if(!is_hw_new_band_support)
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
        }
        if (args[3].u_obj != mp_const_none) {
            lte_obj.cid = args[3].u_int;
        }

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
        //printf("Pausing ppp...\n");
        lte_pause_ppp();
        //printf("Pausing ppp done...\n");
        lteppp_set_state(E_LTE_SUSPENDED);
        while (true) {
            mp_hal_delay_ms(LTE_RX_TIMEOUT_MIN_MS);
            //printf("Sending AT...\n");
            if (lte_push_at_command("AT", LTE_RX_TIMEOUT_MAX_MS)) {
                //printf("OK\n");
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
        if (args[1].u_bool || !lte_push_at_command_ext("ATO", LTE_RX_TIMEOUT_MAX_MS, LTE_CONNECT_RSP)) {
            char at_cmd[LTE_AT_CMD_SIZE_MAX - 4];
            if (args[0].u_obj != mp_const_none) {
                lte_obj.cid = args[0].u_int;
            }
            sprintf(at_cmd, "AT+CGDATA=\"PPP\",%d", lte_obj.cid);
            // set the PPP state in advance, to avoid CEREG? to be sent right after PPP is entered
            if (!lte_push_at_command_ext(at_cmd, LTE_RX_TIMEOUT_MAX_MS, LTE_CONNECT_RSP)) {
                nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
            }
        }
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
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));
    }
    lte_check_attached(lte_legacyattach_flag);

    if (lteppp_get_state() == E_LTE_SUSPENDED || lteppp_get_state() == E_LTE_ATTACHED) {
        if (lteppp_get_state() == E_LTE_ATTACHED && lteppp_get_legacy() == E_LTE_LEGACY) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));
        }
//        char at_cmd[LTE_AT_CMD_SIZE_MAX - 4];
        if (args[0].u_obj != mp_const_none) {
            lte_obj.cid = args[0].u_int;
        }

        if (lte_push_at_command_ext("ATO", LTE_RX_TIMEOUT_MAX_MS, LTE_CONNECT_RSP)) {
            lteppp_connect();
            lteppp_resume();
            lteppp_set_state(E_LTE_PPP);
            vTaskDelay(1500);
        } else {
            lteppp_disconnect();
            lteppp_set_state(E_LTE_ATTACHED);
            lte_check_attached(lte_legacyattach_flag);
            return lte_connect(n_args, pos_args, kw_args);
        }
    } else if (lteppp_get_state() == E_LTE_PPP) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "modem already connected"));
    } else {
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
        lte_push_at_command("ATH", LTE_RX_TIMEOUT_MIN_MS);
        while (true) {
            mp_hal_delay_ms(LTE_RX_TIMEOUT_MIN_MS);
            if (lte_push_at_command("AT", LTE_RX_TIMEOUT_MAX_MS)) {
                break;
            }
        }
        lte_check_attached(lte_legacyattach_flag);
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
        { MP_QSTR_delay,      MP_ARG_KW_ONLY  | MP_ARG_INT, {.u_int = 0} },
    };
    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);
    if (args[0].u_obj == mp_const_none) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "the command must be specified!"));
    }
    const char *cmd = mp_obj_str_get_str(args[0].u_obj);
    lte_push_at_command_delay((char *)cmd, LTE_RX_TIMEOUT_MAX_MS, args[1].u_int);
    vstr_t vstr;
    vstr_init(&vstr, 0);
    vstr_add_str(&vstr, modlte_rsp.data);
    MP_THREAD_GIL_EXIT();
    while(modlte_rsp.data_remaining)
    {
        lte_push_at_command_delay("Pycom_Dummy", LTE_RX_TIMEOUT_MAX_MS, args[1].u_int);
        vstr_add_str(&vstr, modlte_rsp.data);
    }
    MP_THREAD_GIL_ENTER();
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
    char *pos;
    vstr_t vstr;
    vstr_init_len(&vstr, strlen("AT+SQNCCID?"));
    strcpy(vstr.buf, "AT+SQNCCID?");
    lte_send_raw_at(MP_OBJ_NULL, mp_obj_new_str_from_vstr(&mp_type_str, &vstr));
    if ((pos = strstr(modlte_rsp.data, "SQNCCID:")) && (strlen(pos) > 25)) {
        vstr_init_len(&vstr, 20);
        memcpy(vstr.buf, &pos[10], 20);
        return mp_obj_new_str_from_vstr(&mp_type_str, &vstr);
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

STATIC const mp_map_elem_t lte_locals_dict_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR_init),                (mp_obj_t)&lte_init_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_deinit),              (mp_obj_t)&lte_deinit_obj },
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
#ifdef LTE_DEBUG_BUFF
    { MP_OBJ_NEW_QSTR(MP_QSTR_debug_buff),          (mp_obj_t)&lte_debug_buff_obj },
#endif

    // class constants
    { MP_OBJ_NEW_QSTR(MP_QSTR_IP),                   MP_OBJ_NEW_QSTR(MP_QSTR_IP) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_IPV4V6),               MP_OBJ_NEW_QSTR(MP_QSTR_IPV4V6) },
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
};
