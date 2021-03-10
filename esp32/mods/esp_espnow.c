/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2020 Nick Moore
 * Copyright (c) 2018 shawwwn <shawwwn1@gmail.com>
 * Copyright (c) 2020 Glenn Moloney @glenn20
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */


#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "esp_log.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"

#include "py/runtime.h"
#include "py/mphal.h"
#include "py/mperrno.h"
#include "py/objstr.h"
#include "py/objarray.h"
#include "py/stream.h"

#include "mpconfigport.h"
// #include "mphalport.h"
#include "ring_buffer.h"
#include "mpirq.h"
static const uint8_t ESPNOW_MAGIC = 0x99;

// ESPNow buffer packet format:
// |ESPNOW_MAGIC|MSG_LEN|PEER_ADDR(6bytes)|MSG...(max 250bytes)|

// Size and offset of elements in recv_buffer packets.
// static const size_t ESPNOW_MAGIC_OFFSET = 0;
// static const size_t ESPNOW_MSGLEN_OFFSET = 1;
static const size_t ESPNOW_PEER_OFFSET = 2;
#define ESPNOW_MSG_OFFSET (ESPNOW_PEER_OFFSET + ESP_NOW_ETH_ALEN)
#define ESPNOW_HDR_LEN ESPNOW_MSG_OFFSET
#define ESPNOW_MAX_PKT_LEN (ESPNOW_HDR_LEN + ESP_NOW_MAX_DATA_LEN)

#define DEFAULT_RECV_BUFFER_SIZE (2 * ESPNOW_MAX_PKT_LEN)
// Enough for 2 full-size packets: 2 * (6 + 2 + 250) = 516 bytes
// Will allocate an additional 9 bytes for buffer overhead

static const size_t DEFAULT_RECV_TIMEOUT = (5 * 60 * 1000);
// 5 mins - in milliseconds

static const size_t DEFAULT_SEND_TIMEOUT = (10 * 1000);
// 10 seconds - in milliseconds

// Number of milliseconds to wait (mp_hal_wait_ms()) in each loop
// while waiting for send or receive packet.
// Needs to be >15ms to permit yield to other tasks.
static const size_t BUSY_WAIT_MS = 25;  // milliseconds

typedef struct _esp_espnow_obj_t {
    mp_obj_base_t base;

    int initialised;
    buffer_t recv_buffer;           // A buffer for received packets
    size_t recv_buffer_size;
    size_t sent_packets;            // Count of sent packets
    volatile size_t recv_packets;   // Count of received packets
    size_t dropped_rx_pkts;         // Count of dropped packets (buffer full)
    size_t recv_timeout;            // Timeout for recv()/irecv()/poll()/ipoll()
    volatile size_t sent_responses; // # of sent packet responses received
    volatile size_t sent_failures;  // # of sent packet responses failed
    size_t peer_count;              // Cache the number of peers
    mp_obj_tuple_t *irecv_tuple;    // Preallocated tuple for irecv()
    mp_obj_array_t *irecv_peer;
    mp_obj_array_t *irecv_msg;
} esp_espnow_obj_t;

// Initialised below.
const mp_obj_type_t esp_espnow_type;

// A static pointer to the espnow module singleton
STATIC esp_espnow_obj_t *espnow_singleton = NULL;

// ### Consolidated buffer Handling Support functions
//
// Forward declare buffer and packet handling functions defined at end of file
// in lieu of splitting out to another esp_espnow_buf.[ch].
#define BUF_EMPTY (-1)      // The ring buffer is empty - flag EAGAIN
#define BUF_ERROR (-2)      // An error in the packet format
#define BUF_ESIZE (-3)      // Packet is too big for readout bufsize

// Put received data into the buffer (called from recv_cb()).
static void _buf_put_recv_data(buffer_t buf, const uint8_t *mac,
    const uint8_t *data, size_t msg_len);
// Get the peer mac address and message from a packet in the buffer.
static bool _buf_get_recv_data(buffer_t buf, uint8_t *mac, uint8_t *msg, int msg_len);
// Validate a recv packet header, check message fits in max size and return
static int _buf_check_message_length(const uint8_t *header, size_t size);
static int _buf_check_packet_length(const uint8_t *header, size_t size);
// Peek at the next recv packet in the ring buffer to get the message length.
static int _buf_peek_message_length(buffer_t buf);
// Copy the whole recv packet from the ring buffer.
static int _buf_get_recv_packet(uint8_t *buf, size_t size);
// Get ptrs to the peer mac address, message and length from an ESPNow packet.
static int _buf_get_data_from_packet(const uint8_t *buf, size_t size,
    const uint8_t **peer_addr,
    const uint8_t **msg, size_t *msg_len);
// These functions help cleanup boiler plate code for processing args.
static bool _get_bool(mp_obj_t obj);
static const uint8_t *_get_bytes_len(mp_obj_t obj, size_t len);
static const uint8_t *_get_peer(mp_obj_t obj);

// ### Initialisation and Config functions
//
// Check the ESP-IDF error code and raise an OSError if it's not ESP_OK.
void check_esp_err(esp_err_t code) {
    if (code != ESP_OK) {
        mp_raise_OSError(code);
    }
}

// Allocate and initialise the ESPNow module singleton
// Returns the initialise singleton.
STATIC mp_obj_t espnow_make_new(const mp_obj_type_t *type, size_t n_args,
    size_t n_kw, const mp_obj_t *all_args) {

    if (espnow_singleton != NULL) {
        return espnow_singleton;
    }
    esp_espnow_obj_t *self = m_malloc0(sizeof(esp_espnow_obj_t));
    self->initialised = 0;
    self->base.type = &esp_espnow_type;
    self->recv_buffer_size = DEFAULT_RECV_BUFFER_SIZE;
    self->recv_timeout = DEFAULT_RECV_TIMEOUT;

    // Allocate and initialise the "callee-owned" tuple for irecv().
    uint8_t msg_tmp[ESP_NOW_MAX_DATA_LEN], peer_tmp[ESP_NOW_ETH_ALEN];
    self->irecv_peer = MP_OBJ_TO_PTR(
        mp_obj_new_bytearray(ESP_NOW_ETH_ALEN, peer_tmp));
    self->irecv_msg = MP_OBJ_TO_PTR(
        mp_obj_new_bytearray(ESP_NOW_MAX_DATA_LEN, msg_tmp));
    self->irecv_tuple = mp_obj_new_tuple(2, NULL);
    self->irecv_tuple->items[0] = MP_OBJ_FROM_PTR(self->irecv_peer);
    self->irecv_tuple->items[1] = MP_OBJ_FROM_PTR(self->irecv_msg);

    // Set the global singleton pointer for the espnow protocol.
    espnow_singleton = self;

    return self;
}

// Forward declare the send and recv ESPNow callbacks
STATIC void IRAM_ATTR
send_cb(const uint8_t *mac_addr, esp_now_send_status_t status);

STATIC void IRAM_ATTR
recv_cb(const uint8_t *mac_addr, const uint8_t *data, int len);

// ESPNow.init()
// Initialise the Espressif ESPNOW software stack, register callbacks and
// allocate the recv data buffers.
STATIC mp_obj_t espnow_init(mp_obj_t _) {
    esp_espnow_obj_t *self = espnow_singleton;
    if (self->initialised) {
        return mp_const_none;
    }
    self->recv_buffer = buffer_init(self->recv_buffer_size);
    self->recv_buffer_size = buffer_size(self->recv_buffer);
    buffer_print("Recv buffer", self->recv_buffer);

    self->initialised = 1;
    // esp_initialise_wifi();
    check_esp_err(esp_now_init());
    check_esp_err(esp_now_register_recv_cb(recv_cb));
    check_esp_err(esp_now_register_send_cb(send_cb));

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(espnow_init_obj, espnow_init);

// ESPNow.deinit()
// De-initialise the Espressif ESPNOW software stack, disable callbacks and
// deallocate the recv data buffers.
STATIC mp_obj_t espnow_deinit(mp_obj_t _) {
    esp_espnow_obj_t *self = espnow_singleton;
    if (!self->initialised) {
        return mp_const_none;
    }
    check_esp_err(esp_now_unregister_recv_cb());
    check_esp_err(esp_now_unregister_send_cb());
    check_esp_err(esp_now_deinit());
    self->initialised = 0;
    buffer_release(self->recv_buffer);
    self->peer_count = 0;   // esp_now_deinit() removes all peers.

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(espnow_deinit_obj, espnow_deinit);

// ESPNow.config(['param'|param=value, ..])
// Get or set configuration values. Supported params:
//    rxbuf: size of internal buffer for rx packets (default=514 bytes)
//    timeout: Default read timeout (default=300,000 milliseconds)
STATIC mp_obj_t espnow_config(
    size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    esp_espnow_obj_t *self = espnow_singleton;
    enum { ARG_get, ARG_rxbuf, ARG_timeout };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_get, MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_rxbuf, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = -1} },
        { MP_QSTR_timeout, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = -1} },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args,
        MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    if (args[ARG_rxbuf].u_int >= 0) {
        self->recv_buffer_size = args[ARG_rxbuf].u_int;
    }
    if (args[ARG_timeout].u_int >= 0) {
        self->recv_timeout = args[ARG_timeout].u_int;
    }
    if (args[ARG_get].u_obj == MP_OBJ_NULL) {
        return mp_const_none;
    }
#define QS(x) (uintptr_t)MP_OBJ_NEW_QSTR(x)
    // Return the value of the requested parameter
    uintptr_t name = (uintptr_t)args[ARG_get].u_obj;
    if (name == QS(MP_QSTR_rxbuf)) {
        return mp_obj_new_int(
            (self->initialised ? buffer_size(self->recv_buffer)
                                : self->recv_buffer_size));
    } else if (name == QS(MP_QSTR_timeout)) {
        return mp_obj_new_int(self->recv_timeout);
    } else {
        mp_raise_ValueError("Unknown config param");
        // mp_raise_ValueError(MP_ERROR_TEXT("unknown config param"));
    }
#undef QS

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(espnow_config_obj, 1, espnow_config);

// ESPNow.clear(True): Clear out any data in the recv buffer.
// Require arg==True as check against inadvertent use.
// If arg != True, print the current buffer state.
// Warning: Discards all data in the buffers. Use this to recovery from
// buffer errors (*should* not happen).
STATIC mp_obj_t espnow_clear(const mp_obj_t _, const mp_obj_t arg) {
    esp_espnow_obj_t *self = espnow_singleton;

    buffer_print("Response", self->recv_buffer);
    if (arg != mp_const_true) {
        mp_raise_ValueError("Arg must be True");
    }
    buffer_flush(self->recv_buffer);
    buffer_print("Resp", self->recv_buffer);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_2(espnow_clear_obj, espnow_clear);

// ESPnow.stats(): Report
STATIC mp_obj_t espnow_stats(mp_obj_t _) {
    const esp_espnow_obj_t *self = espnow_singleton;
    mp_obj_t items[] = {
        mp_obj_new_int(self->sent_packets),
        mp_obj_new_int(self->sent_responses),
        mp_obj_new_int(self->sent_failures),
        mp_obj_new_int(self->recv_packets),
        mp_obj_new_int(self->dropped_rx_pkts),
    };
    return mp_obj_new_tuple(MP_ARRAY_SIZE(items), items);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(espnow_stats_obj, espnow_stats);

// set_pmk(primary_key)
STATIC mp_obj_t espnow_set_pmk(mp_obj_t _, mp_obj_t key) {
    check_esp_err(esp_now_set_pmk(_get_bytes_len(key, ESP_NOW_KEY_LEN)));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(espnow_set_pmk_obj, espnow_set_pmk);

STATIC mp_obj_t espnow_version(mp_obj_t _) {
    uint32_t version;
    check_esp_err(esp_now_get_version(&version));
    return mp_obj_new_int(version);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(espnow_version_obj, espnow_version);

// ### The ESP_Now send and recv callback routines
//

// Triggered when receipt of a sent packet is acknowledged (or not)
// Just count number of responses and number of successes
STATIC void IRAM_ATTR send_cb(
    const uint8_t *mac_addr, esp_now_send_status_t status) {

    espnow_singleton->sent_responses++;
    if (status != ESP_NOW_SEND_SUCCESS) {
        espnow_singleton->sent_failures++;
    }
}

// Triggered when an ESP-Now packet is received.
STATIC void IRAM_ATTR recv_cb(
    const uint8_t *mac_addr, const uint8_t *msg, int msg_len) {

    esp_espnow_obj_t *self = espnow_singleton;
    if (ESPNOW_HDR_LEN + msg_len >= buffer_free(self->recv_buffer)) {
        self->dropped_rx_pkts++;
        return;
    }
    _buf_put_recv_data(self->recv_buffer, mac_addr, msg, msg_len);
    self->recv_packets++;
}

// ### Send and Receive ESP_Now data
//

// Wait for a packet to be received and return the message length.
// Called by espnow_recv() and espnow_irecv().
// Timeout is set in args or by default.
static int _wait_for_recv_packet(size_t n_args, const mp_obj_t *args) {
    esp_espnow_obj_t *self = espnow_singleton;
    if (!self->initialised) {
        mp_raise_msg(&mp_type_OSError, "Not initalised");
    }
    // Setup the timeout and wait for a packet to arrive
    size_t timeout = (
        (n_args > 1) ? mp_obj_get_int(args[1]) : self->recv_timeout);
    if (timeout > 0) {
        int64_t start = mp_hal_ticks_ms();
        while (buffer_empty(self->recv_buffer) &&
               mp_hal_ticks_ms() - start <= timeout) {
            mp_hal_delay_ms(BUSY_WAIT_MS);
        }
    }
    // Get the length of the message in the buffer.
    int msg_len = _buf_peek_message_length(self->recv_buffer);
    if (msg_len < 0) {
        if (msg_len == BUF_EMPTY) {
            return BUF_EMPTY;   // No data ready - just return None
        } else {
            mp_raise_ValueError("Buffer error");
        }
    }
    return msg_len;
}

// ESPNow.recv([timeout]):
// Returns a tuple of byte strings: (peer_addr, message) where peer_addr is
// the MAC address of the sending peer.
// Takes an optional "timeout" argument in milliseconds.
// Default timeout is set with ESPNow.config(timeout=milliseconds).
// Returns None on timeout.
STATIC mp_obj_t espnow_recv(size_t n_args, const mp_obj_t *args) {
    esp_espnow_obj_t *self = espnow_singleton;

    int msg_len = _wait_for_recv_packet(n_args, args);
    if (msg_len < 0) {
        return mp_const_none;   // Timed out - just return None
    }
    // Allocate vstrs as new storage buffers for the mac address and message.
    // The storage will be handed over to mp_obj_new_str_from_vstr() below.
    vstr_t peer_addr, message;
    vstr_init_len(&peer_addr, ESP_NOW_ETH_ALEN);
    vstr_init_len(&message, msg_len);
    if (!_buf_get_recv_data(
        self->recv_buffer,
        (uint8_t *)peer_addr.buf, (uint8_t *)message.buf, msg_len)) {
        vstr_clear(&peer_addr);
        vstr_clear(&message);
        mp_raise_ValueError("Buffer error");
    }
    // Create and return a tuple of byte strings: (mac_addr, message)
    mp_obj_t items[] = {
        mp_obj_new_str_from_vstr(&mp_type_bytes, &peer_addr),
        mp_obj_new_str_from_vstr(&mp_type_bytes, &message),
    };
    return mp_obj_new_tuple(2, items);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(espnow_recv_obj, 1, 2, espnow_recv);

// ESPNow.irecv([timeout]):
// Like ESPNow.recv() but returns a "callee-owned" tuple of byte strings.
// This provides an allocation-free way to read successive messages.
// Beware: the tuple and bytestring storage is re-used between all calls
// to irecv().
// Takes an optional "timeout" argument in milliseconds.
// Default timeout is set with ESPNow.config(timeout=milliseconds).
// Returns None on timeout.
STATIC mp_obj_t espnow_irecv(size_t n_args, const mp_obj_t *args) {
    esp_espnow_obj_t *self = espnow_singleton;

    int msg_len = _wait_for_recv_packet(n_args, args);
    if (msg_len < 0) {
        self->irecv_msg->len = 0;
        return mp_const_none;   // Timed out - just return None
    }
    if (!_buf_get_recv_data(
        self->recv_buffer,
        self->irecv_peer->items,
        self->irecv_msg->items,
        msg_len)) {
        mp_raise_ValueError("Buffer error");
    }
    self->irecv_msg->len = msg_len;
    return MP_OBJ_FROM_PTR(self->irecv_tuple);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(espnow_irecv_obj, 1, 2, espnow_irecv);

// Used by _do_espnow_send() for sends() with sync==True.
// Wait till all pending sent packet responses have been received.
// ie. self->sent_responses == self->sent_packets.
// Return the number of responses where status != ESP_NOW_SEND_SUCCESS.
static void _wait_for_pending_responses(esp_espnow_obj_t *self) {
    int64_t start = mp_hal_ticks_ms();
    // Note: the send timeout is just a fallback - in normal operation
    // we should never reach that timeout.
    while (self->sent_responses < self->sent_packets &&
           (mp_hal_ticks_ms() - start) <= DEFAULT_SEND_TIMEOUT) {
        // Won't yield unless delay > portTICK_PERIOD_MS (10ms)
        mp_hal_delay_ms(BUSY_WAIT_MS);
    }
    if (self->sent_responses != self->sent_packets) {
        mp_raise_ValueError("Send timeout on synch.");
    }
}

// Send an ESPNow message to the peer_addr and optionally wait for the
// send response.
// Returns the number of "Not received" responses (which may be more than
// one if the send is a broadcast).
static int _do_espnow_send(
    const uint8_t *peer_addr, const uint8_t *message, size_t length, bool sync) {

    esp_espnow_obj_t *self = espnow_singleton;
    // First we have to wait for any pending sent packet responses.
    // There may be a backlog immediately after a burst of non-sync send()s.
    if (sync) {
        // Flush out any pending responses
        _wait_for_pending_responses(self);
    }
    int saved_failures = self->sent_failures;
    // Send the packet - try, try again if internal esp-now buffers are full.
    esp_err_t e;
    int64_t start = mp_hal_ticks_ms();
    while ((e = esp_now_send(peer_addr, message, length))
           == ESP_ERR_ESPNOW_NO_MEM &&
           (mp_hal_ticks_ms() - start) <= DEFAULT_SEND_TIMEOUT) {
        // Won't yield unless delay > portTICK_PERIOD_MS (10ms)
        mp_hal_delay_ms(BUSY_WAIT_MS);
    }

    if (e != ESP_OK) {
        mp_printf(MP_PYTHON_PRINTER,"DEBUG: Send Error | e = %x = %s\r\n", e, esp_err_to_name(e));
        check_esp_err(e);
    }
    // Increment the sent packet count. Broadcasts send to all peers.
    self->sent_packets += ((peer_addr == NULL) ? self->peer_count : 1);
    if (sync) {
        _wait_for_pending_responses(self);
    }
    // Return number of non-responsive peers.
    return self->sent_failures - saved_failures;
}

// ESPNow.send(peer_addr, message, [sync (=true)])
// ESPNow.send(message)
// Send a message to the peer's mac address. Optionally wait for a response.
// If peer_addr == None, send to all registered peers (broadcast).
// If sync == True, wait for response after sending.
// Returns:
//   True  if sync==False and message sent successfully.
//   True  if sync==True and message is received successfully by all recipients
//   False if sync==True and message is not received by any recipients
// Raises: EAGAIN if the internal espnow buffers are full.
STATIC mp_obj_t espnow_send(size_t n_args, const mp_obj_t *args) {
    esp_espnow_obj_t *self = espnow_singleton;
    if (!self->initialised) {
        mp_raise_msg(&mp_type_OSError, "Not initialised");
    }
    // Check the various combinations of input arguments
    mp_obj_t peer = (n_args > 2) ? args[1] : mp_const_none;
    mp_obj_t msg = (n_args > 2) ? args[2] : (n_args == 2) ? args[1] : MP_OBJ_NULL;
    mp_obj_t sync = (n_args > 3) ? args[3] : mp_const_true;

    // Get a pointer to the buffer of obj
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(msg, &bufinfo, MP_BUFFER_READ);

    int failed_responses =
        _do_espnow_send(
            _get_peer(peer), bufinfo.buf, bufinfo.len, _get_bool(sync));
    if (failed_responses < 0) {
        mp_raise_OSError(MP_EAGAIN);
    }
    return (failed_responses == 0) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(espnow_send_obj, 2, 4, espnow_send);

// ### Peer Management Functions
//

// Invoked from add_peer() and mod_peer() to process the args and kw_args:
// overriding values in the provided peer_info struct.
STATIC bool _update_peer_info(
    esp_now_peer_info_t *peer, size_t n_args,
    const mp_obj_t *pos_args, mp_map_t *kw_args) {

    enum { ARG_lmk, ARG_channel, ARG_ifidx, ARG_encrypt };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_lmk, MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_channel, MP_ARG_INT, {.u_int = -1} },
        { MP_QSTR_ifidx, MP_ARG_INT, {.u_int = -1} },
        { MP_QSTR_encrypt, MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args,
        MP_ARRAY_SIZE(allowed_args), allowed_args, args);
    if (args[ARG_lmk].u_obj != MP_OBJ_NULL) {
        mp_obj_t obj = args[ARG_lmk].u_obj;
        peer->encrypt = mp_obj_is_true(obj);
        if (peer->encrypt) {
            // Key can be <= 16 bytes - padded with '\0'.
            memcpy(peer->lmk,
                _get_bytes_len(obj, ESP_NOW_KEY_LEN),
                ESP_NOW_KEY_LEN);
        }
    }
    if (args[ARG_channel].u_int != -1) {
        peer->channel = args[ARG_channel].u_int;
    }
    if (args[ARG_ifidx].u_int != -1) {
        peer->ifidx = args[ARG_ifidx].u_int;
    }
    if (args[ARG_encrypt].u_obj != MP_OBJ_NULL) {
        peer->encrypt = mp_obj_is_true(args[ARG_encrypt].u_obj);
    }
    return true;
}

// Updates the cached peer count in self->peer_count;
STATIC void _update_peer_count() {
    esp_espnow_obj_t *self = espnow_singleton;
    esp_now_peer_num_t peer_num = {0};
    check_esp_err(esp_now_get_peer_num(&peer_num));
    self->peer_count = peer_num.total_num;
}

// ESPNow.add_peer(peer_mac, [lmk, [channel, [ifidx, [encrypt]]]]) or
// ESPNow.add_peer(peer_mac, [lmk=b'0123456789abcdef'|b''|None|False],
//          [channel=1..11|0], [ifidx=0|1], [encrypt=True|False])
// Positional args set to None will be left at defaults.
STATIC mp_obj_t espnow_add_peer(
    size_t n_args, const mp_obj_t *args, mp_map_t *kw_args) {

    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, _get_peer(args[1]), ESP_NOW_ETH_ALEN);
    _update_peer_info(&peer, n_args - 2, args + 2, kw_args);

    check_esp_err(esp_now_add_peer(&peer));
    _update_peer_count();

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(espnow_add_peer_obj, 2, espnow_add_peer);

// ESPNow.mod_peer(peer_mac, [lmk, [channel, [ifidx, [encrypt]]]]) or
// ESPNow.mod_peer(peer_mac, [lmk=b'0123456789abcdef'|b''|None|False],
//          [channel=1..11|0], [ifidx=0|1], [encrypt=True|False])
// Positional args set to None will be left at current values.
STATIC mp_obj_t espnow_mod_peer(
    size_t n_args, const mp_obj_t *args, mp_map_t *kw_args) {

    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, _get_peer(args[1]), ESP_NOW_ETH_ALEN);
    check_esp_err(esp_now_get_peer(peer.peer_addr, &peer));

    _update_peer_info(&peer, n_args - 2, args + 2, kw_args);

    check_esp_err(esp_now_mod_peer(&peer));
    _update_peer_count();

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(espnow_mod_peer_obj, 2, espnow_mod_peer);

// ESPNow.get_peer(peer_mac).
// Get the ESPNOW peer info struct for the peer_mac as a tuple.
// Returns: a tuple of (peer_addr, lmk, channel, ifidx, encrypt)
STATIC mp_obj_t espnow_get_peer(mp_obj_t _, mp_obj_t arg1) {
    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, _get_peer(arg1), ESP_NOW_ETH_ALEN);

    check_esp_err(esp_now_get_peer(peer.peer_addr, &peer));

    // Return a tuple of (peer_addr, lmk, channel, ifidx, encrypt)
    mp_obj_t items[] = {
        mp_obj_new_bytes(peer.peer_addr, sizeof(peer.peer_addr)),
        mp_obj_new_bytes(peer.lmk, sizeof(peer.lmk)),
        mp_obj_new_int(peer.channel),
        mp_obj_new_int(peer.ifidx),
        (peer.encrypt) ? mp_const_true : mp_const_false,
    };

    return mp_obj_new_tuple(5, items);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(espnow_get_peer_obj, espnow_get_peer);

// ESPNow.del_peer(peer_mac)
// Delete the peer_mac from the ESPNOW list of registered peers.
// Returns: None
STATIC mp_obj_t espnow_del_peer(mp_obj_t _, mp_obj_t arg1) {
    uint8_t peer_addr[ESP_NOW_ETH_ALEN];
    memcpy(peer_addr, _get_peer(arg1), ESP_NOW_ETH_ALEN);

    check_esp_err(esp_now_del_peer(peer_addr));
    _update_peer_count();

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(espnow_del_peer_obj, espnow_del_peer);

// ESPNow.get_peers()
// Fetch a tuple of peer_info records for all registered ESPNow peers.
// Returns: a tuple of tuples:
//     ((peer_addr, lmk, channel, ifidx, encrypt),
//      (peer_addr, lmk, channel, ifidx, encrypt), ...)
STATIC mp_obj_t espnow_get_peers(mp_obj_t _) {
    esp_now_peer_num_t peer_num;
    check_esp_err(esp_now_get_peer_num(&peer_num));

    mp_obj_tuple_t *peerinfo_tuple = mp_obj_new_tuple(peer_num.total_num, NULL);
    esp_now_peer_info_t peer = {0};
    bool from_head = true;
    int count = 0;
    while (esp_now_fetch_peer(from_head, &peer) == ESP_OK) {
        from_head = false;
        mp_obj_t items[] = {
            mp_obj_new_bytes(peer.peer_addr, sizeof(peer.peer_addr)),
            mp_obj_new_bytes(peer.lmk, sizeof(peer.lmk)),
            mp_obj_new_int(peer.channel),
            mp_obj_new_int(peer.ifidx),
            (peer.encrypt) ? mp_const_true : mp_const_false,
        };
        peerinfo_tuple->items[count] = mp_obj_new_tuple(5, items);
        if (++count >= peer_num.total_num) {
            break;          // Should not happen
        }
    }

    return peerinfo_tuple;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(espnow_get_peers_obj, espnow_get_peers);

// ESPNow.espnow_peer_count()
// Get the number of registered peers.
// Returns: a tuple of (num_total_peers, num_encrypted_peers)
STATIC mp_obj_t espnow_peer_count(mp_obj_t _) {
    esp_now_peer_num_t peer_num = {0};
    check_esp_err(esp_now_get_peer_num(&peer_num));

    mp_obj_t items[] = {
        mp_obj_new_int(peer_num.total_num),
        mp_obj_new_int(peer_num.encrypt_num),
    };
    return mp_obj_new_tuple(2, items);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(espnow_peer_count_obj, espnow_peer_count);

// ### Stream I/O protocol functions (to support uasyncio)
//

// Read an ESPNow packet into a stream buffer
STATIC mp_uint_t espnow_stream_read(mp_obj_t self_in, void *buf_in,
    mp_uint_t size, int *errcode) {
    esp_espnow_obj_t *self = espnow_singleton;
    if (!self->initialised) {
        mp_raise_msg(&mp_type_OSError, "not initialised");
    }

    int bytes_read = _buf_get_recv_packet(buf_in, size);
    if (bytes_read < 0) {
        if (bytes_read == BUF_EMPTY || bytes_read == BUF_ESIZE) {
            *errcode = MP_EAGAIN;
            return MP_STREAM_ERROR;
        } else if (bytes_read == BUF_ERROR) {
            mp_raise_ValueError( "ESP-Now: Error in ring buffer");
        }
    }

    return bytes_read;
}

// Adapted from py/stream.c:stream_readinto()
// Want to force just a single read - don't keep looping to fill the buffer.
STATIC mp_obj_t espnow_stream_readinto1(size_t n_args, const mp_obj_t *args) {
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(args[1], &bufinfo, MP_BUFFER_WRITE);

    int bytes_read = _buf_get_recv_packet(bufinfo.buf, bufinfo.len);
    if (bytes_read < 0) {
        if (bytes_read == BUF_EMPTY) {
            return mp_const_none;
        } else if (bytes_read == BUF_ERROR) {
            mp_raise_ValueError("ESP-Now: Error in ring buffer");
        } else if (bytes_read == BUF_ESIZE) {
            mp_raise_ValueError("ESP-Now: Buffer too small");
       }
    }

    return MP_OBJ_NEW_SMALL_INT(bytes_read);
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(
    espnow_stream_readinto1_obj, 2, 3, espnow_stream_readinto1);

// Send messages from a packet in the stream to the peer.
STATIC mp_uint_t espnow_stream_write(mp_obj_t self_in, const void *buf_in,
    mp_uint_t size, int *errcode) {
    const uint8_t *peer_addr, *message;
    size_t msg_len;

    int pkt_len = _buf_get_data_from_packet(
        buf_in, (size_t)size, &peer_addr, &message, &msg_len);

    if (pkt_len > 0) {
        // Send the message to the peer
        int status = _do_espnow_send(peer_addr, message, msg_len, false);
        if (status == BUF_EMPTY) {
            *errcode = MP_EAGAIN;
            return MP_STREAM_ERROR;
        }
    } else if (pkt_len == BUF_ERROR) {
        mp_raise_ValueError("ESP-Now: Error in buffer");
    } else if (pkt_len == BUF_ESIZE) {
        mp_raise_ValueError("ESP-Now: Buffer too small");
    }
    return pkt_len;
}

// Support MP_STREAM_POLL for asyncio
STATIC mp_uint_t espnow_stream_ioctl(mp_obj_t self_in, mp_uint_t request,
    mp_uint_t arg, int *errcode) {
    esp_espnow_obj_t *self = espnow_singleton;
    mp_uint_t ret;
    if (request == MP_STREAM_POLL) {
        mp_uint_t flags = arg;
        ret = 0;
        // Consider read ready when the incoming buffer is not empty
        if ((flags & MP_STREAM_POLL_RD) && !buffer_empty(self->recv_buffer)) {
            ret |= MP_STREAM_POLL_RD;
        }
        // Consider write ready when all sent packets have been acknowledged.
        if ((flags & MP_STREAM_POLL_WR) &&
            self->sent_responses >= self->sent_packets) {
            ret |= MP_STREAM_POLL_WR;
        }
    } else {
        *errcode = MP_EINVAL;
        ret = MP_STREAM_ERROR;
    }
    return ret;
}

// Iterating over ESPNow returns tuples of (peer_addr, message)...
STATIC mp_obj_t espnow_iternext(mp_obj_t self_in) {
    esp_espnow_obj_t *self = espnow_singleton;
    if (!self->initialised) {
        return MP_OBJ_STOP_ITERATION;
    }
    return espnow_irecv(1, &self_in);
}

STATIC void espnow_print(const mp_print_t *print, mp_obj_t self_in,
    mp_print_kind_t kind) {
    esp_espnow_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_printf(print, "ESPNow(rxbuf=%u, timeout=%u)",
        self->recv_buffer_size, self->recv_timeout);
}

STATIC const mp_rom_map_elem_t esp_espnow_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_version), MP_ROM_PTR(&espnow_version_obj) },
    { MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&espnow_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_deinit), MP_ROM_PTR(&espnow_deinit_obj) },
    { MP_ROM_QSTR(MP_QSTR_config), MP_ROM_PTR(&espnow_config_obj) },
    { MP_ROM_QSTR(MP_QSTR_clear), MP_ROM_PTR(&espnow_clear_obj) },
    { MP_ROM_QSTR(MP_QSTR_stats), MP_ROM_PTR(&espnow_stats_obj) },
    { MP_ROM_QSTR(MP_QSTR_recv), MP_ROM_PTR(&espnow_recv_obj) },
    { MP_ROM_QSTR(MP_QSTR_irecv), MP_ROM_PTR(&espnow_irecv_obj) },
    { MP_ROM_QSTR(MP_QSTR_send), MP_ROM_PTR(&espnow_send_obj) },

    // Peer management functions
    { MP_ROM_QSTR(MP_QSTR_set_pmk), MP_ROM_PTR(&espnow_set_pmk_obj) },
    { MP_ROM_QSTR(MP_QSTR_add_peer), MP_ROM_PTR(&espnow_add_peer_obj) },
    { MP_ROM_QSTR(MP_QSTR_mod_peer), MP_ROM_PTR(&espnow_mod_peer_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_peer), MP_ROM_PTR(&espnow_get_peer_obj) },
    { MP_ROM_QSTR(MP_QSTR_del_peer), MP_ROM_PTR(&espnow_del_peer_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_peers), MP_ROM_PTR(&espnow_get_peers_obj) },
    { MP_ROM_QSTR(MP_QSTR_peer_count), MP_ROM_PTR(&espnow_peer_count_obj) },

    // StreamIO and uasyncio support
    { MP_ROM_QSTR(MP_QSTR_read), MP_ROM_PTR(&mp_stream_read_obj) },
    { MP_ROM_QSTR(MP_QSTR_read1), MP_ROM_PTR(&mp_stream_read1_obj) },
    { MP_ROM_QSTR(MP_QSTR_readinto), MP_ROM_PTR(&mp_stream_readinto_obj) },
    { MP_ROM_QSTR(MP_QSTR_readinto1), MP_ROM_PTR(&espnow_stream_readinto1_obj) },
    { MP_ROM_QSTR(MP_QSTR_write), MP_ROM_PTR(&mp_stream_write_obj) },
};
STATIC MP_DEFINE_CONST_DICT(esp_espnow_locals_dict, esp_espnow_locals_dict_table);

STATIC const mp_rom_map_elem_t espnow_globals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_espnow) },
    { MP_ROM_QSTR(MP_QSTR_ESPNow), MP_ROM_PTR(&esp_espnow_type) },
    { MP_ROM_QSTR(MP_QSTR_MAX_DATA_LEN), MP_ROM_INT(ESP_NOW_MAX_DATA_LEN)},
    { MP_ROM_QSTR(MP_QSTR_KEY_LEN), MP_ROM_INT(ESP_NOW_KEY_LEN)},
    { MP_ROM_QSTR(MP_QSTR_MAX_TOTAL_PEER_NUM), MP_ROM_INT(ESP_NOW_MAX_TOTAL_PEER_NUM)},
    { MP_ROM_QSTR(MP_QSTR_MAX_ENCRYPT_PEER_NUM), MP_ROM_INT(ESP_NOW_MAX_ENCRYPT_PEER_NUM)},
};
STATIC MP_DEFINE_CONST_DICT(espnow_globals_dict, espnow_globals_dict_table);

STATIC const mp_stream_p_t espnow_stream_p = {
    .read = espnow_stream_read,
    .write = espnow_stream_write,
    .ioctl = espnow_stream_ioctl,
    .is_text = false,
};

const mp_obj_type_t esp_espnow_type = {
    { &mp_type_type },
    .name = MP_QSTR_ESPNow,
    .make_new = espnow_make_new,
    .print = espnow_print,
    .getiter = mp_identity_getiter,
    .iternext = espnow_iternext,
    .protocol = &espnow_stream_p,
    .locals_dict = (mp_obj_t)&esp_espnow_locals_dict,
};

const mp_obj_module_t mp_module_esp_espnow = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&espnow_globals_dict,
};

// These functions help cleanup boiler plate code for processing args.
static bool _get_bool(mp_obj_t obj) {
    if (obj == mp_const_true || obj == mp_const_false) {
        return obj == mp_const_true;
    }
    mp_raise_TypeError("Expected True or False");
}

static const uint8_t *_get_bytes_len(mp_obj_t obj, size_t len) {
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(obj, &bufinfo, MP_BUFFER_READ);
    if (bufinfo.len != len) {
        mp_raise_ValueError("Wrong length");
    }
    return (const uint8_t *)bufinfo.buf;
}

// Check obj is a byte string and return ptr to mac address.
static const uint8_t *_get_peer(mp_obj_t obj) {
    return mp_obj_is_true(obj)
        ? _get_bytes_len(obj, ESP_NOW_ETH_ALEN) : NULL;
}

// Put received message into the buffer (called from recv_cb()).
static void
_buf_put_recv_data(buffer_t buf, const uint8_t *mac,
    const uint8_t *msg, size_t msg_len
    ) {
    uint8_t header[2] = {ESPNOW_MAGIC, msg_len};
    buffer_put(buf, header, sizeof(header));
    buffer_put(buf, mac, ESP_NOW_ETH_ALEN);
    buffer_put(buf, msg, msg_len);
}

// Get the peer mac address and message from a packet in the buffer.
// Assumes msg_len is correct and does not check packet format.
// Get the length and check the packet first with _buf_peek_message_length().
static bool
_buf_get_recv_data(buffer_t buf, uint8_t *mac, uint8_t *msg, int msg_len) {
    uint8_t header[2];              // Copy out the header and ignore it
    return msg_len > 0 &&
           buffer_get(buf, header, sizeof(header)) &&
           buffer_get(buf, mac, ESP_NOW_ETH_ALEN) &&
           buffer_get(buf, msg, msg_len);
}

// Validate a recv packet header, check message fits in max size and return
// length of the message.
//   >0: Number of bytes copied from the ring buffer
//    0: requested size == 0
//   BUF_ERROR (=-2): Error in the ring buffer
//   BUF_ESIZE (=-3): Packet is too large for buffer size provided
static int
_buf_check_message_length(const uint8_t *header, size_t max_size) {
    // header[0] == magic number and header[1] = message length
    return
        (max_size > 0)
            ? (header[0] == ESPNOW_MAGIC && header[1] <= ESP_NOW_MAX_DATA_LEN)
                ? (header[1] <= max_size)
                    ? header[1]     // Success: return length of message
                    : BUF_ESIZE     // Packet is too big for buffer
                : BUF_ERROR         // Packet header is wrong
            : 0;                    // Required by semantics of read()/write()
}

static int
_buf_check_packet_length(const uint8_t *header, size_t max_size) {
    int msg_len = _buf_check_message_length(header, max_size - ESPNOW_HDR_LEN);
    return (msg_len > 0) ? msg_len + ESPNOW_HDR_LEN : msg_len;
}

// Peek at the next recv packet in the ring buffer to get the message length.
// Also validates the packet header.
static int
_buf_peek_message_length(buffer_t buf) {
    // Check for the magic byte followed by the message length
    uint8_t header[2];
    return buffer_peek(buf, header, sizeof(header))
            ? _buf_check_message_length(header, ESP_NOW_MAX_DATA_LEN)
            : BUF_EMPTY;
}

// ### Buffer functions for the stream I/O read/write functions
//

// Copy the whole recv packet from the ring buffer.
// Used by the stream I/O functions read(), read(1) and readinto() and asyncio.
static int
_buf_get_recv_packet(uint8_t *buf, size_t size) {
    esp_espnow_obj_t *self = espnow_singleton;
    // Get length of message in packet and validate the packet
    uint8_t header[2];
    int pkt_len = (
        buffer_peek(self->recv_buffer, header, sizeof(header))
            ? _buf_check_packet_length(header, size)
            : BUF_EMPTY);
    return (pkt_len <= 0)
            ? pkt_len       // Return the error code.
            : buffer_get(self->recv_buffer, buf, pkt_len)
                ? pkt_len
                : BUF_EMPTY;
}

// Get ptrs to the peer mac address, message and message length from
// an ESPNow packet.
// Used by the stream I/O write() function and asyncio.
static int
_buf_get_data_from_packet(const uint8_t *buf, size_t size, const uint8_t **peer,
    const uint8_t **msg, size_t *msg_len
    ) {
    // Get a pointer to the peer MAC address and the message
    *peer = buf + ESPNOW_PEER_OFFSET;
    *msg = buf + ESPNOW_MSG_OFFSET;
    int pkt_len = _buf_check_packet_length(buf, size);
    if (pkt_len > 0) {
        *msg_len = pkt_len - ESPNOW_HDR_LEN;
    }
    return pkt_len;
}
