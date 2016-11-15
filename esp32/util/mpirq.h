/*
 * Copyright (c) 2016, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#ifndef MPIRQ_H_
#define MPIRQ_H_

/******************************************************************************
 DEFINE CONSTANTS
 ******************************************************************************/
#define mp_irq_INIT_NUM_ARGS                    4

/******************************************************************************
 DEFINE TYPES
 ******************************************************************************/
typedef mp_obj_t (*mp_irq_init_t) (mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args);
typedef void (*mp_irq_void_method_t) (mp_obj_t self);
typedef int (*mp_irq_int_method_t)  (mp_obj_t self);

typedef struct {
    mp_irq_init_t init;
    mp_irq_void_method_t enable;
    mp_irq_void_method_t disable;
    mp_irq_int_method_t flags;
} mp_irq_methods_t;

typedef struct {
    mp_obj_base_t base;
    mp_obj_t parent;
    mp_obj_t handler;
    mp_irq_methods_t *methods;
    bool isenabled;
    bool pyhandler;
} mp_irq_obj_t;

/******************************************************************************
 DECLARE EXPORTED DATA
 ******************************************************************************/
extern const mp_arg_t mp_irq_init_args[];
extern const mp_obj_type_t mp_irq_type;

/******************************************************************************
 DECLARE PUBLIC FUNCTIONS
 ******************************************************************************/
void mp_irq_init0 (void);
mp_obj_t mp_irq_new (mp_obj_t parent, mp_obj_t handler, const mp_irq_methods_t *methods, bool pyhandler);
mp_irq_obj_t *mp_irq_find (mp_obj_t parent);
void mp_irq_wake_all (void);
void mp_irq_disable_all (void);
void mp_irq_remove (const mp_obj_t parent);
void mp_irq_handler (mp_obj_t self_in);
uint mp_irq_translate_priority (uint priority);

#endif /* MPIRQ_H_ */
