/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#ifndef MACHPWM_H_
#define MACHPWM_H_

#include "ledc.h"

typedef struct {
    mp_obj_base_t base;
    ledc_channel_config_t config;
} mach_pwm_channel_obj_t;


typedef struct {
    mp_obj_base_t base;
    ledc_timer_config_t config;
    mach_pwm_channel_obj_t *mach_pwm_channel_obj_t[LEDC_CHANNEL_7 + 1];

} mach_pwm_timer_obj_t;


extern const mp_obj_type_t mach_pwm_timer_type;


#endif  // MACHPWM_H_
