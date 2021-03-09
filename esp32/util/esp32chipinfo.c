/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include "esp_system.h"

static esp_chip_info_t chip_info;

void esp32_init_chip_info(void)
{
    // Get chip Info
    esp_chip_info(&chip_info);
}

uint8_t esp32_get_chip_rev(void)
{
    return chip_info.revision;
}


