/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

//#include "ot-settings.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "pycom_config.h"
#include "nvs.h"

#include <openthread/instance.h>
#include <openthread/ip6.h>
#include <openthread/platform/settings.h>
#include "ot-log.h"

#define OT_NVS_NAMESPACE                       "LORA_MESH"

#define OT_PLAT_SETTINGS_NUM					255
#define OT_PLAT_SETTINGS_VAL_MAX_SIZE 		256
#define OT_PLAT_SETTINGS_MAX_INDEX_PER_KEY 	256

/* number of chars to store key and index as string, 256 key max, 256 index max, so in hex FFFF 4 chars + 1 termination */
#define KEY_STRING_CHARS_NUM             5

// returns the string, zzzOutStr, created from integer: 2 hex digits xxxKey and 2 hex digits yyyIndex
#define KEY_INDEX_TO_STR(xxxKey, yyyIndex, zzzOutStr)              itoa((((xxxKey << 16) | yyyIndex) & 0xFFFF), zzzOutStr, 16)

static nvs_handle ot_nvs_handle;

/**
 * Performs any initialization for the settings subsystem, if necessary.
 *
 * @param[in]  aInstance
 *             The OpenThread instance structure.
 *
 */

void otPlatSettingsInit(otInstance *aInstance) {
    (void) aInstance;

    if (nvs_open(OT_NVS_NAMESPACE, NVS_READWRITE, &ot_nvs_handle) != ESP_OK) {
        otPlatLog(OT_LOG_LEVEL_INFO, 0, "otPlatSettingsInit NOT OK");
    }
    //nvs_erase_all(ot_nvs_handle);
    otPlatLog(OT_LOG_LEVEL_INFO, 0, "otPlatSettingsInit ok");
    //printSettings();
}

/// Fetches the value of a setting
/** This function fetches the value of the setting identified
 *  by aKey and write it to the memory pointed to by aValue.
 *  It then writes the length to the integer pointed to by
 *  aValueLength. The initial value of aValueLength is the
 *  maximum number of bytes to be written to aValue.
 *
 *  This function can be used to check for the existence of
 *  a key without fetching the value by setting aValue and
 *  aValueLength to NULL. You can also check the length of
 *  the setting without fetching it by setting only aValue
 *  to NULL.
 *
 *  Note that the underlying storage implementation is not
 *  required to maintain the order of settings with multiple
 *  values. The order of such values MAY change after ANY
 *  write operation to the store.
 *
 *  @param[in]     aInstance
 *                 The OpenThread instance structure.
 *  @param[in]     aKey
 *                 The key associated with the requested setting.
 *  @param[in]     aIndex
 *                 The index of the specific item to get.
 *  @param[out]    aValue
 *                 A pointer to where the value of the setting
 *                 should be written. May be set to NULL if just
 *                 testing for the presence or length of a setting.
 *  @param[inout]  aValueLength
 *                 A pointer to the length of the value. When
 *                 called, this pointer should point to an
 *                 integer containing the maximum value size that
 *                 can be written to aValue. At return, the actual
 *                 length of the setting is written. This may be
 *                 set to NULL if performing a presence check.
 *
 *  @retval OT_ERROR_NONE
 *          The given setting was found and fetched successfully.
 *  @retval OT_ERROR_NOT_FOUND
 *          The given setting was not found in the setting store.
 *  @retval OT_ERROR_NOT_IMPLEMENTED
 *          This function is not implemented on this platform.
 */
otError otPlatSettingsGet(otInstance *aInstance, uint16_t aKey, int aIndex,
        uint8_t *aValue, uint16_t *aValueLength) {
    (void) aInstance;
    esp_err_t err;
    size_t length = 0;
    char key_str[KEY_STRING_CHARS_NUM];

    // transform key to string
    KEY_INDEX_TO_STR(aKey, aIndex, key_str);

    otPlatLog(OT_LOG_LEVEL_INFO, 0, "otPlatSettingsGet %d 0x%s %d %d %p", aKey, key_str, aIndex,
            *aValueLength, aValue);

    if (aKey > OT_PLAT_SETTINGS_NUM || aIndex > OT_PLAT_SETTINGS_MAX_INDEX_PER_KEY) {
        otPlatLog(OT_LOG_LEVEL_INFO, 0, " not found\n");
        return OT_ERROR_NOT_FOUND;
    }

    if (aValue == NULL && aValueLength == NULL) {
        // checking if key exists
        if (ESP_OK != (err = nvs_get_blob(ot_nvs_handle, key_str, aValue, &length))) {
            otPlatLog(OT_LOG_LEVEL_INFO, 0, " nvs_get_blob 1 0x%X\n", err);
            return OT_ERROR_NOT_FOUND;
        }
    } else if (aValue == NULL && aValueLength != NULL) {
        // checking the size of a key
        if (ESP_OK != (err = nvs_get_blob(ot_nvs_handle, key_str, aValue, &length))) {
            otPlatLog(OT_LOG_LEVEL_INFO, 0, " nvs_get_blob 2 0x%X\n", err);
            return OT_ERROR_NOT_FOUND;
        }
        *aValueLength = length;
    } else {
        length = *aValueLength;
        if (ESP_OK != (err = nvs_get_blob(ot_nvs_handle, key_str, aValue, &length))) {
            otPlatLog(OT_LOG_LEVEL_INFO, 0, " nvs_get_blob 3 0x%X\n", err);
            return OT_ERROR_NOT_FOUND;
        }
        *aValueLength = length;
    }

    otPlatLog(OT_LOG_LEVEL_INFO, 0, " ok\n");
    return OT_ERROR_NONE;
}

/// Sets or replaces the value of a setting
/** This function sets or replaces the value of a setting
 *  identified by aKey. If there was more than one
 *  value previously associated with aKey, then they are
 *  all deleted and replaced with this single entry.
 *
 *  Calling this function successfully may cause unrelated
 *  settings with multiple values to be reordered.
 *
 *  @param[in]  aInstance
 *              The OpenThread instance structure.
 *  @param[in]  aKey
 *              The key associated with the setting to change.
 *  @param[out] aValue
 *              A pointer to where the new value of the setting
 *              should be read from. MUST NOT be NULL if aValueLength
 *              is non-zero.
 *  @param[in]  aValueLength
 *              The length of the data pointed to by aValue.
 *              May be zero.
 *
 *  @retval OT_ERROR_NONE
 *          The given setting was changed or staged.
 *  @retval OT_ERROR_NOT_IMPLEMENTED
 *          This function is not implemented on this platform.
 */
otError otPlatSettingsSet(otInstance *aInstance, uint16_t aKey,
        const uint8_t *aValue, uint16_t aValueLength) {
    (void) aInstance;

    otPlatLog(OT_LOG_LEVEL_INFO, 0, "otPlatSettingsSet key %d len %d", aKey, aValueLength);

    // delete all records for this aKey
    otPlatSettingsDelete(aInstance, aKey, -1);

    return otPlatSettingsAdd(aInstance, aKey, aValue, aValueLength);
}

/// Removes a setting from the setting store
/** This function deletes a specific value from the
 *  setting identified by aKey from the settings store.
 *
 *  Note that the underlying implementation is not required
 *  to maintain the order of the items associated with a
 *  specific key.
 *
 *  @param[in] aInstance
 *             The OpenThread instance structure.
 *  @param[in] aKey
 *             The key associated with the requested setting.
 *  @param[in] aIndex
 *             The index of the value to be removed. If set to
 *             -1, all values for this aKey will be removed.
 *
 *  @retval OT_ERROR_NONE
 *          The given key and index was found and removed successfully.
 *  @retval OT_ERROR_NOT_FOUND
 *          The given key or index  was not found in the setting store.
 *  @retval OT_ERROR_NOT_IMPLEMENTED
 *          This function is not implemented on this platform.
 */
otError otPlatSettingsDelete(otInstance *aInstance, uint16_t aKey, int aIndex) {
    (void) aInstance;
    char key_str[KEY_STRING_CHARS_NUM];

    otPlatLog(OT_LOG_LEVEL_INFO, 0, "otPlatSettingsDelete %d %d", aKey,
            aIndex);

    if (aKey > OT_PLAT_SETTINGS_NUM || aIndex > OT_PLAT_SETTINGS_MAX_INDEX_PER_KEY)
        return OT_ERROR_NOT_FOUND;

    if (aIndex != -1) {
        // transform key to string
        KEY_INDEX_TO_STR(aKey, aIndex, key_str);

        if (ESP_OK == nvs_erase_key(ot_nvs_handle, key_str)) {
            nvs_commit(ot_nvs_handle);
        }
    } else {
        // delete all indexes for this key
        for (int index = 0; index < OT_PLAT_SETTINGS_MAX_INDEX_PER_KEY; index++) {
            KEY_INDEX_TO_STR(aKey, index, key_str);
            // try to erase the key for this index
            if (ESP_ERR_NVS_NOT_FOUND == nvs_erase_key(ot_nvs_handle, key_str)) {
                // key is not found, so we're done for this aKey
                break;
            }
        }
        nvs_commit(ot_nvs_handle);
    }

    otPlatLog(OT_LOG_LEVEL_INFO, 0, "otPlatSettingsDelete ok");
    return OT_ERROR_NONE;

}

/// Adds a value to a setting
/** This function adds the value to a setting
 *  identified by aKey, without replacing any existing
 *  values.
 *
 *  Note that the underlying implementation is not required
 *  to maintain the order of the items associated with a
 *  specific key. The added value may be added to the end,
 *  the beginning, or even somewhere in the middle. The order
 *  of any pre-existing values may also change.
 *
 *  Calling this function successfully may cause unrelated
 *  settings with multiple values to be reordered.
 *
 * @param[in]     aInstance
 *                The OpenThread instance structure.
 * @param[in]     aKey
 *                The key associated with the setting to change.
 * @param[out]    aValue
 *                A pointer to where the new value of the setting
 *                should be read from. MUST NOT be NULL if aValueLength
 *                is non-zero.
 * @param[inout]  aValueLength
 *                The length of the data pointed to by aValue.
 *                May be zero.
 *
 * @retval OT_ERROR_NONE
 *         The given setting was added or staged to be added.
 * @retval OT_ERROR_NOT_IMPLEMENTED
 *         This function is not implemented on this platform.
 */
otError otPlatSettingsAdd(otInstance *aInstance, uint16_t aKey,
        const uint8_t *aValue, uint16_t aValueLength) {
    int index = 0;
    size_t length = 0;
    esp_err_t err = ESP_OK;
    char key_str[KEY_STRING_CHARS_NUM];

    (void) aInstance;

    otPlatLog(OT_LOG_LEVEL_INFO, 0, "otPlatSettingsAdd %d 0x%p %d", aKey,
            aValue, aValueLength);

    if (aKey > OT_PLAT_SETTINGS_NUM)
        return OT_ERROR_NOT_FOUND;

    // search for first index available for aKey
    for (index = 0; index < OT_PLAT_SETTINGS_MAX_INDEX_PER_KEY; index++) {
        KEY_INDEX_TO_STR(aKey, index, key_str);
        // checking if key exists
        if (ESP_OK != (err = nvs_get_blob(ot_nvs_handle, key_str, NULL, &length))) {
            otPlatLog(OT_LOG_LEVEL_INFO, 0, "found free index %d\n", index);
            // key doesn't exist
            break;
        }
    }

    if (ESP_OK != err) {
        // try to write
        if (ESP_OK != (err = nvs_set_blob(ot_nvs_handle, key_str, aValue, aValueLength))) {
            otPlatLog(OT_LOG_LEVEL_INFO, 0, " nvs_set_blob 0x%X\n", err);
            return OT_ERROR_NOT_IMPLEMENTED;
        }
        nvs_commit(ot_nvs_handle);
        otPlatLog(OT_LOG_LEVEL_INFO, 0, "stored as index %d\n", index);
    }
    return OT_ERROR_NONE;
}

/// Removes all settings from the setting store
/** This function deletes all settings from the settings
 *  store, resetting it to its initial factory state.
 *
 *  @param[in] aInstance
 *             The OpenThread instance structure.
 */
void otPlatSettingsWipe(otInstance *aInstance) {
    nvs_erase_all(ot_nvs_handle);
    nvs_commit(ot_nvs_handle);
    otPlatLog(OT_LOG_LEVEL_INFO, 0, "!!!!! otPlatSettingsWipe");
}

/**
 * This function performs a software reset on the platform, if supported.
 *
 * @param[in] aInstance  The OpenThread instance structure.
 *
 */
void otPlatReset(otInstance *aInstance) {
    return;
}
