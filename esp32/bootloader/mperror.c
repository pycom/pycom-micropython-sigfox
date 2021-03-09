/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */
#ifndef RGB_LED_DISABLE
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "xtensa/xtruntime.h"
#include "mpconfigboard.h"
#include "mperror.h"

#include "esp_heap_caps.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"

#include "gpio.h"

/******************************************************************************
 DEFINE CONSTANTS
 ******************************************************************************/
#define MPERROR_TOOGLE_MS                           (50)
#define MPERROR_SIGNAL_ERROR_MS                     (1200)
#define MPERROR_HEARTBEAT_ON_MS                     (80)
#define MPERROR_HEARTBEAT_OFF_MS                    (3920)

#define MPERROR_HEARTBEAT_PRIORITY                  (5)

/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/

struct mperror_heart_beat {
    uint32_t off_time;
    uint32_t on_time;
    bool beating;
    bool enabled;
    bool do_disable;
} mperror_heart_beat = {.off_time = 0, .on_time = 0, .beating = false, .enabled = false, .do_disable = false};

void TASK_Heartbeat (void *pvParameters);

/******************************************************************************
 DEFINE PUBLIC FUNCTIONS
 ******************************************************************************/

void mperror_init0 (void) {
    gpio_config_t gpioconf = {.pin_bit_mask = 1ull << MICROPY_HW_HB_PIN_NUM,
                              .mode = GPIO_MODE_OUTPUT,
                              .pull_up_en = GPIO_PULLUP_DISABLE,
                              .pull_down_en = GPIO_PULLDOWN_DISABLE,
                              .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&gpioconf);

    mperror_heart_beat.enabled = true;
    //delay introduced to separate last falling edge of signal and next color code
    ets_delay_us(300);
    mperror_heartbeat_switch_off();
}

__attribute__((noreturn)) void mperror_fatal_error (void) {
    // signal the crash with the system led
    mperror_set_rgb_color(MPERROR_FATAL_COLOR);
    for ( ; ; );
}

void mperror_heartbeat_switch_off (void) {
    if (mperror_heart_beat.enabled) {
        mperror_heart_beat.on_time = 0;
        mperror_heart_beat.off_time = 0;
        mperror_set_rgb_color(0);
    }
}

void mperror_enable_heartbeat (bool enable) {
    if (enable) {
        mperror_heart_beat.enabled = true;
        mperror_heart_beat.do_disable = false;
        mperror_heartbeat_switch_off();
    } else {
        mperror_heart_beat.do_disable = true;
        mperror_heart_beat.enabled = false;
    }
}

bool mperror_is_heartbeat_enabled (void) {
    return mperror_heart_beat.enabled;
}

#define BIT_1_HIGH_TIME_NS                  (950)
#define BIT_1_LOW_TIME_NS                   (22)
#define BIT_0_HIGH_TIME_NS                  (40)
#define BIT_0_LOW_TIME_NS                   (500)
#define RESET_TIME_US                       (52)

#define NS_TO_COUNT(ns)                     (ns / 22)

static inline uint32_t get_ccount(void) {
    uint32_t r;
    asm volatile ("rsr %0, ccount" : "=r"(r));
    return r;
}

static void IRAM_ATTR wait_for_count(uint32_t count) {
    uint32_t volatile register cr = get_ccount();
    uint32_t volatile register ct = cr + count;
    if (ct > cr) {
        while (get_ccount() < ct);
    } else {
        while (ct < get_ccount());
    }
}

#define DELAY_NS(ns)                        wait_for_count(NS_TO_COUNT(ns))
#define GP0_PIN_NUMBER                      (0)

void IRAM_ATTR mperror_set_rgb_color (uint32_t rgbcolor) {
    uint32_t volatile register grbcolor =
            ((rgbcolor << 8) & 0x00FF0000) | ((rgbcolor >> 8) & 0x0000FF00) | (rgbcolor & 0x000000FF);

    uint32_t volatile register ilevel = XTOS_DISABLE_ALL_INTERRUPTS;

    for (int volatile register i = 24; i != 0; --i) {
        if (grbcolor & 0x800000) {
            GPIO_REG_WRITE(GPIO_OUT_W1TS_REG, 1 << GP0_PIN_NUMBER);
            DELAY_NS(BIT_1_HIGH_TIME_NS);
            GPIO_REG_WRITE(GPIO_OUT_W1TC_REG, 1 << GP0_PIN_NUMBER);
            DELAY_NS(BIT_1_LOW_TIME_NS);
        } else {
            GPIO_REG_WRITE(GPIO_OUT_W1TS_REG, 1 << GP0_PIN_NUMBER);
//            DELAY_NS(BIT_0_HIGH_TIME_NS);
            GPIO_REG_WRITE(GPIO_OUT_W1TC_REG, 1 << GP0_PIN_NUMBER);
            DELAY_NS(BIT_0_LOW_TIME_NS);
        }
        // put the next bit in place
        grbcolor <<= 1;
    }
    XTOS_RESTORE_INTLEVEL(ilevel);
    ets_delay_us(RESET_TIME_US);
}

#else

__attribute__((noreturn)) void mperror_fatal_error (void) {
    for ( ; ; );
}

#endif //RGB_LED_DISABLE
