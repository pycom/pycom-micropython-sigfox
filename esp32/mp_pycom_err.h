/*
 * mp_pycom_err.h
 *
 *  Created on: 22 Feb 2019
 *      Author: iwahdan
 */

#ifndef ESP32_MP_PYCOM_ERR_H_
#define ESP32_MP_PYCOM_ERR_H_

#include "mbedtls/net_sockets.h"
#include "lwip/err.h"
#include "lwip/netdb.h"
#include "esp_err.h"

#define MP_MBEDTLS_ERR_SSL_TIMEOUT                          MBEDTLS_ERR_SSL_TIMEOUT
#define MP_MBEDTLS_ERR_NET_SOCKET_FAILED                    MBEDTLS_ERR_NET_SOCKET_FAILED
#define MP_MBEDTLS_ERR_NET_CONNECT_FAILED                   MBEDTLS_ERR_NET_CONNECT_FAILED
#define MP_MBEDTLS_ERR_NET_BIND_FAILED                      MBEDTLS_ERR_NET_BIND_FAILED
#define MP_MBEDTLS_ERR_NET_LISTEN_FAILED                    MBEDTLS_ERR_NET_LISTEN_FAILED
#define MP_MBEDTLS_ERR_NET_ACCEPT_FAILED                    MBEDTLS_ERR_NET_ACCEPT_FAILED
#define MP_MBEDTLS_ERR_NET_RECV_FAILED                      MBEDTLS_ERR_NET_RECV_FAILED
#define MP_MBEDTLS_ERR_NET_SEND_FAILED                      MBEDTLS_ERR_NET_SEND_FAILED
#define MP_MBEDTLS_ERR_NET_CONN_RESET                       MBEDTLS_ERR_NET_CONN_RESET
#define MP_MBEDTLS_ERR_NET_UNKNOWN_HOST                     MBEDTLS_ERR_NET_UNKNOWN_HOST
#define MP_MBEDTLS_ERR_NET_BUFFER_TOO_SMALL                 MBEDTLS_ERR_NET_BUFFER_TOO_SMALL
#define MP_MBEDTLS_ERR_NET_INVALID_CONTEXT                  MBEDTLS_ERR_NET_INVALID_CONTEXT
#define MP_MBEDTLS_ERR_NET_POLL_FAILED                      MBEDTLS_ERR_NET_POLL_FAILED
#define MP_MBEDTLS_ERR_NET_BAD_INPUT_DATA                   MBEDTLS_ERR_NET_BAD_INPUT_DATA

#define MP_ERR_MEM             ERR_MEM
#define MP_ERR_BUF             ERR_BUF
#define MP_ERR_TIMEOUT         ERR_TIMEOUT
#define MP_ERR_RTE             ERR_RTE
#define MP_ERR_INPROGRESS      ERR_INPROGRESS
#define MP_ERR_VAL             ERR_VAL
#define MP_ERR_WOULDBLOCK      ERR_WOULDBLOCK
#define MP_ERR_USE             ERR_USE

#define MP_ERR_ALREADY         ERR_ALREADY
#define MP_ERR_ISCONN          ERR_ISCONN
#define MP_ERR_ABRT            ERR_ABRT
#define MP_ERR_RST             ERR_RST
#define MP_ERR_CLSD            ERR_CLSD
#define MP_ERR_CONN            ERR_CONN
#define MP_ERR_ARG             ERR_ARG
#define MP_ERR_IF              ERR_IF

#define MP_ESP_ERR_NO_MEM               ESP_ERR_NO_MEM   /*!< Out of memory */
#define MP_ESP_ERR_INVALID_ARG          ESP_ERR_INVALID_ARG   /*!< Invalid argument */
#define MP_ESP_ERR_INVALID_STATE        ESP_ERR_INVALID_STATE   /*!< Invalid state */
#define MP_ESP_ERR_INVALID_SIZE         ESP_ERR_INVALID_SIZE   /*!< Invalid size */
#define MP_ESP_ERR_NOT_FOUND            ESP_ERR_NOT_FOUND   /*!< Requested resource not found */
#define MP_ESP_ERR_NOT_SUPPORTED        ESP_ERR_NOT_SUPPORTED   /*!< Operation or feature not supported */
#define MP_ESP_ERR_TIMEOUT              ESP_ERR_TIMEOUT   /*!< Operation timed out */
#define MP_ESP_ERR_INVALID_RESPONSE     ESP_ERR_INVALID_RESPONSE   /*!< Received response was invalid */
#define MP_ESP_ERR_INVALID_CRC          ESP_ERR_INVALID_CRC   /*!< CRC or checksum was invalid */
#define MP_ESP_ERR_INVALID_VERSION      ESP_ERR_INVALID_VERSION   /*!< Version was invalid */
#define MP_ESP_ERR_INVALID_MAC          ESP_ERR_INVALID_MAC   /*!< MAC address was invalid */

#define MP_EAI_NONAME      EAI_NONAME
#define MP_EAI_SERVICE     EAI_SERVICE
#define MP_EAI_FAIL        EAI_FAIL
#define MP_EAI_MEMORY      EAI_MEMORY
#define MP_EAI_FAMILY      EAI_FAMILY
#define MP_HOST_NOT_FOUND  HOST_NOT_FOUND
#define MP_NO_DATA         NO_DATA
#define MP_NO_RECOVERY     NO_RECOVERY
#define MP_TRY_AGAIN       TRY_AGAIN

#endif /* ESP32_MP_PYCOM_ERR_H_ */
