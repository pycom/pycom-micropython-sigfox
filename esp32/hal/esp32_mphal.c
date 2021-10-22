/*
 * Copyright (c) 2021, Pycom Limited.
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

#include "esp_heap_caps.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"
#include "esp_attr.h"

#include "machuart.h"
#include "telnet.h"
#include "esp32_mphal.h"
#include "serverstask.h"
#include "moduos.h"
#include "mpexception.h"
#include "modmachine.h"
#include "updater.h"
#include "bootloader.h"
#include "modwlan.h"
#include "modbt.h"
#include "machtimer.h"
#include "mpirq.h"

#include "driver/timer.h"

typedef void (*HAL_tick_user_cb_t)(void);
#if defined (LOPY) || defined(LOPY4) || defined(FIPY)
DRAM_ATTR static HAL_tick_user_cb_t HAL_tick_user_cb;

#define TIMER1_ALARM_TIME_MS        1U
#define TIMER1_DIVIDER              16U
#define TIMER1_ALARM_COUNT          ((uint64_t)TIMER_BASE_CLK * (uint64_t)TIMER1_ALARM_TIME_MS) / ((uint64_t)TIMER1_DIVIDER * (uint64_t)1000)

#endif


#if defined (LOPY) || defined(LOPY4) || defined(FIPY)
IRAM_ATTR static void HAL_TimerCallback (void* arg) {

    if (HAL_tick_user_cb != NULL) {

        HAL_tick_user_cb();
    }

    TIMERG0.int_clr_timers.t1 = 1;
    TIMERG0.hw_timer[1].update=1;
    TIMERG0.hw_timer[1].config.alarm_en = 1;

}

void HAL_set_tick_cb (void *cb) {
    HAL_tick_user_cb = (HAL_tick_user_cb_t)cb;
}
#endif

void mp_hal_init(bool soft_reset) {
    if (!soft_reset) {
    #if defined (LOPY) || defined(LOPY4) || defined(FIPY)
        // setup the HAL timer for LoRa
        HAL_tick_user_cb = NULL;

        timer_config_t config;

        config.alarm_en = 1;
        config.auto_reload = 1;
        config.counter_dir = TIMER_COUNT_UP;
        config.divider = TIMER1_DIVIDER;
        config.intr_type = TIMER_INTR_LEVEL;
        config.counter_en = TIMER_PAUSE;
        /*Configure timer*/
        timer_init(TIMER_GROUP_0, TIMER_1, &config);
        /*Stop timer counter*/
        timer_pause(TIMER_GROUP_0, TIMER_1);
        /*Load counter value */
        timer_set_counter_value(TIMER_GROUP_0, TIMER_1, 0x00000000ULL);
        /*Set alarm value*/
        timer_set_alarm_value(TIMER_GROUP_0, TIMER_1, (uint64_t)TIMER1_ALARM_COUNT);
        /*Enable timer interrupt*/
        timer_enable_intr(TIMER_GROUP_0, TIMER_1);
        /* Register Interrupt */
        timer_isr_register(TIMER_GROUP_0, TIMER_1, HAL_TimerCallback, NULL, ESP_INTR_FLAG_IRAM, NULL);
        /* Start Timer */
        timer_start(TIMER_GROUP_0, TIMER_1);

    #endif
    }
}

void mp_hal_feed_watchdog(void) {

}

void mp_hal_delay_us(uint32_t us) {
    if (us <= 1000) {
        if (us > 0) {
            ets_delay_us(us);
        }
    } else {
        uint32_t ms = us / 1000;
        us = us % 1000;
        MP_THREAD_GIL_EXIT();
        vTaskDelay (ms / portTICK_PERIOD_MS);
        MP_THREAD_GIL_ENTER();
        if (us > 0) {
            ets_delay_us(us);
        }
    }
}

int mp_hal_stdin_rx_chr(void) {
    for ( ; ; ) {
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

uint32_t mp_hal_ticks_s(void) {
    struct timeval now;
    gettimeofday(&now, NULL);
    return now.tv_sec;
}

IRAM_ATTR uint32_t mp_hal_ticks_ms(void) {
    struct timeval now;
    gettimeofday(&now, NULL);
    return now.tv_sec * 1000 + now.tv_usec / 1000;
}

IRAM_ATTR uint32_t mp_hal_ticks_us(void) {
    struct timeval now;
    gettimeofday(&now, NULL);
    return now.tv_sec * 1000000 + now.tv_usec;
}

IRAM_ATTR uint64_t mp_hal_ticks_ms_non_blocking(void) {
    return esp_timer_get_time() / 1000;
}

IRAM_ATTR uint64_t mp_hal_ticks_us_non_blocking(void) {
    return esp_timer_get_time();
}

void mp_hal_delay_ms(uint32_t delay) {
    MP_THREAD_GIL_EXIT();
    vTaskDelay (delay / portTICK_PERIOD_MS);
    MP_THREAD_GIL_ENTER();
}

void mp_hal_reset_safe_and_boot(bool reset) {
    boot_info_t boot_info;
    uint32_t boot_info_offset;
    /* Disable Wifi/BT to avoid cache region being accessed since it will be disabled when updating Safe boot flag in flash */
    machtimer_deinit();
#if MICROPY_PY_THREAD
    mp_irq_kill();
    mp_thread_deinit();
#endif
    wlan_deinit(NULL);
    modbt_deinit(false);
    if (updater_read_boot_info (&boot_info, &boot_info_offset)) {
        boot_info.safeboot = SAFE_BOOT_SW;
        updater_write_boot_info (&boot_info, boot_info_offset);
    }
    if (reset) {
        machine_reset();
    }
}
