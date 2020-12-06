#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <timer.h>

#include "py/mpconfig.h"
#include "py/nlr.h"
#include "py/runtime.h"
#include "py/mperrno.h"
#include "util/mpirq.h"

#include "esp_system.h"
#include "machtimer.h"
#include "machtimer_alarm.h"

/******************************************************************************
 DEFINE PRIVATE CONSTANTS
 ******************************************************************************/
#define ALARM_HEAP_MAX_ELEMENTS                     (16U)

/******************************************************************************
 DEFINE PRIVATE TYPES
 ******************************************************************************/
typedef struct {
    mp_obj_base_t base;
    uint64_t when;
    uint64_t interval;
    uint32_t heap_index;
    mp_obj_t handler;
    mp_obj_t handler_arg;
    bool periodic;
} mp_obj_alarm_t;

struct {
    uint32_t count;
    mp_obj_alarm_t **data;
} alarm_heap;

/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/
IRAM_ATTR void timer_alarm_isr(void *arg);
STATIC void load_next_alarm(void);
STATIC mp_obj_t alarm_delete(mp_obj_t self_in);
STATIC void alarm_set_callback_helper(mp_obj_t self_in, mp_obj_t handler, mp_obj_t handler_arg);

/******************************************************************************
 DEFINE PUBLIC FUNCTIONS
 ******************************************************************************/
void mach_timer_alarm_preinit(void) {
    timer_isr_register(TIMER_GROUP_0, TIMER_0, timer_alarm_isr, NULL, 0, NULL);
}

void mach_timer_alarm_init_heap(void) {
    alarm_heap.count = 0;
    MP_STATE_PORT(mp_alarm_heap) = m_malloc(ALARM_HEAP_MAX_ELEMENTS * sizeof(mp_obj_alarm_t *));
    alarm_heap.data = MP_STATE_PORT(mp_alarm_heap);
    if (alarm_heap.data == NULL) {
        mp_printf(&mp_plat_print, "FATAL ERROR: not enough memory for the alarms heap\n");
        for (;;);
    }
}

// Insert a new alarm into the heap
// Note: It has already been checked that there is at least 1 free space on the heap
// Note: The heap will remain ordered after the operation.
// Note: If the new element is placed at the first place, which means its timestamp is the smallest, it is loaded immediatelly
STATIC IRAM_ATTR void insert_alarm(mp_obj_alarm_t *alarm) {
    uint32_t index = 0, index2;

    // the list is ordered at any given time. Find the place where the new element shall be inserted
    if(alarm_heap.count != 0) {
        for (; index < alarm_heap.count; ++index){
            if (alarm->when <= alarm_heap.data[index]->when) {
                break;
            }
        }

        // restructuring the list while keeping the order so the new element can be inserted at its correct place
        for(index2 = alarm_heap.count; index2 > index; index2--) {
            alarm_heap.data[index2] = alarm_heap.data[(index2 - 1)];
            alarm_heap.data[index2]->heap_index = index2;
        }
    }

    // insert the new element
    alarm_heap.data[index] = alarm;
    alarm->heap_index = index;
    alarm_heap.count++;

    // start the newly added alarm if it is the first in the list
    if (index == 0) {
        load_next_alarm();
    }
}


// Remove the alarm from the heap at the position alarm_heap_index
// Note: The heap will remain ordered after the operation.
STATIC IRAM_ATTR void remove_alarm(uint32_t alarm_heap_index) {
    uint32_t index;

    /* Need to disable the HW timer if the alarm is currently active, because it can happen
     * that during the removal process the HW timer expires and generates an interrupt.
     * In this case the ISR should not be performed at all because the user had requested
     * to cancel the alarm before it expired. */
    if(alarm_heap_index == 0) {
        TIMERG0.hw_timer[0].config.alarm_en = 0; // disable the alarm system
    }

    // invalidate the element
    alarm_heap.data[alarm_heap_index]->heap_index = -1;
    alarm_heap.data[alarm_heap_index] = NULL;
    alarm_heap.count--;

    for (index = alarm_heap_index; index < alarm_heap.count; ++index) {
        alarm_heap.data[index] = alarm_heap.data[index+1];
        alarm_heap.data[index]->heap_index = index;
        alarm_heap.data[index + 1] = NULL;
    }

    // If the removed element was currently scheduled, start the next one
    if(alarm_heap_index == 0) {
        load_next_alarm();
    }
}

STATIC IRAM_ATTR void load_next_alarm(void) {
    TIMERG0.hw_timer[0].config.alarm_en = 0; // disable the alarm system
    // everything here done without calling any timers function, so it works inside the interrupts
    if (alarm_heap.count > 0) {
        uint64_t when;
        when = alarm_heap.data[0]->when;
        TIMERG0.hw_timer[0].alarm_high = (uint32_t) (when >> 32);
        TIMERG0.hw_timer[0].alarm_low = (uint32_t) when;
        TIMERG0.hw_timer[0].config.alarm_en = 1; // enable the alarm system
    }
}

STATIC IRAM_ATTR void set_alarm_when(mp_obj_alarm_t *alarm, uint64_t delta) {
    TIMERG0.hw_timer[0].update = 1;
    alarm->when = ((uint64_t) TIMERG0.hw_timer[0].cnt_high << 32)
        | (TIMERG0.hw_timer[0].cnt_low);
    alarm->when += delta;
}

STATIC void alarm_handler(void *arg) {
    // this function will be called by the interrupt thread
    mp_obj_alarm_t *alarm = arg;

    if (alarm->handler && alarm->handler != mp_const_none) {
        mp_call_function_1(alarm->handler, alarm->handler_arg);
    }
    if (!alarm->periodic) {
        mp_irq_remove(alarm);
        INTERRUPT_OBJ_CLEAN(alarm);
    }
}

IRAM_ATTR void timer_alarm_isr(void *arg) {
    TIMERG0.int_clr_timers.t0 = 1; // acknowledge the interrupt

    // need to check whether all the alarms have been removed from the list
    // or not since the last time the HW timer was set up
    if (alarm_heap.count > 0) {
        mp_obj_alarm_t *alarm = alarm_heap.data[0];

        // This will automatically load the next alarm in the queue
        remove_alarm(0);

        if (alarm->periodic) {
            set_alarm_when(alarm, alarm->interval);
            // If this alarm is inserted back to the 0th place, load again
            insert_alarm(alarm);
        }

        mp_irq_queue_interrupt(alarm_handler, alarm);
    }
}

STATIC mp_obj_t alarm_make_new(const mp_obj_type_t *type, mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *all_args) {

    STATIC const mp_arg_t allowed_args[] = {
        { MP_QSTR_handler,      MP_ARG_OBJ  | MP_ARG_REQUIRED,   {.u_obj = mp_const_none} },
        { MP_QSTR_s,            MP_ARG_OBJ,                      {.u_obj = mp_const_none} },
        { MP_QSTR_ms,           MP_ARG_INT  | MP_ARG_KW_ONLY,    {.u_int = 0} },
        { MP_QSTR_us,           MP_ARG_INT  | MP_ARG_KW_ONLY,    {.u_int = 0} },
        { MP_QSTR_arg,          MP_ARG_OBJ  | MP_ARG_KW_ONLY,    {.u_obj = mp_const_none} },
        { MP_QSTR_periodic,     MP_ARG_BOOL | MP_ARG_KW_ONLY,    {.u_bool = false} },
    };

    // parse arguments
    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, all_args + n_args);
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, all_args, &kw_args, MP_ARRAY_SIZE(args), allowed_args, args);

    float s = 0.0f;
    if (args[1].u_obj != mp_const_none) {
        s = mp_obj_get_float(args[1].u_obj);
    }
    uint32_t ms = args[2].u_int;
    uint32_t us = args[3].u_int;

    if (((s != 0.0) + (ms != 0) + (us != 0)) != 1) {
        mp_raise_ValueError("please provide a single duration");
    }

    if (s < 0.0 || ms < 0 || us < 0) {
        mp_raise_ValueError("please provide a positive number");
    }

    uint64_t clocks = (uint64_t) (s * CLK_FREQ + 0.5) + ms * (CLK_FREQ / 1000) + us * (CLK_FREQ / 1000000);

    mp_obj_alarm_t *self = m_new_obj_with_finaliser(mp_obj_alarm_t);

    self->base.type = type;
    self->interval = clocks;
    self->periodic = args[5].u_bool;

    self->heap_index = -1;
    alarm_set_callback_helper(self, args[0].u_obj, args[4].u_obj);
    return self;
}

STATIC void alarm_set_callback_helper(mp_obj_t self_in, mp_obj_t handler, mp_obj_t handler_arg) {
    bool error = false;
    mp_obj_alarm_t *self = self_in;

    // do as much as possible outside the atomic section
    // handler is given by the user for sure
    self->handler = handler;

    if (handler_arg == mp_const_none) {
        handler_arg = self_in;
    }
    self->handler_arg = handler_arg;

    // Both remove_alarm and insert_alarm need to be guarded for the following reasons:
    // remove_alarm: If the ISR removes the 0th alarm then then the whole heap is restructured the indexes are changed,
    //               not the correct alarm would be deleted.
    // insert_alarm: If the ISR removes (re-adds) the 0th alarm then then the whole heap is restructured the indexes are changed,
    //               the current alarm may not be added to the correct position.
    //

    uint32_t state = MICROPY_BEGIN_ATOMIC_SECTION();
    // Check whether this alarm is currently active so should be removed
    if (self->heap_index != -1) {
        remove_alarm(self->heap_index);
        mp_irq_remove(self);
        INTERRUPT_OBJ_CLEAN(self);
    }

    if (alarm_heap.count == ALARM_HEAP_MAX_ELEMENTS) {
        error = true;
    } else if (self->handler != mp_const_none) {
        mp_irq_add(self, handler);
        set_alarm_when(self, self->interval);
        insert_alarm(self);
    }

    MICROPY_END_ATOMIC_SECTION(state);

    if (error) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_MemoryError, "maximum number of %d alarms already reached", ALARM_HEAP_MAX_ELEMENTS));
    }
}

STATIC mp_obj_t alarm_callback(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_handler,  MP_ARG_OBJ | MP_ARG_REQUIRED, {.u_obj = mp_const_none} },
        { MP_QSTR_arg,      MP_ARG_OBJ | MP_ARG_KW_ONLY,  {.u_obj = mp_const_none} },
    };

    mp_obj_t *self = MP_OBJ_TO_PTR(pos_args[0]);
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    alarm_set_callback_helper(self, args[0].u_obj, args[1].u_obj);

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_KW(alarm_callback_obj, 1, alarm_callback);

STATIC mp_obj_t alarm_delete(mp_obj_t self_in) {
    mp_obj_alarm_t *self = self_in;

    // Atomic section is placed here due to the following reasons:
    // 1. If user calls "cancel" function it might be interrupted by the ISR when reordering the list because
    //    in this case the alarm is an active alarm which means it is placed on the alarm_heap and its heap_index != -1.
    // 2. When GC calls this function it is 100% percent sure that the heap_index is -1, because
    //    GC will only collect this object if it is not referred from the alarm_heap, which means it is not active thus
    //    its heap_index == -1.
    uint32_t state = MICROPY_BEGIN_ATOMIC_SECTION();

    if (self->heap_index != -1) {
        remove_alarm(self->heap_index);
    }
    mp_irq_remove(self);
    INTERRUPT_OBJ_CLEAN(self);
    MICROPY_END_ATOMIC_SECTION(state);

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(alarm_delete_obj, alarm_delete);


STATIC const mp_map_elem_t mach_timer_alarm_dict_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR___name__),            MP_OBJ_NEW_QSTR(MP_QSTR_alarm) },
    { MP_OBJ_NEW_QSTR(MP_QSTR___del__),             (mp_obj_t) &alarm_delete_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_callback),            (mp_obj_t) &alarm_callback_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_cancel),              (mp_obj_t) &alarm_delete_obj },
};

STATIC MP_DEFINE_CONST_DICT(mach_timer_alarm_dict, mach_timer_alarm_dict_table);

const mp_obj_type_t mach_timer_alarm_type = {
    { &mp_type_type },
    .name = MP_QSTR_Alarm,
    .make_new = alarm_make_new,
    .locals_dict = (mp_obj_t)&mach_timer_alarm_dict,
};
