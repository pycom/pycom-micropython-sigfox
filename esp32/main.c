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

#include "py/mpconfig.h"
#include "mptask.h"

/******************************************************************************
 DECLARE PUBLIC DATA
 ******************************************************************************/
StackType_t mpTaskStack[MICROPY_TASK_STACK_LEN] __attribute__((aligned (4)));

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

    // create the MicroPython task
    xTaskCreateStaticPinnedToCore(TASK_Micropython, "MicroPy", MICROPY_TASK_STACK_LEN, NULL,
                                  MICROPY_TASK_PRIORITY, mpTaskStack, &mpTaskTCB, 0);
}
