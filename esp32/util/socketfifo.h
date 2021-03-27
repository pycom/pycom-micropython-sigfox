/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#ifndef SOCKETFIFO_H_
#define SOCKETFIFO_H_

/*----------------------------------------------------------------------------
 ** Imports
 */

/*----------------------------------------------------------------------------
 ** Define constants
 */

/*----------------------------------------------------------------------------
 ** Define types
 */

typedef struct {
    void                    *data;
    int32_t                 *sd;
    unsigned short          datasize;
    unsigned char           closesockets;
    bool                    freedata;

}SocketFifoElement_t;

/*----------------------------------------------------------------------------
 ** Declare public functions
 */
extern void SOCKETFIFO_Init (FIFO_t *fifo, void *elements, uint32_t maxcount);
extern bool SOCKETFIFO_Push (const void * const element);
extern bool SOCKETFIFO_Pop (void * const element);
extern bool SOCKETFIFO_Peek (void * const element);
extern bool SOCKETFIFO_IsEmpty (void);
extern bool SOCKETFIFO_IsFull (void);
extern void SOCKETFIFO_Flush (void);
extern unsigned int SOCKETFIFO_Count (void);

#endif /* SOCKETFIFO_H_ */
