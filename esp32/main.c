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

#include "heap_alloc_caps.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_attr.h"
#include "esp_spi_flash.h"
#include "rom/spi_flash.h"
#include "rom/ets_sys.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "soc/dport_reg.h"

#include "mptask.h"

/******************************************************************************
 * FunctionName : app_main
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void app_main(void) {
    // create the MicroPython task
    xTaskCreatePinnedToCore(TASK_Micropython, "MicroPy", MICROPY_TASK_STACK_LEN, NULL, MICROPY_TASK_PRIORITY, NULL, 0);
}

int ets_printf_dummy(const char *fmt, ...) {
    return 0;
}


// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include "driver/gpio.h"
// #include "sdmmc_cmd.h"
// #include "sdmmc_req.h"
// #include "esp_log.h"
// #include "esp_heap_alloc_caps.h"

// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "freertos/semphr.h"
// #include "freertos/queue.h"
// #include "freertos/timers.h"
// #include "freertos/xtensa_api.h"


// static void print_card_info(const sdmmc_card_info_t* card) {
//     printf("Card name: %s\n", card->cid.name);
//     printf("Card CSD: ver=%d, flags=%x, sector_size=%d, capacity=%d read_bl_len=%d\n",
//             card->csd.csd_ver, card->ocr & SD_OCR_SDHC_CAP,
//             card->csd.sector_size, card->csd.capacity, card->csd.read_block_len);
//     printf("SCR: sd_spec=%d, bus_width=%d\n", card->scr.sd_spec, card->scr.bus_width);
// }

// void test_sd(void)
// {
//     sdmmc_req_init();
//     sdmmc_init_config_t config = {
//             .flags = SDMMC_FLAG_1BIT,
//             .slot = SDMMC_SLOT_1,
//             .max_freq_khz = 20000,
//             .io_voltage = 3.3f
//     };
//     sdmmc_card_info_t* card = malloc(sizeof(sdmmc_card_info_t));
//     sdmmc_card_init(&config, card);
//     print_card_info(card);
//     sdmmc_req_deinit();
//     free(card);
// }

// void app_main(void) {
//     vTaskDelay(1000);
//     // create the MicroPython task
//     test_sd();
// }