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

#include "fifo.h"


/******************************************************************************
 DEFINE PUBLIC FUNCTIONS
 ******************************************************************************/
void FIFO_Init (FIFO_t *fifo, unsigned int uiElementsMax,
                void (*pfElmentPush)(void * const pvFifo, const void * const pvElement),
                void (*pfElementPop)(void * const pvFifo, void * const pvElement)) {
    if (fifo) {
        fifo->uiFirst = 0;
        fifo->uiLast = uiElementsMax - 1;
        fifo->uiElementCount = 0;
        fifo->uiElementsMax = uiElementsMax;
        fifo->pfElementPush = pfElmentPush;
        fifo->pfElementPop = pfElementPop;
    }
}

bool FIFO_bPushElement (FIFO_t *fifo, const void * const pvElement) {
    if (!fifo) {
        return false;
    }
    // Check if the queue is full
    if (true == FIFO_IsFull (fifo)) {
        return false;
    }

    // Increment the element count
    if (fifo->uiElementsMax > fifo->uiElementCount) {
        fifo->uiElementCount++;
    }
    fifo->uiLast++;
    if (fifo->uiLast == fifo->uiElementsMax) {
        fifo->uiLast = 0;
    }
    // Insert the element into the queue
    fifo->pfElementPush(fifo, pvElement);
    return true;
}

bool FIFO_bPopElement (FIFO_t *fifo, void * const pvElement) {
    if (!fifo) {
        return false;
    }
    // Check if the queue is empty
    if (true == FIFO_IsEmpty (fifo)) {
        return false;
    }

    // Get the element from the queue
    fifo->pfElementPop(fifo, pvElement);
    // Decrement the element count
    if (fifo->uiElementCount > 0) {
        fifo->uiElementCount--;
    }
    fifo->uiFirst++;
    if (fifo->uiFirst == fifo->uiElementsMax) {
        fifo->uiFirst = 0;
    }
    return true;
}

bool FIFO_bPeekElement (FIFO_t *fifo, void * const pvElement) {
    if (!fifo) {
        return false;
    }
    // Check if the queue is empty
    if (true == FIFO_IsEmpty (fifo)) {
        return false;
    }
    // Get the element from the queue
    fifo->pfElementPop(fifo, pvElement);
    return true;
}

bool FIFO_IsEmpty (FIFO_t *fifo) {
    if (fifo) {
        return ((fifo->uiElementCount == 0) ? true : false);
    }
    return false;
}

bool FIFO_IsFull (FIFO_t *fifo) {
    if (fifo) {
        return ((fifo->uiElementCount < fifo->uiElementsMax) ? false : true);
    }
    return false;
}

void FIFO_Flush (FIFO_t *fifo) {
    if (fifo) {
        fifo->uiElementCount = 0;
        fifo->uiFirst = 0;
        fifo->uiLast = fifo->uiElementsMax - 1;
    }
}
