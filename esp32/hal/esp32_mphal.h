/*
 * Copyright (c) 2019, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#ifndef _INCLUDED_MPHAL_H_
#define _INCLUDED_MPHAL_H_

#if defined (LOPY) || defined(LOPY4) || defined(FIPY)
void HAL_set_tick_cb (void *cb);
#endif
void mp_hal_init(bool soft_reset);
void mp_hal_feed_watchdog(void);
void mp_hal_delay_us(uint32_t us);
int mp_hal_stdin_rx_chr(void);
void mp_hal_stdout_tx_str(const char *str);
void mp_hal_stdout_tx_strn(const char *str, uint32_t len);
void mp_hal_stdout_tx_strn_cooked(const char *str, uint32_t len);
uint32_t mp_hal_ticks_s(void);
uint32_t mp_hal_ticks_ms(void);
uint32_t mp_hal_ticks_us(void);
uint64_t mp_hal_ticks_ms_non_blocking(void);
uint64_t mp_hal_ticks_us_non_blocking(void);
void mp_hal_delay_ms(uint32_t delay);
void mp_hal_set_interrupt_char(int c);
void mp_hal_set_reset_char(int c);
void mp_hal_reset_safe_and_boot(bool reset);

// https://github.com/micropython/micropython/blob/master/ports/esp32/mphalport.h
#define mp_hal_quiet_timing_enter() MICROPY_BEGIN_ATOMIC_SECTION()
#define mp_hal_quiet_timing_exit(irq_state) MICROPY_END_ATOMIC_SECTION(irq_state)
#define mp_hal_delay_us_fast(us) ets_delay_us(us)

#endif // _INCLUDED_MPHAL_H_
