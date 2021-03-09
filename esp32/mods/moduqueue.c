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
#include "py/objstr.h"
#include "py/runtime.h"
#include "mpexception.h"
#include "moduqueue.h"

/******************************************************************************
 DEFINE CONSTANTS
 ******************************************************************************/

/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/
STATIC const mp_obj_type_t mp_queue_type;

/******************************************************************************/
// Micro Python bindings; Queue class

STATIC mp_obj_t mod_uqueue_queue(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    STATIC const mp_arg_t allowed_args[] = {
        { MP_QSTR_maxsize,                      MP_ARG_INT, {.u_int = 1} },
    };

    // parse arguments
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    if (args[0].u_int <= 0) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "queue size cannot be infinite"));
    }

    int32_t maxsize = args[0].u_int;

    // create the queue object
    mp_obj_queue_t *queue = m_new_obj_with_finaliser(mp_obj_queue_t);

    // allocate the queue storage and the queue buffer
    queue->buffer = malloc(sizeof(StaticQueue_t));
    if (NULL == queue->buffer) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_MemoryError, "no memory available to create the queue"));
    }
    queue->storage = m_new(mp_obj_t, maxsize);
    queue->base.type = &mp_queue_type;
    queue->maxsize = maxsize;
    queue->handle = xQueueCreateStatic(maxsize, sizeof(mp_obj_t), (uint8_t *)queue->storage, queue->buffer);

    return queue;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mod_uqueue_queue_obj, 0, mod_uqueue_queue);

STATIC const mp_map_elem_t mp_module_uqueue_globals_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR___name__),            MP_OBJ_NEW_QSTR(MP_QSTR_queue) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_Queue),               (mp_obj_t)&mod_uqueue_queue_obj },
};

STATIC MP_DEFINE_CONST_DICT(mp_module_uqueue_globals, mp_module_uqueue_globals_table);

const mp_obj_module_t mp_module_uqueue = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&mp_module_uqueue_globals,
};

STATIC mp_obj_t mp_queue_delete(mp_obj_t self_in) {
    mp_obj_queue_t *self = self_in;
    if (self->buffer) {
        free(self->buffer);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mp_queue_delete_obj, mp_queue_delete);

STATIC mp_obj_t mp_queue_put(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_item,       MP_ARG_REQUIRED | MP_ARG_OBJ, },
        { MP_QSTR_block,                        MP_ARG_BOOL, {.u_bool = true} },
        { MP_QSTR_timeout,                      MP_ARG_OBJ,  {.u_obj = mp_const_none} },
    };

    // parse args
    mp_obj_queue_t *self = pos_args[0];
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), allowed_args, args);

    uint32_t timeout_ms = (args[2].u_obj == mp_const_none) ? portMAX_DELAY : mp_obj_get_int_truncated(args[2].u_obj);
    MP_THREAD_GIL_EXIT();
    if (!xQueueSend(self->handle, (void *)&args[0].u_obj, args[1].u_bool ? (TickType_t)(timeout_ms / portTICK_PERIOD_MS) : 0)) {
        MP_THREAD_GIL_ENTER();
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "Full"));
    }
    MP_THREAD_GIL_ENTER();

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mp_queue_put_obj, 1, mp_queue_put);

STATIC mp_obj_t mp_queue_get(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_block,                        MP_ARG_BOOL, {.u_bool = true} },
        { MP_QSTR_timeout,                      MP_ARG_OBJ,  {.u_obj = mp_const_none} },
    };

    // parse args
    mp_obj_queue_t *self = pos_args[0];
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), allowed_args, args);

    uint32_t timeout_ms = (args[1].u_obj == mp_const_none) ? portMAX_DELAY : mp_obj_get_int_truncated(args[1].u_obj);
    mp_obj_t item;
    MP_THREAD_GIL_EXIT();
    if (!xQueueReceive(self->handle, (void *)&item, args[0].u_bool ? (TickType_t)(timeout_ms / portTICK_PERIOD_MS) : 0)) {
        MP_THREAD_GIL_ENTER();
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, "Empty"));
    }
    MP_THREAD_GIL_ENTER();

    return item;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mp_queue_get_obj, 1, mp_queue_get);

STATIC mp_obj_t mp_queue_empty(mp_obj_t self_in) {
    mp_obj_queue_t *self = self_in;
    mp_obj_t buffer;
    if (xQueuePeek(self->handle, &buffer, 0)) {
        return mp_const_false;
    }
    return mp_const_true;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mp_queue_empty_obj, mp_queue_empty);

STATIC mp_obj_t mp_queue_full(mp_obj_t self_in) {
    mp_obj_queue_t *self = self_in;
    if (uxQueueSpacesAvailable(self->handle) > 0) {
        return mp_const_false;
    }
    return mp_const_true;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mp_queue_full_obj, mp_queue_full);

STATIC const mp_map_elem_t queue_locals_dict_table[] = {
    // instance methods
    { MP_OBJ_NEW_QSTR(MP_QSTR___del__),                 (mp_obj_t)&mp_queue_delete_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_put),                     (mp_obj_t)&mp_queue_put_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_get),                     (mp_obj_t)&mp_queue_get_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_empty),                   (mp_obj_t)&mp_queue_empty_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_full),                    (mp_obj_t)&mp_queue_full_obj },

    { MP_OBJ_NEW_QSTR(MP_QSTR_Full),                    (mp_obj_t)&mp_type_OSError },
    { MP_OBJ_NEW_QSTR(MP_QSTR_Empty),                   (mp_obj_t)&mp_type_OSError },
};

STATIC MP_DEFINE_CONST_DICT(queue_locals_dict, queue_locals_dict_table);

STATIC const mp_obj_type_t mp_queue_type = {
    { &mp_type_type },
    .name = MP_QSTR_Queue,
    .locals_dict = (mp_obj_t)&queue_locals_dict,
};
