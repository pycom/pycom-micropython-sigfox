/*
 * Copyright (C) 2016, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include <stdint.h>

#include "py/mpconfig.h"
#include "py/obj.h"
#include "py/runtime.h"
#include "py/objlist.h"
#include "py/stream.h"
#include "py/mphal.h"

#include "heap_alloc_caps.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"
#include "esp_event.h"

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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/xtensa_api.h"

/// \module machine - functions related to the SoC
///

/******************************************************************************/
// Micro Python bindings;

STATIC mp_obj_t machine_reset(void) {
    esp_restart();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(machine_reset_obj, machine_reset);

STATIC mp_obj_t machine_freq(void) {
    return mp_obj_new_int(ets_get_cpu_frequency() * 1000000);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(machine_freq_obj, machine_freq);

STATIC mp_obj_t machine_unique_id(void) {
    uint8_t id[6];
    esp_efuse_read_mac(id);
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

STATIC mp_obj_t machine_deepsleep (void) {
    // TODO
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(machine_deepsleep_obj, machine_deepsleep);

STATIC mp_obj_t machine_reset_cause (void) {
    return mp_obj_new_int(mpsleep_get_reset_cause());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(machine_reset_cause_obj, machine_reset_cause);

STATIC mp_obj_t machine_wake_reason (void) {
    // TODO
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(machine_wake_reason_obj, machine_wake_reason);

STATIC mp_obj_t machine_disable_irq(void) {
    return mp_obj_new_int(MICROPY_BEGIN_ATOMIC_SECTION());
}
MP_DEFINE_CONST_FUN_OBJ_0(machine_disable_irq_obj, machine_disable_irq);

STATIC mp_obj_t machine_enable_irq(uint n_args, const mp_obj_t *arg) {
    if (n_args == 0) {
        XTOS_SET_INTLEVEL(0);
    } else {
        MICROPY_END_ATOMIC_SECTION(mp_obj_get_int(arg[0]));
    }
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(machine_enable_irq_obj, 0, 1, machine_enable_irq);

STATIC const mp_rom_map_elem_t machine_module_globals_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR___name__),            MP_OBJ_NEW_QSTR(MP_QSTR_umachine) },

    { MP_ROM_QSTR(MP_QSTR_mem8),                    (mp_obj_t)(&machine_mem8_obj) },
    { MP_ROM_QSTR(MP_QSTR_mem16),                   (mp_obj_t)(&machine_mem16_obj) },
    { MP_ROM_QSTR(MP_QSTR_mem32),                   (mp_obj_t)(&machine_mem32_obj) },

    { MP_OBJ_NEW_QSTR(MP_QSTR_reset),               (mp_obj_t)(&machine_reset_obj) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_freq),                (mp_obj_t)(&machine_freq_obj) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_unique_id),           (mp_obj_t)(&machine_unique_id_obj) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_main),                (mp_obj_t)(&machine_main_obj) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_rng),                 (mp_obj_t)(&machine_rng_get_obj) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_idle),                (mp_obj_t)(&machine_idle_obj) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_sleep),               (mp_obj_t)(&machine_sleep_obj) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_deepsleep),           (mp_obj_t)(&machine_deepsleep_obj) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_reset_cause),         (mp_obj_t)(&machine_reset_cause_obj) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_wake_reason),         (mp_obj_t)(&machine_wake_reason_obj) },

    { MP_OBJ_NEW_QSTR(MP_QSTR_disable_irq),         (mp_obj_t)&machine_disable_irq_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_enable_irq),          (mp_obj_t)&machine_enable_irq_obj },

    { MP_OBJ_NEW_QSTR(MP_QSTR_Pin),                 (mp_obj_t)&pin_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_UART),                (mp_obj_t)&mach_uart_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_SPI),                 (mp_obj_t)&mach_spi_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_I2C),                 (mp_obj_t)&machine_i2c_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_PWM),                 (mp_obj_t)&mach_pwm_timer_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ADC),                 (mp_obj_t)&pyb_adc_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_DAC),                 (mp_obj_t)&pyb_dac_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_SD),                  (mp_obj_t)&pyb_sd_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_Timer),               (mp_obj_t)&mach_timer_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_RTC),                 (mp_obj_t)&mach_rtc_type },

    // constants
    { MP_OBJ_NEW_QSTR(MP_QSTR_PWRON_RESET),         MP_OBJ_NEW_SMALL_INT(MPSLEEP_PWRON_RESET) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_SOFT_RESET),          MP_OBJ_NEW_SMALL_INT(MPSLEEP_SOFT_RESET) },
};

STATIC MP_DEFINE_CONST_DICT(machine_module_globals, machine_module_globals_table);

const mp_obj_module_t machine_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&machine_module_globals,
};
