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

typedef struct _lte_obj_t {
    mp_obj_base_t           base;
    uint32_t                ip;
    uint32_t                ip4;
} lte_obj_t;


/******************************************************************************
 DECLARE PUBLIC DATA
 ******************************************************************************/
extern lte_obj_t lte_obj;

/******************************************************************************
 DECLARE PUBLIC FUNCTIONS
 ******************************************************************************/
extern void modlte_init0(void);

#endif /* MODLTE_H_ */