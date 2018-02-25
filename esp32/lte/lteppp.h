/*
 * Copyright (c) 2018, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#ifndef _LTEPPP_H_
#define _LTEPPP_H_

/******************************************************************************
 DEFINE CONSTANTS
 ******************************************************************************/
#define LTE_UART_ID                                             2

#define LTE_UART_BUFFER_SIZE                                            (2048)
#define LTE_CMD_QUEUE_SIZE_MAX                                          (1)
#define LTE_RSP_QUEUE_SIZE_MAX                                          (1)
#define LTE_AT_CMD_SIZE_MAX                                             (128)
#define LTE_AT_RSP_SIZE_MAX                                             (LTE_UART_BUFFER_SIZE)

#define LTE_OK_RSP                                                      "OK"
#define LTE_CONNECT_RSP                                                 "CONNECT"
#define LTE_RX_TIMEOUT_MAX_MS                                           (7500)
#define LTE_RX_TIMEOUT_MIN_MS                                           (250)
#define LTE_PPP_BACK_OFF_TIME_MS                                        (1150)

#define LTE_MUTEX_TIMEOUT                                               (5050 / portTICK_RATE_MS)
#define LTE_TASK_STACK_SIZE                                             (3072)
#define LTE_TASK_PRIORITY                                               (6)

/******************************************************************************
 DEFINE TYPES
 ******************************************************************************/
typedef enum {
    E_LTE_INIT = 0,
    E_LTE_IDLE,
    E_LTE_ATTACHING,
    E_LTE_ATTACHED,
    E_LTE_PPP
} lte_state_t;

typedef enum {
    E_LTE_CMD_AT = 0,
    E_LTE_CMD_PPP_ENTER,
    E_LTE_CMD_PPP_EXIT
} lte_task_cmd_t;

typedef struct {
    uint32_t timeout;
    char data[LTE_AT_CMD_SIZE_MAX - 4];
} lte_task_cmd_data_t;

typedef struct {
    char data[LTE_UART_BUFFER_SIZE];
} lte_task_rsp_data_t;

/******************************************************************************
 DECLARE PUBLIC FUNCTIONS
 ******************************************************************************/

extern void lteppp_init(void);

extern void lteppp_start (void);

extern void lteppp_set_state(lte_state_t state);

extern void lteppp_connect(void);

extern void lteppp_disconnect(void);

extern lte_state_t lteppp_get_state(void);

extern uint32_t lteppp_ipv4(void);

extern void lteppp_deinit (void);

extern void lteppp_send_at_command (lte_task_cmd_data_t *cmd, lte_task_rsp_data_t *rsp);

extern bool lteppp_wait_at_rsp (const char *expected_rsp, uint32_t timeout);

#endif  // _LTEPPP_H_
