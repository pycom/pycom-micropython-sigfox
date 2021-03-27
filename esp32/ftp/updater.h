/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */


#ifndef UPDATER_H_
#define UPDATER_H_

#include "bootloader.h"

/**
 * @brief  Checks the default path.
 *
 * @param  path the file-path
 *
 * @return true if filepath matches; false otherwise.
 */
extern bool updater_check_path(void *path);


/**
 * @brief  Initialized the OTA update process.
 *
 * @return true if initialization succeeded; false otherwise.
 */
extern bool updater_start(void);


/**
 * @brief  OTA Write next chunk to Flash.
 *
 * @note The OTA process has to be previously initialized with updater_start().
 *        The buf is written as it is (not-encrypted) into Flash.
 *        If Flash Encryption is enabled, the buf must be already encrypted (by the OTA server).
 *
 * @param  buf  buffer with the data-chunk which needs to be written into Flash
 * @param  len  length of the buf data.
 *
 * @return true if write into Flash succeeded.
 */
extern bool updater_write(uint8_t *buf, uint32_t len);

/**
 * @brief  Closing the OTA process. This provokes updating the boot info from the otadata partition.
 *
 * @return true if boot info was saved successful; false otherwise.
 */
extern bool updater_finish(void);

/**
 * @brief  Verifies the newly written OTA image.
 *
 * @note If Secure Boot is enabled the signature is checked.
 *          Anyway the image integrity (SHA256) is checked.
 *
 * @return true if boot info was saved successful; false otherwise.
 */
extern bool updater_verify(void);

/**
 * @brief  Reads the boot information, what partition is going to be booted from.
 *
 * @param  boot_info        [in/out] filled with boot info
 * @param  boot_info_offset [in/out] filled with the address in Flash, of the boot_info (otadata partition)
 *
 * @return true if reading was done successful.
 */
extern bool updater_read_boot_info(boot_info_t *boot_info, uint32_t *boot_info_offset);

/**
 * @brief  Returns the address of the next OTA partition, where the next update will be written.
 *
 * @return address from Flash, in Bytes.
 */
extern int updater_ota_next_slot_address();

/**
 * @brief  Writes the boot information into the otadata partition.
 *
 * @param  boot_info        [in] the boot info data structure
 * @param  boot_info_offset [in] the address in Flash, of the boot_info (otadata partition)
 *
 * @return true if reading was done successful.
 */
extern bool updater_write_boot_info(boot_info_t *boot_info, uint32_t boot_info_offset);

#ifdef DIFF_UPDATE_ENABLED
/**
 * @brief  Patches the current image with the delta file and writes the final image to the free partition.
 *         The implementation is based on the bsdiff's patching algorithm.
 * 
 * @return true if patching was successful.
 */
extern bool updater_patch(void);
#endif

#endif /* UPDATER_H_ */
