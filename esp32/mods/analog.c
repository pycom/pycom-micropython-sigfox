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
#include "analog.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/portmacro.h"

//Internal functions that are not thread-safe, you should not call these functions directly without lock.
extern void touch_init(touch_sensor_pad pad);
extern void touch_read(uint16_t *pad_out, uint16_t sample_num);
extern uint8_t temprature_sens_read(void);
extern uint16_t hall_sens_read(void);
extern uint16_t adc1_read(uint8_t pad, uint8_t atten);
extern void dac_out(uint8_t dac_en, uint8_t tone_en, uint16_t dc_value, uint16_t tone_scale, uint16_t tone_step);

//Mutex to protect each API
static portMUX_TYPE analog_mux = portMUX_INITIALIZER_UNLOCKED;

//TODO: TO USE esp_log.h
#define ANALOG_DBG_ERROR_ENABLE (0)
#if SENSOR_DBG_ERROR_ENABLE
#define ANALOG_ERROR(format,...) do{\
        ets_printf("[error][%s#%u]",__FUNCTION__,__LINE__);\
        ets_printf(format,##__VA_ARGS__);\
}while(0)
#else
#define ANALOG_ERROR(...)
#endif

esp_err_t analog_touch_init(touch_sensor_pad pad)
{
    if(pad >= TOUCH_SENSOR_MAX) {
        ANALOG_ERROR("SENSOR PAD ERROR\n");
        return ESP_ERR_INVALID_ARG;
    }
    portENTER_CRITICAL(&analog_mux);
    touch_init(pad);
    portEXIT_CRITICAL(&analog_mux);
    return ESP_OK;
}

void analog_touch_read(uint16_t *pad_out, uint16_t sample_num)
{
    portENTER_CRITICAL(&analog_mux);
    touch_read(pad_out, sample_num);
    portEXIT_CRITICAL(&analog_mux);
}

uint8_t analog_temperature_read(void)
{
    portENTER_CRITICAL(&analog_mux);
    uint8_t temp_val = temprature_sens_read();
    portEXIT_CRITICAL(&analog_mux);
    return temp_val;
}

uint16_t analog_hall_read(void)
{
    portENTER_CRITICAL(&analog_mux);
    uint16_t hall_val = hall_sens_read();
    portEXIT_CRITICAL(&analog_mux);
    return hall_val;
}

int analog_adc1_read(adc_channel_t channel, adc_atten_t atten)
{
    if(channel >= ADC1_CH_MAX) {
        ANALOG_ERROR("ADC CHANNEL ERROR\n");
        return -1;
    }
    if(atten >= ADC_ATTEN_MAX) {
        ANALOG_ERROR("ADC ATTENUATION ERROR\n");
        return -1;
    }
    portENTER_CRITICAL(&analog_mux);
    uint16_t adc_value = adc1_read(channel, atten);
    portEXIT_CRITICAL(&analog_mux);
    return adc_value;
}

esp_err_t analog_dac_out(dac_enable_t dac_en, dac_tone_t tone_en, uint16_t dc_value, uint16_t tone_scale, uint16_t tone_step)
{
    if(dac_en > DAC1_EN_DAC2_EN) {
        ANALOG_ERROR("ADC CHANNEL ERROR\n");
        return ESP_ERR_INVALID_ARG;
    }
    if(tone_en > DAC1_TONE_DAC2_TONE) {
        ANALOG_ERROR("ADC ATTENUATION ERROR\n");
        return ESP_ERR_INVALID_ARG;
    }
    portENTER_CRITICAL(&analog_mux);
    dac_out(dac_en, tone_en, dc_value, tone_scale, tone_step);
    portEXIT_CRITICAL(&analog_mux);
    return ESP_OK;
}

