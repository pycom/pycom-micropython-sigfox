/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#ifndef MODETH_H_
#define MODETH_H_

#include "ksz8851.h"
#include "tcpip_adapter.h"
#include "modnetwork.h"

extern TaskHandle_t ethernetTaskHandle;
extern const mod_network_nic_type_t mod_network_nic_type_eth;
extern xQueueHandle eth_cmdQueue;

extern void eth_pre_init (void);
extern void modeth_get_mac(uint8_t *mac);
extern bool is_eth_link_up(void);

typedef struct _eth_obj_t {
    mp_obj_base_t           base;
    uint8_t                 mac[ETH_MAC_SIZE];
    uint8_t                 hostname[TCPIP_HOSTNAME_MAX_SIZE];
    bool                    link_status;
    SemaphoreHandle_t       sem;
    uint32_t                trigger;
    int32_t                 events;
    mp_obj_t                handler;
    mp_obj_t                handler_arg;
} eth_obj_t;

typedef enum
{
    ETH_CMD_TX = 0,
    ETH_CMD_RX,
    ETH_CMD_OVERRUN,
    ETH_CMD_CHK_LINK,
    ETH_CMD_OTHER,
    ETH_CMD_HW_INT
}modeth_cmd_t;

typedef struct
{
    modeth_cmd_t cmd;
    uint8_t* buf;
    uint16_t len;
    uint16_t isr;
}modeth_cmd_ctx_t;

#endif
