/*
 * Copyright (c) 2016, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include <stdint.h>
#include <stdio.h>

#include "py/mpconfig.h"

#include "heap_alloc_caps.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"
#include "esp_event.h"

#include "gpio.h"
#include "antenna.h"


#if MICROPY_HW_ANTENNA_DIVERSITY

/******************************************************************************
DEFINE CONSTANTS
******************************************************************************/

/******************************************************************************
DEFINE PRIVATE DATA
******************************************************************************/
static antenna_type_t antenna_type_selected = ANTENNA_TYPE_INTERNAL;

/******************************************************************************
DEFINE PUBLIC FUNCTIONS
******************************************************************************/
void antenna_init0(void) {
    gpio_config_t gpioconf = {.pin_bit_mask = 1ull << MICROPY_HW_ANTENNA_DIVERSITY_PIN_NUM,
                              .mode = GPIO_MODE_OUTPUT,
                              .pull_up_en = GPIO_PULLUP_DISABLE,
                              .pull_down_en = GPIO_PULLDOWN_DISABLE,
                              .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&gpioconf);

    // select the currently active antenna
    antenna_select(ANTENNA_TYPE_INTERNAL);
}

void antenna_select (antenna_type_t _antenna) {
    if (MICROPY_HW_ANTENNA_DIVERSITY_PIN_NUM < 32) {
        // set the pin value
        if (_antenna == ANTENNA_TYPE_EXTERNAL) {
            GPIO_REG_WRITE(GPIO_OUT_W1TS_REG, 1 << MICROPY_HW_ANTENNA_DIVERSITY_PIN_NUM);
        } else {
            GPIO_REG_WRITE(GPIO_OUT_W1TC_REG, 1 << MICROPY_HW_ANTENNA_DIVERSITY_PIN_NUM);
        }
    } else {
        if (_antenna == ANTENNA_TYPE_EXTERNAL) {
            GPIO_REG_WRITE(GPIO_OUT1_W1TS_REG, 1 << (MICROPY_HW_ANTENNA_DIVERSITY_PIN_NUM & 31));
        } else {
            GPIO_REG_WRITE(GPIO_OUT1_W1TC_REG, 1 << (MICROPY_HW_ANTENNA_DIVERSITY_PIN_NUM & 31));
        }
    }
    antenna_type_selected = _antenna;
}

#endif
