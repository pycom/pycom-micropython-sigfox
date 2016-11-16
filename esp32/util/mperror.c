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

#include "gpio.h"
#include "machpin.h"
#include "pins.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

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
#ifndef BOOTLOADER_BUILD
STATIC const mp_obj_base_t pyb_heartbeat_obj = {&pyb_heartbeat_type};
#endif

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
void mperror_pre_init(void) {
    xTaskCreatePinnedToCore(TASK_Heartbeat, "Heart", 2048, NULL, MPERROR_HEARTBEAT_PRIORITY, NULL, 0);
}

void mperror_init0 (void) {
#ifndef BOOTLOADER_BUILD
    // configure the heartbeat led pin
    pin_config(&pin_GPIO0, -1, -1, GPIO_MODE_OUTPUT, MACHPIN_PULL_NONE, 0, 0);
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
        mperror_set_rgb_color(toggle ? MPERROR_HEARTBEAT_COLOR : 0);
        toggle = ~toggle;
        mp_hal_delay_ms(MPERROR_TOOGLE_MS);
    }
}

void mperror_heartbeat_switch_off (void) {
    if (mperror_heart_beat.enabled) {
        mperror_heart_beat.on_time = 0;
        mperror_heart_beat.off_time = 0;
        mperror_set_rgb_color(0);
    }
}

void mperror_heartbeat_signal (void) {
    if (mperror_heart_beat.do_disable) {
        mperror_heart_beat.do_disable = false;
    } else if (mperror_heart_beat.enabled) {
        if (!mperror_heart_beat.beating) {
            if ((mperror_heart_beat.on_time = mp_hal_ticks_ms()) - mperror_heart_beat.off_time > MPERROR_HEARTBEAT_OFF_MS) {
                mperror_set_rgb_color(MPERROR_HEARTBEAT_COLOR);
                mperror_heart_beat.beating = true;
            }
        } else {
            if ((mperror_heart_beat.off_time = mp_hal_ticks_ms()) - mperror_heart_beat.on_time > MPERROR_HEARTBEAT_ON_MS) {
                mperror_set_rgb_color(0);
                mperror_heart_beat.beating = false;
            }
        }
    }
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
    mperror_set_rgb_color(MPERROR_FATAL_COLOR);
    for ( ;; ); //{__WFI();}
}

void __assert_func(const char *file, int line, const char *func, const char *expr) {
    (void) func;
    mp_printf(&mp_plat_print, "Assertion failed: %s, func %s, file %s, line %d\n", expr, func, file, line);
    __fatal_error(NULL);
}

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

#define NS_TO_COUNT(ns)                     (ns / 21)

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

void TASK_Heartbeat (void *pvParameters) {
    mperror_init0();

    while (true) {
        mperror_heartbeat_signal();
        vTaskDelay (10 / portTICK_PERIOD_MS);
    }
}
