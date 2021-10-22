/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include "modmesh.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "py/mpconfig.h"
#include "py/mpstate.h"
#include "py/obj.h"
#include "py/nlr.h"
#include "py/runtime.h"
#include "py/mphal.h"
#include "py/stream.h"
#include "py/mperrno.h"
#include "bufhelper.h"
#include "mpexception.h"
#include "radio.h"
#include "modnetwork.h"
#include "py/stream.h"
#include "modusocket.h"
#include "pycom_config.h"

#include "util/mpirq.h"

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
#include <openthread/link.h>

#include "lora/otplat_alarm.h"
#include "lora/otplat_radio.h"
#include "lora/ot-settings.h"
#include "lora/ot-log.h"

/******************************************************************************
 DEFINE PRIVATE CONSTANTS
 ******************************************************************************/
#define MESH_STACK_SIZE                                             (8192)
#define MESH_TASK_PRIORITY                                          (6)
#define OT_DATA_QUEUE_SIZE_MAX                                      (5)
#define OT_RX_PACK_SIZE_MAX                                         (512)
#define IPV6_HEADER_UDP_PROTOCOL_CODE                               (17)
#define MESH_CLI_OUTPUT_SIZE                                        (1024)
#define MESH_NEIGBORS_MAX                                           (16)
#define MESH_IP_ADDRESSES_MAX                                       (7)
#define MESH_ROUTERS_MAX                                            (16)

// number of Mesh.routers() micropy command fields
#define MESH_ROUTERS_FIELDS_NUM                                     (5)

// number of Mesh.neighbors() micropy command fields
#define MESH_NEIGHBOR_FIELDS_NUM                                    (5)

// number of Mesh.leader() micropy command fields
#define MESH_LEADER_FIELDS_NUM                                      (3)

// number of Mesh.border_router() micropy command fields
#define MESH_BR_FIELDS_NUM                                          (2)

// max number of Border Routers prefix entries for a single node
#define MESH_BR_MAX                                                 (2)

// max number of UDP sockets
#define UDP_SOCKETS_MAX                                             (3)

// UDP datagrams arrived on Border Router interface have an additional header
// first part header size, in bytes
#define BORDER_ROUTER_HEADER_1                                      (1)
// actual value of the MAGIC value for BR header
#define BORDER_ROUTER_HEADER_1_CONST                                (0xBB)
// second part of BR header is the IPv6 destination (16 bytes)
#define BORDER_ROUTER_HEADER_2                                      (OT_IP6_ADDRESS_SIZE)
// third part of BR header is the UDP port destination (2 bytes)
#define BORDER_ROUTER_HEADER_3                                      (sizeof(uint16_t))
// total size of BR header
#define BORDER_ROUTER_HEADER_SIZE                                   (BORDER_ROUTER_HEADER_1 + BORDER_ROUTER_HEADER_2 + BORDER_ROUTER_HEADER_3)

/******************************************************************************
 DEFINE TYPES
 ******************************************************************************/
typedef struct {
    mp_obj_base_t base;
    bool ot_ready;

    char otCliBuffer[128];
    int otCliBufferLen;

    char meshCliOutput[MESH_CLI_OUTPUT_SIZE];
    int meshCliOutputLen;
    bool meshCliOutputDone;
}mesh_obj_t;

typedef struct {
    mp_obj_t          handler;
    mp_obj_t          handler_arg;
    uint8_t           events;
    uint8_t           trigger;
}ot_obj_t;

typedef struct {
    uint8_t data[OT_RX_PACK_SIZE_MAX];
    uint16_t len;
    otIp6Address src_ip;
    uint16_t src_port;
    otIp6Address dest_ip;
    uint16_t dest_port;
} ot_rx_data_t;

typedef struct {
    uint8_t preamble[6];
    uint8_t payload_type;
    uint8_t hop_limit;
    otIp6Address source_ip6;
    otIp6Address dest_ip6;
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
}ipv6_and_udp_header_t;

typedef struct {
    char net[MOD_USOCKET_IPV6_CHARS_MAX];
    int8_t preference;
}border_router_info_t;


typedef struct {
    mod_network_socket_obj_t *s;            // pointer to the NIC socket
    otUdpSocket udp_sock;                   // socket from openthread
    uint16_t port;                          // UDP port
    otIp6Address ip;                        // ipv6
    //char ip_str[MOD_USOCKET_IPV6_CHARS_MAX];// IPv6 in string
    QueueHandle_t rx_queue;                 // queue for RX packages
}pymesh_socket_t;

/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/
static ot_obj_t ot_obj;
static otInstance *ot = NULL;
static otIp6Prefix border_router_prefix;
static mesh_obj_t mesh_obj;
static pymesh_socket_t sockets[UDP_SOCKETS_MAX];

/******************************************************************************
 DECLARE PUBLIC DATA
 ******************************************************************************/

TaskHandle_t xMeshTaskHndl;

/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/
static void modmesh_init(void);

static void TASK_Mesh(void *pvParameters);

static void socket_udp_cb(void *aContext, otMessage *aMessage,
        const otMessageInfo *aMessageInfo);

static void br_ip6_rcv_cb(otMessage *aMessage, void *aContext);

static bool otIp6ToString(otIp6Address ipv6, char* ip_str, int str_len);

static otInstance* openthread_init(uint8_t key[]);

static void openthread_deinit(void);

static int mesh_state(void);

static bool mesh_singleton(void);

static int mesh_rloc(void);

static int mesh_ipaddr(uint8_t ip_num, uint8_t ip_size, char ipaddr[ip_num][ip_size]);

static int mesh_neighbors(uint8_t nei_num, otNeighborInfo neighbors[nei_num]);

static int mesh_routers(uint8_t routers_num, otRouterInfo routers[routers_num]);

static int mesh_leader_data(otRouterInfo *leaderRouterData, otLeaderData *leaderData);

static int mesh_rx_callback(mp_obj_t cb_obj, mp_obj_t cb_arg_obj);

static int mesh_add_border_router(const char* ipv6_net_str, int preference_level);

static int mesh_del_border_router(const char* ipv6_net_str);

static pymesh_socket_t *find_socket(mod_network_socket_obj_t *nic_sock);

/******************************************************************************
 DEFINE PUBLIC FUNCTIONS
 ******************************************************************************/

/*
 * returns true if Pymesh interface is initialized
 */
bool lora_mesh_ready(void) {
    return mesh_obj.ot_ready;
}

// opens a new UDP socket on Pymesh
int mesh_socket_open(mod_network_socket_obj_t *s, int *_errno) {

    *_errno = 0;
    pymesh_socket_t *sock;

    otEXPECT_ACTION(NULL != (sock = find_socket(NULL)), *_errno = MP_EMFILE);

    memset(&sock->udp_sock, 0, sizeof(otUdpSocket));

    otEXPECT_ACTION(NULL != (sock->rx_queue = xQueueCreate(OT_DATA_QUEUE_SIZE_MAX, sizeof(ot_rx_data_t))),
            *_errno = MP_ENOBUFS);

    // open socket
    otEXPECT_ACTION(
            OT_ERROR_NONE == otUdpOpen(ot, &sock->udp_sock, &socket_udp_cb, sock),
            *_errno = MP_ENOENT);

    sock->s = s;
//    printf("opened: %p\n", s);

    exit: if (*_errno != 0) {
        printf("err: %d", *_errno);
        vQueueDelete(sock->rx_queue);
        sock->s = NULL;
        return -1;
    }

    return 0;
}

// closes the specified socket
// if s is NULL, closes all sockets for Pymesh interface
void mesh_socket_close(mod_network_socket_obj_t *s) {
    if (s) {
        // destroy a specific socket
        pymesh_socket_t *sock = find_socket(s);
        otUdpClose(&sock->udp_sock);
        vQueueDelete(sock->rx_queue);
        memset(sock, 0, sizeof(pymesh_socket_t));
    } else {
        // destroy all sockets
        for (int i = 0 ; i < UDP_SOCKETS_MAX; i++) {
            if (sockets[i].s) {
                otUdpClose(&sockets[i].udp_sock);
                vQueueDelete(sockets[i].rx_queue);
                memset(&sockets[i], 0, sizeof(pymesh_socket_t));
            }
        }
    }
}

int mesh_socket_bind(mod_network_socket_obj_t *s, byte *ip, mp_uint_t port, int *_errno) {
    otSockAddr sockaddr;

    *_errno = 0;
    pymesh_socket_t *sock;

    otEXPECT_ACTION(NULL != (sock = find_socket(s)), *_errno = MP_ENOENT);

    memset(&sockaddr, 0, sizeof(sockaddr));

    otEXPECT_ACTION(
            OT_ERROR_NONE == otIp6AddressFromString((const char*) ip, &sockaddr.mAddress),
            *_errno = MP_ENOENT);

    sockaddr.mPort = port;
    sockaddr.mScopeId = OT_NETIF_INTERFACE_ID_THREAD;

    // bind socket (server assigns its own address)
    otEXPECT_ACTION(OT_ERROR_NONE == otUdpBind(&sock->udp_sock, &sockaddr), *_errno =
            MP_ENOENT);

    sock->ip = sockaddr.mAddress;
    sock->port = port;

    exit: if (*_errno != 0) {
        printf("err: %d", *_errno);
        return -1;
    }
    return 0;
}

int mesh_socket_sendto(mod_network_socket_obj_t *s, const byte *buf, mp_uint_t len, byte *ip, mp_uint_t port,
        int *_errno) {
    otMessageInfo messageInfo;
    otMessage *message = NULL;

    *_errno = 0;
    pymesh_socket_t *sock;

    otEXPECT_ACTION(NULL != (sock = find_socket(s)), *_errno = MP_ENOENT);

    memset(&messageInfo, 0, sizeof(messageInfo));

    otEXPECT_ACTION(
            OT_ERROR_NONE
                    == otIp6AddressFromString((char* )ip,
                            &messageInfo.mPeerAddr), *_errno = MP_EAFNOSUPPORT);

    messageInfo.mPeerPort = port;
    messageInfo.mInterfaceId = OT_NETIF_INTERFACE_ID_THREAD;

    message = otUdpNewMessage(ot, NULL);
    otEXPECT_ACTION(NULL != message, *_errno = MP_ENOMEM);

    otEXPECT_ACTION(OT_ERROR_NONE == otMessageSetLength(message, len), *_errno =
            MP_EACCES);//MP_ENOMEM);

    otEXPECT_ACTION(len == otMessageWrite(message, 0, buf, len), *_errno =
            MP_EFAULT);//MP_ENOMEM);

    otEXPECT_ACTION(
            OT_ERROR_NONE == otUdpSend(&sock->udp_sock, message, &messageInfo),
            *_errno = MP_EHOSTUNREACH);

    exit:
    if (*_errno != 0) {
        printf("err: %d\n", *_errno);

        if (NULL != message) {
            printf("otMessageFree\n");
            otMessageFree(message);
        }
        return -1;
    }
    return len;
}

int mesh_socket_recvfrom(mod_network_socket_obj_t *s, byte *buf, mp_uint_t len, byte *ip, mp_uint_t *port,
        int *_errno) {

    ot_rx_data_t rx_data;

    *_errno = 0;

    pymesh_socket_t *sock;

    otEXPECT_ACTION(NULL != (sock = find_socket(s)), *_errno = MP_ENOENT);

    if (xQueueReceive(sock->rx_queue, &rx_data, 0)) {
        // adjust the len
        if (rx_data.len < len) {
            len = rx_data.len;
        }

        memcpy(buf, rx_data.data, len);
        otIp6ToString(rx_data.src_ip, (char*)ip, MOD_USOCKET_IPV6_CHARS_MAX);
        *port = rx_data.src_port;

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
/*
 * Main function executed by Mesh task
 */
static void TASK_Mesh(void *pvParameters) {
    
    for (;;) {
        vTaskDelay(1 / portTICK_PERIOD_MS);

        // openThread process tasklets

        // CLI
        if (mesh_obj.ot_ready) {

            // Radio 802.15.4 TX/RX state-machine
            otRadioProcess(ot);

            // openThread Process
            otTaskletsProcess(ot);

            if (mesh_obj.otCliBufferLen > 0) {
                otCliConsoleInputLine(mesh_obj.otCliBuffer, mesh_obj.otCliBufferLen);
                mesh_obj.otCliBufferLen = 0;
            }

            // Log output
            otPlatLogFlush();
        }

    }
}

/*
 * creating Mesh freeRtos Task
 */
static void modmesh_init(void) {

    //xRxQueue = xQueueCreate(OT_DATA_QUEUE_SIZE_MAX, sizeof(ot_rx_data_t));

    ot_obj.handler = mp_const_none;
    ot_obj.handler_arg = mp_const_none;
    
    memset(&mesh_obj, 0, sizeof(mesh_obj));

    for (int i = 0 ; i < UDP_SOCKETS_MAX ; i++)
        memset(&sockets[i], 0, sizeof(pymesh_socket_t));

    xTaskCreatePinnedToCore(TASK_Mesh, "Mesh",
    MESH_STACK_SIZE / sizeof(StackType_t),
    NULL,
    MESH_TASK_PRIORITY, &xMeshTaskHndl, 1);
}

/*
 * returns the pymesh_socket which has nic_socket as s
 * if s is NULL, then searches the first unused socket
 */
static pymesh_socket_t *find_socket(mod_network_socket_obj_t *nic_sock) {
    int i;

    //printf("find_socket: %p\n", nic_sock);
    for (i = 0 ; i < UDP_SOCKETS_MAX; i++) {
        //printf("%d: %p ", i, sockets[i].s);
        if (sockets[i].s == nic_sock) {
            //printf("found\n");
            return &sockets[i];
        }
    }
    //printf("find_socket: null\n");
    return NULL;
}

/*
 * this function will be called by the micropython interrupt thread
 */
STATIC void rx_interrupt_queue_handler(void *arg) {
    ot_obj_t *self = arg;

    if (self->handler && self->handler != mp_const_none) {
            mp_call_function_1(self->handler, self->handler_arg);
        }
}

// initialize Thread interface
static otInstance* openthread_init(uint8_t key[]) {
    otError err;
    otExtAddress extAddr;

    otPlatRadioInit();

    if (NULL == (ot = otInstanceInitSingle())) {
        printf("can't otInstanceInitSingle\n");
    }
    otPlatAlarmInit(ot);

    // set master key
    otMasterKey masterkey;
    memcpy(masterkey.m8, key, OT_MASTER_KEY_SIZE);
    if (OT_ERROR_NONE != (err = otThreadSetMasterKey(ot, &masterkey))) {
        printf("Can't set Masterkey: %d\n", err);
    }

    // > panid 0x1234
    if (OT_ERROR_NONE != otLinkSetPanId(ot, 0x1234)) {
        printf("panid can't be set\n");
    }

    // > ifconfig up
    if (OT_ERROR_NONE != otIp6SetEnabled(ot, true)) {
        printf("err ifconfig up\n");
    }

    // force 802.15.4 Extended Address to LoRa MAC, against Thread Specs
    otPlatRadioGetIeeeEui64(ot, (uint8_t *)&(extAddr.m8));
    if (OT_ERROR_NONE != (err = otLinkSetExtendedAddress(ot, &extAddr))) {
        printf("Can't assign Lora MAC as ExtAddr: %d\n", err);
    }

    if (OT_ERROR_NONE != (err = otThreadSetEnabled(ot, true))) {
        printf("Thread can't start: %d\n", err);
    }

    otIp6SetReceiveFilterEnabled(ot, true);

    return ot;
}

// de-initialize Thread interface
static void openthread_deinit(void) {

    if (!ot)
        return; // nothing to do

    mesh_obj.ot_ready = false;
    mesh_socket_close(NULL);

    otInstanceFactoryReset(ot);
    otInstanceFinalize(ot);
    ot = NULL;
}

static int mesh_state(void) {
    int state = -1;

    if (ot) {
        state = otThreadGetDeviceRole(ot);
    }

    /*
    typedef enum {
    OT_DEVICE_ROLE_DISABLED = 0, ///< The Thread stack is disabled.
    OT_DEVICE_ROLE_DETACHED = 1, ///< Not currently participating in a Thread network/partition.
    OT_DEVICE_ROLE_CHILD    = 2, ///< The Thread Child role.
    OT_DEVICE_ROLE_ROUTER   = 3, ///< The Thread Router role.
    OT_DEVICE_ROLE_LEADER   = 4, ///< The Thread Leader role.
    } otDeviceRole;
     */
    return state;
}

static bool mesh_singleton(void) {
    int single = true;

    if (ot) {
        single = otThreadIsSingleton(ot);
    }

    return single;
}

static int mesh_rloc(void) {
    int rloc = -1;

    if (ot) {
        rloc = otThreadGetRloc16(ot);
    }
    return rloc;
}

static int mesh_ipaddr(uint8_t ip_num, uint8_t ip_size, char ipaddr[ip_num][ip_size]) {
    uint8_t i = 0;

    if (!ot)
        return 0;

    const otNetifAddress *unicastAddrs = otIp6GetUnicastAddresses(ot);

    for (const otNetifAddress *addr = unicastAddrs; addr; addr = addr->mNext)
    {
        if (i >= ip_num)
            break;// no more space in the string list

        ipaddr[i][0] = 0;
        snprintf(ipaddr[i], ip_size, "%x:%x:%x:%x:%x:%x:%x:%x", HostSwap16(addr->mAddress.mFields.m16[0]),
                              HostSwap16(addr->mAddress.mFields.m16[1]), HostSwap16(addr->mAddress.mFields.m16[2]),
                              HostSwap16(addr->mAddress.mFields.m16[3]), HostSwap16(addr->mAddress.mFields.m16[4]),
                              HostSwap16(addr->mAddress.mFields.m16[5]), HostSwap16(addr->mAddress.mFields.m16[6]),
                              HostSwap16(addr->mAddress.mFields.m16[7]));

        i++;
    }

    return i;
}

// returns the neighbors
static int mesh_neighbors(uint8_t nei_num, otNeighborInfo neighbors[nei_num]) {
    uint8_t i = 0;
    otNeighborInfoIterator iterator = OT_NEIGHBOR_INFO_ITERATOR_INIT;

    if (!ot)
        return 0;

    while (otThreadGetNextNeighborInfo(ot, &iterator, &neighbors[i]) == OT_ERROR_NONE)
    {
        i++;
        if (i >= nei_num)
            break;// no more space pre-allocated
    }
    return i;
}

// returns the routers from whole mesh network
static int mesh_routers(uint8_t routers_num, otRouterInfo routers[routers_num]) {

    if (!ot)
        return 0;

    uint8_t router_crt = 0;
    uint8_t maxRouterId = otThreadGetMaxRouterId(ot);

    for (uint8_t i = 0; i <= maxRouterId; i++)
    {
        if (otThreadGetRouterInfo(ot, i, &routers[router_crt]) != OT_ERROR_NONE)
        {
            continue;
        }
        router_crt ++;
        if (router_crt >= routers_num)
            break; // no more space
    }

    return router_crt;
}


// returns Leader Data, ex: ip address of the leader
static int mesh_leader_data(otRouterInfo *leaderRouterData, otLeaderData *leaderData) {
    if (!ot)
        return -1;

    if ((otThreadGetLeaderData(ot, leaderData) == OT_ERROR_NONE) &&
            ( otThreadGetRouterInfo(ot, leaderData->mLeaderRouterId, leaderRouterData) == OT_ERROR_NONE)) {
        // Ext Address is in leaderRouterData->mExtAddress.m8 (type char [8])
        return 1;
    }
    return -1;
}

// register the callback to be triggered(with argument) when a new packet arrived on mesh
static int mesh_rx_callback(mp_obj_t cb_obj, mp_obj_t cb_arg_obj) {
    if (!ot)
        return -1;

    ot_obj.handler = cb_obj;
    ot_obj.handler_arg = cb_arg_obj;
    mp_irq_add(&ot_obj, cb_obj);

    return 1;
}

// register a Border Router for the mesh network, by its network addres (on-mesh prefix) and preference
static int mesh_add_border_router(const char* ipv6_net_str, int preference_level) {

    // this code is from the CLI command: "prefix add <prefix> [pvdcsr] [prf]"
    int                     error = 0;
    otBorderRouterConfig    config;

    memset(&config, 0, sizeof(otBorderRouterConfig));

    char *prefixLengthStr;

    // parse and check if ipv6_net_str is a real IPv6 network address(prefix)
    // ipv6 prefix example: "2001:dead:beef:cafe::/64"
    otEXPECT_ACTION((prefixLengthStr = strchr(ipv6_net_str, '/')) != NULL, error = MP_ENXIO);

    *prefixLengthStr++ = '\0';

    otEXPECT_ACTION((error = otIp6AddressFromString(ipv6_net_str, &config.mPrefix.mPrefix)) == OT_ERROR_NONE,);

    config.mPrefix.mLength = (uint8_t)(strtol(prefixLengthStr, NULL, 0));

    otEXPECT_ACTION(preference_level >= OT_ROUTE_PREFERENCE_LOW && preference_level <= OT_ROUTE_PREFERENCE_HIGH,
            error = MP_EINVAL);
    config.mPreference = preference_level;

    config.mPreferred = true;
    config.mSlaac = true;
    config.mDefaultRoute = true;
    config.mOnMesh = true;
    config.mStable = true;

    otEXPECT_ACTION((error = otBorderRouterAddOnMeshPrefix(ot, &config)) == OT_ERROR_NONE, );
    if (error == OT_ERROR_INVALID_ARGS) {
        error = MP_ENXIO;
    } else if (error == OT_ERROR_NO_BUFS) {
        error = MP_ENOMEM;
    } else {
        // no error, then register the IP receive callback
        otIp6SetReceiveCallback(ot, &br_ip6_rcv_cb, NULL);

        // record BR prefix, for using it to filter IP src packets
        border_router_prefix = config.mPrefix;
    }

exit:
    return error;

}

// remove a border router entry
static int mesh_del_border_router(const char* ipv6_net_str) {

    // this code is from the CLI command: "prefix remvoe <prefix>"
    int                     error = 0;
    struct otIp6Prefix      prefix;

    memset(&prefix, 0, sizeof(otIp6Prefix));

    char *prefixLengthStr;

    // parse and check if ipv6_net_str is a real IPv6 network address(prefix)
    // ipv6 prefix example: "2001:dead:beef:cafe::/64"
    otEXPECT_ACTION((prefixLengthStr = strchr(ipv6_net_str, '/')) != NULL, error = MP_ENXIO);

    *prefixLengthStr++ = '\0';

    otEXPECT_ACTION((error = otIp6AddressFromString(ipv6_net_str, &prefix.mPrefix)) == OT_ERROR_NONE,);

    prefix.mLength = (uint8_t)(strtol(prefixLengthStr, NULL, 0));

    error = otBorderRouterRemoveOnMeshPrefix(ot, &prefix);

    if (error == OT_ERROR_NONE) {
        // remove ipv6 callback
        otIp6SetReceiveCallback(ot, NULL, NULL);

        // erase border_router_prefix
        memset(&border_router_prefix, 0, sizeof(otIp6Prefix));
    }
exit:
    return error;
}

// returns the border routers net address set on current mesh node
static int mesh_br_list(uint8_t routers_num, border_router_info_t routers[routers_num]) {

    if (!ot)
        return 0;

    uint8_t router_crt = 0;

    otNetworkDataIterator iterator = OT_NETWORK_DATA_ITERATOR_INIT;
    otBorderRouterConfig config;

    while (otBorderRouterGetNextOnMeshPrefix(ot, &iterator, &config) == OT_ERROR_NONE)
    {
        snprintf(routers[router_crt].net,sizeof(routers[router_crt].net),
                "%x:%x:%x:%x::/%d",
                HostSwap16(config.mPrefix.mPrefix.mFields.m16[0]),
                HostSwap16(config.mPrefix.mPrefix.mFields.m16[1]),
                HostSwap16(config.mPrefix.mPrefix.mFields.m16[2]),
                HostSwap16(config.mPrefix.mPrefix.mFields.m16[3]),
                config.mPrefix.mLength);

        routers[router_crt].preference = config.mPreference;
        router_crt++;

        if (router_crt >= routers_num)
            // no more space to store BR
            break;
    }

    return router_crt;
}

// transform otIp6Address to string
static bool otIp6ToString(otIp6Address ipv6, char* ip_str, int str_len) {

    if (str_len < MOD_USOCKET_IPV6_CHARS_MAX)
        return false;

    ip_str[0] = 0;

    snprintf(ip_str, MOD_USOCKET_IPV6_CHARS_MAX, "%x:%x:%x:%x:%x:%x:%x:%x",
            HostSwap16(ipv6.mFields.m16[0]),
            HostSwap16(ipv6.mFields.m16[1]),
            HostSwap16(ipv6.mFields.m16[2]),
            HostSwap16(ipv6.mFields.m16[3]),
            HostSwap16(ipv6.mFields.m16[4]),
            HostSwap16(ipv6.mFields.m16[5]),
            HostSwap16(ipv6.mFields.m16[6]),
            HostSwap16(ipv6.mFields.m16[7]));

    return true;
}

// function called by openthread when a UDP packet arrived
static void socket_udp_cb(void *aContext, otMessage *aMessage,
        const otMessageInfo *aMessageInfo) {

    pymesh_socket_t *sock = aContext;

//    otPlatLog(0, 0,"socket_udp_cb %p", sock->s);

    if (!sock) return;

    ot_rx_data_t rx_data;
    uint16_t payloadLength = otMessageGetLength(aMessage) - otMessageGetOffset(aMessage);

    if (payloadLength > sizeof(rx_data.data))
        payloadLength = sizeof(rx_data.data);

    rx_data.len = otMessageRead(aMessage, otMessageGetOffset(aMessage),
            rx_data.data, payloadLength);
    rx_data.src_port = aMessageInfo->mPeerPort;

    rx_data.src_ip = aMessageInfo->mPeerAddr;

//    char ip_str[MOD_USOCKET_IPV6_CHARS_MAX];
//    otIp6ToString(rx_data.src_ip, ip_str, MOD_USOCKET_IPV6_CHARS_MAX);
//    otPlatLog(0, 0,"socket_udp_cb %dB p=%d %s", rx_data.len, rx_data.src_port, ip_str);
//    otPlatLog(0, 0,"reg in queue, call handler");

    // store packet received in queue, to be consumed by socket.recvfrom()
    if (!xQueueSend(sock->rx_queue, (void *) &rx_data, 0)) {
        // queue is full, so we should drop the oldest item, to add the new one
        ot_rx_data_t rx_data_drop;
        xQueueReceive(sock->rx_queue, &rx_data_drop, 0);

        // try to store it again
        xQueueSend(sock->rx_queue, (void *) &rx_data, 0);
    }


    // callback to mpy if registered
    if (ot_obj.handler != mp_const_none) {
        //printf("Calling cb\n");
        mp_irq_queue_interrupt(rx_interrupt_queue_handler, (void *)&ot_obj);
    }

#if 0
    // code for sending back ACK pack to the same IP+port that this package was received

    buf[length] = '\0';
    otPlatLog(0, 0, "%s\n", buf);
    memcpy(&mPeer, aMessageInfo, sizeof(otMessageInfo));
    mPeer.
    // respond with ACK
    otError error = OT_ERROR_NONE;
    otMessage *message;

    otEXPECT_ACTION((message = otUdpNewMessage(ot, NULL)) != NULL, error = OT_ERROR_NO_BUFS);
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


// function called by openthread when an IPv6 datagram arrived
// in *aContext we could put the IP of the BR, based on which we should make the filtering
void br_ip6_rcv_cb(otMessage *aMessage, void *aContext){

    ipv6_and_udp_header_t header;

    if (border_router_prefix.mLength == 0)
    {
        // no border router set
//        otPlatLog(0, 0,"No Border Router set");
        return;
    }

    uint16_t messageLength = otMessageGetLength(aMessage);

    // suppose we have IPv6+UDP message, so offset is after header
    uint16_t messageOffset = sizeof(ipv6_and_udp_header_t);//otMessageGetOffset(aMessage);

    ot_rx_data_t rx_data;
    rx_data.len = messageLength - sizeof(ipv6_and_udp_header_t) + BORDER_ROUTER_HEADER_SIZE;

    if (rx_data.len > sizeof(rx_data.data))
        rx_data.len = sizeof(rx_data.data);

    // read data content of UDP datagram
    otMessageRead(aMessage, messageOffset, rx_data.data + BORDER_ROUTER_HEADER_SIZE, rx_data.len);

    // read ipv6+UDP datagram header, to find ipv6 source and destination and ports
    otMessageRead(aMessage, 0, (void*)&header, sizeof(ipv6_and_udp_header_t));

    // ports should be swapped, according to host endianess
    header.dst_port = HostSwap16(header.dst_port);
    header.src_port = HostSwap16(header.src_port);

    // create a small preamble(header), as BORDER_ROUTER_HEADER_1_CONST + dest IP + dest port
    rx_data.data[0] = BORDER_ROUTER_HEADER_1_CONST;

    // add IP destination
    memcpy( rx_data.data + BORDER_ROUTER_HEADER_1,
            &header.dest_ip6.mFields.m8[0],
            BORDER_ROUTER_HEADER_2);

    // add port destination
    rx_data.data[BORDER_ROUTER_HEADER_1 + BORDER_ROUTER_HEADER_2] = header.dst_port >> 8;
    rx_data.data[BORDER_ROUTER_HEADER_1 + BORDER_ROUTER_HEADER_2 + 1] = header.dst_port;

//    char source[MOD_USOCKET_IPV6_CHARS_MAX];
//    otIp6ToString(header.source_ip6, source, MOD_USOCKET_IPV6_CHARS_MAX);
//
//    char dest[MOD_USOCKET_IPV6_CHARS_MAX];
//    otIp6ToString(header.dest_ip6, dest, MOD_USOCKET_IPV6_CHARS_MAX);
//
//    //otPlatLog(0, 0,"IP data: len:%d, off: %d, prot: %d",messageLength, messageOffset, header.payload_type);
//    otPlatLog(0, 0,"IP data: len:%d dest: %s:%d, src: %s:%d",
//            messageLength, dest, header.dst_port, source, header.src_port);
    //otPlatLogBuf((char*)data, payloadLength);
    //otPlatLogBufHex(data, payloadLength);

    if (header.payload_type != IPV6_HEADER_UDP_PROTOCOL_CODE)
        return;

    // check if source matches BR prefix
    uint8_t matching_bits = otIp6PrefixMatch(&header.source_ip6, &border_router_prefix.mPrefix);
    //otPlatLog(0, 0,"matching_bits: %d", matching_bits);

    if (matching_bits < border_router_prefix.mLength)
        return;

    // the source IPv6 matches the BR prefix, so we should add data payload into the RX_queue

    // first, let's find the socket
    pymesh_socket_t *sock = NULL;

    for (int i = 0; i < UDP_SOCKETS_MAX ; i++) {
        if (sockets[i].s) {
            // valid slot occupied by a socket
            sock = &sockets[i];

            // search if this socket is opened for an interface with border router prefix
            matching_bits = otIp6PrefixMatch(&sockets[i].udp_sock.mSockName.mAddress,
                    &border_router_prefix.mPrefix);

            if (matching_bits >= border_router_prefix.mLength) {
//                otPlatLog(0, 0,"found sock: %p", sock->s);
                break;
            }
        }
    }
    // so, after the for, we'll have either the exact BR socket(if opened), or any valid socket

    if (!sock) {
//        otPlatLog(0, 0,"no sock found");
        return;
    }


    rx_data.dest_ip = header.dest_ip6;
    rx_data.src_ip = header.source_ip6;
    rx_data.dest_port = header.dst_port;
    rx_data.src_port = header.src_port;

    //otPlatLog(0, 0,"reg in queue, call handler %d", rx_data.src_port);

    // store packet received in queue, to be consumed by socket.recvfrom()

    if (!xQueueSend(sock->rx_queue, (void *) &rx_data, 0)) {
        // queue is full, so we should drop the oldest item, to add the new one
        ot_rx_data_t rx_data_drop;
        xQueueReceive(sock->rx_queue, &rx_data_drop, 0);

        // try to store it again
        xQueueSend(sock->rx_queue, (void *) &rx_data, 0);
    }

    // free message
    otMessageFree(aMessage);

    // callback to mpy if registered
    if (ot_obj.handler != mp_const_none) {
        //printf("Calling cb\n");
        mp_irq_queue_interrupt(rx_interrupt_queue_handler, (void *)&ot_obj);
    }
}

/******************************************************************************
 MICROPYTHON BINDINGS
 network.LoRa.Mesh module
 ******************************************************************************/

/*
 * openthread CLI callback, triggered when an response from openthread has to be published on REPL
 */
void otConsoleCb(const char *aBuf, uint16_t aBufLength, void *aContext){

    // check if "Done" was sent
    if (strncmp(aBuf, "Done", 4) != 0) {

        // Done not received

        if (MESH_CLI_OUTPUT_SIZE - mesh_obj.meshCliOutputLen < aBufLength)
            aBufLength = MESH_CLI_OUTPUT_SIZE - mesh_obj.meshCliOutputLen;

        memcpy(&mesh_obj.meshCliOutput[mesh_obj.meshCliOutputLen], aBuf, aBufLength);

        mesh_obj.meshCliOutputLen += aBufLength;

        // if Error received also signal done
        if ((strncmp(aBuf, "Error", 5) == 0) ||
            // a PING response, should signal done, too
            (NULL != strstr(aBuf, "icmp_seq=")))
        {
            mesh_obj.meshCliOutputDone = true;
        }
    }
    else {
        // "Done" received
        // cut the last 2 chars \r\n
        if (mesh_obj.meshCliOutputLen>2) mesh_obj.meshCliOutputLen-=2;
        mesh_obj.meshCliOutputDone = true;
    }
}

STATIC const mp_arg_t mesh_init_args[] = {
    { MP_QSTR_id,                             MP_ARG_INT,   {.u_int  = 0} },
    { MP_QSTR_key,         MP_ARG_KW_ONLY  | MP_ARG_OBJ,   {.u_obj  = MP_OBJ_NULL} },
};
/*
 * start Lora Mesh openthread
 */
STATIC mp_obj_t mesh_make_new(const mp_obj_type_t *type, mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *all_args) {
    mesh_obj_t *self = 0;

    // if ot_ready already started, do nothing
    if (!mesh_obj.ot_ready) {

        // parse args
        mp_map_t kw_args;
        mp_map_init_fixed_table(&kw_args, n_kw, all_args + n_args);
        // parse args
        mp_arg_val_t args[MP_ARRAY_SIZE(mesh_init_args)];
        mp_arg_parse_all(n_args, all_args, &kw_args, MP_ARRAY_SIZE(args), mesh_init_args, args);

        uint8_t master_key[OT_MASTER_KEY_SIZE] = {0x12, 0x34, 0xC0, 0xDE, 0x1A, 0xB5, 0x12, 0x34, 0xC0, 0xDE, 0x1A, 0xB5, 0xCA, 0x1A, 0x11, 0x0F};

        // get/set the Master Key
        if (args[1].u_obj != MP_OBJ_NULL) {
            mp_buffer_info_t bufinfo;
            mp_get_buffer_raise(args[1].u_obj, &bufinfo, MP_BUFFER_READ);
            memcpy(master_key, bufinfo.buf, sizeof(master_key));
        }

        modmesh_init();
        //printf("mesh task started\n");
        
        // setup the object
        self = (mesh_obj_t *)&mesh_obj;
        self->base.type = &lora_mesh_type;

        // must start Mesh
        if (NULL != openthread_init(master_key)) {

            // init CLI and send and \n
            mesh_obj.meshCliOutput[0] = 0;
            mesh_obj.meshCliOutputLen = 0;
            otCliConsoleInit(ot, (void*)&otConsoleCb, NULL);
            mesh_obj.otCliBufferLen = 0;
        }
        mesh_obj.ot_ready = (ot != NULL);
    }

    return (mp_obj_t)self;
}

/*
 * deinitialise Lora Mesh openthread, so LoRa can be used with Raw or LoRaWAN
 */
STATIC mp_obj_t mesh_deinit_cmd (mp_obj_t self_in) {
    
    openthread_deinit();
    vTaskDelete(xMeshTaskHndl);

    return mp_obj_new_bool(true);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mesh_deinit_obj, mesh_deinit_cmd);

/*
 * returns state of Mesh mode (either disabled, detached, Child, Router, Leader)
 */
STATIC mp_obj_t mesh_state_cmd (mp_obj_t self_in) {
    int state = mesh_state();
    return mp_obj_new_int(state);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mesh_state_obj, mesh_state_cmd);

/*
 * openthread CLI, console interface
 * commands available: https://github.com/openthread/openthread/blob/master/src/cli/README.md
 */
STATIC mp_obj_t mesh_cli(mp_obj_t self_in, mp_obj_t data) {
    //mesh_obj_t *self = self_in;
    int timeout = 5000;

    if (!mesh_obj.ot_ready){
        printf("Mesh not enabled\n");
        return mp_const_none;
    }

    mesh_obj.otCliBufferLen = 0;
    char *cmdstr = (char *)mp_obj_str_get_str(data);
    int len = strlen(cmdstr);

    if (len > sizeof(mesh_obj.otCliBuffer) - 2) {
        printf("Command exceeds 128 chars\n");
        return mp_const_none;
    }

    memset(mesh_obj.otCliBuffer, 0, sizeof(mesh_obj.otCliBuffer));
    strcpy(mesh_obj.otCliBuffer, cmdstr);

    mesh_obj.otCliBufferLen = len;
    mesh_obj.meshCliOutputDone = false;
    while (!mesh_obj.meshCliOutputDone && timeout >= 0) {
        mp_hal_delay_ms(300);
        timeout -= 300;
    }

    if (mesh_obj.meshCliOutputDone) {
        // micropy needs to output meshCliOutput string in size of meshCliOutputLen
        mp_obj_t res = mp_obj_new_str(mesh_obj.meshCliOutput, mesh_obj.meshCliOutputLen);

        mesh_obj.meshCliOutput[0] = 0;
        mesh_obj.meshCliOutputLen = 0;
        return res;
    }

    return mp_obj_new_str("", 0);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mesh_cli_obj, mesh_cli);

/*
 * returns true if Mesh node is single in the network
 */
STATIC mp_obj_t mesh_single (mp_obj_t self_in) {
    bool single = mesh_singleton();
    return mp_obj_new_bool(single);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mesh_single_obj, mesh_single);

/*
 * returns the RLOC code(2Bytes) of the Mesh node
 */
STATIC mp_obj_t mesh_rloc_cmd (mp_obj_t self_in) {
    int rloc = mesh_rloc();
    return mp_obj_new_int(rloc);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mesh_rloc_obj, mesh_rloc_cmd);

/*
 * returns all the unicast IPv6 addresses, that the node has (RLOC16, ML-EID, radio link)
 */
STATIC mp_obj_t mesh_ipaddr_cmd (mp_obj_t self_in) {
    mp_obj_t ipaddr_list[MESH_IP_ADDRESSES_MAX];
    char ipaddr[MESH_IP_ADDRESSES_MAX][MOD_USOCKET_IPV6_CHARS_MAX]; // list of max 5 IP addresses, 40 chars each
    int ip_num;

    ip_num = mesh_ipaddr(MESH_IP_ADDRESSES_MAX, MOD_USOCKET_IPV6_CHARS_MAX, ipaddr);
    if (ip_num > 0) {
        for (int i = 0; i < ip_num; i++){
            ipaddr_list[i] = mp_obj_new_str(ipaddr[i], strlen(ipaddr[i]));
        }
        return mp_obj_new_list(ip_num, ipaddr_list);
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mesh_ipaddr_obj, mesh_ipaddr_cmd);

/*
 * returns a list with all neighbors properties(mac, role, rloc16, rssi and age)
 */
STATIC mp_obj_t mesh_neigbors_cmd (mp_obj_t self_in) {
    //Ex: [(mac=72623859790382856, role=3, rloc16=18432, rssi=0, age=16)]
    static const qstr lora_ot_neighbors_info_fields[MESH_NEIGHBOR_FIELDS_NUM] = {
            MP_QSTR_mac, MP_QSTR_role, MP_QSTR_rloc16, MP_QSTR_rssi, MP_QSTR_age
    };

    mp_obj_t neighbors_list[MESH_NEIGBORS_MAX];
    mp_obj_t neighbor_tuple[MESH_NEIGHBOR_FIELDS_NUM];
    otNeighborInfo neighbors[MESH_NEIGBORS_MAX];
    long long mac;
    int nei_num = MESH_NEIGBORS_MAX;

    nei_num = mesh_neighbors(nei_num, neighbors);
    if (nei_num > 0) {
        for (int i = 0; i < nei_num; i++) {

            // create the tupple with current neighbor
            neighbor_tuple[1] = mp_obj_new_int(neighbors[i].mIsChild? OT_DEVICE_ROLE_CHILD : OT_DEVICE_ROLE_ROUTER);
            neighbor_tuple[2] = mp_obj_new_int_from_uint(neighbors[i].mRloc16);
            neighbor_tuple[3] = mp_obj_new_int(neighbors[i].mAverageRssi);
            neighbor_tuple[4] = mp_obj_new_int_from_uint(neighbors[i].mAge);

            // create MAC as 64bit integer
            mac = 0;
            for (size_t j = 0; j < OT_EXT_ADDRESS_SIZE; j++)
                mac = (mac << 8) + neighbors[i].mExtAddress.m8[j];

            neighbor_tuple[0] = mp_obj_new_int_from_ll(mac);

            // add the tupple in the list

            neighbors_list[i] = mp_obj_new_attrtuple(lora_ot_neighbors_info_fields,
                    sizeof(neighbor_tuple) / sizeof(neighbor_tuple[0]),
                    neighbor_tuple);
        }
        return mp_obj_new_list(nei_num, neighbors_list);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mesh_neighbors_obj, mesh_neigbors_cmd);


/*
 * returns a list with all routers properties (mac, role, rloc16, rssi and age)
 */
STATIC mp_obj_t mesh_routers_cmd (mp_obj_t self_in) {
    //Ex: [(mac=0, rloc16=11264, id=11, path_cost=0, age=0), (mac=72623859790382856, rloc16=18432, id=18, path_cost=0, age=15)]
    static const qstr mesh_esh_routers_info_fields[MESH_ROUTERS_FIELDS_NUM] = {
            MP_QSTR_mac, MP_QSTR_rloc16, MP_QSTR_id, MP_QSTR_path_cost, MP_QSTR_age
    };

    mp_obj_t routers_list[MESH_ROUTERS_MAX];
    mp_obj_t router_tuple[MESH_ROUTERS_FIELDS_NUM];
    otRouterInfo routers[MESH_ROUTERS_MAX];
    long long mac;
    int routers_num = MESH_ROUTERS_MAX;

    routers_num = mesh_routers(routers_num, routers);
    if (routers_num > 0) {
        for (int i = 0; i < routers_num; i++) {

            // create the tupple with current neighbor
            router_tuple[1] = mp_obj_new_int_from_uint(routers[i].mRloc16);
            router_tuple[2] = mp_obj_new_int_from_uint(routers[i].mRouterId);
            router_tuple[3] = mp_obj_new_int_from_uint(routers[i].mPathCost);
            router_tuple[4] = mp_obj_new_int_from_uint(routers[i].mAge);

            // create MAC as 64bit integer
            mac = 0;
            for (size_t j = 0; j < OT_EXT_ADDRESS_SIZE; j++)
                mac = (mac << 8) + routers[i].mExtAddress.m8[j];

            router_tuple[0] = mp_obj_new_int_from_ll(mac);

            // add the tupple in the list

            routers_list[i] = mp_obj_new_attrtuple(mesh_esh_routers_info_fields,
                    sizeof(router_tuple) / sizeof(router_tuple[0]),
                    router_tuple);
        }
        return mp_obj_new_list(routers_num, routers_list);
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mesh_routers_obj, mesh_routers_cmd);

/*
 * returns a list with all Leader properties(partition, mac and rloc16)
 */
STATIC mp_obj_t mesh_leader (mp_obj_t self_in) {
    // Ex: (part_id=15338162, mac=72623859790382856, rloc16=18432)
    static const qstr lora_ot_leader_info_fields[MESH_LEADER_FIELDS_NUM] = {
            MP_QSTR_part_id, MP_QSTR_mac, MP_QSTR_rloc16
    };

    mp_obj_t leader_tuple[MESH_LEADER_FIELDS_NUM];
    otRouterInfo leaderRouterData;
    otLeaderData leaderData;
    long long mac;

    if (mesh_leader_data(&leaderRouterData, &leaderData) > 0) {

        // create the tupple with Leader Data
        leader_tuple[0] = mp_obj_new_int_from_uint(leaderData.mPartitionId);

        // create MAC as 64bit integer
        mac = 0;
        for (size_t j = 0; j < OT_EXT_ADDRESS_SIZE; j++)
            mac = (mac << 8) + leaderRouterData.mExtAddress.m8[j];
        leader_tuple[1] = mp_obj_new_int_from_ll(mac);

        leader_tuple[2] = mp_obj_new_int_from_uint(leaderRouterData.mRloc16);

        return mp_obj_new_attrtuple(lora_ot_leader_info_fields,
                    sizeof(leader_tuple) / sizeof(leader_tuple[0]),
                    leader_tuple);
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mesh_leader_obj, mesh_leader);

/*
 * register a callback(with argument) to be triggered when mesh interface receives data
 */
STATIC mp_obj_t mesh_rx_cb (mp_obj_t self_in, mp_obj_t cb_obj, mp_obj_t cb_arg_obj) {
    int res = 0;

    if ((mesh_obj.ot_ready) && (cb_obj != mp_const_none)) {

        mesh_rx_callback(cb_obj, cb_arg_obj);
        res = 1;
    }
    return mp_obj_new_int(res);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(mesh_rx_cb_obj, mesh_rx_cb);


/*
 * list the Border Router entries, if no param OR
 * register a Border Router for the mesh network, by its network addres (on-mesh prefix) and preference
 */
STATIC mp_obj_t mesh_border_router (mp_uint_t n_args, const mp_obj_t *args) {

    if (n_args == 1) {
        // list the BR entries
        static const qstr mesh_br_info_fields[MESH_BR_FIELDS_NUM] = {
                MP_QSTR_net, MP_QSTR_preference};

        mp_obj_t br_list[MESH_BR_MAX];
        mp_obj_t br_tuple[MESH_BR_FIELDS_NUM];
        border_router_info_t brouters[MESH_BR_MAX];
        int br_num = MESH_BR_MAX;

        br_num = mesh_br_list(br_num, brouters);
        for (int i = 0; i < br_num; i++) {

            // create the tupple with current neighbor
            br_tuple[0] = mp_obj_new_str(brouters[i].net, strlen(brouters[i].net));
            br_tuple[1] = mp_obj_new_int((int)brouters[i].preference);

            // add the tupple in the list
            br_list[i] = mp_obj_new_attrtuple(mesh_br_info_fields,
                    sizeof(br_tuple) / sizeof(br_tuple[0]),
                    br_tuple);
        }
        return mp_obj_new_list(br_num, br_list);

    } else if (n_args == 3) {
        // add new Border Router (by net address and preference level)

        const char *ipv6_net_str = mp_obj_str_get_str(args[1]);
        int preference_lvl = mp_obj_get_int(args[2]);

        return mp_obj_new_int(mesh_add_border_router(ipv6_net_str, preference_lvl));
    }
    // no good params
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mesh_border_router_obj, 1, 3, mesh_border_router);

/*
 * delete a Border Router entry
 */
STATIC mp_obj_t mesh_border_router_del (mp_obj_t self_in, mp_obj_t br_prefixj) {

    const char *ipv6_net_str = mp_obj_str_get_str(br_prefixj);

    return mp_obj_new_int(mesh_del_border_router(ipv6_net_str));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mesh_border_router_del_obj, mesh_border_router_del);

STATIC const mp_map_elem_t mesh_locals_dict_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR_state),                   (mp_obj_t)&mesh_state_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_cli),                     (mp_obj_t)&mesh_cli_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_single),                  (mp_obj_t)&mesh_single_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_rloc),                    (mp_obj_t)&mesh_rloc_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ipaddr),                  (mp_obj_t)&mesh_ipaddr_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_neighbors),               (mp_obj_t)&mesh_neighbors_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_routers),                 (mp_obj_t)&mesh_routers_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_leader),                  (mp_obj_t)&mesh_leader_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_rx_cb),                   (mp_obj_t)&mesh_rx_cb_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_border_router),           (mp_obj_t)&mesh_border_router_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_border_router_del),       (mp_obj_t)&mesh_border_router_del_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_deinit),                  (mp_obj_t)&mesh_deinit_obj },
};

STATIC MP_DEFINE_CONST_DICT(mesh_locals_dict, mesh_locals_dict_table);

// sub-module of LoRa module (modlora.c) 
const mp_obj_type_t lora_mesh_type = {
    { &mp_type_type },
    .name = MP_QSTR_Mesh,
    .make_new = mesh_make_new,
    .locals_dict = (mp_obj_t)&mesh_locals_dict,
};
