/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#ifndef MACHTIMER_H_
#define MACHTIMER_H_

#define CLK_FREQ                                    (APB_CLK_FREQ / 2)

extern const mp_obj_type_t mach_timer_type;


extern void machtimer_preinit(void);

extern void machtimer_init0(void);

extern void machtimer_deinit(void);

extern uint64_t machtimer_get_timer_counter_value(void);

#endif  // MACHTIMER_H_
