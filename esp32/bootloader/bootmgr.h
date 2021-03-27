/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#ifndef __BOOTMGR_H
#define __BOOTMGR_H

#include "bootloader.h"

bool wait_for_safe_boot (const boot_info_t *boot_info, uint32_t *ActiveImg);

#endif // __BOOTMGR_H
