/*
 * Copyright (c) 2017, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#ifndef MODLED_H
#define	MODLED_H

#include "driver/rmt.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
    
/******************************************************************************
 DEFINE PUBLIC CONSTANTS
 ******************************************************************************/
#define COLOR_BITS           (24) // 24 bit color

/******************************************************************************
 DEFINE PUBLIC TYPES
 ******************************************************************************/

typedef union {
    struct component_t {
        uint8_t blue;
        uint8_t green;
        uint8_t red;
    } component;
    uint32_t value : 24;
} color_rgb_t; 

typedef struct {
    rmt_channel_t rmt_channel;
    gpio_num_t gpio;
    color_rgb_t  color;
    rmt_item32_t *rmt_grb_buf;
    rmt_item32_t *rmt_white_buf;
} led_info_t;

/******************************************************************************
 DECLARE PUBLIC FUNCTIONS
 ******************************************************************************/

bool led_send_color(led_info_t *led_info);
bool led_send_reset(led_info_t *led_info);
bool led_set_color(led_info_t *led_info, bool synchronize);
bool led_init(led_info_t *led_info);

#endif	/* MODLED_H */

