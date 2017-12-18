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
    
    uint32_t                status;

    uint32_t                ip;

    pin_obj_t *pins[4];

//    int8_t                  mode;
//    uint8_t                 auth;

    // my own ssid, key and mac
	//uint8_t                 ssid[(MODWLAN_SSID_LEN_MAX + 1)];
    //uint8_t                 key[65];
    //uint8_t                 mac[6];
	bool                    started;
	bool                    disconnected;
	uint32_t                ip4;
} lte_obj_t;


/******************************************************************************
 DECLARE PUBLIC DATA
 ******************************************************************************/
extern lte_obj_t lte_obj;

/******************************************************************************
 DECLARE PUBLIC FUNCTIONS
 ******************************************************************************/
extern void lte_pre_init (void);
extern void lte_setup ();
extern void lte_update(void);
extern void lte_get_ip (uint32_t *ip);
extern bool lte_is_connected (void);
extern void lte_off_on (void);
extern mp_obj_t lte_deinit(mp_obj_t self_in);

#endif /* MODLTE_H_ */