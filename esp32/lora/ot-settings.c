/*
 * Copyright (c) 2018, Pycom Limited.
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

#include <openthread/instance.h>
#include <openthread/ip6.h>
#include <openthread/platform/settings.h>

#define OT_PLAT_SETTINGS_NUM					7
#define OT_PLAT_SETTINGS_VAL_MAX_SIZE 		128
#define OT_PLAT_SETTINGS_MAX_INDEX_PER_KEY 	1

typedef struct settings_row_s {
    int16_t size;
    uint8_t value[OT_PLAT_SETTINGS_VAL_MAX_SIZE];
} settings_row_t;

typedef struct settings_s {
    //uint16_t key; # key is the actual index
    int8_t rows_num;
    settings_row_t rows[OT_PLAT_SETTINGS_MAX_INDEX_PER_KEY];
} settings_t;

settings_t settings[OT_PLAT_SETTINGS_NUM];

void printRow(settings_row_t row) {
    printf("size %d:", row.size);
    /*
     for (int i = 0; i < row.size; i++)
     printf("%d, ", row.value[i]);
     */
    printf("\n");
}

void printKey(settings_t key) {
    printf(" num_rows: %d", key.rows_num);
    for (int i = 0; i < key.rows_num; i++) {
        printf("\nRow %d, ", i);
        printRow(key.rows[i]);
    }
}

void otPlatSettingsPrint(void) {
    printf("----------------------------------------------");
    for (int i = 0; i < OT_PLAT_SETTINGS_NUM; i++) {
        printf("\nKey %d: ", i);
        printKey(settings[i]);
    }
    printf("----------------------------------------------\n");
}
/**
 * Performs any initialization for the settings subsystem, if necessary.
 *
 * @param[in]  aInstance
 *             The OpenThread instance structure.
 *
 */

void otPlatSettingsInit(otInstance *aInstance) {
    (void) aInstance;
    otPlatLog(OT_LOG_LEVEL_DEBG, 0, "otPlatSettingsInit mem %d B\n",
            OT_PLAT_SETTINGS_NUM * sizeof(settings_t));
    for (int i = 0; i < OT_PLAT_SETTINGS_NUM; i++) {
        settings[i].rows_num = -1;
        for (int j = 0; j < OT_PLAT_SETTINGS_MAX_INDEX_PER_KEY; j++) {
            settings[i].rows[j].size = -1;
        }
    }
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
    otPlatLog(OT_LOG_LEVEL_DEBG, 0, "otPlatSettingsGet %d %d %d ", aKey, aIndex,
            *aValueLength);

    if (aKey > OT_PLAT_SETTINGS_NUM)
        return OT_ERROR_NOT_FOUND;

    if (aIndex > settings[aKey].rows_num
            || settings[aKey].rows[aIndex].size < 0) {
        otPlatLog(OT_LOG_LEVEL_DEBG, 0, " not found\n");
        return OT_ERROR_NOT_FOUND;
    }

    if (*aValueLength < settings[aKey].rows[aIndex].size) {
        aValueLength = NULL;
    } else {
        memcpy(aValue, settings[aKey].rows[aIndex].value,
                settings[aKey].rows[aIndex].size);
        *aValueLength = settings[aKey].rows[aIndex].size;
    }
    otPlatLog(OT_LOG_LEVEL_DEBG, 0, " ok\n");
    //printSettings();
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
    otPlatLog(OT_LOG_LEVEL_DEBG, 0, "otPlatSettingsSet key %d len %d", aKey,
            aValueLength);

    if (aKey > OT_PLAT_SETTINGS_NUM)
        return OT_ERROR_NOT_IMPLEMENTED;

    if (aValueLength > OT_PLAT_SETTINGS_VAL_MAX_SIZE)
        return OT_ERROR_NOT_IMPLEMENTED;

    settings[aKey].rows_num = 1;

    settings[aKey].rows[0].size = aValueLength;

    if (aValueLength > 0) {
        memcpy(settings[aKey].rows[0].value, aValue, aValueLength);
    }
    otPlatLog(OT_LOG_LEVEL_DEBG, 0, " ok\n");
    //printSettings();
    return OT_ERROR_NONE;
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
    otPlatLog(OT_LOG_LEVEL_DEBG, 0, "otPlatSettingsDelete %d %d\n", aKey,
            aIndex);

    if (aKey > OT_PLAT_SETTINGS_NUM)
        return OT_ERROR_NOT_FOUND;

    if (aIndex >= OT_PLAT_SETTINGS_MAX_INDEX_PER_KEY)
        return OT_ERROR_NOT_FOUND;

    if (aIndex == -1) {
        for (int i = 0; i < settings[aKey].rows_num; i++)
            settings[aKey].rows[i].size = -1;

        settings[aKey].rows_num = -1;
    }
    otPlatLog(OT_LOG_LEVEL_DEBG, 0, "otPlatSettingsDelete ok\n");
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
    (void) aInstance;
    otPlatLog(OT_LOG_LEVEL_DEBG, 0, "!!!!! otPlatSettingsAdd %d %d %d\n", aKey,
            *aValue, aValueLength);
    return OT_ERROR_NOT_IMPLEMENTED;
}

/// Removes all settings from the setting store
/** This function deletes all settings from the settings
 *  store, resetting it to its initial factory state.
 *
 *  @param[in] aInstance
 *             The OpenThread instance structure.
 */
void otPlatSettingsWipe(otInstance *aInstance) {
    for (int i = 0; i < OT_PLAT_SETTINGS_NUM; i++) {
        settings[i].rows_num = -1;
        for (int j = 0; j < OT_PLAT_SETTINGS_MAX_INDEX_PER_KEY; j++) {
            settings[i].rows[j].size = -1;
        }
    }
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
