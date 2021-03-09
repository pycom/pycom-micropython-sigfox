/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include <stdint.h>
#include <stdio.h>

#include "py/mpconfig.h"
#include "py/obj.h"
#include "py/runtime.h"

#include "esp_heap_caps.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"

#include "gpio.h"
#include "antenna.h"


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
    // the second gen modules have a pull-down to select the internal antenna by default
    if (micropy_hw_antenna_diversity_pin_num == MICROPY_FIRST_GEN_ANT_SELECT_PIN_NUM) {
        gpio_config_t gpioconf = {.pin_bit_mask = 1ull << micropy_hw_antenna_diversity_pin_num,
                                  .mode = GPIO_MODE_OUTPUT,
                                  .pull_up_en = GPIO_PULLUP_DISABLE,
                                  .pull_down_en = GPIO_PULLDOWN_DISABLE,
                                  .intr_type = GPIO_INTR_DISABLE};
        gpio_config(&gpioconf);

        // select the currently active antenna
        antenna_select(ANTENNA_TYPE_INTERNAL);
    }
}

void antenna_select (antenna_type_t _antenna) {
    static bool init_done = false;

    if (micropy_hw_antenna_diversity_pin_num < 32) {
        // set the pin value
        if (_antenna == ANTENNA_TYPE_EXTERNAL) {
            // config the pin for first time if is the second generation as it was already ulled down by default
            if((micropy_hw_antenna_diversity_pin_num == MICROPY_SECOND_GEN_ANT_SELECT_PIN_NUM) && (!init_done))
            {
                gpio_config_t gpioconf = {.pin_bit_mask = 1ull << MICROPY_SECOND_GEN_ANT_SELECT_PIN_NUM,
                                          .mode = GPIO_MODE_OUTPUT,
                                          .pull_up_en = GPIO_PULLUP_DISABLE,
                                          .pull_down_en = GPIO_PULLDOWN_DISABLE,
                                          .intr_type = GPIO_INTR_DISABLE};
                if(ESP_OK == gpio_config(&gpioconf))
                {
                    init_done = true;
                }
            }

            GPIO_REG_WRITE(GPIO_OUT_W1TS_REG, 1 << micropy_hw_antenna_diversity_pin_num);
        } else if (_antenna == ANTENNA_TYPE_INTERNAL) {
            GPIO_REG_WRITE(GPIO_OUT_W1TC_REG, 1 << micropy_hw_antenna_diversity_pin_num);
        }
    } else {
        if (_antenna == ANTENNA_TYPE_EXTERNAL) {
            // config the pin for first time if is the second generation as it was already ulled down by default
            if((micropy_hw_antenna_diversity_pin_num == MICROPY_SECOND_GEN_ANT_SELECT_PIN_NUM) && (!init_done))
            {
                gpio_config_t gpioconf = {.pin_bit_mask = 1ull << (MICROPY_SECOND_GEN_ANT_SELECT_PIN_NUM & 31),
                                          .mode = GPIO_MODE_OUTPUT,
                                          .pull_up_en = GPIO_PULLUP_DISABLE,
                                          .pull_down_en = GPIO_PULLDOWN_DISABLE,
                                          .intr_type = GPIO_INTR_DISABLE};
                if(ESP_OK == gpio_config(&gpioconf))
                {
                    init_done = true;
                }
            }
            GPIO_REG_WRITE(GPIO_OUT1_W1TS_REG, 1 << (micropy_hw_antenna_diversity_pin_num & 31));
        } else if (_antenna == ANTENNA_TYPE_INTERNAL) {
            GPIO_REG_WRITE(GPIO_OUT1_W1TC_REG, 1 << (micropy_hw_antenna_diversity_pin_num & 31));
        }
    }
    antenna_type_selected = _antenna;
}

void antenna_validate_antenna (uint8_t antenna) {
    if (antenna > ANTENNA_TYPE_MANUAL) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "invalid antenna type %d", antenna));
    }
}
