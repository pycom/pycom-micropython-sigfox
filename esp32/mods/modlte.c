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
#include "pybioctl.h"
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

/******************************************************************************
 DEFINE TYPES
 ******************************************************************************/

/******************************************************************************
 DEFINE CONSTANTS
 ******************************************************************************/

/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/
static lte_obj_t lte_obj = {.init = false};
static lte_task_rsp_data_t modlte_rsp;

/******************************************************************************
 DECLARE PUBLIC DATA
 ******************************************************************************/

/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/
static bool lte_push_at_command_ext (char *cmd_str, uint32_t timeout, const char *expected_rsp);
static bool lte_push_at_command (char *cmd_str, uint32_t timeout);
static void lte_pause_ppp(void);
static bool lte_check_attached(void);
static void lte_check_init(void);
static bool lte_check_sim_present(void);

STATIC mp_obj_t lte_isconnected(mp_obj_t self_in);
STATIC mp_obj_t lte_disconnect(mp_obj_t self_in);

/******************************************************************************
 DEFINE PUBLIC FUNCTIONS
 ******************************************************************************/

void modlte_init0(void) {
    lteppp_init();
}

//*****************************************************************************
// DEFINE STATIC FUNCTIONS
//*****************************************************************************

static bool lte_push_at_command_ext (char *cmd_str, uint32_t timeout, const char *expected_rsp) {
    lte_task_cmd_data_t cmd = {
        .timeout = timeout
    };
    memcpy(cmd.data, cmd_str, strlen(cmd_str));
    // printf("%s\n",  cmd_str);
    lteppp_send_at_command (&cmd, &modlte_rsp);
    if (strstr(modlte_rsp.data, expected_rsp) != NULL) {
        return true;
    }
    return false;
}

static bool lte_push_at_command (char *cmd_str, uint32_t timeout) {
    return lte_push_at_command_ext(cmd_str, timeout, LTE_OK_RSP);
}

static void lte_pause_ppp(void) {
    mp_hal_delay_ms(LTE_PPP_BACK_OFF_TIME_MS);
    if (!lte_push_at_command("+++", LTE_PPP_BACK_OFF_TIME_MS)) {
        mp_hal_delay_ms(LTE_PPP_BACK_OFF_TIME_MS);
        if (!lte_push_at_command("+++", LTE_PPP_BACK_OFF_TIME_MS)) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
        }
    }
}

static bool lte_check_attached(void) {
    char *pos;
    bool inppp = false;;
    bool attached = false;

    if (lte_isconnected(NULL) == mp_const_true) {
        inppp = true;
        lte_pause_ppp();
        while (true) {
            mp_hal_delay_ms(LTE_RX_TIMEOUT_MIN_MS);
            if (lte_push_at_command("AT", LTE_RX_TIMEOUT_MIN_MS)) {
                break;
            }
        }
    }

    lte_push_at_command("AT+CEREG?", LTE_RX_TIMEOUT_MIN_MS);
    if (((pos = strstr(modlte_rsp.data, "+CEREG: 2,1,")) || (pos = strstr(modlte_rsp.data, "+CEREG: 2,5,")))
        && (strlen(pos) >= 31) && pos[30] == '7') {
        if (E_LTE_PPP != lteppp_get_state()) {
            lteppp_set_state(E_LTE_ATTACHED);
        }
        attached = true;
    } else {
        lte_push_at_command("AT+CFUN?", LTE_RX_TIMEOUT_MIN_MS);
        if (E_LTE_ATTACHING == lteppp_get_state()) {
            // for some reason the modem has crashed, enabled the radios again...
            if (!strstr(modlte_rsp.data, "+CFUN: 1")) {
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

    if (inppp) {
        lte_push_at_command("ATO", LTE_RX_TIMEOUT_MIN_MS);
    }

    return attached;
}

static void lte_check_init(void) {
    if (!lte_obj.init) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "LTE modem not initialized"));
    }
}

static bool lte_check_sim_present(void) {
    lte_push_at_command("AT+CPIN?", LTE_RX_TIMEOUT_MAX_MS);
    if (strstr(modlte_rsp.data, "ERROR")) {
        return false;
    } else {
        return true;
    }
}

/******************************************************************************/
// Micro Python bindings; LTE class

static mp_obj_t lte_init_helper(lte_obj_t *self, const mp_arg_val_t *args) {
    char at_cmd[LTE_AT_CMD_SIZE_MAX - 4];

    // wake up the radio
    lteppp_start();
    lte_push_at_command("AT", LTE_RX_TIMEOUT_MAX_MS);
    lte_push_at_command("AT", LTE_RX_TIMEOUT_MAX_MS);

    lte_push_at_command("AT+CFUN?", LTE_RX_TIMEOUT_MIN_MS);
    if (strstr(modlte_rsp.data, "+CFUN: 0")) {
        const char *carrier = "standard";
        if (args[0].u_obj != mp_const_none) {
            carrier = mp_obj_str_get_str(args[0].u_obj);
            if ((!strstr(carrier, "standard")) && (!strstr(carrier, "verizon")) && (!strstr(carrier, "at&t"))) {
                nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "invalid carrier %s", carrier));
            }
        }

        // configure the carrier
        lte_push_at_command("AT+SQNCTM?", LTE_RX_TIMEOUT_MAX_MS);
        if (!strstr(modlte_rsp.data, carrier)) {
            sprintf(at_cmd, "AT+SQNCTM=\"%s\"", carrier);
            lte_push_at_command(at_cmd, LTE_RX_TIMEOUT_MAX_MS);
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
    return mp_const_none;
}

static const mp_arg_t lte_init_args[] = {
    { MP_QSTR_id,                                      MP_ARG_INT, {.u_int = 0} },
    { MP_QSTR_carrier,               MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} }
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

mp_obj_t lte_deinit(mp_obj_t self_in) {
    lte_check_init();
    lte_obj_t *self = self_in;
    if (lte_isconnected(self) == mp_const_true) {
        lte_disconnect(self);
    }

    if (lte_check_sim_present()) {
        if (!lte_push_at_command("AT+CFUN=4", LTE_RX_TIMEOUT_MAX_MS)) {
            goto error;
        }
    } else {
        if (!lte_push_at_command("AT+CFUN=0", LTE_RX_TIMEOUT_MAX_MS)) {
            goto error;
        }
    }

    lte_obj.init = false;
    lteppp_deinit();
    return mp_const_none;

error:
    nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lte_deinit_obj, lte_deinit);

STATIC mp_obj_t lte_attach(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    lte_check_init();

    STATIC const mp_arg_t allowed_args[] = {
        { MP_QSTR_band,      MP_ARG_KW_ONLY  | MP_ARG_OBJ, {.u_obj = mp_const_none} },
    };

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    lte_check_attached();
    if (lteppp_get_state() < E_LTE_ATTACHING) {
        // configuring scanning in all 6 bands
        lte_push_at_command("AT!=\"clearscanconfig\"", LTE_RX_TIMEOUT_MIN_MS);
        if (args[0].u_obj == mp_const_none) {
            lte_push_at_command("AT!=\"RRC::addscanfreq band=3 dl-earfcn=1575\"", LTE_RX_TIMEOUT_MIN_MS);
            lte_push_at_command("AT!=\"RRC::addscanfreq band=4 dl-earfcn=2175\"", LTE_RX_TIMEOUT_MIN_MS);
            lte_push_at_command("AT!=\"RRC::addscanfreq band=12 dl-earfcn=5095\"", LTE_RX_TIMEOUT_MIN_MS);
            lte_push_at_command("AT!=\"RRC::addscanfreq band=13 dl-earfcn=5230\"", LTE_RX_TIMEOUT_MIN_MS);
            lte_push_at_command("AT!=\"RRC::addscanfreq band=20 dl-earfcn=6300\"", LTE_RX_TIMEOUT_MIN_MS);
            lte_push_at_command("AT!=\"RRC::addscanfreq band=28 dl-earfcn=9435\"", LTE_RX_TIMEOUT_MIN_MS);
        } else {
            uint32_t band = mp_obj_get_int(args[0].u_obj);
            if (band == 3) {
                lte_push_at_command("AT!=\"RRC::addscanfreq band=3 dl-earfcn=1575\"", LTE_RX_TIMEOUT_MIN_MS);
            } else if (band == 4) {
                lte_push_at_command("AT!=\"RRC::addscanfreq band=4 dl-earfcn=2175\"", LTE_RX_TIMEOUT_MIN_MS);
            } else if (band == 12) {
                lte_push_at_command("AT!=\"RRC::addscanfreq band=12 dl-earfcn=5095\"", LTE_RX_TIMEOUT_MIN_MS);
            } else if (band == 13) {
                lte_push_at_command("AT!=\"RRC::addscanfreq band=13 dl-earfcn=5230\"", LTE_RX_TIMEOUT_MIN_MS);
            } else if (band == 20) {
                lte_push_at_command("AT!=\"RRC::addscanfreq band=20 dl-earfcn=6300\"", LTE_RX_TIMEOUT_MIN_MS);
            } else if (band == 28) {
                lte_push_at_command("AT!=\"RRC::addscanfreq band=28 dl-earfcn=9435\"", LTE_RX_TIMEOUT_MIN_MS);
            } else {
                nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "band %d not supported", band));
            }
        }
        lteppp_set_state(E_LTE_ATTACHING);
        if (!lte_push_at_command("AT+CFUN=1", LTE_RX_TIMEOUT_MAX_MS)) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
        }
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(lte_attach_obj, 1, lte_attach);

mp_obj_t lte_dettach(mp_obj_t self_in) {
    lte_check_init();
    lte_obj_t *self = self_in;
    if (lte_isconnected(self) == mp_const_true) {
        lte_disconnect(self);
    }
    if (lte_check_sim_present()) {
        if (!lte_push_at_command("AT+CFUN=4", LTE_RX_TIMEOUT_MAX_MS)) {
            goto error;
        }
    } else {
        if (!lte_push_at_command("AT+CFUN=0", LTE_RX_TIMEOUT_MAX_MS)) {
            goto error;
        }
    }
    lteppp_set_state(E_LTE_IDLE);
    return mp_const_none;

error:
    nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lte_dettach_obj, lte_dettach);

STATIC mp_obj_t lte_isattached(mp_obj_t self_in) {
    lte_check_init();
    if (lte_check_attached()) {
        return mp_const_true;
    }
    return mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lte_isattached_obj, lte_isattached);

STATIC mp_obj_t lte_connect(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    lte_check_init();
    STATIC const mp_arg_t allowed_args[] = {
        { MP_QSTR_cid,      MP_ARG_KW_ONLY  | MP_ARG_INT, {.u_int = 1} },
    };

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    lte_check_attached();
    if (lteppp_get_state() == E_LTE_ATTACHED) {
        char at_cmd[LTE_AT_CMD_SIZE_MAX - 4];
        lte_obj.cid = args[0].u_int;
        sprintf(at_cmd, "AT+CGDATA=\"PPP\",%d", lte_obj.cid);
        // try to stablish a new connection first
        if (lte_push_at_command_ext(at_cmd, LTE_RX_TIMEOUT_MAX_MS, LTE_CONNECT_RSP)) {
            if (!lte_push_at_command_ext("AT", LTE_RX_TIMEOUT_MIN_MS, "ERROR")) {
                mp_hal_delay_ms(LTE_PPP_BACK_OFF_TIME_MS);
                goto connect;
            }
        }
        // that failed, so try to resume the connection
        if (lte_push_at_command_ext("ATO", LTE_RX_TIMEOUT_MAX_MS, LTE_CONNECT_RSP)) {
            if (!lte_push_at_command_ext("AT", LTE_RX_TIMEOUT_MIN_MS, "ERROR")) {
                mp_hal_delay_ms(LTE_PPP_BACK_OFF_TIME_MS);
                goto connect;
            }
        } else {
            // try a new connection once again..
            if (lte_push_at_command_ext(at_cmd, LTE_RX_TIMEOUT_MAX_MS, LTE_CONNECT_RSP)) {
                if (!lte_push_at_command_ext("AT", LTE_RX_TIMEOUT_MIN_MS, "ERROR")) {
                    mp_hal_delay_ms(LTE_PPP_BACK_OFF_TIME_MS);
                    goto connect;
                }
            } else {
                nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
            }
        }
    } else {
        if (lte_isconnected(NULL) == mp_const_true) {
            return mp_const_none;
        }
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "modem not attached"));
    }

connect:
    lteppp_connect();
    lteppp_set_state(E_LTE_PPP);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(lte_connect_obj, 1, lte_connect);

STATIC mp_obj_t lte_isconnected(mp_obj_t self_in) {
    lte_check_init();
    if (lteppp_get_state() == E_LTE_PPP && lteppp_ipv4() > 0) {
        return mp_const_true;
    }
    return mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lte_isconnected_obj, lte_isconnected);

STATIC mp_obj_t lte_disconnect(mp_obj_t self_in) {
    lte_check_init();
    lte_obj_t *self = self_in;
    if (lte_isconnected(self) == mp_const_true) {
        lteppp_disconnect();
        lte_pause_ppp();
        lteppp_set_state(E_LTE_ATTACHED);
        lte_check_attached();
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lte_disconnect_obj, lte_disconnect);

STATIC mp_obj_t lte_send_raw_at(mp_obj_t self_in, mp_obj_t cmd_o) {
    lte_check_init();
    lte_obj_t *self = self_in;
    bool inppp = false;
    if (lte_isconnected(self) == mp_const_true) {
        inppp = true;
        lte_pause_ppp();
        while (true) {
            mp_hal_delay_ms(LTE_RX_TIMEOUT_MIN_MS);
            if (lte_push_at_command("AT", LTE_RX_TIMEOUT_MIN_MS)) {
                break;
            }
        }
    }

    const char *cmd = mp_obj_str_get_str(cmd_o);
    lte_push_at_command((char *)cmd, LTE_RX_TIMEOUT_MAX_MS);
    vstr_t vstr;
    vstr_init_len(&vstr, strlen(modlte_rsp.data));
    strcpy(vstr.buf, modlte_rsp.data);
    if (inppp) {
        lte_push_at_command("ATO", LTE_RX_TIMEOUT_MIN_MS);
    }
    return mp_obj_new_str_from_vstr(&mp_type_str, &vstr);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lte_send_raw_at_obj, lte_send_raw_at);

STATIC mp_obj_t lte_imei(mp_obj_t self_in) {
    char *pos;
    vstr_t vstr;
    vstr_init_len(&vstr, strlen("AT+CGSN"));
    strcpy(vstr.buf, "AT+CGSN");
    lte_send_raw_at(MP_OBJ_NULL, mp_obj_new_str_from_vstr(&mp_type_str, &vstr));
    if ((pos = strstr(modlte_rsp.data, "35434")) && (strlen(pos) > 20)) {
        vstr_init_len(&vstr, 15);
        memcpy(vstr.buf, pos, 15);
        return mp_obj_new_str_from_vstr(&mp_type_str, &vstr);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lte_imei_obj, lte_imei);

STATIC mp_obj_t lte_iccid(mp_obj_t self_in) {
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

STATIC mp_obj_t lte_reset(mp_obj_t self_in) {
    lte_check_init();
    lte_obj_t *self = self_in;
    if (lte_isconnected(self) == mp_const_true) {
        lte_pause_ppp();
        while (true) {
            mp_hal_delay_ms(LTE_RX_TIMEOUT_MIN_MS);
            if (lte_push_at_command("AT", LTE_RX_TIMEOUT_MIN_MS)) {
                break;
            }
        }
    }
    lte_push_at_command("AT^RESET", LTE_RX_TIMEOUT_MIN_MS);
    lteppp_set_state(E_LTE_IDLE);
    mp_hal_delay_ms(LTE_RX_TIMEOUT_MIN_MS);
    lteppp_wait_at_rsp("+SYSSTART", LTE_RX_TIMEOUT_MAX_MS, true);
    if (!lte_push_at_command("AT", LTE_RX_TIMEOUT_MIN_MS)) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_operation_failed));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lte_reset_obj, lte_reset);

STATIC const mp_map_elem_t lte_locals_dict_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR_init),                (mp_obj_t)&lte_init_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_deinit),              (mp_obj_t)&lte_deinit_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_attach),              (mp_obj_t)&lte_attach_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_dettach),             (mp_obj_t)&lte_dettach_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_isattached),          (mp_obj_t)&lte_isattached_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_connect),             (mp_obj_t)&lte_connect_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_disconnect),          (mp_obj_t)&lte_disconnect_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_isconnected),         (mp_obj_t)&lte_isconnected_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_imei),                (mp_obj_t)&lte_imei_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_iccid),               (mp_obj_t)&lte_iccid_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_send_at_cmd),         (mp_obj_t)&lte_send_raw_at_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_reset),               (mp_obj_t)&lte_reset_obj },

    // class constants
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
};
