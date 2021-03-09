/*
 * Copyright (c) 2021, Pycom Limited.
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

#include "esp_bt.h"
#include "common/bt_trace.h"
#include "stack/bt_types.h"
#include "stack/btm_api.h"
#include "bta/bta_api.h"
#include "bta/bta_gatt_api.h"
#include "api/esp_gap_ble_api.h"
#include "api/esp_gattc_api.h"
#include "api/esp_gatt_defs.h"
#include "api/esp_bt_main.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "freertos/xtensa_api.h"

#include "esp_heap_caps.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_attr.h"
#include "esp_spi_flash.h"
#include "rom/spi_flash.h"
#include "rom/ets_sys.h"
#include "nvs_flash.h"
#include "soc/dport_reg.h"
#include "esp_log.h"

#include "py/mpstate.h"
#include "py/mpconfig.h"
#include "py/runtime.h"
#include "mptask.h"
#include "machpin.h"
#include "pins.h"
#include "mperror.h"
#include "machtimer.h"
#include "esp32chipinfo.h"
#include "esp_event_loop.h"
#include "app_sys_evt.h"


TaskHandle_t mpTaskHandle;
TaskHandle_t svTaskHandle;
TaskHandle_t SmartConfTaskHandle;
TaskHandle_t ethernetTaskHandle;
#if defined(LOPY) || defined (LOPY4) || defined (FIPY)
TaskHandle_t xLoRaTaskHndl;
DRAM_ATTR TaskHandle_t xLoRaTimerTaskHndl;
#endif
#if defined(SIPY) || defined (LOPY4) || defined (FIPY)
TaskHandle_t xSigfoxTaskHndl;
#endif
#if defined(GPY) || defined (FIPY)
TaskHandle_t xLTETaskHndl = NULL;
TaskHandle_t xLTEUartEvtTaskHndl;
TaskHandle_t xLTEUpgradeTaskHndl;
#endif
TaskHandle_t xSocketOpsTaskHndl;

extern void machine_init0(void);

/******************************************************************************
 DECLARE PUBLIC DATA
 ******************************************************************************/
StackType_t *mpTaskStack;

// board configuration options from mpconfigboard.h
uint32_t micropy_hw_flash_size;

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
static system_event_cb_t main_evt_cb_list[APP_SYS_EVT_NUM] = {NULL, NULL};
/******************************************************************************
 DECLARE PRIVATE FUNC
 ******************************************************************************/
static esp_err_t app_sys_event_handler(void *ctx, system_event_t *event);
/******************************************************************************
 DECLARE PUBLIC FUNC
 ******************************************************************************/
static esp_err_t app_sys_event_handler(void *ctx, system_event_t *event)
{
    for(uint8_t i = 0; i < APP_SYS_EVT_NUM; i++)
    {
        if(main_evt_cb_list[i] != NULL)
        {
            main_evt_cb_list[i](ctx, event);
        }
    }
    return ESP_OK;
}
/******************************************************************************
 * FunctionName : app_main
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void app_main(void) {

    esp32_init_chip_info();

    // remove all the logs from the IDF
    esp_log_level_set("*", ESP_LOG_NONE);

    // Register sys event callback
    ESP_ERROR_CHECK(esp_event_loop_init(app_sys_event_handler, NULL));

    // setup the timer used as a reference in mphal
    machtimer_preinit();

    // this one gets the remaining sleep time
    machine_init0();

    // initalize the non-volatile flash space
    esp_err_t ret = nvs_flash_init();
    if ((ret == ESP_ERR_NVS_NO_FREE_PAGES) || (ret == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
        nvs_flash_erase();
        nvs_flash_init();
    }
#ifndef RGB_LED_DISABLE
    // initialise heartbeat on Core 0
    mperror_pre_init();
#endif

    // differentiate the Flash Size (either 8MB or 4MB) based on ESP32 rev id
    micropy_hw_flash_size = (esp32_get_chip_rev() > 0 ? 0x800000 : 0x400000);

    // propagating the Flash Size in the global variable (used in multiple IDF modules)
    g_rom_flashchip.chip_size = micropy_hw_flash_size;

    if (esp32_get_chip_rev() > 0) {
        micropy_hw_antenna_diversity_pin_num = MICROPY_SECOND_GEN_ANT_SELECT_PIN_NUM;

        micropy_lpwan_ncs_pin_index = 1;
        micropy_lpwan_ncs_pin_num = 18;
        micropy_lpwan_ncs_pin = &pin_GPIO18;

        micropy_lpwan_use_reset_pin = false;

        micropy_lpwan_dio_pin_index = 2;
        micropy_lpwan_dio_pin_num = 23;
        micropy_lpwan_dio_pin = &pin_GPIO23;

        mpTaskStack = malloc(MICROPY_TASK_STACK_SIZE_PSRAM);

        // create the MicroPython task
        mpTaskHandle =
        (TaskHandle_t)xTaskCreateStaticPinnedToCore(TASK_Micropython, "MicroPy", (MICROPY_TASK_STACK_SIZE_PSRAM / sizeof(StackType_t)), NULL,
                                                    MICROPY_TASK_PRIORITY, mpTaskStack, &mpTaskTCB, 1);

    } else {
        micropy_hw_antenna_diversity_pin_num = MICROPY_FIRST_GEN_ANT_SELECT_PIN_NUM;

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

        mpTaskStack = malloc(MICROPY_TASK_STACK_SIZE);

        // create the MicroPython task
        mpTaskHandle =
        (TaskHandle_t)xTaskCreateStaticPinnedToCore(TASK_Micropython, "MicroPy", (MICROPY_TASK_STACK_SIZE / sizeof(StackType_t)), NULL,
                                                    MICROPY_TASK_PRIORITY, mpTaskStack, &mpTaskTCB, 1);
    }
}

void app_sys_register_evt_cb(main_app_sys_evt_t sys_evt, system_event_cb_t cb)
{
    if((cb != NULL) && (sys_evt < APP_SYS_EVT_NUM))
    {
        main_evt_cb_list[sys_evt] = cb;
    }
}
