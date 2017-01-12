/*
 * Copyright (c) 2016, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "py/mpconfig.h"
#include "py/obj.h"
#include "py/runtime.h"
#include "py/mphal.h"
#include "mperror.h"

#include "heap_alloc_caps.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "modled.h"

#include "gpio.h"
#include "machpin.h"
#include "pins.h"
#include "soc/soc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "esp_freertos_hooks.h"

/******************************************************************************
 DEFINE CONSTANTS
 ******************************************************************************/
#define MPERROR_TOOGLE_MS                           (50)
#define MPERROR_SIGNAL_ERROR_MS                     (1200)
#define MPERROR_HEARTBEAT_ON_MS                     (80)
#define MPERROR_HEARTBEAT_OFF_MS                    (3920)

#define MPERROR_HEARTBEAT_PRIORITY                  (5)

#define MPERROR_HEARTBEAT_LED_GPIO                  (0)                
/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/
#ifndef BOOTLOADER_BUILD
STATIC const mp_obj_base_t pyb_heartbeat_obj = {&pyb_heartbeat_type};

static rmt_item32_t rmt_grb_items[COLOR_BITS];
static rmt_item32_t rmt_white_items[COLOR_BITS];

led_info_t led_info = {
    .rmt_channel = RMT_CHANNEL_1,
    .gpio = MPERROR_HEARTBEAT_LED_GPIO,
    .rmt_grb_buf = rmt_grb_items,
    .rmt_white_buf = rmt_white_items,
    .color = {
        .value = MPERROR_HEARTBEAT_COLOR,
    }
};

#endif

struct mperror_heart_beat {
    uint32_t off_time;
    uint32_t on_time;
    bool beating;
    bool enabled;
    bool do_disable;
} mperror_heart_beat = {.off_time = 0, .on_time = 0, .beating = false, .enabled = false, .do_disable = false};

/******************************************************************************
 DEFINE PUBLIC FUNCTIONS
 ******************************************************************************/
void mperror_pre_init(void) {
    esp_register_freertos_idle_hook(mperror_heartbeat_signal);
}

void mperror_init0 (void) {
#ifndef BOOTLOADER_BUILD
    // configure the heartbeat led pin
    pin_config(&pin_GPIO0, -1, -1, GPIO_MODE_OUTPUT, MACHPIN_PULL_NONE, 0, 0);
 
    led_init(&led_info);
#else
    gpio_config_t gpioconf = {.pin_bit_mask = 1ull << MICROPY_HW_HB_PIN_NUM,
                              .mode = GPIO_MODE_OUTPUT,
                              .pull_up_en = GPIO_PULLUP_DISABLE,
                              .pull_down_en = GPIO_PULLDOWN_DISABLE,
                              .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&gpioconf);
#endif
    mperror_heart_beat.enabled = true;
    mperror_heartbeat_switch_off();
}

void mperror_signal_error (void) {
    uint32_t count = 0;
    bool toggle = true;
    while ((MPERROR_TOOGLE_MS * count++) < MPERROR_SIGNAL_ERROR_MS) {
        // toogle the led
        if (!toggle){
            led_send_color(&led_info);
        } else {
            led_send_reset(&led_info);
        }
        toggle = ~toggle;
        mp_hal_delay_ms(MPERROR_TOOGLE_MS);
    }
}

void mperror_heartbeat_switch_off (void) {
    if (mperror_heart_beat.enabled) {
        mperror_heart_beat.on_time = 0;
        mperror_heart_beat.off_time = 0;
        led_send_reset(&led_info);
    }
}

bool mperror_heartbeat_signal (void) {
    if (mperror_heart_beat.do_disable) {
        mperror_heart_beat.do_disable = false;
    } else if (mperror_heart_beat.enabled) {
        if (!mperror_heart_beat.beating) {
            if ((mperror_heart_beat.on_time = mp_hal_ticks_ms()) - mperror_heart_beat.off_time > MPERROR_HEARTBEAT_OFF_MS) {
                led_send_color(&led_info);
                mperror_heart_beat.beating = true;
            }
        } else {
            if ((mperror_heart_beat.off_time = mp_hal_ticks_ms()) - mperror_heart_beat.on_time > MPERROR_HEARTBEAT_ON_MS) {
                led_send_reset(&led_info);
                mperror_heart_beat.beating = false;
            }
        }
    }
    return true;
}

#ifndef BOOTLOADER_BUILD
void NORETURN __fatal_error(const char *msg) {
#ifdef DEBUG
    if (msg != NULL) {
        // wait for 20ms
        mp_hal_delay_ms(20);
        mp_hal_stdout_tx_str("\r\nFATAL ERROR:");
        mp_hal_stdout_tx_str(msg);
        mp_hal_stdout_tx_str("\r\n");
    }
#endif
    // signal the crash with the system led
    led_info.color.value = MPERROR_FATAL_COLOR;
    led_set_color(&led_info, false);
    for ( ;; ); //{__WFI();}
}

// void __assert_func(const char *file, int line, const char *func, const char *expr) {
//     (void) func;
//     mp_printf(&mp_plat_print, "Assertion failed: %s, func %s, file %s, line %d\n", expr, func, file, line);
//     __fatal_error(NULL);
// }

void nlr_jump_fail(void *val) {
#ifdef DEBUG
    char msg[64];
    snprintf(msg, sizeof(msg), "uncaught exception %p\n", val);
    __fatal_error(msg);
#else
    __fatal_error(NULL);
#endif
}
#endif

void mperror_enable_heartbeat (bool enable) {
    if (enable) {
//    #ifndef BOOTLOADER_BUILD
//        // configure the led again
//        pin_config ((pin_obj_t *)&MICROPY_SYS_LED_GPIO, -1, -1, GPIO_DIR_MODE_OUT, PIN_TYPE_STD, 0, PIN_STRENGTH_6MA);
//    #endif
        led_info.color.value = MPERROR_HEARTBEAT_COLOR;
        led_set_color(&led_info, false);
        
        mperror_heart_beat.enabled = true;
        mperror_heart_beat.do_disable = false;
        mperror_heartbeat_switch_off();
    } else {
        led_send_reset(&led_info);
        mperror_heart_beat.do_disable = true;
        mperror_heart_beat.enabled = false;
    }
}

bool mperror_is_heartbeat_enabled (void) {
    return mperror_heart_beat.enabled;
}