/*
 * Copyright (c) 2016, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "py/mpstate.h"
#include "py/mphal.h"
#include "py/runtime.h"
#include "py/objstr.h"
#include "py/mpstate.h"

#include "heap_alloc_caps.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_attr.h"

#include "machuart.h"
#include "telnet.h"
#include "esp32_mphal.h"
#include "serverstask.h"
#include "moduos.h"
#include "mpexception.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "freertos/xtensa_api.h"


#ifdef LOPY
static void (*HAL_tick_user_cb)(void);
#endif

#define TIMER_TICKS             160000        // 1 ms @160MHz

#ifdef LOPY
static IRAM_ATTR void HAL_TimerCallback (TimerHandle_t xTimer) {
    if (HAL_tick_user_cb) {
        HAL_tick_user_cb();
    }
    // xthal_set_ccompare(1, xthal_get_ccount() + TIMER_TICKS);
    // if (HAL_tick_user_cb) {
    //     HAL_tick_user_cb();
    // }
}

void HAL_set_tick_cb (void *cb) {
    HAL_tick_user_cb = cb;
}
#endif

void mp_hal_init(bool soft_reset) {
    if (!soft_reset) {
    #ifdef LOPY
        // setup the HAL timer for LoRa
        HAL_tick_user_cb = NULL;
        // xt_set_interrupt_handler(XCHAL_TIMER_INTERRUPT(1), HAL_TimerCallback, NULL);
        // xt_ints_on(1 << XCHAL_TIMER_INTERRUPT(1));
        // xthal_set_ccompare(1, xthal_get_ccount() + TIMER_TICKS);
        TimerHandle_t hal_timer =
                xTimerCreate("HAL_Timer", 1 / portTICK_PERIOD_MS, pdTRUE, (void *) 0, HAL_TimerCallback);
        xTimerStart (hal_timer, 0);
    #endif
    }
}

void mp_hal_feed_watchdog(void) {

}

void mp_hal_delay_us(uint32_t us) {
    if (us < 1000) {
        ets_delay_us(us);
    } else {
        MP_THREAD_GIL_EXIT();
        ets_delay_us(us);
        MP_THREAD_GIL_ENTER();
    }
}

int mp_hal_stdin_rx_chr(void) {
    for ( ;; ) {
        // read telnet first
        if (telnet_rx_any()) {
            return telnet_rx_char();
        } else if (MP_STATE_PORT(mp_os_stream_o) != MP_OBJ_NULL) { // then the stdio_dup
            if (MP_OBJ_IS_TYPE(MP_STATE_PORT(mp_os_stream_o), &mach_uart_type)) {
                if (uart_rx_any(MP_STATE_PORT(mp_os_stream_o))) {
                    return uart_rx_char(MP_STATE_PORT(mp_os_stream_o));
                }
            } else {
                MP_STATE_PORT(mp_os_read)[2] = mp_obj_new_int(1);
                mp_obj_t data = mp_call_method_n_kw(1, 0, MP_STATE_PORT(mp_os_read));
                // data len is > 0
                if (mp_obj_is_true(data)) {
                    mp_buffer_info_t bufinfo;
                    mp_get_buffer_raise(data, &bufinfo, MP_BUFFER_READ);
                    return ((int *)(bufinfo.buf))[0];
                }
            }
        }
        mp_hal_delay_ms(1);
    }
    return -1;
}

void mp_hal_stdout_tx_str(const char *str) {
    mp_hal_stdout_tx_strn(str, strlen(str));
}

void mp_hal_stdout_tx_strn(const char *str, uint32_t len) {
    if (MP_STATE_PORT(mp_os_stream_o) != MP_OBJ_NULL) {
        if (MP_OBJ_IS_TYPE(MP_STATE_PORT(mp_os_stream_o), &mach_uart_type)) {
            uart_tx_strn(MP_STATE_PORT(mp_os_stream_o), str, len);
        } else {
            MP_STATE_PORT(mp_os_write)[2] = mp_obj_new_str_of_type(&mp_type_str, (const byte *)str, len);
            mp_call_method_n_kw(1, 0, MP_STATE_PORT(mp_os_write));
        }
    }
    // and also to telnet
    telnet_tx_strn(str, len);
}

void mp_hal_stdout_tx_strn_cooked(const char *str, uint32_t len) {
    int32_t nslen = 0;
    const char *_str = str;

    for (int i = 0; i < len; i++) {
        if (str[i] == '\n') {
            mp_hal_stdout_tx_strn(_str, nslen);
            mp_hal_stdout_tx_strn("\r\n", 2);
            _str += nslen + 1;
            nslen = 0;
        } else {
            nslen++;
        }
    }
    if (_str < str + len) {
        mp_hal_stdout_tx_strn(_str, nslen);
    }
}

uint32_t mp_hal_ticks_ms(void) {
    struct timeval now;
    gettimeofday(&now, NULL);
    return now.tv_sec * 1000 + now.tv_usec / 1000;
}

uint32_t mp_hal_ticks_us(void) {
    struct timeval now;
    gettimeofday(&now, NULL);
    return now.tv_sec * 1000000 + now.tv_usec;
}

void mp_hal_delay_ms(uint32_t delay) {
    MP_THREAD_GIL_EXIT();
    vTaskDelay (delay / portTICK_PERIOD_MS);
    MP_THREAD_GIL_ENTER();
}
