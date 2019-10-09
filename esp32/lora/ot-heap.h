/*
 * Copyright (c) 2019, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#ifndef LORA_OT_HEAP_H_
#define LORA_OT_HEAP_H_

void* otHeapCAllocFunction(size_t aCount, size_t aSize);
void otHeapFreeFunction(void *aPointer);

#endif /* LORA_OT_HEAP_H_ */
