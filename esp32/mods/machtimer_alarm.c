#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <timer.h>

#include "py/mpconfig.h"
#include "py/nlr.h"
#include "py/runtime.h"
#include "py/gc.h"
#include "py/mperrno.h"
#include "util/mpirq.h"

#include "esp_system.h"

#define CMP(a, b) ((a)->when <= (b)->when)
#define MIN_HEAP_ELEMENTS (4)

#define CLK_FREQ (APB_CLK_FREQ / 2)

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
    uint32_t size;
    uint32_t count;
    mp_obj_alarm_t **data;
} alarm_heap;

mp_obj_alarm_t *alarms[MIN_HEAP_ELEMENTS];

IRAM_ATTR void timer_alarm_isr(void *arg);
STATIC void load_next_alarm(void);
STATIC mp_obj_t alarm_delete(mp_obj_t self_in);
STATIC void alarm_set_callback_helper(mp_obj_t self_in, mp_obj_t handler, mp_obj_t handler_arg);


void init_alarm_heap(void) {
    alarm_heap.size = MIN_HEAP_ELEMENTS;
    alarm_heap.count = 0;
    alarm_heap.data = gc_alloc(MIN_HEAP_ELEMENTS, false);
    MP_STATE_PORT(mp_alarm_heap) = alarm_heap.data;
    if (alarm_heap.data == NULL) {
        printf("ERROR: no enough memory for the alarms heap\n");
        for (;;);
    }
    timer_enable_intr(TIMER_GROUP_0, TIMER_0);
    timer_isr_register(TIMER_GROUP_0, TIMER_0, timer_alarm_isr, NULL, ESP_INTR_FLAG_IRAM, NULL);
}

STATIC IRAM_ATTR void insert_alarm(mp_obj_alarm_t *alarm) {
    uint32_t index, parent;

    if (alarm_heap.count == alarm_heap.size) {
        // no need to panic, interrupt doesn't alter the heap size when reinserting a periodic alarm
        // so this is not going to be called from within the interrupt for a periodic alarm reinsertion
        alarm_heap.size <<= 1;


        void *new_data;
        new_data = gc_realloc(alarm_heap.data, alarm_heap.size * sizeof(mp_obj_alarm_t *), true);
        if (!new_data) {
            mp_raise_OSError(MP_ENOMEM);
        }
        MP_STATE_PORT(mp_alarm_heap) = alarm_heap.data;
        alarm_heap.data = new_data;
    }

    // Find out where to put the element and put it
    for (index = alarm_heap.count++; index; index = parent) {
        parent = (index - 1) >> 1;
        if (CMP(alarm_heap.data[parent], alarm)) break;
        alarm_heap.data[index] = alarm_heap.data[parent];
        alarm_heap.data[index]->heap_index = index;
    }
    alarm_heap.data[index] = alarm;

    alarm->heap_index = index;
    if (index == 0) load_next_alarm();
}

// remove top alarm from the heap
STATIC IRAM_ATTR void remove_alarm(uint32_t el) {
    uint32_t index, swap, other;

    // mark as disabled
    alarm_heap.data[el]->heap_index = -1;

    // Remove the biggest element
    mp_obj_alarm_t *temp = alarm_heap.data[--alarm_heap.count];

    // Reorder the elements
    for (index = el; 1; index = swap) {
        // Find the child to swap with
        swap = (index << 1) + 1;
        if (swap >= alarm_heap.count) break; // If there are no children, the heap is reordered
        other = swap + 1;
        if ((other < alarm_heap.count) && CMP(alarm_heap.data[other], alarm_heap.data[swap])) swap = other;
        if (CMP(temp, alarm_heap.data[swap])) break; // If the bigger child is bigger than or equal to its parent, the heap is reordered

        alarm_heap.data[index] = alarm_heap.data[swap];
        alarm_heap.data[index]->heap_index = index;
    }
    alarm_heap.data[index] = temp;
    if (alarm_heap.count) {
        alarm_heap.data[index]->heap_index = index;
    }
    if (el == 0) load_next_alarm();
}

STATIC IRAM_ATTR void load_next_alarm(void) {
    // everything here done without calling any timers function, so it works inside the interrupts
    if (alarm_heap.count != 0) {
        uint64_t when;
        when = alarm_heap.data[0]->when;
        TIMERG0.hw_timer[0].alarm_high = (uint32_t) (when >> 32);
        TIMERG0.hw_timer[0].alarm_low = (uint32_t) when;
        TIMERG0.hw_timer[0].config.alarm_en = 1; // enable the alarm system
    } else {
        TIMERG0.hw_timer[0].config.alarm_en = 0; // disable the alarm system
    }
}

STATIC IRAM_ATTR void set_alarm_when(mp_obj_alarm_t *alarm, uint64_t delta) {
    TIMERG0.hw_timer[0].update = 1;
    alarm->when = ((uint64_t) TIMERG0.hw_timer[0].cnt_high << 32)
        | (TIMERG0.hw_timer[0].cnt_low);
    alarm->when += delta;
}

STATIC void tidy_alarm_memory(void) {
    // Resize the heap if it's consuming too much memory
    if ((alarm_heap.count <= (alarm_heap.size >> 2)) && (alarm_heap.size > MIN_HEAP_ELEMENTS)) {
        alarm_heap.size >>= 1;

        void *new_data;
        new_data = gc_realloc(alarm_heap.data, sizeof(mp_obj_alarm_t *) * alarm_heap.size, true);
        if (!new_data) {
            mp_raise_OSError(MP_ENOMEM);
        }
        MP_STATE_PORT(mp_alarm_heap) = alarm_heap.data;
        alarm_heap.data = new_data;
    }
}

STATIC void alarm_handler(void *arg) {
    // this function will be called by the interrupt thread
    mp_obj_alarm_t *alarm = arg;

    tidy_alarm_memory();
    if (alarm->handler != mp_const_none) {
        mp_call_function_1(alarm->handler, alarm->handler_arg);
    }
}

IRAM_ATTR void timer_alarm_isr(void *arg) {
    TIMERG0.int_clr_timers.t0 = 1; // acknowledge the interrupt

    mp_obj_alarm_t *alarm;
    alarm = alarm_heap.data[0];

    remove_alarm(0);

    if (alarm->periodic) {
        set_alarm_when(alarm, alarm->interval);
        insert_alarm(alarm);
    }

    mp_irq_queue_interrupt(alarm_handler, alarm);
}

STATIC mp_obj_t alarm_make_new(const mp_obj_type_t *type, mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *all_args) {

    STATIC const mp_arg_t allowed_args[] = {
        { MP_QSTR_handler,      MP_ARG_OBJ,                     {.u_obj = mp_const_none} },
        { MP_QSTR_s,            MP_ARG_OBJ,                     {.u_obj = mp_const_none} },
        { MP_QSTR_ms,           MP_ARG_INT | MP_ARG_KW_ONLY,    {.u_int = 0} },
        { MP_QSTR_us,           MP_ARG_INT | MP_ARG_KW_ONLY,    {.u_int = 0} },
        { MP_QSTR_arg,          MP_ARG_OBJ | MP_ARG_KW_ONLY,    {.u_obj = mp_const_none} },
        { MP_QSTR_periodic,     MP_ARG_BOOL | MP_ARG_KW_ONLY,   {.u_bool = false} },
    };

    // parse arguments
    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, all_args + n_args);
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, all_args, &kw_args, MP_ARRAY_SIZE(args), allowed_args, args);

    float s = 0.0;
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
    mp_obj_alarm_t *self = self_in;

    self->handler = handler;
    if (handler == mp_const_none) {
        alarm_delete(self_in);
        return;
    }

    if (self->heap_index == -1) {
        set_alarm_when(self, self->interval);
        insert_alarm(self);
    }

    if (handler_arg == mp_const_none) {
        handler_arg = self_in;
    }

    self->handler_arg = handler_arg;
}

STATIC mp_obj_t alarm_callback(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_handler,  MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_arg,      MP_ARG_OBJ, {.u_obj = mp_const_none} },
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

    if (self->heap_index != -1) {
        remove_alarm(self->heap_index);
        tidy_alarm_memory();
    }
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(alarm_delete_obj, alarm_delete);

STATIC const mp_map_elem_t mach_timer_alarm_dict_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR___name__),            MP_OBJ_NEW_QSTR(MP_QSTR_alarm) },
    { MP_OBJ_NEW_QSTR(MP_QSTR___del__),             (mp_obj_t) &alarm_delete_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_callback),            (mp_obj_t) &alarm_callback_obj },
};

STATIC MP_DEFINE_CONST_DICT(mach_timer_alarm_dict, mach_timer_alarm_dict_table);

const mp_obj_type_t mach_timer_alarm_type = {
   { &mp_type_type },
   .name = MP_QSTR_Alarm,
   .make_new = alarm_make_new,
   .locals_dict = (mp_obj_t)&mach_timer_alarm_dict,
};
