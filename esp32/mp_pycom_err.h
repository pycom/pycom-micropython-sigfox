/*
 * mp_pycom_err.h
 *
 *  Created on: 22 Feb 2019
 *      Author: iwahdan
 */

#ifndef ESP32_MP_PYCOM_ERR_H_
#define ESP32_MP_PYCOM_ERR_H_

#include "mbedtls/net_sockets.h"
#include "mbedtls/pk.h"
#include "mbedtls/ssl.h"
#include "lwip/err.h"
#include "lwip/netdb.h"
#include "esp_err.h"

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

#define MP_MBEDTLS_ERR_PK_ALLOC_FAILED        MBEDTLS_ERR_PK_ALLOC_FAILED  /**< Memory allocation failed. */
#define MP_MBEDTLS_ERR_PK_TYPE_MISMATCH       MBEDTLS_ERR_PK_TYPE_MISMATCH  /**< Type mismatch, eg attempt to encrypt with an ECDSA key */
#define MP_MBEDTLS_ERR_PK_BAD_INPUT_DATA      MBEDTLS_ERR_PK_BAD_INPUT_DATA  /**< Bad input parameters to function. */
#define MP_MBEDTLS_ERR_PK_FILE_IO_ERROR       MBEDTLS_ERR_PK_FILE_IO_ERROR  /**< Read/write of file failed. */
#define MP_MBEDTLS_ERR_PK_KEY_INVALID_VERSION MBEDTLS_ERR_PK_KEY_INVALID_VERSION  /**< Unsupported key version */
#define MP_MBEDTLS_ERR_PK_KEY_INVALID_FORMAT  MBEDTLS_ERR_PK_KEY_INVALID_FORMAT  /**< Invalid key tag or value. */
#define MP_MBEDTLS_ERR_PK_UNKNOWN_PK_ALG      MBEDTLS_ERR_PK_UNKNOWN_PK_ALG  /**< Key algorithm is unsupported (only RSA and EC are supported). */
#define MP_MBEDTLS_ERR_PK_PASSWORD_REQUIRED   MBEDTLS_ERR_PK_PASSWORD_REQUIRED  /**< Private key password can't be empty. */
#define MP_MBEDTLS_ERR_PK_PASSWORD_MISMATCH   MBEDTLS_ERR_PK_PASSWORD_MISMATCH  /**< Given private key password does not allow for correct decryption. */
#define MP_MBEDTLS_ERR_PK_INVALID_PUBKEY      MBEDTLS_ERR_PK_INVALID_PUBKEY  /**< The pubkey tag or value is invalid (only RSA and EC are supported). */
#define MP_MBEDTLS_ERR_PK_INVALID_ALG         MBEDTLS_ERR_PK_INVALID_ALG  /**< The algorithm tag or value is invalid. */
#define MP_MBEDTLS_ERR_PK_UNKNOWN_NAMED_CURVE MBEDTLS_ERR_PK_UNKNOWN_NAMED_CURVE  /**< Elliptic curve is unsupported (only NIST curves are supported). */
#define MP_MBEDTLS_ERR_PK_FEATURE_UNAVAILABLE MBEDTLS_ERR_PK_FEATURE_UNAVAILABLE  /**< Unavailable feature, e.g. RSA disabled for RSA key. */
#define MP_MBEDTLS_ERR_PK_SIG_LEN_MISMATCH    MBEDTLS_ERR_PK_SIG_LEN_MISMATCH  /**< The buffer contains a valid signature followed by more data. */
#define MP_MBEDTLS_ERR_PK_HW_ACCEL_FAILED     MBEDTLS_ERR_PK_HW_ACCEL_FAILED  /**< PK hardware accelerator failed. */

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

#define MP_MBEDTLS_ERR_SSL_FEATURE_UNAVAILABLE                   MBEDTLS_ERR_SSL_FEATURE_UNAVAILABLE                /* < The requested feature is not available. */
#define MP_MBEDTLS_ERR_SSL_BAD_INPUT_DATA                        MBEDTLS_ERR_SSL_BAD_INPUT_DATA                     /* < Bad input parameters to function. */
#define MP_MBEDTLS_ERR_SSL_INVALID_MAC                           MBEDTLS_ERR_SSL_INVALID_MAC                        /* < Verification of the message MAC failed. */
#define MP_MBEDTLS_ERR_SSL_INVALID_RECORD                        MBEDTLS_ERR_SSL_INVALID_RECORD                     /* < An invalid SSL record was received. */
#define MP_MBEDTLS_ERR_SSL_CONN_EOF                              MBEDTLS_ERR_SSL_CONN_EOF                           /* < The connection indicated an EOF. */
#define MP_MBEDTLS_ERR_SSL_UNKNOWN_CIPHER                        MBEDTLS_ERR_SSL_UNKNOWN_CIPHER                     /* < An unknown cipher was received. */
#define MP_MBEDTLS_ERR_SSL_NO_CIPHER_CHOSEN                      MBEDTLS_ERR_SSL_NO_CIPHER_CHOSEN                   /* < The server has no ciphersuites in common with the client. */
#define MP_MBEDTLS_ERR_SSL_NO_RNG                                MBEDTLS_ERR_SSL_NO_RNG                             /* < No RNG was provided to the SSL module. */
#define MP_MBEDTLS_ERR_SSL_NO_CLIENT_CERTIFICATE                 MBEDTLS_ERR_SSL_NO_CLIENT_CERTIFICATE              /* < No client certification received from the client, but required by the authentication mode. */
#define MP_MBEDTLS_ERR_SSL_CERTIFICATE_TOO_LARGE                 MBEDTLS_ERR_SSL_CERTIFICATE_TOO_LARGE              /* < Our own certificate(s) is/are too large to send in an SSL message. */
#define MP_MBEDTLS_ERR_SSL_CERTIFICATE_REQUIRED                  MBEDTLS_ERR_SSL_CERTIFICATE_REQUIRED               /* < The own certificate is not set, but needed by the server. */
#define MP_MBEDTLS_ERR_SSL_PRIVATE_KEY_REQUIRED                  MBEDTLS_ERR_SSL_PRIVATE_KEY_REQUIRED               /* < The own private key or pre-shared key is not set, but needed. */
#define MP_MBEDTLS_ERR_SSL_CA_CHAIN_REQUIRED                     MBEDTLS_ERR_SSL_CA_CHAIN_REQUIRED                  /* < No CA Chain is set, but required to operate. */
#define MP_MBEDTLS_ERR_SSL_UNEXPECTED_MESSAGE                    MBEDTLS_ERR_SSL_UNEXPECTED_MESSAGE                 /* < An unexpected message was received from our peer. */
#define MP_MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE                   MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE                /* < A fatal alert message was received from our peer. */
#define MP_MBEDTLS_ERR_SSL_PEER_VERIFY_FAILED                    MBEDTLS_ERR_SSL_PEER_VERIFY_FAILED                 /* < Verification of our peer failed. */
#define MP_MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY                     MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY                  /* < The peer notified us that the connection is going to be closed. */
#define MP_MBEDTLS_ERR_SSL_BAD_HS_CLIENT_HELLO                   MBEDTLS_ERR_SSL_BAD_HS_CLIENT_HELLO                /* < Processing of the ClientHello handshake message failed. */
#define MP_MBEDTLS_ERR_SSL_BAD_HS_SERVER_HELLO                   MBEDTLS_ERR_SSL_BAD_HS_SERVER_HELLO                /* < Processing of the ServerHello handshake message failed. */
#define MP_MBEDTLS_ERR_SSL_BAD_HS_CERTIFICATE                    MBEDTLS_ERR_SSL_BAD_HS_CERTIFICATE                 /* < Processing of the Certificate handshake message failed. */
#define MP_MBEDTLS_ERR_SSL_BAD_HS_CERTIFICATE_REQUEST            MBEDTLS_ERR_SSL_BAD_HS_CERTIFICATE_REQUEST         /* < Processing of the CertificateRequest handshake message failed. */
#define MP_MBEDTLS_ERR_SSL_BAD_HS_SERVER_KEY_EXCHANGE            MBEDTLS_ERR_SSL_BAD_HS_SERVER_KEY_EXCHANGE         /* < Processing of the ServerKeyExchange handshake message failed. */
#define MP_MBEDTLS_ERR_SSL_BAD_HS_SERVER_HELLO_DONE              MBEDTLS_ERR_SSL_BAD_HS_SERVER_HELLO_DONE           /* < Processing of the ServerHelloDone handshake message failed. */
#define MP_MBEDTLS_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE            MBEDTLS_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE         /* < Processing of the ClientKeyExchange handshake message failed. */
#define MP_MBEDTLS_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE_RP         MBEDTLS_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE_RP      /* < Processing of the ClientKeyExchange handshake message failed in DHM / ECDH Read Public. */
#define MP_MBEDTLS_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE_CS         MBEDTLS_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE_CS      /* < Processing of the ClientKeyExchange handshake message failed in DHM / ECDH Calculate Secret. */
#define MP_MBEDTLS_ERR_SSL_BAD_HS_CERTIFICATE_VERIFY             MBEDTLS_ERR_SSL_BAD_HS_CERTIFICATE_VERIFY          /* < Processing of the CertificateVerify handshake message failed. */
#define MP_MBEDTLS_ERR_SSL_BAD_HS_CHANGE_CIPHER_SPEC             MBEDTLS_ERR_SSL_BAD_HS_CHANGE_CIPHER_SPEC          /* < Processing of the ChangeCipherSpec handshake message failed. */
#define MP_MBEDTLS_ERR_SSL_BAD_HS_FINISHED                       MBEDTLS_ERR_SSL_BAD_HS_FINISHED                    /* < Processing of the Finished handshake message failed. */
#define MP_MBEDTLS_ERR_SSL_ALLOC_FAILED                          MBEDTLS_ERR_SSL_ALLOC_FAILED                       /* < Memory allocation failed */
#define MP_MBEDTLS_ERR_SSL_HW_ACCEL_FAILED                       MBEDTLS_ERR_SSL_HW_ACCEL_FAILED                    /* < Hardware acceleration function returned with error */
#define MP_MBEDTLS_ERR_SSL_HW_ACCEL_FALLTHROUGH                  MBEDTLS_ERR_SSL_HW_ACCEL_FALLTHROUGH               /* < Hardware acceleration function skipped / left alone data */
#define MP_MBEDTLS_ERR_SSL_COMPRESSION_FAILED                    MBEDTLS_ERR_SSL_COMPRESSION_FAILED                 /* < Processing of the compression / decompression failed */
#define MP_MBEDTLS_ERR_SSL_BAD_HS_PROTOCOL_VERSION               MBEDTLS_ERR_SSL_BAD_HS_PROTOCOL_VERSION            /* < Handshake protocol not within min/max boundaries */
#define MP_MBEDTLS_ERR_SSL_BAD_HS_NEW_SESSION_TICKET             MBEDTLS_ERR_SSL_BAD_HS_NEW_SESSION_TICKET          /* < Processing of the NewSessionTicket handshake message failed. */
#define MP_MBEDTLS_ERR_SSL_SESSION_TICKET_EXPIRED                MBEDTLS_ERR_SSL_SESSION_TICKET_EXPIRED             /* < Session ticket has expired. */
#define MP_MBEDTLS_ERR_SSL_PK_TYPE_MISMATCH                      MBEDTLS_ERR_SSL_PK_TYPE_MISMATCH                   /* < Public key type mismatch (eg, asked for RSA key exchange and presented EC key) */
#define MP_MBEDTLS_ERR_SSL_UNKNOWN_IDENTITY                      MBEDTLS_ERR_SSL_UNKNOWN_IDENTITY                   /* < Unknown identity received (eg, PSK identity) */
#define MP_MBEDTLS_ERR_SSL_INTERNAL_ERROR                        MBEDTLS_ERR_SSL_INTERNAL_ERROR                     /* < Internal error (eg, unexpected failure in lower-level module) */
#define MP_MBEDTLS_ERR_SSL_COUNTER_WRAPPING                      MBEDTLS_ERR_SSL_COUNTER_WRAPPING                   /* < A counter would wrap (eg, too many messages exchanged). */
#define MP_MBEDTLS_ERR_SSL_WAITING_SERVER_HELLO_RENEGO           MBEDTLS_ERR_SSL_WAITING_SERVER_HELLO_RENEGO        /* < Unexpected message at ServerHello in renegotiation. */
#define MP_MBEDTLS_ERR_SSL_HELLO_VERIFY_REQUIRED                 MBEDTLS_ERR_SSL_HELLO_VERIFY_REQUIRED              /* < DTLS client must retry for hello verification */
#define MP_MBEDTLS_ERR_SSL_BUFFER_TOO_SMALL                      MBEDTLS_ERR_SSL_BUFFER_TOO_SMALL                   /* < A buffer is too small to receive or write a message */
#define MP_MBEDTLS_ERR_SSL_NO_USABLE_CIPHERSUITE                 MBEDTLS_ERR_SSL_NO_USABLE_CIPHERSUITE              /* < None of the common ciphersuites is usable (eg, no suitable certificate, see debug messages). */
#define MP_MBEDTLS_ERR_SSL_WANT_READ                             MBEDTLS_ERR_SSL_WANT_READ                          /* < No data of requested type currently available on underlying transport. */
#define MP_MBEDTLS_ERR_SSL_WANT_WRITE                            MBEDTLS_ERR_SSL_WANT_WRITE                         /* < Connection requires a write call. */
#define MP_MBEDTLS_ERR_SSL_TIMEOUT                               MBEDTLS_ERR_SSL_TIMEOUT                            /* < The operation timed out. */
#define MP_MBEDTLS_ERR_SSL_CLIENT_RECONNECT                      MBEDTLS_ERR_SSL_CLIENT_RECONNECT                   /* < The client initiated a reconnect from the same port. */
#define MP_MBEDTLS_ERR_SSL_UNEXPECTED_RECORD                     MBEDTLS_ERR_SSL_UNEXPECTED_RECORD                  /* < Record header looks valid but is not expected. */
#define MP_MBEDTLS_ERR_SSL_NON_FATAL                             MBEDTLS_ERR_SSL_NON_FATAL                          /* < The alert message received indicates a non-fatal error. */
#define MP_MBEDTLS_ERR_SSL_INVALID_VERIFY_HASH                   MBEDTLS_ERR_SSL_INVALID_VERIFY_HASH                /* < Couldn't set the hash for verifying CertificateVerify */
#define MP_MBEDTLS_ERR_SSL_CONTINUE_PROCESSING                   MBEDTLS_ERR_SSL_CONTINUE_PROCESSING                /* < Internal-only message signaling that further message-processing should be done */
#define MP_MBEDTLS_ERR_SSL_ASYNC_IN_PROGRESS                     MBEDTLS_ERR_SSL_ASYNC_IN_PROGRESS                  /* < The asynchronous operation is not completed yet. */

#endif /* ESP32_MP_PYCOM_ERR_H_ */
