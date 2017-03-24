/*
 * Copyright (c) 2016, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#ifndef MPERROR_H_
#define MPERROR_H_

#ifndef BOOTLOADER_BUILD
extern const mp_obj_type_t pyb_heartbeat_type;
extern void NORETURN __fatal_error(const char *msg);
#endif

#define MPERROR_HEARTBEAT_COLOR                     (0x50)          // blue
#define MPERROR_FATAL_COLOR                         (0x500000)      // red

void mperror_pre_init(void);
void mperror_init0 (void);
void mperror_bootloader_check_reset_cause (void);
void mperror_deinit_sfe_pin (void);
void mperror_signal_error (void);
void mperror_heartbeat_switch_off (void);
bool mperror_heartbeat_signal (void);
void mperror_enable_heartbeat (bool enable);
bool mperror_is_heartbeat_enabled (void);

void mperror_set_rgb_color(uint32_t rgbcolor);

#endif // MPERROR_H_
