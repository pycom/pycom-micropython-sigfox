/*
 * Copyright (c) 2016, Pycom Limited.
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
#include "py/stackctrl.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#if MICROPY_PY_THREAD

/******************************************************************************
 DECLARE PUBLIC DATA
 ******************************************************************************/
QueueHandle_t interruptsQueue;

bool mp_irq_is_alive;

/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/
static void *TASK_Interrupts(void *pvParameters) {
    mp_callback_obj_t cb;
    mp_state_thread_t ts;
    mp_thread_set_state(&ts);

    mp_stack_set_top(&ts + 1); // need to include ts in root-pointer scan
    mp_stack_set_limit(INTERRUPTS_TASK_STACK_SIZE);

    // signal that we are set up and running
    mp_thread_start();

    for (;;) {
        xQueueReceive(interruptsQueue, &cb, portMAX_DELAY);

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
            // uncaught exception
            // check for SystemExit
            mp_obj_base_t *exc = (mp_obj_base_t*)nlr.ret_val;
            if (mp_obj_is_subclass_fast(MP_OBJ_FROM_PTR(exc->type), MP_OBJ_FROM_PTR(&mp_type_SystemExit))) {
                // swallow exception silently
            } else {
                // print exception out
                mp_printf(&mp_plat_print, "Unhandled exception in interrupt handler\n");
                mp_obj_print_exception(&mp_plat_print, MP_OBJ_FROM_PTR(exc));
            }
        }
        MP_THREAD_GIL_EXIT();
    }

    mp_irq_is_alive = false;

    return NULL;
}

/******************************************************************************
 DEFINE PUBLIC FUNCTIONS
 ******************************************************************************/
void mp_irq_preinit(void) {
    interruptsQueue = xQueueCreate(INTERRUPTS_QUEUE_LEN, sizeof(mp_callback_obj_t));
}

void mp_irq_init0(void) {
    uint32_t stack_size = INTERRUPTS_TASK_STACK_SIZE;
    mp_irq_is_alive = true;
    xQueueReset(interruptsQueue);
    mp_thread_create_ex(TASK_Interrupts, NULL, &stack_size, INTERRUPTS_TASK_PRIORITY, "Interrupts");
}

void IRAM_ATTR mp_irq_queue_interrupt(void (* handler)(void *), void *arg) {
    mp_callback_obj_t cb = {.handler = handler, .arg = arg};
    xQueueSendFromISR(interruptsQueue, &cb, NULL);
}

void mp_irq_kill(void) {
    // sending a NULL handler will kill the interrupt task
    mp_irq_queue_interrupt(NULL, NULL);
    MP_THREAD_GIL_EXIT();       // release the GIL if we have it
    do {
        // it needs to be this one in order to not mess with the GIL
        vTaskDelay(3 / portTICK_PERIOD_MS);
    } while (mp_irq_is_alive);
    xQueueReset(interruptsQueue);
    // TODO disable all interrupts here at hardware level
}

#else

void IRAM_ATTR mp_irq_queue_interrupt(void (* handler)(void *), void *arg) {

}

#endif  // MICROPY_PY_THREAD
