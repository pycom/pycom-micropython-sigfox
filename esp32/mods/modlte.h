/* 
* Copyright (c) 2017, Pycom Limited and its licensors.
*
* This software is licensed under the GNU GPL version 3 or any later version,
* with permitted additional terms. For more information see the Pycom Licence
* v1.0 document supplied with this file, or available at:
* https://www.pycom.io/opensource/licensing
*
* This file contains code under the following copyright and licensing notices.
* The code has been changed but otherwise retained.
*/

#ifndef MODLTE_H_
#define MODLTE_H_


#define LTE_MAX_RX_SIZE                1024

#define LTE_TRIGGER_NONE                0x00000000
#define LTE_TRIGGER_SIG_LOST            0x00000001

typedef struct _lte_obj_t {
    mp_obj_base_t           base;
    uint32_t                ip;
    uint32_t                ip4;
    uint8_t                 cid;
    bool                    init;
    bool                    carrier;
    uint32_t                trigger;
    int32_t                 events;
    mp_obj_t                handler;
    mp_obj_t                handler_arg;
} lte_obj_t;

typedef enum
{
    LTE_EVENT_COVERAGE_LOST = 0,
    LTE_EVENT_MAX
}lte_events_t;


/******************************************************************************
 DECLARE PUBLIC FUNCTIONS
 ******************************************************************************/
extern void modlte_init0(void);
extern void modlte_start_modem(void);
extern void modlte_urc_events(lte_events_t events);

#endif /* MODLTE_H_ */