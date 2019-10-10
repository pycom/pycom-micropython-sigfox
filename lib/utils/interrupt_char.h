/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013-2016 Damien P. George
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef MICROPY_INCLUDED_LIB_UTILS_INTERRUPT_CHAR_H
#define MICROPY_INCLUDED_LIB_UTILS_INTERRUPT_CHAR_H
typedef void (*_sig_func_cb_ptr)(int);
extern int mp_interrupt_char;
extern int mp_reset_char;
void mp_hal_set_interrupt_char(int c);
void mp_hal_set_reset_char(int c);
void mp_keyboard_interrupt(void);
void IRAM_ATTR mp_hal_trig_term_sig(void);
void mp_hal_set_signal_exit_cb (_sig_func_cb_ptr fun);

#endif // MICROPY_INCLUDED_LIB_UTILS_INTERRUPT_CHAR_H
