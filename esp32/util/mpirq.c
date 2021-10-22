/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include "py/mpconfig.h"
#include "py/obj.h"
#include "py/runtime.h"

#include "mpexception.h"
#include "mperror.h"
#include "mpirq.h"
#include "mpthreadport.h"
#include "py/stackctrl.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_attr.h"

#if MICROPY_PY_THREAD

typedef struct {
    mp_obj_dict_t *dict_locals;
    mp_obj_dict_t *dict_globals;
} mpirq_args_t;

/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/
STATIC QueueHandle_t InterruptsQueue;
STATIC bool mp_irq_is_alive;

STATIC mpirq_args_t mpirq_args;

/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/
static void *TASK_Interrupts(void *pvParameters) {
    mpirq_args_t *args = (mpirq_args_t *)pvParameters;

    mp_callback_obj_t cb;
    mp_state_thread_t ts;
    mp_thread_set_state(&ts);

    mp_stack_set_top(&ts + 1); // need to include ts in root-pointer scan
    mp_stack_set_limit(INTERRUPTS_TASK_STACK_SIZE - 1024);

    mp_locals_set(args->dict_locals);
    mp_globals_set(args->dict_globals);

    MP_THREAD_GIL_ENTER();
    // signal that we are up and running
    mp_thread_start();
    MP_THREAD_GIL_EXIT();

    for (;;) {
        xQueueReceive(InterruptsQueue, &cb, portMAX_DELAY);

        // a NULL handler means that we need to exit the loop
        if (NULL == cb.handler) {
            break;
        }

        MP_THREAD_GIL_ENTER();

        nlr_buf_t nlr;
        if (nlr_push(&nlr) == 0) {
            cb.handler(cb.arg);
            nlr_pop();
        } else {
            // uncaught exception, check for SystemExit
            mp_obj_base_t *exc = (mp_obj_base_t*)nlr.ret_val;
            if (mp_obj_is_subclass_fast(MP_OBJ_FROM_PTR(exc->type), MP_OBJ_FROM_PTR(&mp_type_SystemExit))) {
                // swallow the exception silently
            } else {
                // print the exception out
                mp_printf(&mp_plat_print, "Unhandled exception in callback handler\n");
                mp_obj_print_exception(&mp_plat_print, MP_OBJ_FROM_PTR(exc));
                // kill this thread
                MP_THREAD_GIL_EXIT();
                break;
            }
        }
        MP_THREAD_GIL_EXIT();
    }

    MP_THREAD_GIL_ENTER();
    // signal that we are finished
    mp_thread_finish();
    MP_THREAD_GIL_EXIT();

    mp_irq_is_alive = false;

    return NULL;
}

/******************************************************************************
 DEFINE PUBLIC FUNCTIONS
 ******************************************************************************/
void mp_irq_preinit(void) {
    InterruptsQueue = xQueueCreate(INTERRUPTS_QUEUE_LEN, sizeof(mp_callback_obj_t));
}

void mp_irq_init0(void) {
    uint32_t stack_size = INTERRUPTS_TASK_STACK_SIZE;

    // initialize the callback objects list
    mp_obj_list_init(&MP_STATE_PORT(mp_irq_obj_list), 0);

    mp_irq_is_alive = true;
    xQueueReset(InterruptsQueue);

    mpirq_args.dict_locals = mp_locals_get();
    mpirq_args.dict_globals = mp_globals_get();

    mp_thread_create_ex(TASK_Interrupts, &mpirq_args, &stack_size, INTERRUPTS_TASK_PRIORITY, "IRQs");
}

void mp_irq_add (mp_obj_t parent, mp_obj_t handler) {
    mp_obj_tuple_t *irq = mp_obj_new_tuple(2, NULL);
    irq->items[0] = parent;
    irq->items[1] = handler;
    // remove it in case it was already registered
    mp_irq_remove(parent);
    mp_obj_list_append(&MP_STATE_PORT(mp_irq_obj_list), irq);
}

void mp_irq_remove (const mp_obj_t parent) {
    mp_obj_tuple_t *irq;
    if ((irq = mp_irq_find(parent))) {
        mp_obj_list_remove(&MP_STATE_PORT(mp_irq_obj_list), irq);
    }
}

mp_obj_tuple_t *mp_irq_find (mp_obj_t parent) {
    for (mp_uint_t i = 0; i < MP_STATE_PORT(mp_irq_obj_list).len; i++) {
        mp_obj_tuple_t *irq = ((mp_obj_tuple_t *)(MP_STATE_PORT(mp_irq_obj_list).items[i]));
        if (irq->items[0] == parent) {
            return irq;
        }
    }
    return NULL;
}

void IRAM_ATTR mp_irq_queue_interrupt(void (* handler)(void *), void *arg) {
    mp_callback_obj_t cb = {.handler = handler, .arg = arg};

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    xQueueSendFromISR(InterruptsQueue, &cb, &xHigherPriorityTaskWoken);

    if( xHigherPriorityTaskWoken)
    {
        portYIELD_FROM_ISR();
    }
}

void mp_irq_queue_interrupt_non_ISR(void (* handler)(void *), void *arg) {
    mp_callback_obj_t cb = {.handler = handler, .arg = arg};
    (void)xQueueSend(InterruptsQueue, &cb, 0);
}

void IRAM_ATTR mp_irq_queue_interrupt_immediate_thread_delete(TaskHandle_t id) {

    // Check if IRQ task is not being shutdown
    if(mp_irq_is_alive == true){
        mp_callback_obj_t cb = {.handler = vTaskDelete, .arg = id};
        (void)xQueueSend(InterruptsQueue, &cb, 0);
    }
}

void mp_irq_kill(void) {
    // sending a NULL handler will kill the interrupt task
    mp_irq_queue_interrupt(NULL, NULL);
    // release the GIL if we have it
    MP_THREAD_GIL_EXIT();
    do {
        // it needs to be this one in order to not mess with the GIL
        vTaskDelay(3 / portTICK_PERIOD_MS);
    } while (mp_irq_is_alive);
    xQueueReset(InterruptsQueue);
    // TODO disable all interrupts here at hardware level
}

#else

void IRAM_ATTR mp_irq_queue_interrupt(void (* handler)(void *), void *arg) {

}

#endif  // MICROPY_PY_THREAD
