/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#ifndef FIFO_H_
#define FIFO_H_

typedef struct {
    void *pvElements;
    unsigned int uiElementCount;
    unsigned int uiElementsMax;
    unsigned int uiFirst;
    unsigned int uiLast;
    void (*pfElementPush)(void * const pvFifo, const void * const pvElement);
    void (*pfElementPop)(void * const pvFifo, void * const pvElement);
}FIFO_t;

extern void FIFO_Init (FIFO_t *fifo, unsigned int uiElementsMax,
void (*pfElmentPush)(void * const pvFifo, const void * const pvElement),
void (*pfElementPop)(void * const pvFifo, void * const pvElement));
extern bool FIFO_bPushElement (FIFO_t *fifo, const void * const pvElement);
extern bool FIFO_bPopElement (FIFO_t *fifo, void * const pvElement);
extern bool FIFO_bPeekElement (FIFO_t *fifo, void * const pvElement);
extern bool FIFO_IsEmpty (FIFO_t *fifo);
extern bool FIFO_IsFull (FIFO_t *fifo);
extern void FIFO_Flush (FIFO_t *fifo);

#endif /* FIFO_H_ */
