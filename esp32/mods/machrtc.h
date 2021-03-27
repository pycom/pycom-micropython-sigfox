/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#ifndef MACHRTC_H_
#define MACHRTC_H_

extern const mp_obj_type_t mach_rtc_type;

void rtc_init0(void);
void mach_rtc_synced (void);
void mach_rtc_set_us_since_epoch(uint64_t nowus);
uint64_t mach_rtc_get_us_since_epoch(void);
bool mach_is_rtc_synced (void);

#endif // MACHRTC_H_
