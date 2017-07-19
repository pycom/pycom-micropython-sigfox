/*
 * This file is derived from the MicroPython project, http://micropython.org/
 *
 * Copyright (c) 2016, Pycom Limited and its licensors.
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
#include "esp_event.h"
#include "esp_deep_sleep.h"
#include "soc/timer_group_struct.h"

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
#include "machwdt.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/xtensa_api.h"


/// \module machine - functions related to the SoC
///

/******************************************************************************/
// Micro Python bindings;

STATIC mp_obj_t NORETURN machine_reset(void) {
    machtimer_deinit();
    machine_wdt_start(&TIMERG1, 1);
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

STATIC mp_obj_t machine_idle(void) {
    taskYIELD();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(machine_idle_obj, machine_idle);

STATIC mp_obj_t machine_sleep (void) {
    // TODO
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(machine_sleep_obj, machine_sleep);

STATIC mp_obj_t machine_deepsleep (uint n_args, const mp_obj_t *arg) {
    mperror_enable_heartbeat(false);
    bt_deinit(NULL);
    wlan_deinit(NULL);
    if (n_args == 0) {
        esp_deep_sleep_start();
    } else {
        esp_deep_sleep((uint64_t)mp_obj_get_int(arg[0]) * 1000);
    }
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(machine_deepsleep_obj, 0, 1, machine_deepsleep);

STATIC mp_obj_t machine_pin_deepsleep_wakeup (mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
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

    esp_ext1_wakeup_mode_t mode = args[1].u_int;
    if (mode != ESP_EXT1_WAKEUP_ALL_LOW && mode != ESP_EXT1_WAKEUP_ANY_HIGH) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "invalid wakeup mode %d", mode));
    }

    uint64_t mask = 0;
    for (int i = 0; i < len; i++) {
        mask |= (1ull << pin_find(pins[i])->pin_number);
    }
    if (ESP_OK != esp_deep_sleep_enable_ext1_wakeup(mask, mode)) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "the pin(s) selected do not support deepsleep wakeup"));
    }
    if (args[2].u_bool) {
        esp_deep_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    } else {
        esp_deep_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_AUTO);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(machine_pin_deepsleep_wakeup_obj, 2, machine_pin_deepsleep_wakeup);

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
        uint64_t wake_pins = esp_deep_sleep_get_ext1_wakeup_status();
        printf("wake pins = %x:%x\n", (uint32_t)(wake_pins >> 32), (uint32_t)(wake_pins & 0xFFFFFFFF));
        uint64_t mask = 1;

        for (int i = 0; i < 40; i++) {
            if (mask & wake_pins) {
                mp_obj_list_append(pins, pin_find_pin_by_num(&pin_cpu_pins_locals_dict, i));
            }
            mask <<= 1ull;
            printf("mask = %x:%x\n", (uint32_t)(mask >> 32), (uint32_t)(mask & 0xFFFFFFFF));
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
    { MP_OBJ_NEW_QSTR(MP_QSTR_pin_deepsleep_wakeup),    (mp_obj_t)(&machine_pin_deepsleep_wakeup_obj) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_reset_cause),             (mp_obj_t)(&machine_reset_cause_obj) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_wake_reason),             (mp_obj_t)(&machine_wake_reason_obj) },

    { MP_OBJ_NEW_QSTR(MP_QSTR_disable_irq),             (mp_obj_t)&machine_disable_irq_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_enable_irq),              (mp_obj_t)&machine_enable_irq_obj },

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
};

STATIC MP_DEFINE_CONST_DICT(machine_module_globals, machine_module_globals_table);

const mp_obj_module_t machine_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&machine_module_globals,
};
