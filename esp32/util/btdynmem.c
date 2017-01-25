/*
 * Copyright (c) 2016-2017, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */


/* this file's functions dynamically allocate and deallocate memory for the bluetooth structures */

#include <stdio.h>

#include "btdynmem.h"

#if BTM_DYNAMIC_MEMORY == TRUE
#include "btm_int.h"
#endif

#if GATT_DYNAMIC_MEMORY == TRUE
#include "gatt_int.h"
#endif

#if L2C_DYNAMIC_MEMORY == TRUE
#include "l2c_int.h"
#endif

#if BTA_DYNAMIC_MEMORY == TRUE
#include "bta_sys.h"
#include "bta_sys_int.h"
#include "bta_dm_int.h"
#include "bta_gattc_int.h"
#include "bta_gatts_int.h"
#include "bta_hh_int.h"
#endif

#if GAP_DYNAMIC_MEMORY == TRUE
#include "gap_int.h"
#endif

#if BTM_DYNAMIC_MEMORY == TRUE
tBTM_CB *btm_cb_ptr;
#endif

#if GATT_DYNAMIC_MEMORY == TRUE
tGATT_CB *gatt_cb_ptr;
#endif

#if L2C_DYNAMIC_MEMORY == TRUE
tL2C_CB *l2c_cb_ptr;
#endif

#if BTA_DYNAMIC_MEMORY == TRUE
tBTA_SYS_CB *bta_sys_cb_ptr;
tBTA_DM_CB *bta_dm_cb_ptr;
tBTA_DM_SEARCH_CB *bta_dm_search_cb_ptr;
tBTA_DM_DI_CB *bta_dm_di_cb_ptr;
tBTA_GATTC_CB *bta_gattc_cb_ptr;
tBTA_GATTS_CB *bta_gatts_cb_ptr;
tBTA_HH_CB *bta_hh_cb_ptr;
#endif

#if GAP_DYNAMIC_MEMORY == TRUE
tGAP_CB *gap_cb_ptr;
#endif

static void secure_free(void **p) {
    if (*p != NULL) {
        vPortFree(*p);
    }

    *p = NULL;
}

int bluetooth_alloc_memory(void) {
    #if BTM_DYNAMIC_MEMORY == TRUE
        btm_cb_ptr = pvPortMalloc(sizeof(tBTM_CB));
        if (btm_cb_ptr == NULL) {
            bluetooth_free_memory();
            return -1;
        }
    #endif

    #if GATT_DYNAMIC_MEMORY == TRUE
        gatt_cb_ptr = pvPortMalloc(sizeof(tGATT_CB));
        if (gatt_cb_ptr == NULL) {
            bluetooth_free_memory();
            return -1;
        }
    #endif

    #if L2C_DYNAMIC_MEMORY == TRUE
        l2c_cb_ptr = pvPortMalloc(sizeof(tL2C_CB));
        if (l2c_cb_ptr == NULL) {
            bluetooth_free_memory();
            return -1;
        }
    #endif

    #if BTA_DYNAMIC_MEMORY == TRUE
        bta_sys_cb_ptr = pvPortMalloc(sizeof(tBTA_SYS_CB));
        bta_dm_cb_ptr = pvPortMalloc(sizeof(tBTA_DM_CB));
        bta_dm_search_cb_ptr = pvPortMalloc(sizeof(tBTA_DM_SEARCH_CB));
        bta_dm_di_cb_ptr = pvPortMalloc(sizeof(tBTA_DM_DI_CB));
        bta_gattc_cb_ptr = pvPortMalloc(sizeof(tBTA_GATTC_CB));
        bta_gatts_cb_ptr = pvPortMalloc(sizeof(tBTA_GATTS_CB));
        bta_hh_cb_ptr = pvPortMalloc(sizeof(tBTA_HH_CB));
        if (bta_sys_cb_ptr == NULL ||
            bta_dm_cb_ptr == NULL ||
            bta_dm_search_cb_ptr == NULL ||
            bta_dm_di_cb_ptr == NULL ||
            bta_gattc_cb_ptr == NULL ||
            bta_gatts_cb_ptr == NULL ||
            bta_hh_cb_ptr == NULL
        ) {
                bluetooth_free_memory();
                return -1;
        }
    #endif

    #if GAP_DYNAMIC_MEMORY == TRUE
        gap_cb_ptr = pvPortMalloc(sizeof(tGAP_CB));
        if (gap_cb_ptr == NULL) {
            bluetooth_free_memory();
            return -1;
        }
    #endif

    return 0;
}

void bluetooth_free_memory(void) {
    #if BTM_DYNAMIC_MEMORY == TRUE
         secure_free((void **) &btm_cb_ptr);
    #endif

    #if GATT_DYNAMIC_MEMORY == TRUE
         secure_free((void **) &gatt_cb_ptr);
    #endif

    #if L2C_DYNAMIC_MEMORY == TRUE
         secure_free((void **) &l2c_cb_ptr);
    #endif

    #if BTA_DYNAMIC_MEMORY == TRUE
        secure_free((void **) &bta_sys_cb_ptr);
        secure_free((void **) &bta_dm_cb_ptr);
        secure_free((void **) &bta_dm_search_cb_ptr);
        secure_free((void **) &bta_dm_di_cb_ptr);
        secure_free((void **) &bta_gattc_cb_ptr);
        secure_free((void **) &bta_gatts_cb_ptr);
        secure_free((void **) &bta_hh_cb_ptr);
    #endif

    #if GAP_DYNAMIC_MEMORY == TRUE
         secure_free((void **) &gap_cb_ptr);
    #endif
}