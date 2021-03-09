/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include "ot-log.h"

#include "pycom_config.h"
#include <openthread/config.h>
#include <openthread/platform/alarm-milli.h>
#include <openthread/platform/uart.h>
#include <openthread-core-config.h>
#include <openthread/platform/logging.h>

#define LOG_PARSE_BUFFER_SIZE 2048

static char logString[LOG_PARSE_BUFFER_SIZE + 1];
static uint16_t length = 0;

/**
 * This function outputs logs.
 *
 * @param[in]  aLogLevel   The log level.
 * @param[in]  aLogRegion  The log region.
 * @param[in]  aFormat     A pointer to the format string.
 * @param[in]  ...         Arguments for the format specification.
 *
 */
void otPlatLog(otLogLevel aLogLevel, otLogRegion aLogRegion,
        const char *aFormat, ...) {
    (void) aLogRegion;

    if (aLogLevel <= OPENTHREAD_CONFIG_LOG_LEVEL) {

        length += sprintf(&logString[length], " %d ", otPlatAlarmMilliGetNow());
        // Parse user string.
        va_list paramList;
        va_start(paramList, aFormat);
        length += vsnprintf(&logString[length],
                (LOG_PARSE_BUFFER_SIZE - length), aFormat, paramList);

        if (length > LOG_PARSE_BUFFER_SIZE) {
            length = LOG_PARSE_BUFFER_SIZE;
        }

        logString[length++] = '\n';
        va_end(paramList);
    }
}

void otPlatLogBuf(const char *aBuf, uint16_t aBufLength) {

    if (LOG_PARSE_BUFFER_SIZE - length < aBufLength)
        aBufLength = LOG_PARSE_BUFFER_SIZE - length;

    memcpy(&logString[length], aBuf, aBufLength);
    length += aBufLength;
}

void otPlatLogBufHex(const uint8_t *aBuf, uint16_t aBufLength) {

    if (LOG_PARSE_BUFFER_SIZE - length < aBufLength)
        aBufLength = LOG_PARSE_BUFFER_SIZE - length;
    for (int i = 0; i < aBufLength; i++) {
        length += sprintf(&logString[length], "%x ", aBuf[i]);
    }
    logString[length++] = '\n';
}

void otPlatLogFlush(void) {
    ets_printf("%s", logString); // output whole log string
    memset(logString, 0, length);
    length = 0;
}

