/*
 * Copyright (c) 2016, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */


#ifndef UPDATER_H_
#define UPDATER_H_

#include "bootloader.h"

extern void updater_pre_init (void);
extern bool updater_check_path (void *path);
extern bool updater_start (void);
extern bool updater_write (uint8_t *buf, uint32_t len);
extern bool updater_finish (void);
extern bool updater_verify (uint8_t *rbuff, uint8_t *hasbuff);
extern bool updater_read_boot_info (boot_info_t *boot_info, uint32_t *boot_info_offset);

#endif /* UPDATER_H_ */
