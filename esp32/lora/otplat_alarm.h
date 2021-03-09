/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#ifndef LORA_OTPLAT_ALARM_H_
#define LORA_OTPLAT_ALARM_H_

#include <stdint.h>
#include <openthread/instance.h>
#include <openthread/ip6.h>

inline uint16_t HostSwap16(uint16_t v) {
    return (((v & 0x00ffU) << 8) & 0xff00) | (((v & 0xff00U) >> 8) & 0x00ff);
}

inline uint32_t HostSwap32(uint32_t v) {
    return ((v & (uint32_t) (0x000000ffUL)) << 24)
            | ((v & (uint32_t) (0x0000ff00UL)) << 8)
            | ((v & (uint32_t) (0x00ff0000UL)) >> 8)
            | ((v & (uint32_t) (0xff000000UL)) >> 24);
}

void otPlatAlarmInit(otInstance *aInstance);

void printSingleIpv6(otIp6Address *addr);

#endif /* LORA_OTPLAT_ALARM_H_ */
