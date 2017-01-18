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

// esp32_prefix.c becomes the initial portion of the generated pins file.

#include <stdio.h>
#include <stdint.h>

#include "py/mpconfig.h"
#include "py/obj.h"

#include "heap_alloc_caps.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_attr.h"

#include "gpio.h"
#include "machpin.h"


#define PIN(p_pin_name, p_pin_number) \
{ \
    { &pin_type }, \
    .name           = MP_QSTR_ ## p_pin_name, \
    .pin_number     = (p_pin_number), \
    .af_in          = (-1), \
    .af_out         = (-1), \
    .mode           = (GPIO_MODE_INPUT), \
    .pull           = (0), \
    .value          = (0), \
    .irq_trigger    = (0), \
    .hold           = (0), \
}
