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

#include "fifo.h"
#include "socketfifo.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/*----------------------------------------------------------------------------
 ** Declare private functions
 */
static void socketfifo_Push (void * const pvFifo, const void * const pvElement);
static void socketfifo_Pop (void * const pvFifo, void * const pvElement);

/*----------------------------------------------------------------------------
 ** Declare private data
 */
static FIFO_t *socketfifo;

/*----------------------------------------------------------------------------
 ** Define public functions
 */
void SOCKETFIFO_Init (FIFO_t *fifo, void *elements, uint32_t maxcount) {
    // Initialize global data
    socketfifo = fifo;
    socketfifo->pvElements = elements;
    FIFO_Init (socketfifo, maxcount, socketfifo_Push, socketfifo_Pop);
}

bool SOCKETFIFO_Push (const void * const element) {
    return FIFO_bPushElement (socketfifo, element);
}

bool SOCKETFIFO_Pop (void * const element) {
    return FIFO_bPopElement (socketfifo, element);
}

bool SOCKETFIFO_Peek (void * const element) {
    return FIFO_bPeekElement (socketfifo, element);
}

bool SOCKETFIFO_IsEmpty (void) {
    return FIFO_IsEmpty (socketfifo);
}

bool SOCKETFIFO_IsFull (void) {
    return FIFO_IsFull (socketfifo);
}

void SOCKETFIFO_Flush (void) {
    SocketFifoElement_t element;
    while (SOCKETFIFO_Pop(&element)) {
        if (element.freedata) {
            free(element.data);
        }
    }
}

unsigned int SOCKETFIFO_Count (void) {
    return socketfifo->uiElementCount;
}

/*----------------------------------------------------------------------------
 ** Define private functions
 */
static void socketfifo_Push (void * const pvFifo, const void * const pvElement) {
    if ((pvFifo != NULL) && (NULL != pvElement)) {
        unsigned int uiLast = ((FIFO_t *)pvFifo)->uiLast;
        memcpy (&((SocketFifoElement_t *)((FIFO_t *)pvFifo)->pvElements)[uiLast], pvElement, sizeof(SocketFifoElement_t));
    }
}

static void socketfifo_Pop (void * const pvFifo, void * const pvElement) {
    if ((pvFifo != NULL) && (NULL != pvElement)) {
        unsigned int uiFirst = ((FIFO_t *)pvFifo)->uiFirst;
        memcpy (pvElement, &((SocketFifoElement_t *)((FIFO_t *)pvFifo)->pvElements)[uiFirst], sizeof(SocketFifoElement_t));
    }
}

