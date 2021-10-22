/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "mpconfigboard.h"

#include "esp_heap_caps.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"

#include "gpio.h"
#include "mperror.h"
#include "bootloader.h"

//*****************************************************************************
// Local Constants
//*****************************************************************************
#define BOOTMGR_WAIT_SAFE_MODE_0_MS         100

#define BOOTMGR_WAIT_SAFE_MODE_1_MS         3000
#define BOOTMGR_WAIT_SAFE_MODE_1_BLINK_MS   500

#define BOOTMGR_WAIT_SAFE_MODE_2_MS         3000
#define BOOTMGR_WAIT_SAFE_MODE_2_BLINK_MS   250

#define BOOTMGR_WAIT_SAFE_MODE_3_MS         1500
#define BOOTMGR_WAIT_SAFE_MODE_3_BLINK_MS   100

#define BOOTMGR_SAFEBOOT_COLOR              (0x2C1200)

//*****************************************************************************
// Local functions declarations
//*****************************************************************************
static bool wait_while_blinking (uint32_t wait_time, uint32_t period, bool force_wait);
static bool safe_boot_request_start (uint32_t wait_time);

//*****************************************************************************
// Private data
//*****************************************************************************

static void delay_ms (uint32_t delay) {
    if (delay < 100) {
        ets_delay_us(delay * 1000);
    } else {
        uint32_t c_delay = 0;
        while (c_delay < delay) {
            ets_delay_us(50 * 1000);
            c_delay += 50;
        }
    }
}

//*****************************************************************************
//! Wait while the safe mode pin is being held high and blink the system led
//! with the specified period
//*****************************************************************************
static bool wait_while_blinking (uint32_t wait_time, uint32_t period, bool force_wait) {
    uint32_t count;
#ifndef RGB_LED_DISABLE
    static bool toggle = true;
#endif
    for (count = 0; (force_wait || gpio_get_level(MICROPY_HW_SAFE_PIN_NUM)) &&
         ((period * count) < wait_time); count++) {
#ifndef RGB_LED_DISABLE
        // toggle the led
        if (toggle) {
            mperror_set_rgb_color(BOOTMGR_SAFEBOOT_COLOR);
        } else {
            mperror_set_rgb_color(0);
        }
        toggle = !toggle;
#endif
        delay_ms(period);
    }
    return gpio_get_level(MICROPY_HW_SAFE_PIN_NUM) ? true : false;
}

static bool safe_boot_request_start (uint32_t wait_time) {
    if (gpio_get_level(MICROPY_HW_SAFE_PIN_NUM)) {
        delay_ms(wait_time);
    }
    return gpio_get_level(MICROPY_HW_SAFE_PIN_NUM) ? true : false;
}

//*****************************************************************************
//! Check for the safe mode pin
//*****************************************************************************
uint32_t wait_for_safe_boot (const boot_info_t *boot_info, uint32_t *ActiveImg) {
    uint32_t ret = 0;

    // configure the safeboot pin
    gpio_config_t gpioconf = {.pin_bit_mask = 1ull << MICROPY_HW_SAFE_PIN_NUM,
                              .mode = GPIO_MODE_INPUT,
                              .pull_up_en = GPIO_PULLUP_DISABLE,
                              .pull_down_en = GPIO_PULLDOWN_ENABLE,
                              .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&gpioconf);

    if (safe_boot_request_start(BOOTMGR_WAIT_SAFE_MODE_0_MS)) {
        if (wait_while_blinking(BOOTMGR_WAIT_SAFE_MODE_1_MS, BOOTMGR_WAIT_SAFE_MODE_1_BLINK_MS, false)) {
            // go back one step in time
            *ActiveImg = boot_info->PrevImg;
            if (wait_while_blinking(BOOTMGR_WAIT_SAFE_MODE_2_MS, BOOTMGR_WAIT_SAFE_MODE_2_BLINK_MS, false)) {
                // go back directly to the factory image
                *ActiveImg = IMG_ACT_FACTORY;
                wait_while_blinking(BOOTMGR_WAIT_SAFE_MODE_3_MS, BOOTMGR_WAIT_SAFE_MODE_3_BLINK_MS, true);
            }
        }
#ifndef RGB_LED_DISABLE
        // turn off the heartbeat led
        mperror_set_rgb_color(0);
#endif
        // request a HW safe boot
        ret = SAFE_BOOT_HW;
    }
    // deinit the safe boot pin
    gpioconf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&gpioconf);
    return ret;
}
