// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifndef _DRIVER_ANALOG_H_
#define _DRIVER_ANALOG_H_
#include "esp_types.h"
#include "esp_err.h"
#include "soc/soc.h"

#ifdef __cplusplus
extern "C" {
#endif

// ************
// TOUCH
// ************
typedef enum {
    TOUCH_SENSOR_PAD0 = BIT0,
    TOUCH_SENSOR_PAD1 = BIT1,
    TOUCH_SENSOR_PAD2 = BIT2,
    TOUCH_SENSOR_PAD3 = BIT3,
    TOUCH_SENSOR_PAD4 = BIT4,
    TOUCH_SENSOR_PAD5 = BIT5,
    TOUCH_SENSOR_PAD6 = BIT6,
    TOUCH_SENSOR_PAD7 = BIT7,
    TOUCH_SENSOR_PAD8 = BIT8,
    TOUCH_SENSOR_PAD9 = BIT9,
    TOUCH_SENSOR_MAX,
} touch_sensor_pad;

typedef enum {
    DAC1_DIS_DAC2_DIS = 0,     /*DAC1 disable, DAC2 disable*/
    DAC1_EN_DAC2_DIS,          /*DAC1 enable,  DAC2 disable*/
    DAC1_DIS_DAC2_EN,          /*DAC1 disable, DAC2 enable*/
    DAC1_EN_DAC2_EN,           /*DAC1 enable,  DAC2 enable*/
} dac_enable_t;

typedef enum {
    DAC1_DC_DAC2_DC = 0,      /*DAC1 is DC,   DAC2 is DC*/
    DAC1_TONE_DAC2_DC,        /*DAC1 is TONE, DAC2 is DC*/
    DAC1_DC_DAC2_TONE,        /*DAC1 is DC,   DAC2 is TONE*/
    DAC1_TONE_DAC2_TONE,      /*DAC1 is TONE, DAC2 is TONE*/
} dac_tone_t;

/**
  * @brief  Read value from temperature sensor.
  *
  * @param  null
  *
  * @return uint8_t : range [0, 255]
  */
uint8_t analog_temperature_read(void);

/**
  * @brief  Get hall sensor value
  *
  * @return uint16_t : range[0, 65535]
  */
uint16_t analog_hall_read(void);

// ************
// DAC
// ************
/**
  * @brief  Two DAC can work at the same time.
  *         8bit DAC. range is 0 ~ 3.3V
  *
  * @param  uint8_t dac_en      : enable DAC, range[0,3]
  *                               0 : DAC1 disable, DAC2 disable
  *                               1 : DAC1 enable,  DAC2 disable
  *                               2 : DAC1 disable, DAC2 enable
  *                               3 : DAC1 enable,  DAC2 enable
  *
  *
  * @param  uint8_t tone_en     : tone DC select, range[0,3]
  *                               0 : DAC1 is DC,   DAC2 is DC
  *                               1 : DAC1 is tone, DAC2 is DC
  *                               2 : DAC1 is DC,   DAC2 is tone
  *                               3 : DAC1 is tone, DAC2 is tone
  *
  * @param  uint16_t dc_value   : Add a DC value to output.
  *                               dc_value[7:0]  is DAC1 DC value, range[0, 255], voltage is 0 ~ 3.3V
  *                                     dc_value[15:0] is DAC2 DC value, range[0, 255], voltage is 0 ~ 3.3V
  *
  * @param  uint16_t tone_scale : tone_scale[7:0]  is DAC1,
  *                               tone_scale[15:8] is DAC2
  *                               0~3, 0 is atten 0dB, 1 is atten 6dB, 2 is atten 12dB, 3 is atten 18dB
  *
  * @param  uint8_t tone_step   : DAC1 and DAC2 use the same step value, tone_freq = 8M/(2^16/(tone_step+1)
  *                             : tone_step range[0~65534]
  *
  * @return ESP_OK              : success
  *         ESP_ERR_INVALID_ARG : parameter error
  */
esp_err_t analog_dac_out(dac_enable_t dac_en, dac_tone_t tone_en, uint16_t dc_value, uint16_t tone_scale, uint16_t tone_step);


#ifdef __cplusplus
}
#endif

#endif /*_DRIVER_ANALOG_H_*/
