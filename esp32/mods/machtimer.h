/*
 * Copyright (c) 2016, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#ifndef MACHTIMER_H_
#define MACHTIMER_H_

extern void modtimer_init0(void);
uint64_t get_timer_counter_value(void);

extern const mp_obj_type_t mach_timer_type;

#endif  // MACHTIMER_H_
