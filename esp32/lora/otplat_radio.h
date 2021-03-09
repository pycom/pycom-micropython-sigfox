/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#ifndef LORA_OTPLAT_RADIO_H_
#define LORA_OTPLAT_RADIO_H_

#include <stdint.h>
#include <stdbool.h>
#include <openthread/instance.h>

/**
 *  This checks for the specified condition, which is expected to
 *  commonly be true, and branches to the local label 'exit' if the
 *  condition is false.
 *
 *  @param[in]  aCondition  A Boolean expression to be evaluated.
 *
 */
#define otEXPECT(aCondition) \
    do                       \
    {                        \
        if (!(aCondition))   \
        {                    \
            goto exit;       \
        }                    \
    } while (0)

/**
 *  This checks for the specified condition, which is expected to
 *  commonly be true, and both executes @p anAction and branches to
 *  the local label 'exit' if the condition is false.
 *
 *  @param[in]  aCondition  A Boolean expression to be evaluated.
 *  @param[in]  aAction     An expression or block to execute when the
 *                          assertion fails.
 *
 */
#define otEXPECT_ACTION(aCondition, aAction) \
    do                                       \
    {                                        \
        if (!(aCondition))                   \
        {                                    \
            aAction;                         \
            goto exit;                       \
        }                                    \
    } while (0)

void otPlatRadioInit(void);

void otRadioProcess(otInstance *aInstance);

#endif /* LORA_OTPLAT_RADIO_H_ */
