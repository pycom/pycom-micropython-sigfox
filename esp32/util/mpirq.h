/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#ifndef MPIRQ_H_
#define MPIRQ_H_

#include "freertos/task.h"

/******************************************************************************
 DEFINE CONSTANTS
 ******************************************************************************/
#define INTERRUPTS_TASK_PRIORITY                   11
#define INTERRUPTS_TASK_STACK_SIZE                 (8 * 1024)
#define INTERRUPTS_TASK_STACK_LEN                  (INTERRUPTS_TASK_STACK_SIZE / sizeof(StackType_t))

#define INTERRUPTS_QUEUE_LEN                       (32)

#define INTERRUPT_OBJ_CLEAN(obj)                   {\
                                                       (obj)->handler = NULL; \
                                                       (obj)->handler_arg = NULL; \
                                                   }

/******************************************************************************
 DEFINE TYPES
 ******************************************************************************/
typedef struct {
    void (* handler)(void *);
    void *arg;
} mp_callback_obj_t;

/******************************************************************************
 DECLARE EXPORTED DATA
 ******************************************************************************/

extern const mp_obj_type_t mp_irq_type;

/******************************************************************************
 DECLARE PUBLIC FUNCTIONS
 ******************************************************************************/
void mp_irq_preinit(void);
void mp_irq_init0 (void);
void mp_irq_add (mp_obj_t parent, mp_obj_t handler);
void mp_irq_remove (mp_obj_t parent);
mp_obj_tuple_t *mp_irq_find (mp_obj_t parent);
void mp_irq_queue_interrupt(void (* handler)(void *), void *arg);
void mp_irq_queue_interrupt_non_ISR(void (* handler)(void *), void *arg);
void mp_irq_queue_interrupt_immediate_thread_delete(TaskHandle_t id);
void mp_irq_kill(void);
#endif /* MPIRQ_H_ */
