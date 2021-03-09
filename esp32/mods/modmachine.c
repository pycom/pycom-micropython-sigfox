/*
 * This file is derived from the MicroPython project, http://micropython.org/
 *
 * Copyright (c) 2021, Pycom Limited and its licensors.
 *
 * This software is licensed under the GNU GPL version 3 or any later version,
 * with permitted additional terms. For more information see the Pycom Licence
 * v1.0 document supplied with this file, or available at:
 * https://www.pycom.io/opensource/licensing
 */

/*
 * This file is part of the Micro Python project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013, 2014 Damien P. George
 * Copyright (c) 2015 Daniel Campora
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdint.h>

#include "py/mpconfig.h"
#include "py/obj.h"
#include "py/runtime.h"
#include "py/objlist.h"
#include "py/stream.h"
#include "py/mphal.h"

#include "esp_heap_caps.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"
#include "esp_sleep.h"
#include "soc/timer_group_struct.h"
#include "esp_flash_encrypt.h"
#include "esp_secure_boot.h"

#include "random.h"
#include "extmod/machine_mem.h"
#include "machpin.h"
#include "machuart.h"
#include "machtimer.h"
#include "machine_i2c.h"
#include "machspi.h"
#include "machpwm.h"
#include "machrtc.h"
#include "mperror.h"
#include "mpsleep.h"
#include "pybadc.h"
#include "pybdac.h"
#include "pybsd.h"
#include "modbt.h"
#include "modwlan.h"
#include "modlora.h"
#include "machwdt.h"
#include "machcan.h"
#include "machrmt.h"
#include "machtouch.h"
#include "pycom_config.h"
#include "modmachine.h"
#if defined (GPY) || defined (FIPY)
#include "lteppp.h"
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "freertos/xtensa_api.h"

#include "rom/ets_sys.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"

#ifdef PYGATE_ENABLED
#include "cmd_manager.h"
#include "../pygate/concentrator/loragw_hal_esp.h"
#include "lora_pkt_fwd.h"
#include "mpirq.h"

#include <stdio.h>
#include <stdlib.h>

#define PYGATE_STOP_EVENT           (0x00001)
#define PYGATE_START_EVENT          (0x00002)
#define PYGATE_ERROR_EVENT          (0x00004)
#endif

static RTC_DATA_ATTR int64_t mach_expected_wakeup_time;
static int64_t mach_remaining_sleep_time;

#ifdef PYGATE_ENABLED
typedef struct _machine_obj_t {
    mp_obj_base_t           base;
    uint32_t                trigger;
    int32_t                 events;
    mp_obj_t                handler;
    mp_obj_t                handler_arg;
} machine_obj_t;
#endif

// Function name is not a typo - undocumented ESP-IDF to get die temperature
uint8_t temprature_sens_read(); 

#ifdef PYGATE_ENABLED
static _sig_func_cb_ptr pygate_signal = NULL;
static machine_pygate_states_t pygate_status;

STATIC void machine_callback_handler(void* arg);

static machine_obj_t machine_obj = {
        .trigger = 0,
        .handler = NULL,
        .handler_arg = NULL,
        .events = 0
};
#endif

void machine_init0(void) {
    if (mach_expected_wakeup_time > 0) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        mach_remaining_sleep_time = (mach_expected_wakeup_time - (int64_t)((tv.tv_sec * 1000000ull) + tv.tv_usec)) / 1000;
        if (mach_remaining_sleep_time < 0) {
            mach_remaining_sleep_time = 0;
        }
    } else {
        mach_remaining_sleep_time = 0;
    }
#ifdef PYGATE_ENABLED
    machine_obj.base.type = (mp_obj_t)&machine_module;
    pygate_status = PYGATE_STOPPED;
#endif
}

#ifdef PYGATE_ENABLED
void machine_register_pygate_sig_handler(_sig_func_cb_ptr sig_handler)
{
    pygate_signal = sig_handler;
}

void machine_pygate_set_status(machine_pygate_states_t status)
{
    bool trig = false;
    pygate_status = status;
    switch (pygate_status)
    {
    case PYGATE_STOPPED:
        if(machine_obj.trigger & PYGATE_STOP_EVENT)
        {
            trig = true;
            machine_obj.events |= PYGATE_STOP_EVENT;
        }
        break;
    case PYGATE_STARTED:
        if(machine_obj.trigger & PYGATE_START_EVENT)
        {
            trig = true;
            machine_obj.events |= PYGATE_START_EVENT;
        }
        break;
    case PYGATE_ERROR:
        if(machine_obj.trigger & PYGATE_ERROR_EVENT)
        {
            trig = true;
            machine_obj.events |= PYGATE_ERROR_EVENT;
        }
        break;
    default:
        break;
    }
    if(trig)
    {
        mp_irq_queue_interrupt(machine_callback_handler, &machine_obj);
    }
}
#endif
/// \module machine - functions related to the SoC
///

/******************************************************************************/
// Micro Python bindings;

extern TaskHandle_t mpTaskHandle;
extern TaskHandle_t svTaskHandle;
extern TaskHandle_t xLoRaTaskHndl;
extern TaskHandle_t xSigfoxTaskHndl;

STATIC mp_obj_t machine_info(void) {
    // FreeRTOS info
    mp_printf(&mp_plat_print, "---------------------------------------------\n");
    mp_printf(&mp_plat_print, "System memory info (in bytes)\n");
    mp_printf(&mp_plat_print, "---------------------------------------------\n");
    mp_printf(&mp_plat_print, "MPTask stack water mark: %d\n", (unsigned int)uxTaskGetStackHighWaterMark((TaskHandle_t)mpTaskHandle));
    mp_printf(&mp_plat_print, "ServersTask stack water mark: %d\n", (unsigned int)uxTaskGetStackHighWaterMark((TaskHandle_t)svTaskHandle));
#if defined (LOPY) || defined (LOPY4) || defined (FIPY)
    mp_printf(&mp_plat_print, "LoRaTask stack water mark: %d\n", (unsigned int)uxTaskGetStackHighWaterMark((TaskHandle_t)xLoRaTaskHndl));
#endif
#if defined (SIPY) || defined (LOPY4) || defined (FIPY)
    mp_printf(&mp_plat_print, "SigfoxTask stack water mark: %d\n", (unsigned int)uxTaskGetStackHighWaterMark((TaskHandle_t)xSigfoxTaskHndl));
#endif
    mp_printf(&mp_plat_print, "TimerTask stack water mark: %d\n", (unsigned int)uxTaskGetStackHighWaterMark(xTimerGetTimerDaemonTaskHandle()));
    mp_printf(&mp_plat_print, "IdleTask stack water mark: %d\n", (unsigned int)uxTaskGetStackHighWaterMark(xTaskGetIdleTaskHandle()));
    mp_printf(&mp_plat_print, "System free heap: %d\n", (unsigned int)esp_get_free_heap_size());
    mp_printf(&mp_plat_print, "---------------------------------------------\n");

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(machine_info_obj, machine_info);

mp_obj_t NORETURN machine_reset(void) {
    machtimer_deinit();
    machine_wdt_start(1);
    for ( ; ; );
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(machine_reset_obj, machine_reset);

STATIC mp_obj_t machine_freq(void) {
    return mp_obj_new_int(ets_get_cpu_frequency() * 1000000);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(machine_freq_obj, machine_freq);

STATIC mp_obj_t machine_unique_id(void) {
    uint8_t id[6];
    esp_efuse_mac_get_default(id);
    return mp_obj_new_bytes((byte *)&id, sizeof(id));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(machine_unique_id_obj, machine_unique_id);

STATIC mp_obj_t machine_main(mp_obj_t main) {
    if (MP_OBJ_IS_STR(main)) {
        MP_STATE_PORT(machine_config_main) = main;
    } else {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_TypeError, "expecting a string file name"));
    }
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(machine_main_obj, machine_main);

#ifdef PYGATE_ENABLED
STATIC mp_obj_t machine_pygate_init (mp_obj_t global_conf) {
    if (global_conf != mp_const_none) {
        esp_lgw_connect();

        mp_buffer_info_t bufinfo;
        mp_get_buffer_raise(global_conf, &bufinfo, MP_BUFFER_READ);

        //printf("Info: %d\n", bufinfo.len);

        lora_gw_init((char *)bufinfo.buf);
    }
    else
    {
        esp_lgw_connect();
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(machine_pygate_init_obj, machine_pygate_init);

STATIC mp_obj_t machine_pygate_deinit (void) {
    if(pygate_signal != NULL)
    {
        pygate_signal(3);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(machine_pygate_deinit_obj, machine_pygate_deinit);


STATIC mp_obj_t machine_pygate_reset (void) {
    pygate_reset();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(machine_pygate_reset_obj, machine_pygate_reset);

STATIC mp_obj_t machine_pygate_debug_level (mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    STATIC const mp_arg_t allowed_args[] = {
        { MP_QSTR_level,  MP_ARG_INT, },
    };

    // parse arguments
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(args), allowed_args, args);

    if (n_args > 0) {
        // set
        lora_gw_set_debug_level(args[0].u_int);
        return mp_const_none;
    } else {
        // get
        return mp_obj_new_int(lora_gw_get_debug_level());
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(machine_pygate_debug_level_obj, 0, machine_pygate_debug_level);

STATIC mp_obj_t machine_pygate_cmd_decode (mp_obj_t cmd_in) {
    // get the cmd data
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(cmd_in, &bufinfo, MP_BUFFER_READ);

    // pass the data to the cmd manager
    cmd_manager_DecodeCmd(bufinfo.buf);

    // return the number of bytes written
    return mp_obj_new_int(bufinfo.len);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(machine_pygate_cmd_decode_obj, machine_pygate_cmd_decode);

STATIC mp_obj_t machine_pygate_cmd_get (void) {
    vstr_t vstr;
    uint32_t len = 0;
    uint8_t *BufToHost;

    // get the command from the cmd manager
    cmd_manager_GetCmdToHost (&BufToHost);
    len = (uint16_t)((BufToHost[CMD_LENGTH_MSB] << 8) + BufToHost[CMD_LENGTH_LSB] + CMD_HEADER_TX_SIZE);
    vstr_init_len(&vstr, len);
    memcpy(vstr.buf, BufToHost, len);

    // return the received data
    return mp_obj_new_str_from_vstr(&mp_type_bytes, &vstr);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(machine_pygate_cmd_get_obj, machine_pygate_cmd_get);

STATIC mp_obj_t machine_callback(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    STATIC const mp_arg_t allowed_args[] = {
        { MP_QSTR_trigger,      MP_ARG_REQUIRED | MP_ARG_OBJ,   },
        { MP_QSTR_handler,      MP_ARG_OBJ,                     {.u_obj = mp_const_none} },
        { MP_QSTR_arg,          MP_ARG_OBJ,                     {.u_obj = mp_const_none} },
    };

    // parse arguments
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(args), allowed_args, args);

    // enable the callback
    if (args[0].u_obj != mp_const_none && args[1].u_obj != mp_const_none) {
        machine_obj.trigger = mp_obj_get_int(args[0].u_obj);
        machine_obj.handler = args[1].u_obj;
        if (args[2].u_obj == mp_const_none) {
            machine_obj.handler_arg = &machine_obj;
        } else {
            machine_obj.handler_arg = args[2].u_obj;
        }
    } else {  // disable the callback
        machine_obj.trigger = 0;
        mp_irq_remove((mp_obj_t)(&machine_obj));
        INTERRUPT_OBJ_CLEAN(&machine_obj);
    }

    mp_irq_add((mp_obj_t)(&machine_obj), args[1].u_obj);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(machine_callback_obj, 0, machine_callback);

STATIC void machine_callback_handler(void* arg)
{
    machine_obj_t *self = arg;

    if (self->handler && self->handler != mp_const_none) {

        mp_call_function_1(self->handler, self->handler_arg);
    }
}

STATIC mp_obj_t machine_events(void) {

    int32_t events = machine_obj.events;
    machine_obj.events = 0;
    return mp_obj_new_int(events);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(machine_events_obj, machine_events);
#endif

STATIC mp_obj_t machine_idle(void) {
    taskYIELD();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(machine_idle_obj, machine_idle);

STATIC mp_obj_t machine_sleep (uint n_args, const mp_obj_t *arg) {

    bool reconnect = false;

#if defined(FIPY) || defined(GPY)
    if (lteppp_get_modem_conn_state() < E_LTE_MODEM_DISCONNECTED) {
        lteppp_deinit();
    }
#endif

#ifdef MOD_LORA_ENABLED
    /* Send LoRa module to Sleep Mode */
    modlora_sleep_module();
    while(!modlora_is_module_sleep())
    {
        mp_hal_delay_ms(2);
    }
#endif

    if (n_args == 0)
    {
        mach_expected_wakeup_time = 0;
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
    }
    else
    {
        int64_t sleep_time = (int64_t)mp_obj_get_int_truncated(arg[0]) * 1000;
        struct timeval tv;
        gettimeofday(&tv, NULL);
        mach_expected_wakeup_time = (int64_t)((tv.tv_sec * 1000000ull) + tv.tv_usec) + sleep_time;
        esp_sleep_enable_timer_wakeup(sleep_time);
        if(n_args == 2)
        {
            reconnect = (bool)mp_obj_is_true(arg[1]);
        }
        else
        {
            reconnect = false;
        }
    }

    modbt_deinit(reconnect);
    // TRUE means wlan_deinit is called from machine_sleep
    wlan_deinit(mp_const_true);

    if(ESP_OK != esp_light_sleep_start())
    {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "Wifi or BT not stopped before sleep"));
    }

    /* resume wlan */
    wlan_resume(reconnect);
    /* resume bt */
    bt_resume(reconnect);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(machine_sleep_obj,0, 2, machine_sleep);

STATIC mp_obj_t machine_deepsleep (uint n_args, const mp_obj_t *arg) {
#ifndef RGB_LED_DISABLE
    mperror_enable_heartbeat(false);
#endif
    modbt_deinit(false);
    wlan_deinit(NULL);
#if defined(FIPY) || defined(GPY)
    if (lteppp_get_modem_conn_state() < E_LTE_MODEM_DISCONNECTED) {
        lteppp_deinit();
    }
#endif
    if (n_args == 0) {
        mach_expected_wakeup_time = 0;
        esp_deep_sleep_start();
    } else {
        int64_t sleep_time = (int64_t)mp_obj_get_int_truncated(arg[0]) * 1000;
        struct timeval tv;
        gettimeofday(&tv, NULL);
        mach_expected_wakeup_time = (int64_t)((tv.tv_sec * 1000000ull) + tv.tv_usec) + sleep_time;
        esp_deep_sleep(sleep_time);
    }
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(machine_deepsleep_obj, 0, 1, machine_deepsleep);

STATIC mp_obj_t machine_remaining_sleep_time (void) {
    return mp_obj_new_int_from_uint(mach_remaining_sleep_time);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(machine_remaining_sleep_time_obj, machine_remaining_sleep_time);

STATIC mp_obj_t machine_pin_sleep_wakeup (mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_pins,             MP_ARG_REQUIRED | MP_ARG_OBJ, },
        { MP_QSTR_mode,             MP_ARG_REQUIRED | MP_ARG_INT, },
        { MP_QSTR_enable_pull,                        MP_ARG_BOOL, {.u_bool = false} },
    };

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(args), allowed_args, args);

    uint32_t len;
    mp_obj_t *pins;
    mp_obj_get_array(args[0].u_obj, &len, &pins);

    esp_sleep_ext1_wakeup_mode_t mode = args[1].u_int;
    if (mode != ESP_EXT1_WAKEUP_ALL_LOW && mode != ESP_EXT1_WAKEUP_ANY_HIGH) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "invalid wakeup mode %d", mode));
    }

    uint64_t mask = 0;
    for (int i = 0; i < len; i++) {
        mask |= (1ull << pin_find(pins[i])->pin_number);
    }
    if (ESP_OK != esp_sleep_enable_ext1_wakeup(mask, mode)) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "deepsleep wake up is not supported on the selected pin(s)"));
    }
    if (args[2].u_bool) {
        esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    } else {
        esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_AUTO);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(machine_pin_sleep_wakeup_obj, 0, machine_pin_sleep_wakeup);

STATIC mp_obj_t machine_reset_cause (void) {
    return mp_obj_new_int(mpsleep_get_reset_cause());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(machine_reset_cause_obj, machine_reset_cause);

STATIC mp_obj_t machine_wake_reason (void) {
    mpsleep_wake_reason_t wake_reason = mpsleep_get_wake_reason();
    mp_obj_t tuple[2];
    mp_obj_t pins = mp_const_none;

    tuple[0] = mp_obj_new_int(wake_reason);
    if (wake_reason == MPSLEEP_GPIO_WAKE) {
        pins = mp_obj_new_list(0, NULL);
        uint64_t wake_pins = esp_sleep_get_ext1_wakeup_status();
        uint64_t mask = 1;

        for (int i = 0; i < GPIO_PIN_COUNT; i++) {
            if (mask & wake_pins) {
                mp_obj_list_append(pins, pin_find_pin_by_num(&pin_cpu_pins_locals_dict, i));
            }
            mask <<= 1ull;
        }
    }
    tuple[1] = pins;
    return mp_obj_new_tuple(2, tuple);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(machine_wake_reason_obj, machine_wake_reason);

STATIC mp_obj_t machine_disable_irq (void) {
    return mp_obj_new_int(MICROPY_BEGIN_ATOMIC_SECTION());
}
MP_DEFINE_CONST_FUN_OBJ_0(machine_disable_irq_obj, machine_disable_irq);

STATIC mp_obj_t machine_enable_irq (uint n_args, const mp_obj_t *arg) {
    if (n_args == 0) {
        XTOS_SET_INTLEVEL(0);
    } else {
        MICROPY_END_ATOMIC_SECTION(mp_obj_get_int(arg[0]));
    }
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(machine_enable_irq_obj, 0, 1, machine_enable_irq);


/*
 Implement ESP32 core temperature read
 Uses largely undocumented method from https://github.com/espressif/esp-idf/blob/master/components/esp32/test/test_tsens.c
 This is a bad HACK until temperature is officially supported by ESP-IDF
*/
STATIC mp_obj_t machine_temperature (void) {
    SET_PERI_REG_BITS(SENS_SAR_MEAS_WAIT2_REG, SENS_FORCE_XPD_SAR, 3, SENS_FORCE_XPD_SAR_S);
    SET_PERI_REG_BITS(SENS_SAR_TSENS_CTRL_REG, SENS_TSENS_CLK_DIV, 10, SENS_TSENS_CLK_DIV_S);
    CLEAR_PERI_REG_MASK(SENS_SAR_TSENS_CTRL_REG, SENS_TSENS_POWER_UP);
    CLEAR_PERI_REG_MASK(SENS_SAR_TSENS_CTRL_REG, SENS_TSENS_DUMP_OUT);
    SET_PERI_REG_MASK(SENS_SAR_TSENS_CTRL_REG, SENS_TSENS_POWER_UP_FORCE);
    SET_PERI_REG_MASK(SENS_SAR_TSENS_CTRL_REG, SENS_TSENS_POWER_UP);
    ets_delay_us(100);
    SET_PERI_REG_MASK(SENS_SAR_TSENS_CTRL_REG, SENS_TSENS_DUMP_OUT);
    ets_delay_us(5);
    int res = GET_PERI_REG_BITS2(SENS_SAR_SLAVE_ADDR3_REG, SENS_TSENS_OUT, SENS_TSENS_OUT_S);

    return mp_obj_new_int(res);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(machine_temperature_obj, machine_temperature);

STATIC mp_obj_t flash_encrypt (void) {
    return mp_obj_new_int(esp_flash_encryption_enabled());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(machine_flash_encrypt_obj, flash_encrypt);

STATIC mp_obj_t secure_boot (void) {
    return mp_obj_new_int(esp_secure_boot_enabled());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(machine_secure_boot_obj, secure_boot);

STATIC const mp_rom_map_elem_t machine_module_globals_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR___name__),                MP_OBJ_NEW_QSTR(MP_QSTR_umachine) },

    { MP_ROM_QSTR(MP_QSTR_mem8),                        (mp_obj_t)(&machine_mem8_obj) },
    { MP_ROM_QSTR(MP_QSTR_mem16),                       (mp_obj_t)(&machine_mem16_obj) },
    { MP_ROM_QSTR(MP_QSTR_mem32),                       (mp_obj_t)(&machine_mem32_obj) },

    { MP_OBJ_NEW_QSTR(MP_QSTR_reset),                   (mp_obj_t)(&machine_reset_obj) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_freq),                    (mp_obj_t)(&machine_freq_obj) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_unique_id),               (mp_obj_t)(&machine_unique_id_obj) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_main),                    (mp_obj_t)(&machine_main_obj) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_rng),                     (mp_obj_t)(&machine_rng_get_obj) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_idle),                    (mp_obj_t)(&machine_idle_obj) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_sleep),                   (mp_obj_t)(&machine_sleep_obj) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_deepsleep),               (mp_obj_t)(&machine_deepsleep_obj) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_remaining_sleep_time),    (mp_obj_t)(&machine_remaining_sleep_time_obj) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_pin_sleep_wakeup),        (mp_obj_t)(&machine_pin_sleep_wakeup_obj) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_reset_cause),             (mp_obj_t)(&machine_reset_cause_obj) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_wake_reason),             (mp_obj_t)(&machine_wake_reason_obj) },

    { MP_OBJ_NEW_QSTR(MP_QSTR_disable_irq),             (mp_obj_t)&machine_disable_irq_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_enable_irq),              (mp_obj_t)&machine_enable_irq_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_info),                    (mp_obj_t)&machine_info_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_temperature),             (mp_obj_t)&machine_temperature_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_flash_encrypt),           (mp_obj_t)&machine_flash_encrypt_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_secure_boot),             (mp_obj_t)&machine_secure_boot_obj },

#ifdef PYGATE_ENABLED
    { MP_OBJ_NEW_QSTR(MP_QSTR_pygate_init),             (mp_obj_t)&machine_pygate_init_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_pygate_deinit),           (mp_obj_t)&machine_pygate_deinit_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_pygate_reset),            (mp_obj_t)&machine_pygate_reset_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_pygate_debug_level),      (mp_obj_t)&machine_pygate_debug_level_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_pygate_cmd_decode),       (mp_obj_t)&machine_pygate_cmd_decode_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_pygate_cmd_get),          (mp_obj_t)&machine_pygate_cmd_get_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_callback),                (mp_obj_t)&machine_callback_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_events),                  (mp_obj_t)&machine_events_obj },
#endif

    { MP_OBJ_NEW_QSTR(MP_QSTR_Pin),                     (mp_obj_t)&pin_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_UART),                    (mp_obj_t)&mach_uart_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_SPI),                     (mp_obj_t)&mach_spi_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_I2C),                     (mp_obj_t)&machine_i2c_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PWM),                     (mp_obj_t)&mach_pwm_timer_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADC),                     (mp_obj_t)&pyb_adc_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_DAC),                     (mp_obj_t)&pyb_dac_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_SD),                      (mp_obj_t)&pyb_sd_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_Timer),                   (mp_obj_t)&mach_timer_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_RTC),                     (mp_obj_t)&mach_rtc_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_WDT),                     (mp_obj_t)&mach_wdt_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_CAN),                     (mp_obj_t)&mach_can_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_RMT),                     (mp_obj_t)&mach_rmt_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_Touch),                   (mp_obj_t)&machine_touchpad_type },


    // constants
    { MP_OBJ_NEW_QSTR(MP_QSTR_PWRON_RESET),         MP_OBJ_NEW_SMALL_INT(MPSLEEP_PWRON_RESET) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_HARD_RESET),          MP_OBJ_NEW_SMALL_INT(MPSLEEP_HARD_RESET) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_WDT_RESET),           MP_OBJ_NEW_SMALL_INT(MPSLEEP_WDT_RESET) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_DEEPSLEEP_RESET),     MP_OBJ_NEW_SMALL_INT(MPSLEEP_DEEPSLEEP_RESET) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_SOFT_RESET),          MP_OBJ_NEW_SMALL_INT(MPSLEEP_SOFT_RESET) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_BROWN_OUT_RESET),     MP_OBJ_NEW_SMALL_INT(MPSLEEP_BROWN_OUT_RESET) },

    { MP_OBJ_NEW_QSTR(MP_QSTR_PWRON_WAKE),          MP_OBJ_NEW_SMALL_INT(MPSLEEP_PWRON_WAKE) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PIN_WAKE),            MP_OBJ_NEW_SMALL_INT(MPSLEEP_GPIO_WAKE) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_RTC_WAKE),            MP_OBJ_NEW_SMALL_INT(MPSLEEP_RTC_WAKE) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ULP_WAKE),            MP_OBJ_NEW_SMALL_INT(MPSLEEP_ULP_WAKE) },

    { MP_OBJ_NEW_QSTR(MP_QSTR_WAKEUP_ALL_LOW),      MP_OBJ_NEW_SMALL_INT(ESP_EXT1_WAKEUP_ALL_LOW) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_WAKEUP_ANY_HIGH),     MP_OBJ_NEW_SMALL_INT(ESP_EXT1_WAKEUP_ANY_HIGH) },

#ifdef PYGATE_ENABLED
    { MP_OBJ_NEW_QSTR(MP_QSTR_PYGATE_START_EVT),    MP_OBJ_NEW_SMALL_INT(PYGATE_START_EVENT) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PYGATE_STOP_EVT),     MP_OBJ_NEW_SMALL_INT(PYGATE_STOP_EVENT) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PYGATE_ERROR_EVT),    MP_OBJ_NEW_SMALL_INT(PYGATE_ERROR_EVENT) },
#endif
};

STATIC MP_DEFINE_CONST_DICT(machine_module_globals, machine_module_globals_table);

const mp_obj_module_t machine_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&machine_module_globals,
};
