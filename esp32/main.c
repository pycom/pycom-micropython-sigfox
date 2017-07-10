/*
 * Copyright (c) 2016, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "controller.h"

#include "bt.h"
#include "bt_trace.h"
#include "bt_types.h"
#include "btm_api.h"
#include "bta_api.h"
#include "bta_gatt_api.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "freertos/xtensa_api.h"

#include "esp_heap_alloc_caps.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_attr.h"
#include "esp_spi_flash.h"
#include "rom/spi_flash.h"
#include "rom/ets_sys.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "soc/dport_reg.h"
#include "esp_log.h"

#include "py/mpstate.h"
#include "py/runtime.h"
#include "mptask.h"
#include "machpin.h"
#include "pins.h"

/******************************************************************************
 DECLARE PUBLIC DATA
 ******************************************************************************/
StackType_t mpTaskStack[MICROPY_TASK_STACK_LEN] __attribute__((aligned (8)));

// board configuration options from mpconfigboard.h
uint32_t micropy_hw_flash_size;

bool micropy_hw_antenna_diversity;
uint32_t micropy_hw_antenna_diversity_pin_num;

uint32_t micropy_lpwan_reset_pin_num;
uint32_t micropy_lpwan_reset_pin_index;
void * micropy_lpwan_reset_pin;
bool micropy_lpwan_use_reset_pin;

uint32_t micropy_lpwan_dio_pin_num;
uint32_t micropy_lpwan_dio_pin_index;
void * micropy_lpwan_dio_pin;

uint32_t micropy_lpwan_ncs_pin_num;
uint32_t micropy_lpwan_ncs_pin_index;
void * micropy_lpwan_ncs_pin;

/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/
static StaticTask_t mpTaskTCB;

/******************************************************************************
 * FunctionName : app_main
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void app_main(void) {
    // remove all the logs from the IDF
    esp_log_level_set("*", ESP_LOG_NONE);
    // initalize the non-volatile flash space
    nvs_flash_init();

    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    if (chip_info.revision > 0) {
        micropy_hw_antenna_diversity = false;
        micropy_lpwan_use_reset_pin = false;

        micropy_lpwan_ncs_pin_index = 1;
        micropy_lpwan_ncs_pin_num = 18;
        micropy_lpwan_ncs_pin = &pin_GPIO18;

        micropy_lpwan_dio_pin_index = 2;
        micropy_lpwan_dio_pin_num = 23;
        micropy_lpwan_dio_pin = &pin_GPIO23;
    } else {
        micropy_hw_antenna_diversity = true;
        micropy_hw_antenna_diversity_pin_num = 16;

        micropy_lpwan_ncs_pin_index = 0;
        micropy_lpwan_ncs_pin_num = 17;
        micropy_lpwan_ncs_pin = &pin_GPIO17;

        micropy_lpwan_reset_pin_index = 1;
        micropy_lpwan_reset_pin_num = 18;
        micropy_lpwan_reset_pin = &pin_GPIO18;
        micropy_lpwan_use_reset_pin = true;

        micropy_lpwan_dio_pin_index = 2;
        micropy_lpwan_dio_pin_num = 23;
        micropy_lpwan_dio_pin = &pin_GPIO23;
    }

    micropy_hw_flash_size = spi_flash_get_chip_size();

    // create the MicroPython task
    xTaskCreateStaticPinnedToCore(TASK_Micropython, "MicroPy", MICROPY_TASK_STACK_LEN, NULL,
                                  MICROPY_TASK_PRIORITY, mpTaskStack, &mpTaskTCB, 1);
}
