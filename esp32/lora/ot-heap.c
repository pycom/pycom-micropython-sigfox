/*
 * Copyright (c) 2019, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

/*
 * This file contains code for 2 openthread features:
 * * general Heap (dynamic allocation)
 * * message buffer management
 */
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_heap_caps.h"
#include "esp_system.h"

#include "py/mperrno.h"

#include "lora/otplat_radio.h"

#include <openthread/heap.h>
#include <openthread/message.h>

/******************************************************************************
 DEFINE TYPES
 ******************************************************************************/

typedef struct {
    uint8_t **buffer; /* array of messages */
    bool *free; /* array to indicate if a buffer is unused */
    uint16_t num; /* number of messages slots */
    uint16_t free_num; /* number of free messages slots inside message buffer */
    uint16_t size; /* size (in bytes) of a slots */
} ot_buffer_obj_t;

/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/
/* pointer to the message buffer array */
static ot_buffer_obj_t buff_obj;

static SemaphoreHandle_t xSemaphore = NULL;

/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/

/******************************************************************************
 DEFINE PUBLIC FUNCTIONS - Heap management, only used if :
 1. OPENTHREAD_CONFIG_HEAP_EXTERNAL_ENABLE is set to 1 (in components/openthread)
 2. function otHeapSetCAllocFree() is called before OpenThread platform init
 ******************************************************************************/
/**
 * Function pointer used to set external CAlloc function for OpenThread.
 *
 * @param[in]   aCount  Number of allocate units.
 * @param[in]   aSize   Unit size in bytes.
 *
 * @returns A pointer to the allocated memory.
 *
 * @retval  NULL    Indicates not enough memory.
 *
 */
//typedef void *(*otHeapCAllocFn)(size_t aCount, size_t aSize);
void* otHeapCAllocFunction(size_t aCount, size_t aSize) {
    void* ret = heap_caps_calloc(aCount, aSize,
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    otPlatLog(OT_LOG_LEVEL_DEBG, 0, "otHeapCAllocFunction %d %d %p", aCount,
            aSize, ret);
    return ret;
}

/**
 * Function pointer used to set external Free function for OpenThread.
 *
 * @param[in]   aPointer    A pointer to the memory to free.
 *
 */
//typedef void (*otHeapFreeFn)(void *aPointer);
void otHeapFreeFunction(void *aPointer) {
    otPlatLog(OT_LOG_LEVEL_DEBG, 0, "otHeapFreeFunction");
    heap_caps_free(aPointer);
}

/**
 * This function sets the external heap CAlloc and Free
 * functions to be used by the OpenThread stack.
 *
 * This function must be used before invoking instance initialization.
 *
 * @param[in]  aCAlloc  A pointer to external CAlloc function.
 * @param[in]  aFree    A pointer to external Free function.
 *
 */
//void otHeapSetCAllocFree(otHeapCAllocFn aCAlloc, otHeapFreeFn aFree);
/* This function is called in Pymesh initialization, before ot instance creation */

/******************************************************************************
 DEFINE PUBLIC FUNCTIONS - Message buffer management,
 used only if OPENTHREAD_CONFIG_PLATFORM_MESSAGE_MANAGEMENT is set to 1
 ******************************************************************************/
/**
 * Initialize the platform implemented message pool.
 *
 * @param[in] aInstance            A pointer to the OpenThread instance.
 * @param[in] aMinNumFreeBuffers   An uint16 containing the minimum number of free buffers desired by OpenThread.
 * @param[in] aBufferSize          The size in bytes of a Buffer object.
 *
 */
void otPlatMessagePoolInit(otInstance *aInstance, uint16_t aMinNumFreeBuffers,
        size_t aBufferSize) {

    int error = 0;

    (void) aInstance;

    if (xSemaphore == NULL) {
        xSemaphore = xSemaphoreCreateBinary();
    }
    otEXPECT_ACTION( NULL != xSemaphore, error = MP_ENXIO);

    // according to docs, the semaphore created with xSemaphoreCreateBinary must first be "Given"
    xSemaphoreGive(xSemaphore);

//    ets_printf("otPlatMessagePoolInit %d %d\n", aMinNumFreeBuffers, aBufferSize);

    if (xSemaphoreTake( xSemaphore, ( TickType_t ) portMAX_DELAY ) == pdTRUE) {
        buff_obj.buffer = heap_caps_calloc(aMinNumFreeBuffers,
                sizeof(uint8_t *), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

        otEXPECT_ACTION(NULL != buff_obj.buffer, error = MP_ENOMEM);

        otEXPECT_ACTION(
                NULL
                        != (buff_obj.free = heap_caps_calloc(aMinNumFreeBuffers,
                                sizeof(bool),
                                MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)),
                error = MP_EFAULT);

        buff_obj.num = aMinNumFreeBuffers;
        buff_obj.free_num = aMinNumFreeBuffers;
        buff_obj.size = aBufferSize;

        for (int i = 0; i < aMinNumFreeBuffers; i++) {
            buff_obj.free[i] = true;

            buff_obj.buffer[i] = heap_caps_calloc(aBufferSize, sizeof(uint8_t),
                    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            otEXPECT_ACTION(NULL != buff_obj.buffer[i],
                    error = MP_EFAULT + 1 + i);
        }
        xSemaphoreGive(xSemaphore);
    } else {
        ets_printf("otPlatMessagePoolInit Can't take semaphore\n");
    }

    exit: if (error != 0) {
        xSemaphoreGive(xSemaphore);
        printf("err: %d\n", error);
    }
}

/**
 * Allocate a buffer from the platform managed buffer pool.
 *
 * @param[in] aInstance            A pointer to the OpenThread instance.
 *
 * @returns A pointer to the Buffer or NULL if no Buffers are available.
 *
 */
otMessage *otPlatMessagePoolNew(otInstance *aInstance) {
    (void) aInstance;

    if ( xSemaphoreTake( xSemaphore, ( TickType_t ) portMAX_DELAY ) == pdTRUE) {

        // find the first free message
        for (int i = 0; i < buff_obj.num; i++) {
            if (buff_obj.free[i]) {
                buff_obj.free[i] = false;
                buff_obj.free_num--;
                xSemaphoreGive(xSemaphore);
                otPlatLog(OT_LOG_LEVEL_DEBG, 0,
                        "otPlatMessagePoolNew, found mess %d", i);
                return (otMessage *) buff_obj.buffer[i];
            }
        }
        xSemaphoreGive(xSemaphore);
        otPlatLog(OT_LOG_LEVEL_CRIT, 0,
                "otPlatMessagePoolNew, no free message found !!!");

    } else {
        ets_printf("otPlatMessagePoolNew Can't take semaphore\n");
    }

    // if reach here, then it's not found, return NULL
    return NULL;
}

/**
 * This function is used to free a Buffer back to the platform managed buffer pool.
 *
 * @param[in]  aInstance  A pointer to the OpenThread instance.
 * @param[in]  aBuffer    The Buffer to free.
 *
 */
void otPlatMessagePoolFree(otInstance *aInstance, otMessage *aBuffer) {
    (void) aInstance;

//    ets_printf("otPlatMessagePoolFree 0x%p %d\n", aBuffer, buff_obj.free_num);
    if ( xSemaphoreTake( xSemaphore, ( TickType_t ) portMAX_DELAY ) == pdTRUE) {

        for (int i = 0; i < buff_obj.num; i++) {
            if ((otMessage *) buff_obj.buffer[i] == aBuffer) {
                // just make sure the buffer is really used
                if (!buff_obj.free[i]) {
                    buff_obj.free[i] = true;
                    buff_obj.free_num++;
                }
                xSemaphoreGive(xSemaphore);
//                otPlatLog(OT_LOG_LEVEL_DEBUG, 0, "otPlatMessagePoolFree, found mess %d", i);
                return;
            }
        }
        xSemaphoreGive(xSemaphore);
        otPlatLog(OT_LOG_LEVEL_CRIT, 0, "otPlatMessagePoolFree, no free message found !!!");

    } else {
        ets_printf("otPlatMessagePoolNew Can't take semaphore\n");
    }
}

/**
 * Get the number of free buffers.
 *
 * @param[in]  aInstance  A pointer to the OpenThread instance.
 *
 * @returns The number of buffers currently free and available to OpenThread.
 *
 */
uint16_t otPlatMessagePoolNumFreeBuffers(otInstance *aInstance) {
    (void) aInstance;
    return buff_obj.free_num;
}
