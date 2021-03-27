/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#ifndef LORA_OT_LOG_H_
#define LORA_OT_LOG_H_

void otPlatLogFlush(void);

// openThread log a buffer
void otPlatLogBuf(const char *aBuf, uint16_t aBufLength);
void otPlatLogBufHex(const uint8_t *aBuf, uint16_t aBufLength);

#endif /* LORA_OT_LOG_H_ */
