/*
 * Copyright (c) 2016, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#include "py/mpstate.h"
#include "py/obj.h"

//#include "pybrtc.h"
#include "ftp.h"
#include "updater.h"
#include "modnetwork.h"
//#include "modwlan.h"
#include "modusocket.h"
//#include "debug.h"
#include "serverstask.h"
#include "ff.h"
#include "fifo.h"
#include "socketfifo.h"
#include "timeutils.h"
#include "moduos.h"

#include "heap_alloc_caps.h"
#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"
#include "esp_event.h"

#include "esp_wifi.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/******************************************************************************
 DEFINE PRIVATE CONSTANTS
 ******************************************************************************/
#define FTP_CMD_PORT                        21
#define FTP_ACTIVE_DATA_PORT                20
#define FTP_PASIVE_DATA_PORT                2024
#define FTP_BUFFER_SIZE                     512
#define FTP_TX_RETRIES_MAX                  25
#define FTP_CMD_SIZE_MAX                    6
#define FTP_CMD_CLIENTS_MAX                 1
#define FTP_DATA_CLIENTS_MAX                1
#define FTP_MAX_PARAM_SIZE                  (MICROPY_ALLOC_PATH_MAX + 1)
#define FTP_UNIX_TIME_20000101              946684800
#define FTP_UNIX_TIME_20150101              1420070400
#define FTP_UNIX_SECONDS_180_DAYS           15552000
#define FTP_DATA_TIMEOUT_MS                 5000            // 5 seconds
#define FTP_SOCKETFIFO_ELEMENTS_MAX         4
#define FTP_CYCLE_TIME_MS                   (SERVERS_CYCLE_TIME_MS * 2)

/******************************************************************************
 DEFINE PRIVATE TYPES
 ******************************************************************************/
typedef enum {
    E_FTP_RESULT_OK = 0,
    E_FTP_RESULT_CONTINUE,
    E_FTP_RESULT_FAILED
} ftp_result_t;

typedef enum {
    E_FTP_STE_DISABLED = 0,
    E_FTP_STE_START,
    E_FTP_STE_READY,
    E_FTP_STE_END_TRANSFER,
    E_FTP_STE_CONTINUE_LISTING,
    E_FTP_STE_CONTINUE_FILE_TX,
    E_FTP_STE_CONTINUE_FILE_RX
} ftp_state_t;

typedef enum {
    E_FTP_STE_SUB_DISCONNECTED = 0,
    E_FTP_STE_SUB_LISTEN_FOR_DATA,
    E_FTP_STE_SUB_DATA_CONNECTED
} ftp_substate_t;

typedef struct {
    bool            uservalid : 1;
    bool            passvalid : 1;
} ftp_loggin_t;

typedef enum {
    E_FTP_NOTHING_OPEN = 0,
    E_FTP_FILE_OPEN,
    E_FTP_DIR_OPEN
} ftp_e_open_t;

typedef enum {
    E_FTP_CLOSE_NONE = 0,
    E_FTP_CLOSE_DATA,
    E_FTP_CLOSE_CMD_AND_DATA,
} ftp_e_closesocket_t;

typedef struct {
    uint8_t             *dBuffer;
    uint32_t            ctimeout;
    union {
        DIR             dp;
        FIL             fp;
    };
    int32_t             lc_sd;
    int32_t             ld_sd;
    int32_t             c_sd;
    int32_t             d_sd;
    int32_t             dtimeout;
    uint32_t            volcount;
    uint32_t            ip_addr;
    uint8_t             state;
    uint8_t             substate;
    uint8_t             txRetries;
    uint8_t             logginRetries;
    ftp_loggin_t        loggin;
    uint8_t             e_open;
    bool                closechild;
    bool                enabled;
    bool                special_file;
    bool                listroot;
} ftp_data_t;

typedef struct {
    char * cmd;
} ftp_cmd_t;

typedef struct {
    char * month;
} ftp_month_t;

typedef enum {
    E_FTP_CMD_NOT_SUPPORTED = -1,
    E_FTP_CMD_FEAT = 0,
    E_FTP_CMD_SYST,
    E_FTP_CMD_CDUP,
    E_FTP_CMD_CWD,
    E_FTP_CMD_PWD,
    E_FTP_CMD_XPWD,
    E_FTP_CMD_SIZE,
    E_FTP_CMD_MDTM,
    E_FTP_CMD_TYPE,
    E_FTP_CMD_USER,
    E_FTP_CMD_PASS,
    E_FTP_CMD_PASV,
    E_FTP_CMD_LIST,
    E_FTP_CMD_RETR,
    E_FTP_CMD_STOR,
    E_FTP_CMD_DELE,
    E_FTP_CMD_RMD,
    E_FTP_CMD_MKD,
    E_FTP_CMD_RNFR,
    E_FTP_CMD_RNTO,
    E_FTP_CMD_NOOP,
    E_FTP_CMD_QUIT,
    E_FTP_NUM_FTP_CMDS
} ftp_cmd_index_t;

/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/
static ftp_data_t ftp_data;
static char *ftp_path;
static char *ftp_scratch_buffer;
static char *ftp_cmd_buffer;
static const ftp_cmd_t ftp_cmd_table[] = { { "FEAT" }, { "SYST" }, { "CDUP" }, { "CWD"  },
                                           { "PWD"  }, { "XPWD" }, { "SIZE" }, { "MDTM" },
                                           { "TYPE" }, { "USER" }, { "PASS" }, { "PASV" },
                                           { "LIST" }, { "RETR" }, { "STOR" }, { "DELE" },
                                           { "RMD"  }, { "MKD"  }, { "RNFR" }, { "RNTO" },
                                           { "NOOP" }, { "QUIT" } };

static const ftp_month_t ftp_month[] = { { "Jan" }, { "Feb" }, { "Mar" }, { "Apr" },
                                         { "May" }, { "Jun" }, { "Jul" }, { "Ago" },
                                         { "Sep" }, { "Oct" }, { "Nov" }, { "Dec" } };

static SocketFifoElement_t ftp_fifoelements[FTP_SOCKETFIFO_ELEMENTS_MAX];
static FIFO_t ftp_socketfifo;

/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/
static void ftp_wait_for_enabled (void);
static bool ftp_create_listening_socket (int32_t *sd, uint32_t port, uint8_t backlog);
static ftp_result_t ftp_wait_for_connection (int32_t l_sd, int32_t *n_sd, uint32_t *ip_addr);
static ftp_result_t ftp_send_non_blocking (int32_t sd, void *data, int32_t Len);
static void ftp_send_reply (uint32_t status, char *message);
static void ftp_send_data (uint32_t datasize);
static void ftp_send_from_fifo (void);
static ftp_result_t ftp_recv_non_blocking (int32_t sd, void *buff, int32_t Maxlen, int32_t *rxLen);
static void ftp_process_cmd (void);
static void ftp_close_files (void);
static void ftp_close_filesystem_on_error (void);
static void ftp_close_cmd_data (void);
static ftp_cmd_index_t ftp_pop_command (char **str);
static void ftp_pop_param (char **str, char *param);
static int ftp_print_eplf_item (char *dest, uint32_t destsize, FILINFO *fno);
static int ftp_print_eplf_drive (char *dest, uint32_t destsize, char *name);
static bool ftp_open_file (const char *path, int mode);
static ftp_result_t ftp_read_file (char *filebuf, uint32_t desiredsize, uint32_t *actualsize);
static ftp_result_t ftp_write_file (char *filebuf, uint32_t size);
static ftp_result_t ftp_open_dir_for_listing (const char *path);
static ftp_result_t ftp_list_dir (char *list, uint32_t maxlistsize, uint32_t *listsize);
static void ftp_open_child (char *pwd, char *dir);
static void ftp_close_child (char *pwd);
static void ftp_return_to_previous_path (char *pwd, char *dir);

/******************************************************************************
 DEFINE PUBLIC FUNCTIONS
 ******************************************************************************/
void ftp_init (void) {
    // Allocate memory for the data buffer, and the file system structs (from the RTOS heap)
    ftp_data.dBuffer = pvPortMalloc(FTP_BUFFER_SIZE);
    ftp_path = pvPortMalloc(FTP_MAX_PARAM_SIZE);
    ftp_scratch_buffer = pvPortMalloc(FTP_MAX_PARAM_SIZE);
    ftp_cmd_buffer = pvPortMalloc(FTP_MAX_PARAM_SIZE + FTP_CMD_SIZE_MAX);
    SOCKETFIFO_Init (&ftp_socketfifo, (void *)ftp_fifoelements, FTP_SOCKETFIFO_ELEMENTS_MAX);
    ftp_data.c_sd  = -1;
    ftp_data.d_sd  = -1;
    ftp_data.lc_sd = -1;
    ftp_data.ld_sd = -1;
    ftp_data.e_open = E_FTP_NOTHING_OPEN;
    ftp_data.state = E_FTP_STE_DISABLED;
    ftp_data.substate = E_FTP_STE_SUB_DISCONNECTED;
    ftp_data.special_file = false;
    ftp_data.volcount = 0;
}

void ftp_run (void) {
    switch (ftp_data.state) {
        case E_FTP_STE_DISABLED:
            ftp_wait_for_enabled();
            break;
        case E_FTP_STE_START:
            if (/*wlan_is_connected() && */ ftp_create_listening_socket(&ftp_data.lc_sd, FTP_CMD_PORT, FTP_CMD_CLIENTS_MAX - 1)) {
                ftp_data.state = E_FTP_STE_READY;
            }
            break;
        case E_FTP_STE_READY:
            if (ftp_data.c_sd < 0 && ftp_data.substate == E_FTP_STE_SUB_DISCONNECTED) {
                if (E_FTP_RESULT_OK == ftp_wait_for_connection(ftp_data.lc_sd, &ftp_data.c_sd, &ftp_data.ip_addr)) {
                    ftp_data.txRetries = 0;
                    ftp_data.logginRetries = 0;
                    ftp_data.ctimeout = 0;
                    ftp_data.loggin.uservalid = false;
                    ftp_data.loggin.passvalid = false;
                    strcpy (ftp_path, "/");
                    ftp_send_reply (220, "Micropython FTP Server");
                    break;
                }
            }
            if (SOCKETFIFO_IsEmpty()) {
                if (ftp_data.c_sd > 0 && ftp_data.substate != E_FTP_STE_SUB_LISTEN_FOR_DATA) {
                    ftp_process_cmd();
                    if (ftp_data.state != E_FTP_STE_READY) {
                        break;
                    }
                }
            }
            break;
        case E_FTP_STE_END_TRANSFER:
            break;
        case E_FTP_STE_CONTINUE_LISTING:
            // go on with listing only if the transmit buffer is empty
            if (SOCKETFIFO_IsEmpty()) {
                uint32_t listsize;
                ftp_list_dir((char *)ftp_data.dBuffer, FTP_BUFFER_SIZE, &listsize);
                if (listsize > 0) {
                    ftp_send_data(listsize);
                } else {
                    ftp_send_reply(226, NULL);
                    ftp_data.state = E_FTP_STE_END_TRANSFER;
                }
                ftp_data.ctimeout = 0;
            }
            break;
        case E_FTP_STE_CONTINUE_FILE_TX:
            // read the next block from the file only if the previous one has been sent
            if (SOCKETFIFO_IsEmpty()) {
                uint32_t readsize;
                ftp_result_t result;
                ftp_data.ctimeout = 0;
                result = ftp_read_file ((char *)ftp_data.dBuffer, FTP_BUFFER_SIZE, &readsize);
                if (result == E_FTP_RESULT_FAILED) {
                    ftp_send_reply(451, NULL);
                    ftp_data.state = E_FTP_STE_END_TRANSFER;
                } else {
                    if (readsize > 0) {
                        ftp_send_data(readsize);
                    }
                    if (result == E_FTP_RESULT_OK) {
                        ftp_send_reply(226, NULL);
                        ftp_data.state = E_FTP_STE_END_TRANSFER;
                    }
                }
            }
            break;
        case E_FTP_STE_CONTINUE_FILE_RX:
            if (SOCKETFIFO_IsEmpty()) {
                int32_t len;
                ftp_result_t result;
                if (E_FTP_RESULT_OK == (result = ftp_recv_non_blocking(ftp_data.d_sd, ftp_data.dBuffer, FTP_BUFFER_SIZE, &len))) {
                    ftp_data.dtimeout = 0;
                    ftp_data.ctimeout = 0;
                    // its a software update
                    if (ftp_data.special_file) {
                        if (updater_write(ftp_data.dBuffer, len)) {
                            break;
                        }
                    }
                    // user file being received
                    else if (E_FTP_RESULT_OK == ftp_write_file ((char *)ftp_data.dBuffer, len)) {
                        break;
                    }
                    ftp_send_reply(451, NULL);
                    ftp_data.state = E_FTP_STE_END_TRANSFER;
                } else if (result == E_FTP_RESULT_CONTINUE) {
                    if (ftp_data.dtimeout++ > FTP_DATA_TIMEOUT_MS / FTP_CYCLE_TIME_MS) {
                        ftp_close_files();
                        ftp_send_reply(426, NULL);
                        ftp_data.state = E_FTP_STE_END_TRANSFER;
                    }
                }
                else {
                    if (ftp_data.special_file) {
                        ftp_data.special_file = false;
                        updater_finish();
                    }
                    ftp_close_files();
                    ftp_send_reply(226, NULL);
                    ftp_data.state = E_FTP_STE_END_TRANSFER;
                }
            }
            break;
        default:
            break;
    }

    switch (ftp_data.substate) {
    case E_FTP_STE_SUB_DISCONNECTED:
        break;
    case E_FTP_STE_SUB_LISTEN_FOR_DATA:
        if (E_FTP_RESULT_OK == ftp_wait_for_connection(ftp_data.ld_sd, &ftp_data.d_sd, NULL)) {
            ftp_data.dtimeout = 0;
            ftp_data.substate = E_FTP_STE_SUB_DATA_CONNECTED;
        } else if (ftp_data.dtimeout++ > FTP_DATA_TIMEOUT_MS / FTP_CYCLE_TIME_MS) {
            ftp_data.dtimeout = 0;
            // close the listening socket
            servers_close_socket(&ftp_data.ld_sd);
            ftp_data.substate = E_FTP_STE_SUB_DISCONNECTED;
        }
        break;
    case E_FTP_STE_SUB_DATA_CONNECTED:
        if (ftp_data.state == E_FTP_STE_READY && ftp_data.dtimeout++ > FTP_DATA_TIMEOUT_MS / FTP_CYCLE_TIME_MS) {
            // close the listening and the data socket
            servers_close_socket(&ftp_data.ld_sd);
            servers_close_socket(&ftp_data.d_sd);
            ftp_close_filesystem_on_error ();
            ftp_data.substate = E_FTP_STE_SUB_DISCONNECTED;
        }
        break;
    default:
        break;
    }

    // send data pending in the queue
    ftp_send_from_fifo();

    // check the state of the data sockets
    if (ftp_data.d_sd < 0 && (ftp_data.state > E_FTP_STE_READY)) {
        ftp_data.substate = E_FTP_STE_SUB_DISCONNECTED;
        ftp_data.state = E_FTP_STE_READY;
    }
}

void ftp_enable (void) {
    ftp_data.enabled = true;
}

void ftp_disable (void) {
    ftp_reset();
    ftp_data.enabled = false;
    ftp_data.state = E_FTP_STE_DISABLED;
}

void ftp_reset (void) {
    // close all connections and start all over again
    servers_close_socket(&ftp_data.lc_sd);
    servers_close_socket(&ftp_data.ld_sd);
    ftp_close_cmd_data();
    ftp_data.state = E_FTP_STE_START;
    ftp_data.substate = E_FTP_STE_SUB_DISCONNECTED;
    ftp_data.volcount = 0;
    SOCKETFIFO_Flush();
}

/******************************************************************************
 DEFINE PRIVATE FUNCTIONS
 ******************************************************************************/
static void ftp_wait_for_enabled (void) {
    // Check if the telnet service has been enabled
    if (ftp_data.enabled) {
        ftp_data.state = E_FTP_STE_START;
    }
}

static bool ftp_create_listening_socket (int32_t *sd, uint32_t port, uint8_t backlog) {
    struct sockaddr_in sServerAddress;
    int32_t _sd;
    int32_t result;

    // open a socket for ftp data listen
    *sd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    _sd = *sd;

    if (_sd > 0) {
        // add the new socket to the network administration
        modusocket_socket_add(_sd, false);

        // enable non-blocking mode
        uint32_t option = fcntl(_sd, F_GETFL, 0);
        option |= O_NONBLOCK;
        fcntl(_sd, F_SETFL, option);

        // enable address reusing
        option = 1;
        result = setsockopt(_sd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

        // bind the socket to a port number
        sServerAddress.sin_family = AF_INET;
        sServerAddress.sin_addr.s_addr = INADDR_ANY;
        sServerAddress.sin_len = sizeof(sServerAddress);
        sServerAddress.sin_port = htons(port);

        result |= bind(_sd, (const struct sockaddr *)&sServerAddress, sizeof(sServerAddress));

        // start listening
        result |= listen (_sd, backlog);

        if (!result) {
            return true;
        }
        servers_close_socket(sd);
    }
    return false;
}

static ftp_result_t ftp_wait_for_connection (int32_t l_sd, int32_t *n_sd, uint32_t *ip_addr) {
    struct sockaddr_in  sClientAddress;
    socklen_t  in_addrSize;

    // accepts a connection from a TCP client, if there is any, otherwise returns EAGAIN
    *n_sd = accept(l_sd, (struct sockaddr *)&sClientAddress, (socklen_t *)&in_addrSize);
    int32_t _sd = *n_sd;
    if (_sd < 0) {
        if (errno == EAGAIN) {
            return E_FTP_RESULT_CONTINUE;
        }
        // error
        ftp_reset();
        return E_FTP_RESULT_FAILED;
    }

    if (ip_addr) {
        tcpip_adapter_ip_info_t ip_info;
        wifi_mode_t wifi_mode;
        esp_wifi_get_mode(&wifi_mode);
        if (wifi_mode != WIFI_MODE_APSTA) {
            // easy way
            tcpip_adapter_if_t if_type;
            if (wifi_mode == WIFI_MODE_AP) {
                if_type = TCPIP_ADAPTER_IF_AP;
            } else {
                if_type = TCPIP_ADAPTER_IF_STA;
            }
            tcpip_adapter_get_ip_info(if_type, &ip_info);
        } else {
            // see on which subnet is the client ip address
            tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ip_info);
            if ((ip_info.ip.addr & ip_info.netmask.addr) != (ip_info.netmask.addr & sClientAddress.sin_addr.s_addr)) {
                tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info);
            }
        }
        *ip_addr = ip_info.ip.addr;
    }

    // add the new socket to the network administration
    modusocket_socket_add(_sd, false);

    // enable non-blocking mode
    uint32_t option = fcntl(_sd, F_GETFL, 0);
    option |= O_NONBLOCK;
    fcntl(_sd, F_SETFL, option);

    // client connected, so go on
    return E_FTP_RESULT_OK;
}

static ftp_result_t ftp_send_non_blocking (int32_t sd, void *data, int32_t Len) {
    int32_t result = send(sd, data, Len, 0);

    if (result > 0) {
        ftp_data.txRetries = 0;
        return E_FTP_RESULT_OK;
    } else if ((FTP_TX_RETRIES_MAX >= ++ftp_data.txRetries) && (errno == EAGAIN)) {
        return E_FTP_RESULT_CONTINUE;
    } else {
        // error
        ftp_reset();
        return E_FTP_RESULT_FAILED;
    }
}

static void ftp_send_reply (uint32_t status, char *message) {
    SocketFifoElement_t fifoelement;
    if (!message) {
        message = "";
    }
    snprintf((char *)ftp_cmd_buffer, 4, "%u", status);
    strcat ((char *)ftp_cmd_buffer, " ");
    strcat ((char *)ftp_cmd_buffer, message);
    strcat ((char *)ftp_cmd_buffer, "\r\n");
    fifoelement.sd = &ftp_data.c_sd;
    fifoelement.datasize = strlen((char *)ftp_cmd_buffer);
    fifoelement.data = pvPortMalloc(fifoelement.datasize);
    if (status == 221) {
        fifoelement.closesockets = E_FTP_CLOSE_CMD_AND_DATA;
    } else if (status == 426 || status == 451 || status == 550) {
        fifoelement.closesockets = E_FTP_CLOSE_DATA;
    } else {
        fifoelement.closesockets = E_FTP_CLOSE_NONE;
    }
    fifoelement.freedata = true;
    if (fifoelement.data) {
        memcpy (fifoelement.data, ftp_cmd_buffer, fifoelement.datasize);
        if (!SOCKETFIFO_Push (&fifoelement)) {
            vPortFree(fifoelement.data);
        }
    }
}

static void ftp_send_data (uint32_t datasize) {
    SocketFifoElement_t fifoelement;

    fifoelement.data = ftp_data.dBuffer;
    fifoelement.datasize = datasize;
    fifoelement.sd = &ftp_data.d_sd;
    fifoelement.closesockets = E_FTP_CLOSE_NONE;
    fifoelement.freedata = false;
    SOCKETFIFO_Push (&fifoelement);
}

static void ftp_send_from_fifo (void) {
    SocketFifoElement_t fifoelement;
    if (SOCKETFIFO_Peek (&fifoelement)) {
        int32_t _sd = *fifoelement.sd;
        if (_sd > 0) {
            if (E_FTP_RESULT_OK == ftp_send_non_blocking (_sd, fifoelement.data, fifoelement.datasize)) {
                SOCKETFIFO_Pop (&fifoelement);
                if (fifoelement.closesockets != E_FTP_CLOSE_NONE) {
                    servers_close_socket(&ftp_data.d_sd);
                    if (fifoelement.closesockets == E_FTP_CLOSE_CMD_AND_DATA) {
                        servers_close_socket(&ftp_data.ld_sd);
                        // this one is the command socket
                        servers_close_socket(fifoelement.sd);
                        ftp_data.substate = E_FTP_STE_SUB_DISCONNECTED;
                    }
                    ftp_close_filesystem_on_error();
                }
                if (fifoelement.freedata) {
                    vPortFree(fifoelement.data);
                }
            }
        } else { // socket closed, remove it from the queue
            SOCKETFIFO_Pop (&fifoelement);
            if (fifoelement.freedata) {
                vPortFree(fifoelement.data);
            }
        }
    } else if (ftp_data.state == E_FTP_STE_END_TRANSFER && (ftp_data.d_sd > 0)) {
        // close the listening and the data sockets
        servers_close_socket(&ftp_data.ld_sd);
        servers_close_socket(&ftp_data.d_sd);
        if (ftp_data.special_file) {
            ftp_data.special_file = false;
        }
    }
}

static ftp_result_t ftp_recv_non_blocking (int32_t sd, void *buff, int32_t Maxlen, int32_t *rxLen) {
    *rxLen = recv(sd, buff, Maxlen, 0);

    if (*rxLen > 0) {
        return E_FTP_RESULT_OK;
    } else if (errno != EAGAIN) {
        // error
        return E_FTP_RESULT_FAILED;
    }
    return E_FTP_RESULT_CONTINUE;
}

static void ftp_get_param_and_open_child (char **bufptr) {
    ftp_pop_param (bufptr, ftp_scratch_buffer);
    ftp_open_child (ftp_path, ftp_scratch_buffer);
    ftp_data.closechild = true;
}

static void ftp_process_cmd (void) {
    int32_t len;
    char *bufptr = (char *)ftp_cmd_buffer;
    ftp_result_t result;
    FRESULT fres;
    FILINFO fno;

    ftp_data.closechild = false;
    // also use the reply buffer to receive new commands
    if (E_FTP_RESULT_OK == (result = ftp_recv_non_blocking(ftp_data.c_sd, ftp_cmd_buffer, FTP_MAX_PARAM_SIZE + FTP_CMD_SIZE_MAX, &len))) {
        // bufptr is moved as commands are being popped
        ftp_cmd_index_t cmd = ftp_pop_command(&bufptr);
        if (!ftp_data.loggin.passvalid && (cmd != E_FTP_CMD_USER && cmd != E_FTP_CMD_PASS && cmd != E_FTP_CMD_QUIT)) {
            ftp_send_reply(332, NULL);
            return;
        }
        switch (cmd) {
        case E_FTP_CMD_FEAT:
            ftp_send_reply(211, "no-features");
            break;
        case E_FTP_CMD_SYST:
            ftp_send_reply(215, "UNIX Type: L8");
            break;
        case E_FTP_CMD_CDUP:
            ftp_close_child(ftp_path);
            ftp_send_reply(250, NULL);
            break;
        case E_FTP_CMD_CWD:
            {
                fres = FR_NO_PATH;
                ftp_pop_param (&bufptr, ftp_scratch_buffer);
                ftp_open_child (ftp_path, ftp_scratch_buffer);
                if ((ftp_path[0] == '/' && ftp_path[1] == '\0') || ((fres = f_opendir (&ftp_data.dp, ftp_path)) == FR_OK)) {
                    if (fres == FR_OK) {
                        f_closedir(&ftp_data.dp);
                    }
                    ftp_send_reply(250, NULL);
                } else {
                    ftp_close_child (ftp_path);
                    ftp_send_reply(550, NULL);
                }
            }
            break;
        case E_FTP_CMD_PWD:
        case E_FTP_CMD_XPWD:
            ftp_send_reply(257, ftp_path);
            break;
        case E_FTP_CMD_SIZE:
            {
                ftp_get_param_and_open_child (&bufptr);
                if (FR_OK == f_stat (ftp_path, &fno)) {
                    // send the size
                    snprintf((char *)ftp_data.dBuffer, FTP_BUFFER_SIZE, "%u", (uint32_t)fno.fsize);
                    ftp_send_reply(213, (char *)ftp_data.dBuffer);
                } else {
                    ftp_send_reply(550, NULL);
                }
            }
            break;
        case E_FTP_CMD_MDTM:
            ftp_get_param_and_open_child (&bufptr);
            if (FR_OK == f_stat (ftp_path, &fno)) {
                // send the last modified time
                snprintf((char *)ftp_data.dBuffer, FTP_BUFFER_SIZE, "%u%02u%02u%02u%02u%02u",
                         1980 + ((fno.fdate >> 9) & 0x7f), (fno.fdate >> 5) & 0x0f,
                         fno.fdate & 0x1f, (fno.ftime >> 11) & 0x1f,
                         (fno.ftime >> 5) & 0x3f, 2 * (fno.ftime & 0x1f));
                ftp_send_reply(213, (char *)ftp_data.dBuffer);
            } else {
                ftp_send_reply(550, NULL);
            }
            break;
        case E_FTP_CMD_TYPE:
            ftp_send_reply(200, NULL);
            break;
        case E_FTP_CMD_USER:
            ftp_pop_param (&bufptr, ftp_scratch_buffer);
            if (!memcmp(ftp_scratch_buffer, servers_user, MAX(strlen(ftp_scratch_buffer), strlen(servers_user)))) {
                ftp_data.loggin.uservalid = true && (strlen(servers_user) == strlen(ftp_scratch_buffer));
            }
            ftp_send_reply(331, NULL);
            break;
        case E_FTP_CMD_PASS:
            ftp_pop_param (&bufptr, ftp_scratch_buffer);
            if (!memcmp(ftp_scratch_buffer, servers_pass, MAX(strlen(ftp_scratch_buffer), strlen(servers_pass))) &&
                    ftp_data.loggin.uservalid) {
                ftp_data.loggin.passvalid = true && (strlen(servers_pass) == strlen(ftp_scratch_buffer));
                if (ftp_data.loggin.passvalid) {
                    ftp_send_reply(230, NULL);
                    break;
                }
            }
            ftp_send_reply(530, NULL);
            break;
        case E_FTP_CMD_PASV:
            {
                // some servers (e.g. google chrome) send PASV several times very quickly
                servers_close_socket(&ftp_data.d_sd);
                ftp_data.substate = E_FTP_STE_SUB_DISCONNECTED;
                bool socketcreated = true;
                if (ftp_data.ld_sd < 0) {
                    socketcreated = ftp_create_listening_socket(&ftp_data.ld_sd, FTP_PASIVE_DATA_PORT, FTP_DATA_CLIENTS_MAX - 1);
                }
                if (socketcreated) {
                    uint8_t *pip = (uint8_t *)&ftp_data.ip_addr;
                    ftp_data.dtimeout = 0;
                    snprintf((char *)ftp_data.dBuffer, FTP_BUFFER_SIZE, "(%u,%u,%u,%u,%u,%u)",
                             pip[0], pip[1], pip[2], pip[3], (FTP_PASIVE_DATA_PORT >> 8), (FTP_PASIVE_DATA_PORT & 0xFF));
                    ftp_data.substate = E_FTP_STE_SUB_LISTEN_FOR_DATA;
                    ftp_send_reply(227, (char *)ftp_data.dBuffer);
                } else {
                    ftp_send_reply(425, NULL);
                }
            }
            break;
        case E_FTP_CMD_LIST:
            if (ftp_open_dir_for_listing(ftp_path) == E_FTP_RESULT_CONTINUE) {
                ftp_data.state = E_FTP_STE_CONTINUE_LISTING;
                ftp_send_reply(150, NULL);
            } else {
                ftp_send_reply(550, NULL);
            }
            break;
        case E_FTP_CMD_RETR:
            ftp_get_param_and_open_child (&bufptr);
            if (ftp_open_file (ftp_path, FA_READ)) {
                ftp_data.state = E_FTP_STE_CONTINUE_FILE_TX;
                ftp_send_reply(150, NULL);
            } else {
                ftp_data.state = E_FTP_STE_END_TRANSFER;
                ftp_send_reply(550, NULL);
            }
            break;
        case E_FTP_CMD_STOR:
            ftp_get_param_and_open_child (&bufptr);
            // first check if a software update is being requested
            if (updater_check_path (ftp_path)) {
                if (updater_start()) {
                    ftp_data.special_file = true;
                    ftp_data.state = E_FTP_STE_CONTINUE_FILE_RX;
                    ftp_send_reply(150, NULL);
                } else {
                    // to unlock the updater
                    updater_finish();
                    ftp_data.state = E_FTP_STE_END_TRANSFER;
                    ftp_send_reply(550, NULL);
                }
            } else {
                if (ftp_open_file (ftp_path, FA_WRITE | FA_CREATE_ALWAYS)) {
                    ftp_data.state = E_FTP_STE_CONTINUE_FILE_RX;
                    ftp_send_reply(150, NULL);
                } else {
                    ftp_data.state = E_FTP_STE_END_TRANSFER;
                    ftp_send_reply(550, NULL);
                }
            }
            break;
        case E_FTP_CMD_DELE:
        case E_FTP_CMD_RMD:
            ftp_get_param_and_open_child (&bufptr);
            if (FR_OK == f_unlink(ftp_path)) {
                ftp_send_reply(250, NULL);
            } else {
                ftp_send_reply(550, NULL);
            }
            break;
        case E_FTP_CMD_MKD:
            ftp_get_param_and_open_child (&bufptr);
            if (FR_OK == f_mkdir(ftp_path)) {
                ftp_send_reply(250, NULL);
            } else {
                ftp_send_reply(550, NULL);
            }
            break;
        case E_FTP_CMD_RNFR:
            ftp_get_param_and_open_child (&bufptr);
            if (FR_OK == f_stat (ftp_path, &fno)) {
                ftp_send_reply(350, NULL);
                // save the current path
                strcpy ((char *)ftp_data.dBuffer, ftp_path);
            } else {
                ftp_send_reply(550, NULL);
            }
            break;
        case E_FTP_CMD_RNTO:
            ftp_get_param_and_open_child (&bufptr);
            // old path was saved in the data buffer
            if (FR_OK == (fres = f_rename ((char *)ftp_data.dBuffer, ftp_path))) {
                ftp_send_reply(250, NULL);
            } else {
                ftp_send_reply(550, NULL);
            }
            break;
        case E_FTP_CMD_NOOP:
            ftp_send_reply(200, NULL);
            break;
        case E_FTP_CMD_QUIT:
            ftp_send_reply(221, NULL);
            break;
        default:
            // command not implemented
            ftp_send_reply(502, NULL);
            break;
        }

        if (ftp_data.closechild) {
            ftp_return_to_previous_path(ftp_path, ftp_scratch_buffer);
        }
    } else if (result == E_FTP_RESULT_CONTINUE) {
        if (ftp_data.ctimeout++ > (servers_get_timeout() / FTP_CYCLE_TIME_MS)) {
            ftp_send_reply(221, NULL);
        }
    } else {
        ftp_close_cmd_data();
    }
}

static void ftp_close_files (void) {
    if (ftp_data.e_open == E_FTP_FILE_OPEN) {
        f_close(&ftp_data.fp);
    } else if (ftp_data.e_open == E_FTP_DIR_OPEN) {
        f_closedir(&ftp_data.dp);
    }
    ftp_data.e_open = E_FTP_NOTHING_OPEN;
}

static void ftp_close_filesystem_on_error (void) {
    ftp_close_files();
    if (ftp_data.special_file) {
        updater_finish ();
        ftp_data.special_file = false;
    }
}

static void ftp_close_cmd_data (void) {
    servers_close_socket(&ftp_data.c_sd);
    servers_close_socket(&ftp_data.d_sd);
    ftp_close_filesystem_on_error ();
}

static ftp_cmd_index_t ftp_pop_command (char **str) {
    char _cmd[FTP_CMD_SIZE_MAX];
    ftp_pop_param (str, _cmd);
    stoupper (_cmd);
    for (ftp_cmd_index_t i = 0; i < E_FTP_NUM_FTP_CMDS; i++) {
        if (!strcmp (_cmd, ftp_cmd_table[i].cmd)) {
            // move one step further to skip the space
            (*str)++;
            return i;
        }
    }
    return E_FTP_CMD_NOT_SUPPORTED;
}

static void ftp_pop_param (char **str, char *param) {
    while (**str != ' ' && **str != '\r' && **str != '\n' && **str != '\0') {
        *param++ = **str;
        (*str)++;
    }
    *param = '\0';
}

static int ftp_print_eplf_item (char *dest, uint32_t destsize, FILINFO *fno) {

    char *type = (fno->fattrib & AM_DIR) ? "d" : "-";
    uint32_t tseconds;
    uint mindex = (((fno->fdate >> 5) & 0x0f) > 0) ? (((fno->fdate >> 5) & 0x0f) - 1) : 0;
    uint day = ((fno->fdate & 0x1f) > 0) ? (fno->fdate & 0x1f) : 1;
    uint fseconds = timeutils_seconds_since_epoch(1980 + ((fno->fdate >> 9) & 0x7f),
                                                        (fno->fdate >> 5) & 0x0f,
                                                        fno->fdate & 0x1f,
                                                        (fno->ftime >> 11) & 0x1f,
                                                        (fno->ftime >> 5) & 0x3f,
                                                        2 * (fno->ftime & 0x1f));
    tseconds = 3600; // pyb_rtc_get_seconds(); // FIXME
    if (FTP_UNIX_SECONDS_180_DAYS < tseconds - fseconds) {
        return snprintf(dest, destsize, "%srw-rw-r--   1 root  root %9u %s %2u %5u %s\r\n",
                        type, (uint32_t)fno->fsize, ftp_month[mindex].month, day,
                        1980 + ((fno->fdate >> 9) & 0x7f), fno->fname);
    } else {
        return snprintf(dest, destsize, "%srw-rw-r--   1 root  root %9u %s %2u %02u:%02u %s\r\n",
                        type, (uint32_t)fno->fsize, ftp_month[mindex].month, day,
                        (fno->ftime >> 11) & 0x1f, (fno->ftime >> 5) & 0x3f, fno->fname);
    }
}

static int ftp_print_eplf_drive (char *dest, uint32_t destsize, char *name) {
    timeutils_struct_time_t tm;
    uint32_t tseconds;
    char *type = "d";

    timeutils_seconds_since_epoch_to_struct_time((FTP_UNIX_TIME_20150101 - FTP_UNIX_TIME_20000101), &tm);

    tseconds = 3600; // pyb_rtc_get_seconds(); // FIXME
    if (FTP_UNIX_SECONDS_180_DAYS < tseconds - (FTP_UNIX_TIME_20150101 - FTP_UNIX_TIME_20000101)) {
        return snprintf(dest, destsize, "%srw-rw-r--   1 root  root %9u %s %2u %5u %s\r\n",
                        type, 0, ftp_month[(tm.tm_mon - 1)].month, tm.tm_mday, tm.tm_year, name);
    } else {
        return snprintf(dest, destsize, "%srw-rw-r--   1 root  root %9u %s %2u %02u:%02u %s\r\n",
                        type, 0, ftp_month[(tm.tm_mon - 1)].month, tm.tm_mday, tm.tm_hour, tm.tm_min, name);
    }
}

static bool ftp_open_file (const char *path, int mode) {
    FRESULT res = f_open(&ftp_data.fp, path, mode);
    if (res != FR_OK) {
        return false;
    }
    ftp_data.e_open = E_FTP_FILE_OPEN;
    return true;
}

static ftp_result_t ftp_read_file (char *filebuf, uint32_t desiredsize, uint32_t *actualsize) {
    ftp_result_t result = E_FTP_RESULT_CONTINUE;
    FRESULT res = f_read(&ftp_data.fp, filebuf, desiredsize, (UINT *)actualsize);
    if (res != FR_OK) {
        ftp_close_files();
        result = E_FTP_RESULT_FAILED;
        *actualsize = 0;
    } else if (*actualsize < desiredsize) {
        ftp_close_files();
        result = E_FTP_RESULT_OK;
    }
    return result;
}

static ftp_result_t ftp_write_file (char *filebuf, uint32_t size) {
    ftp_result_t result = E_FTP_RESULT_FAILED;
    uint32_t actualsize;
    FRESULT res = f_write(&ftp_data.fp, filebuf, size, (UINT *)&actualsize);
    if ((actualsize == size) && (FR_OK == res)) {
        result = E_FTP_RESULT_OK;
    } else {
        ftp_close_files();
    }
    return result;
}

static ftp_result_t ftp_open_dir_for_listing (const char *path) {
    // "hack" to detect the root directory
    if (path[0] == '/' && path[1] == '\0') {
        ftp_data.listroot = true;
    } else {
        FRESULT res;
        res = f_opendir(&ftp_data.dp, path);                       /* Open the directory */
        if (res != FR_OK) {
            return E_FTP_RESULT_FAILED;
        }
        ftp_data.e_open = E_FTP_DIR_OPEN;
        ftp_data.listroot = false;
    }
    return E_FTP_RESULT_CONTINUE;
}

static ftp_result_t ftp_list_dir (char *list, uint32_t maxlistsize, uint32_t *listsize) {
    uint next = 0;
    uint listcount = 0;
    FRESULT res;
    ftp_result_t result = E_FTP_RESULT_CONTINUE;
    FILINFO fno;

    // read up to 8 directory items
    while (listcount < 8) {
        if (ftp_data.listroot) {
            // root directory "hack"
            if (0 == ftp_data.volcount) {
                next += ftp_print_eplf_drive((list + next), (maxlistsize - next), "flash");
            } else if (ftp_data.volcount <= MP_STATE_PORT(mount_obj_list).len) {
                os_fs_mount_t *mount_obj = ((os_fs_mount_t *)(MP_STATE_PORT(mount_obj_list).items[(ftp_data.volcount - 1)]));
                next += ftp_print_eplf_drive((list + next), (maxlistsize - next), (char *)&mount_obj->path[1]);
            } else {
                if (!next) {
                    // no volume found this time, we are done
                    ftp_data.volcount = 0;
                }
                break;
            }
            ftp_data.volcount++;
        } else {
            // a "normal" directory
            res = f_readdir(&ftp_data.dp, &fno);                                                       /* Read a directory item */
            if (res != FR_OK || fno.fname[0] == 0) {
                result = E_FTP_RESULT_OK;
                break;                                                                                 /* Break on error or end of dp */
            }
            if (fno.fname[0] == '.' && fno.fname[1] == 0) continue;                                    /* Ignore . entry */
            if (fno.fname[0] == '.' && fno.fname[1] == '.' && fno.fname[2] == 0) continue;             /* Ignore .. entry */

            // add the entry to the list
            next += ftp_print_eplf_item((list + next), (maxlistsize - next), &fno);
        }
        listcount++;
    }
    if (result == E_FTP_RESULT_OK) {
        ftp_close_files();
    }
    *listsize = next;
    return result;
}

static void ftp_open_child (char *pwd, char *dir) {
    if (dir[0] == '/') {
        strcpy (pwd, dir);
    } else {
        if (strlen(pwd) > 1) {
            strcat (pwd, "/");
        }
        strcat (pwd, dir);
    }

    uint len = strlen(pwd);
    if ((len > 1) && (pwd[len - 1] == '/')) {
        pwd[len - 1] = '\0';
    }
}

static void ftp_close_child (char *pwd) {
    uint len = strlen(pwd);
    while (len && (pwd[len] != '/')) {
        len--;
    }
    if (len == 0) {
        strcpy (pwd, "/");
    } else {
        pwd[len] = '\0';
    }
}

static void ftp_return_to_previous_path (char *pwd, char *dir) {
    uint newlen = strlen(pwd) - strlen(dir);
    if ((newlen > 2) && (pwd[newlen - 1] == '/')) {
        pwd[newlen - 1] = '\0';
    } else {
        if (newlen == 0) {
            strcpy (pwd, "/");
        } else {
            pwd[newlen] = '\0';
        }
    }
}

