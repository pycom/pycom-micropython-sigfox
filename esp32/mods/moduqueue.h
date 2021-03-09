/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#ifndef MODUQUEUE_H_
#define MODUQUEUE_H_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

/******************************************************************************
 DEFINE TYPES
 ******************************************************************************/
typedef struct _mp_obj_queue_t {
    mp_obj_base_t base;
    mp_obj_t storage;
    StaticQueue_t *buffer;
    QueueHandle_t handle;
    uint32_t maxsize;
} mp_obj_queue_t;

#endif /* MODUQUEUE_H_ */
