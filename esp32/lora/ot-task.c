/*
 * Copyright (c) 2018, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include "ot-task.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "py/mpconfig.h"
#include "py/mpstate.h"
#include "py/obj.h"
#include "py/nlr.h"
#include "py/runtime.h"
#include "py/mphal.h"
#include "py/stream.h"
#include "py/mperrno.h"

#include "modusocket.h"

// openThread includes
#include <openthread/udp.h>
#include <openthread/ip6.h>
#include <openthread/thread.h>
#include <openthread/thread_ftd.h>
#include <openthread/platform/alarm-milli.h>
#include <openthread/tasklet.h>
#include <openthread/platform/radio.h>
#include <openthread/cli.h>
#include <openthread/platform/uart.h>
#include <openthread/border_router.h>

#include "lora/otplat_alarm.h"
#include "lora/otplat_radio.h"
#include "lora/ot-settings.h"
#include "lora/ot-log.h"

/******************************************************************************
 DEFINE PRIVATE CONSTANTS
 ******************************************************************************/
#define MESH_STACK_SIZE                                         (4096)
#define MESH_TASK_PRIORITY                                      (5)
#define OT_DATA_QUEUE_SIZE_MAX                                    (3)
#define OT_RX_PACK_SIZE_MAX                                        (1500)

/******************************************************************************
 DEFINE TYPES
 ******************************************************************************/
typedef struct {
    uint8_t data[OT_RX_PACK_SIZE_MAX];
    uint8_t len;
    char ip[MOD_USOCKET_IPV6_CHARS_MAX];
    uint16_t port;
} ot_rx_data_t;

/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/
static otInstance *ot = NULL;
static otUdpSocket udp_sock;
//static otMessageInfo mPeer;
static bool socket_opened = false;
static QueueHandle_t xRxQueue;

/******************************************************************************
 DECLARE PUBLIC DATA
 ******************************************************************************/
extern TaskHandle_t xMeshTaskHndl;
extern bool ot_ready;

extern int otCliBufferLen;
extern char otCliBuffer[];

/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/
static void TASK_Mesh(void *pvParameters);
void socket_udp_cb(void *aContext, otMessage *aMessage,
        const otMessageInfo *aMessageInfo);

/******************************************************************************
 DEFINE PUBLIC FUNCTIONS
 ******************************************************************************/

// creating Mesh freeRtos Task
void openthread_task_init(void) {

    xRxQueue = xQueueCreate(OT_DATA_QUEUE_SIZE_MAX, sizeof(ot_rx_data_t));

    xTaskCreatePinnedToCore(TASK_Mesh, "Mesh",
    MESH_STACK_SIZE / sizeof(StackType_t),
    NULL,
    MESH_TASK_PRIORITY, &xMeshTaskHndl, 1);

}

otInstance* openthread_init(void) {
    otError err;

    // flush the rx queue
    xQueueReset(xRxQueue);

    otPlatRadioInit();

    if (NULL == (ot = otInstanceInitSingle())) {
        printf("can't otInstanceInitSingle\n");
    }
    otPlatAlarmInit(ot);

    // > panid 0x1234
    if (OT_ERROR_NONE != otLinkSetPanId(ot, 0x1234)) {
        printf("panid can't be set\n");
    }

    // > ifconfig up
    if (OT_ERROR_NONE != otIp6SetEnabled(ot, true)) {
        printf("err ifconfig up\n");
    }

    if (OT_ERROR_NONE != (err = otThreadSetEnabled(ot, true))) {
        printf("Thread can't start: %d\n", err);
    }

    return ot;
}

int mesh_socket_open(int *_errno) {

    *_errno = 0;
    otEXPECT_ACTION(socket_opened == false, *_errno = MP_EALREADY);

    memset(&udp_sock, 0, sizeof(udp_sock));

    // open socket
    otEXPECT_ACTION(
            OT_ERROR_NONE == otUdpOpen(ot, &udp_sock, &socket_udp_cb, NULL),
            *_errno = MP_ENOENT);

    exit: if (*_errno != 0) {
        printf("err: %d", *_errno);
        return -1;
    }
    socket_opened = true;
    return 0;
}

void mesh_socket_close(void) {
    otUdpClose(&udp_sock);
    socket_opened = false;
}

int mesh_socket_bind(byte *ip, mp_uint_t port, int *_errno) {
    otSockAddr sockaddr;

    *_errno = 0;
    otEXPECT_ACTION(socket_opened == true, *_errno = MP_EALREADY);

    memset(&sockaddr, 0, sizeof(sockaddr));

    otEXPECT_ACTION(
            OT_ERROR_NONE == otIp6AddressFromString((const char*) ip, &sockaddr.mAddress),
            *_errno = MP_ENOENT);

    sockaddr.mPort = port;
    sockaddr.mScopeId = OT_NETIF_INTERFACE_ID_THREAD;

    // bind socket (server assigns its own address)
    otEXPECT_ACTION(OT_ERROR_NONE == otUdpBind(&udp_sock, &sockaddr), *_errno =
            MP_ENOENT);

    exit: if (*_errno != 0) {
        printf("err: %d", *_errno);
        return -1;
    }
    return 0;
}

int mesh_socket_sendto(const byte *buf, mp_uint_t len, byte *ip, mp_uint_t port,
        int *_errno) {
    otMessageInfo messageInfo;
    otMessage *message = NULL;

    *_errno = 0;
    otEXPECT_ACTION(socket_opened == true, *_errno = MP_EALREADY);

    memset(&messageInfo, 0, sizeof(messageInfo));

    otEXPECT_ACTION(
            OT_ERROR_NONE
                    == otIp6AddressFromString((char* )ip,
                            &messageInfo.mPeerAddr), *_errno = MP_EAFNOSUPPORT);

    messageInfo.mPeerPort = port;
    messageInfo.mInterfaceId = OT_NETIF_INTERFACE_ID_THREAD;

    otEXPECT_ACTION(NULL != (message = otUdpNewMessage(ot, true)), *_errno =
            MP_ENOMEM);

    otEXPECT_ACTION(OT_ERROR_NONE == otMessageSetLength(message, len), *_errno =
            MP_ENOMEM);

    otEXPECT_ACTION(len == otMessageWrite(message, 0, buf, len), *_errno =
            MP_ENOMEM);

    otEXPECT_ACTION(
            OT_ERROR_NONE == otUdpSend(&udp_sock, message, &messageInfo),
            *_errno = MP_EHOSTUNREACH);

    exit: if (*_errno != 0) {
        printf("err: %d", *_errno);
        if (NULL != message)
            otMessageFree(message);
        return -1;
    }
    return len;
}

int mesh_socket_recvfrom(byte *buf, mp_uint_t len, byte *ip, mp_uint_t *port,
        int *_errno) {

    ot_rx_data_t rx_data;

    *_errno = 0;
    otEXPECT_ACTION(socket_opened == true, *_errno = MP_EALREADY);

    if (xQueueReceive(xRxQueue, &rx_data, 0)) {
        // adjust the len
        if (rx_data.len < len) {
            len = rx_data.len;
        }

        memcpy(buf, rx_data.data, len);
        strncpy((char*) ip, (rx_data.ip), MOD_USOCKET_IPV6_CHARS_MAX);
        *port = rx_data.port;
        return len;
    }
    exit: if (*_errno != 0) {
        printf("err: %d", *_errno);
        return -1;
    }
    return 0;
}
/******************************************************************************
 DEFINE PRIVATE FUNCTIONS
 ******************************************************************************/
static void TASK_Mesh(void *pvParameters) {

    for (;;) {
        vTaskDelay(2 / portTICK_PERIOD_MS);

        // openThread process tasklets

        // CLI
        if (ot_ready) {

            if (otCliBufferLen > 0) {
                otCliConsoleInputLine(otCliBuffer, otCliBufferLen);
                otCliBufferLen = 0;
            }

            // Radio 802.15.4 TX/RX state-machine
            otRadioProcess(ot);

            // openThread Process
            otTaskletsProcess(ot);

            // Log output
            otPlatLogFlush();
        }

    }
}

// function called by openthread when a UDP packet arrived
void socket_udp_cb(void *aContext, otMessage *aMessage,
        const otMessageInfo *aMessageInfo) {
    ot_rx_data_t rx_data;

    rx_data.len = otMessageRead(aMessage, otMessageGetOffset(aMessage),
            rx_data.data, sizeof(rx_data.data) - 1);
    rx_data.port = aMessageInfo->mPeerPort;

    rx_data.ip[0] = 0;
    snprintf(rx_data.ip, MOD_USOCKET_IPV6_CHARS_MAX, "%x:%x:%x:%x:%x:%x:%x:%x",
            HostSwap16(aMessageInfo->mPeerAddr.mFields.m16[0]),
            HostSwap16(aMessageInfo->mPeerAddr.mFields.m16[1]),
            HostSwap16(aMessageInfo->mPeerAddr.mFields.m16[2]),
            HostSwap16(aMessageInfo->mPeerAddr.mFields.m16[3]),
            HostSwap16(aMessageInfo->mPeerAddr.mFields.m16[4]),
            HostSwap16(aMessageInfo->mPeerAddr.mFields.m16[5]),
            HostSwap16(aMessageInfo->mPeerAddr.mFields.m16[6]),
            HostSwap16(aMessageInfo->mPeerAddr.mFields.m16[7]));

    // store packet received in queue, to be consumed by socket.recvfrom()
    xQueueSend(xRxQueue, (void *) &rx_data, 0);

#if 0
    // code for sending back ACK pack to the same IP+port that this package was received

    buf[length] = '\0';
    otPlatLog(0, 0, "%s\n", buf);
    memcpy(&mPeer, aMessageInfo, sizeof(otMessageInfo));
    mPeer.
    // respond with ACK
    otError error = OT_ERROR_NONE;
    otMessage *message;

    otEXPECT_ACTION((message = otUdpNewMessage(ot, true)) != NULL, error = OT_ERROR_NO_BUFS);
    otEXPECT_ACTION(OT_ERROR_NONE == (error = otMessageSetLength(message, length)), error += 100);
    otMessageWrite(message, 0, buf, length);
    otEXPECT_ACTION(OT_ERROR_NONE == (error = otUdpSend(&udp_sock, message, &mPeer)), error += 1000);
    otPlatLog(0, 0,"server_udp_cb OK");

    exit:

    if (error != OT_ERROR_NONE && message != NULL)
    {
        otMessageFree(message);
        otPlatLog(0, 0,"server_udp_cb err: %d", error);
    }
#endif
}
