/*
 * Copyright (c) 2017, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include <stdbool.h>
#include "modled.h"

/******************************************************************************
 DEFINE PRIVATE CONSTANTS
 ******************************************************************************/
#define LED_RMT_CLK_DIV      (8) // The RMT clock is 80 MHz
#define LED_RMT_DUTY_CYLE    (50)
#define LED_RMT_CARRIER_FREQ (100)
#define LED_RMT_MEM_BLCK     (1)


#define LED_BIT_1_HIGH_PERIOD (9) // 900ns 
#define LED_BIT_1_LOW_PERIOD  (3) // 300ns 
#define LED_BIT_0_HIGH_PERIOD (3) // 300ns 
#define LED_BIT_0_LOW_PERIOD  (9) // 900ns 

/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/

static bool led_init_rmt(led_info_t *led_info);
static void led_encode_color(led_info_t *led_info);
static void led_encode_white(led_info_t *led_info);
static void set_high_bit(rmt_item32_t *item);
static void set_low_bit(rmt_item32_t *item);

/******************************************************************************
 DEFINE PUBLIC FUNCTIONS
 ******************************************************************************/
bool led_init(led_info_t *led_info)
{
    if ((led_info == NULL) ||
        (led_info->rmt_grb_buf == NULL) ||
        (led_info->rmt_white_buf == NULL) ||
        (led_info->rmt_channel == RMT_CHANNEL_MAX) ||
        (led_info->gpio > GPIO_NUM_33)) {
        return false;
    }
    
    led_encode_color(led_info);
    led_encode_white(led_info);
    
    if (!led_init_rmt(led_info)) {
        return false;
    }
    
    return true;
}

bool led_send_color(led_info_t *led_info) 
{
    if ((led_info == NULL) || 
        (led_info->rmt_grb_buf == NULL)){
        return false;
    }
   
    if (rmt_write_items(led_info->rmt_channel, led_info->rmt_grb_buf, COLOR_BITS, false) != ESP_OK){
        return false;
    }
    
    return true;
}

bool led_send_reset(led_info_t *led_info)
{
     if ((led_info == NULL) || 
        (led_info->rmt_white_buf == NULL)){
        return false;
    }
    
    if (rmt_write_items(led_info->rmt_channel, led_info->rmt_white_buf, COLOR_BITS, false) != ESP_OK) {
        return false;
    }
    
    return true;
}

bool led_set_color(led_info_t *led_info, bool synchronize)
{
    if ((led_info == NULL) || 
        (led_info->rmt_grb_buf == NULL)){
        return false;
    }
    
    if (synchronize){
        rmt_wait_tx_done(led_info->rmt_channel);
    }
    
    led_encode_color(led_info);
    if (rmt_write_items(led_info->rmt_channel, led_info->rmt_grb_buf, COLOR_BITS, false) != ESP_OK) {
        return false;
    }
    
    return true;
}

/******************************************************************************
 DEFINE PRIVATE FUNCTIONS
 ******************************************************************************/

static void set_high_bit(rmt_item32_t *item){
    item->duration0 = LED_BIT_1_HIGH_PERIOD;
    item->level0    = 1;
    item->duration1 = LED_BIT_1_LOW_PERIOD;
    item->level1    = 0;
}

static void set_low_bit(rmt_item32_t *item){
    item->duration0 = LED_BIT_0_HIGH_PERIOD;
    item->level0    = 1;
    item->duration1 = LED_BIT_0_LOW_PERIOD;
    item->level1    = 0;
}

static void led_encode_color(led_info_t *led_info)
{   
    uint32_t rmt_idx = 0;
    uint32_t grb_value = ((uint32_t)led_info->color.component.green << 16) | 
                ((uint32_t)led_info->color.component.red << 8) | 
                ((uint32_t)led_info->color.component.blue);
            
    uint32_t MSB_VALUE = 1 << (COLOR_BITS - 1);
    
    for (uint32_t bit_mask = MSB_VALUE; bit_mask != 0; bit_mask >>= 1){
        
        if (grb_value & bit_mask){
            set_high_bit(&(led_info->rmt_grb_buf[rmt_idx]));
        } else {
            set_low_bit(&(led_info->rmt_grb_buf[rmt_idx]));
        }
           
        rmt_idx++;
    }
}

static void led_encode_white(led_info_t *led_info){
    
    for (uint32_t bit = 0; bit < COLOR_BITS; bit++){
        set_low_bit(&(led_info->rmt_white_buf[bit]));
    }
    
}
   
static bool led_init_rmt(led_info_t *led_info)
{
    rmt_config_t rmt_info = {
        .rmt_mode = RMT_MODE_TX,
        .channel = led_info->rmt_channel,
        .clk_div = LED_RMT_CLK_DIV,
        .gpio_num = led_info->gpio,
        .mem_block_num = LED_RMT_MEM_BLCK,
        .tx_config = {
            .loop_en = false,
            .carrier_freq_hz = LED_RMT_CARRIER_FREQ, 
            .carrier_duty_percent = LED_RMT_DUTY_CYLE,
            .carrier_level = RMT_CARRIER_LEVEL_LOW,
            .carrier_en = false,
            .idle_level = RMT_IDLE_LEVEL_LOW,
            .idle_output_en = true,
        }
    };

    if (rmt_config(&rmt_info) != ESP_OK) {
        return false;
    }
    
    if (rmt_driver_install(rmt_info.channel, 0, 0) != ESP_OK) {
        return false;
    }

    return true;
}
