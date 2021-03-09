/*
 * Copyright (c) 2021, Pycom Limited.
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
#define LTE_UART_ID                                                     (2)

#define LTE_UART_BUFFER_SIZE                                            (2048)
#define LTE_CMD_QUEUE_SIZE_MAX                                          (1)
#define LTE_RSP_QUEUE_SIZE_MAX                                          (1)
#define LTE_AT_CMD_SIZE_MAX                                             (128)
#define LTE_AT_CMD_DATA_SIZE_MAX                                        (LTE_AT_CMD_SIZE_MAX - 4)
#define LTE_AT_RSP_SIZE_MAX                                             (LTE_UART_BUFFER_SIZE)

#define LTE_OK_RSP                                                      "OK"
#define LTE_CONNECT_RSP                                                 "CONNECT"
#define LTE_RX_TIMEOUT_MAX_MS                                           (9500)
#define LTE_RX_TIMEOUT_MIN_MS                                           (300)
#define LTE_PPP_BACK_OFF_TIME_MS                                        (1150)

#define LTE_MUTEX_TIMEOUT                                               (5050 / portTICK_RATE_MS)
#define LTE_TASK_STACK_SIZE                                             (3072)
#define LTE_TASK_PRIORITY                                               (6)
#ifdef LTE_DEBUG_BUFF
#define LTE_LOG_BUFF_SIZE                                       (20 * 1024)
#endif

/******************************************************************************
 DEFINE TYPES
 ******************************************************************************/
typedef enum {
    E_LTE_INIT = 0,
    E_LTE_IDLE,
    E_LTE_ATTACHING,
    E_LTE_ATTACHED,
    E_LTE_PPP,
	E_LTE_SUSPENDED
} lte_state_t;

typedef enum {
    E_LTE_NORMAL = 0,
    E_LTE_LEGACY,
} lte_legacy_t;

typedef enum {
    E_LTE_CMD_AT = 0,
    E_LTE_CMD_PPP_ENTER,
    E_LTE_CMD_PPP_EXIT
} lte_task_cmd_t;

typedef enum {
    E_LTE_MODEM_CONNECTED = 0,
    E_LTE_MODEM_CONNECTING,
    E_LTE_MODEM_DISCONNECTED,
    E_LTE_MODEM_RECOVERY
} lte_modem_conn_state_t;

#ifdef LTE_DEBUG_BUFF
typedef struct {
    char* log;
    uint16_t ptr;
    bool truncated;
} lte_log_t;
#endif

typedef struct {
    uint32_t timeout;
    char data[LTE_AT_CMD_DATA_SIZE_MAX];
    size_t dataLen;
    bool expect_continuation;
} lte_task_cmd_data_t;

#pragma pack(1)
typedef struct {
    char data[LTE_UART_BUFFER_SIZE];
    bool data_remaining;
} lte_task_rsp_data_t;
#pragma pack()


/******************************************************************************
 DECLARE PUBLIC DTATA
 ******************************************************************************/
extern SemaphoreHandle_t xLTE_modem_Conn_Sem;
/******************************************************************************
 DECLARE PUBLIC FUNCTIONS
 ******************************************************************************/

extern void lteppp_init(void);

extern void lteppp_start (void);

extern void lteppp_set_state(lte_state_t state);

extern void lteppp_set_legacy(lte_legacy_t legacy);

extern void lteppp_connect(void);

extern void lteppp_disconnect(void);

extern lte_state_t lteppp_get_state(void);

extern lte_legacy_t lteppp_get_legacy(void);

extern uint32_t lteppp_ipv4(void);

extern void lteppp_deinit (void);

extern void lteppp_send_at_command (lte_task_cmd_data_t *cmd, lte_task_rsp_data_t *rsp);

extern bool lteppp_wait_at_rsp (const char *expected_rsp, uint32_t timeout, bool from_mp, void* data_rem);

lte_modem_conn_state_t lteppp_get_modem_conn_state(void);
void lteppp_set_modem_conn_state(lte_modem_conn_state_t state);

extern void connect_lte_uart (void);

extern bool ltepp_is_ppp_conn_up(void);

extern void lteppp_suspend(void);

extern void lteppp_resume(void);

extern void lteppp_set_default_inf(void);

#ifdef LTE_DEBUG_BUFF
extern char* lteppp_get_log_buff(void);
#endif

#endif  // _LTEPPP_H_
