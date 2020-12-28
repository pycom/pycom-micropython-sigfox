/*
 * This file is derived from the MicroPython project, http://micropython.org/
 *
 * Copyright (c) 2020, Pycom Limited and its licensors.
 *
 * This software is licensed under the GNU GPL version 3 or any later version,
 * with permitted additional terms. For more information see the Pycom Licence
 * v1.0 document supplied with this file, or available at:
 * https://www.pycom.io/opensource/licensing
 */

/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Nick Moore
 * Copyright (c) 2018 shawwwn <shawwwn1@gmail.com>
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
 *
 *
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
#include "py/nlr.h"
#include "py/objlist.h"
#include "py/runtime.h"
#include "py/mphal.h"
#include "py/mperrno.h"

#include "modnetwork.h"
#include "modespnow.h"
#include "mpirq.h"
#include "pycom_general_util.h"

/******************************************************************************
 DEFINE CONSTANTS
 ******************************************************************************/
/******************************************************************************
 DEFINE PRIVATE TYPES
 ******************************************************************************/
typedef struct mod_espnow_peer_obj_s {
    mp_obj_base_t                    base;
    uint8_t                          peer_addr[ESP_NOW_ETH_ALEN];
    uint8_t                          lmk[ESP_NOW_KEY_LEN];
    bool                             encrypt;
    struct mod_espnow_peer_obj_s*    prev;
    struct mod_espnow_peer_obj_s*    next;
}mod_espnow_peer_obj_t;
/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/
STATIC void mod_esp_espnow_exceptions(esp_err_t e);
STATIC void get_bytes(mp_obj_t str, size_t len, uint8_t *dst);
STATIC void send_queue_handler(void *arg);
STATIC void IRAM_ATTR send_cb(const uint8_t *macaddr, esp_now_send_status_t status);
STATIC void recv_queue_handler(void *arg);
STATIC void IRAM_ATTR recv_cb(const uint8_t *macaddr, const uint8_t *data, int len);
STATIC esp_err_t mac_str_to_byte(const uint8_t* mac_string, const size_t length, uint8_t* output);
STATIC void get_MAC_addr(mp_obj_t str, uint8_t *dst);
STATIC void add_peer(mod_espnow_peer_obj_t* peer);
STATIC void remove_peer(mod_espnow_peer_obj_t* peer);
STATIC mod_espnow_peer_obj_t* get_peer(const uint8_t* peer_addr);

/******************************************************************************
 DEFINE PRIVATE VARIABLES
 ******************************************************************************/
STATIC const mp_obj_type_t mod_espnow_peer_type;
STATIC bool initialized = false;
STATIC mp_obj_t send_cb_obj = mp_const_none;
STATIC mp_obj_t recv_cb_obj = mp_const_none;
STATIC mod_espnow_peer_obj_t* peer_list = NULL;

/******************************************************************************
 DEFINE PRIVATE FUNCTIONS
 ******************************************************************************/

// Transform the incoming MAC Address from String format to Bytes
STATIC esp_err_t mac_str_to_byte(const uint8_t* mac_string, const size_t length, uint8_t* output) {
    if(length == 2*ESP_NOW_ETH_ALEN) {
        for(int i = 0, j = 0; i < length; i = i+2) {
            uint8_t lower_nibble = hex_from_char(mac_string[i+1]);
            uint8_t upper_nibble = hex_from_char(mac_string[i]);

            // Only HEX characters are allowed
            if(lower_nibble == 16 || upper_nibble == 16) {
                return ESP_FAIL;
            }

            output[j] = lower_nibble | (upper_nibble << 4);
            j++;
        }

        return ESP_OK;
    }
    else {
        return ESP_FAIL;
    }
}

// Form MAC address from a MicroPython string or byte array
STATIC void get_MAC_addr(mp_obj_t str, uint8_t *dst){

    esp_err_t ret = ESP_OK;
    size_t str_len;
    const char *data = mp_obj_str_get_data(str, &str_len);
    // Input parameter contains the MAC address in form of Byte Array
    if(str_len == ESP_NOW_ETH_ALEN) {
        memcpy(dst, data, str_len);
    }
    // Input parameter contains the MAC address in form of String
    else if(str_len == 2*ESP_NOW_ETH_ALEN) {
        ret = mac_str_to_byte((const uint8_t*)data, str_len, dst);
    }
    else {
        ret = ESP_FAIL;
    }

    if(ret != ESP_OK) {
        mp_raise_ValueError("Invalid MAC Address");
    }
}

// Add peer to the linked-list of peers
STATIC void add_peer(mod_espnow_peer_obj_t* peer) {
    peer->next = NULL;

    if(peer_list == NULL) {
        peer->prev = NULL;
        peer_list = peer;
    }
    else{
        mod_espnow_peer_obj_t* peer_tmp = peer_list;
        while(peer_tmp->next != NULL) {
            peer_tmp = peer_tmp->next;
        }
        peer_tmp->next = peer;
        peer->prev = peer_tmp;
    }
}

// Remove a peer from the linked-list of peers
STATIC void remove_peer(mod_espnow_peer_obj_t* peer) {

    // If this is the only element in the list, then invalidate the list
    if(peer->prev == NULL && peer->next == NULL) {
        peer_list = NULL;
    }
    // If this is not the only element in the list then remove it correctly from the list
    else {
        // If this is the first element in the list, move start of the list to next element, it might be NULL, that means the list is empty
        if(peer == peer_list) {
            peer_list = peer->next;
        }

        // Chain together the previous and next element, if any
        if(peer->prev != NULL) {
            peer->prev->next = peer->next;
        }
        if(peer->next != NULL) {
            peer->next->prev = peer->prev;
        }
    }
    // Free up the memory
    m_del_obj((mp_obj_t)&mod_espnow_peer_type, peer);
}

// Find a peer based on MAC address
STATIC mod_espnow_peer_obj_t* get_peer(const uint8_t *peer_addr) {

    mod_espnow_peer_obj_t* peer = peer_list;
    while(peer != NULL) {
        if(memcmp(peer_addr, peer->peer_addr, ESP_NOW_ETH_ALEN) == 0) {
            return peer;
        }
        peer = peer->next;
    }
    return mp_const_none;
}

// Remove all peers from the linked-list
STATIC void remove_all_peers() {

    mod_espnow_peer_obj_t* peer = peer_list;
    mod_espnow_peer_obj_t* peer_tmp = peer;
    while(peer != NULL) {
        peer_tmp = peer->next;
        remove_peer(peer);
        peer = peer_tmp;
    }
}

STATIC void mod_esp_espnow_exceptions(esp_err_t e) {
   switch (e) {
       case ESP_OK:
       break;
      case ESP_ERR_ESPNOW_NOT_INIT:
        mp_raise_msg(&mp_type_OSError, "ESP-Now Not Initialized");
        break;
      case ESP_ERR_ESPNOW_ARG:
        mp_raise_msg(&mp_type_OSError, "ESP-Now Invalid Argument");
        break;
      case ESP_ERR_ESPNOW_NO_MEM:
        mp_raise_msg(&mp_type_OSError, "ESP-Now Out Of Mem");
        break;
      case ESP_ERR_ESPNOW_FULL:
        mp_raise_msg(&mp_type_OSError, "ESP-Now Peer List Full");
        break;
      case ESP_ERR_ESPNOW_NOT_FOUND:
        mp_raise_msg(&mp_type_OSError, "ESP-Now Peer Not Found");
        break;
      case ESP_ERR_ESPNOW_INTERNAL:
        mp_raise_msg(&mp_type_OSError, "ESP-Now Internal");
        break;
      case ESP_ERR_ESPNOW_EXIST:
        mp_raise_msg(&mp_type_OSError, "ESP-Now Peer Exists");
        break;
      default:
        nlr_raise(mp_obj_new_exception_msg_varg(
          &mp_type_RuntimeError, "ESP-Now Unknown Error 0x%04x", e
        ));
        break;
   }
}

STATIC void get_bytes(mp_obj_t str, size_t len, uint8_t *dst) {
    size_t str_len;
    const char *data = mp_obj_str_get_data(str, &str_len);
    if (str_len != len) mp_raise_ValueError("bad len");
    memcpy(dst, data, len);
}

STATIC void send_queue_handler(void *arg) {
    // this function will be called by the interrupt thread
    mp_obj_tuple_t *msg = arg;
    if (send_cb_obj != mp_const_none) {
        mp_call_function_1(send_cb_obj, msg);
    }
}

STATIC void IRAM_ATTR send_cb(const uint8_t *macaddr, esp_now_send_status_t status)
{
    if (send_cb_obj != mp_const_none) {
        mp_obj_tuple_t *msg = mp_obj_new_tuple(2, NULL);
        msg->items[0] = get_peer(macaddr);
        msg->items[1] = (status == ESP_NOW_SEND_SUCCESS) ? mp_const_true : mp_const_false;
        mp_irq_queue_interrupt(send_queue_handler, msg);
    }
}

STATIC void recv_queue_handler(void *arg) {
    // this function will be called by the interrupt thread
    mp_obj_tuple_t *msg = arg;
    if (recv_cb_obj != mp_const_none) {
        mp_call_function_1(recv_cb_obj, msg);
    }
}

STATIC void IRAM_ATTR recv_cb(const uint8_t *macaddr, const uint8_t *data, int len)
{
    if (recv_cb_obj != mp_const_none) {
        mp_obj_tuple_t *msg = mp_obj_new_tuple(3, NULL);
        msg->items[0] = mp_obj_new_str((const char *)macaddr, ESP_NOW_ETH_ALEN);
        msg->items[1] = get_peer(macaddr);
        msg->items[2] = mp_obj_new_bytes(data, len);
        mp_irq_queue_interrupt(recv_queue_handler, msg);
    }
}

/******************************************************************************
 DEFINE ESP-NOW PEER CLASS FUNCTIONS
 ******************************************************************************/

STATIC mp_obj_t mod_espnow_peer_addr(mp_obj_t self_in) {

    mod_espnow_peer_obj_t* self = (mod_espnow_peer_obj_t*)self_in;
    return mp_obj_new_str((const char *)self->peer_addr, ESP_NOW_ETH_ALEN);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mod_espnow_peer_addr_obj, mod_espnow_peer_addr);

STATIC mp_obj_t mod_espnow_peer_lmk(size_t n_args, const mp_obj_t *args) {

    mod_espnow_peer_obj_t* self = (mod_espnow_peer_obj_t*)args[0];

    if(n_args == 1) {
        if(self->encrypt) {
            return mp_obj_new_str((const char *)self->lmk, ESP_NOW_KEY_LEN);
        }
    }
    else {
        esp_now_peer_info_t peer;
        mod_esp_espnow_exceptions(esp_now_get_peer(self->peer_addr, &peer));
        if(args[1] == mp_const_none) {
            peer.encrypt = false;
            mod_esp_espnow_exceptions(esp_now_mod_peer(&peer));
            self->encrypt = false;
        }
        else {
            get_bytes(args[1], ESP_NOW_KEY_LEN, peer.lmk);
            peer.encrypt = true;
            mod_esp_espnow_exceptions(esp_now_mod_peer(&peer));
            memcpy(self->lmk, peer.lmk, ESP_NOW_KEY_LEN);
            self->encrypt = true;
        }

    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_espnow_peer_lmk_obj, 1, 2, mod_espnow_peer_lmk);


STATIC mp_obj_t mod_espnow_peer_send(mp_obj_t self_in, mp_obj_t msg) {

    if(msg != mp_const_none) {
        mod_espnow_peer_obj_t* self = (mod_espnow_peer_obj_t*)self_in;
        mp_uint_t msg_len;
        const uint8_t *msg_buf = (const uint8_t *)mp_obj_str_get_data(msg, &msg_len);
        if (msg_len > ESP_NOW_MAX_DATA_LEN) mp_raise_ValueError("Message is too long");

        mod_esp_espnow_exceptions(esp_now_send(self->peer_addr, msg_buf, msg_len));
    }
    else {
        mp_raise_ValueError("Message is invalid");
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mod_espnow_peer_send_obj, mod_espnow_peer_send);


/******************************************************************************
 DEFINE ESP-NOW MODULE FUNCTIONS
 ******************************************************************************/

STATIC mp_obj_t mod_espnow_init() {

    if(initialized == false) {

        mod_esp_espnow_exceptions(esp_now_init());
        mod_esp_espnow_exceptions(esp_now_register_recv_cb(recv_cb));
        mod_esp_espnow_exceptions(esp_now_register_send_cb(send_cb));

        initialized = true;
    }
    else {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "ESP-NOW module already initialized!"));
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mod_espnow_init_obj, mod_espnow_init);

STATIC mp_obj_t mod_espnow_deinit() {
    if(initialized == true) {
        mod_esp_espnow_exceptions(esp_now_deinit());
        remove_all_peers();
        initialized = false;
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mod_espnow_deinit_obj, mod_espnow_deinit);

STATIC mp_obj_t mod_espnow_on_send(size_t n_args, const mp_obj_t *args) {
    if(initialized == true) {
        if (n_args == 0) {
            return send_cb_obj;
        }
        send_cb_obj = args[0];
    }
    else {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "ESP-NOW module is not initialized!"));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_espnow_on_send_obj, 0, 1, mod_espnow_on_send);

STATIC mp_obj_t mod_espnow_on_recv(size_t n_args, const mp_obj_t *args) {

    if(initialized == true) {
        if (n_args == 0) {
            return recv_cb_obj;
        }
        recv_cb_obj = args[0];
    }
    else {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "ESP-NOW module is not initialized!"));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_espnow_on_recv_obj, 0, 1, mod_espnow_on_recv);

STATIC mp_obj_t mod_espnow_pmk(mp_obj_t key) {

    if(initialized == true) {
        uint8_t buf[ESP_NOW_KEY_LEN];
        get_bytes(key, ESP_NOW_KEY_LEN, buf);
        mod_esp_espnow_exceptions(esp_now_set_pmk(buf));
    }
    else {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "ESP-NOW module is not initialized!"));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mod_espnow_pmk_obj, mod_espnow_pmk);

STATIC mp_obj_t mod_espnow_add_peer(size_t n_args, const mp_obj_t *args) {

    if(initialized == true) {
        esp_now_peer_info_t peer = {0};
        get_MAC_addr(args[0], peer.peer_addr);
        if (n_args > 1) {
            get_bytes(args[1], ESP_NOW_KEY_LEN, peer.lmk);
            peer.encrypt = true;
        }
        else {
            peer.encrypt = false;
        }
        mod_esp_espnow_exceptions(esp_now_add_peer(&peer));

        mod_espnow_peer_obj_t *peer_obj = m_new_obj(mod_espnow_peer_obj_t);
        peer_obj->base.type = (mp_obj_t)&mod_espnow_peer_type;
        memcpy(peer_obj->peer_addr, peer.peer_addr, sizeof(peer_obj->peer_addr));
        if(peer.encrypt) {
            memcpy(peer_obj->lmk, peer.lmk, ESP_NOW_KEY_LEN);
            peer_obj->encrypt = true;
        }
        else {
            peer_obj->encrypt = false;
        }
        add_peer(peer_obj);
        return peer_obj;
    }
    else {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "ESP-NOW module is not initialized!"));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_espnow_add_peer_obj, 1, 2, mod_espnow_add_peer);

STATIC mp_obj_t mod_espnow_del_peer(mp_obj_t peer_obj_in) {
    if(initialized == true) {
        mod_espnow_peer_obj_t* peer_obj = (mod_espnow_peer_obj_t*)peer_obj_in;
        mod_esp_espnow_exceptions(esp_now_del_peer(peer_obj->peer_addr));
        remove_peer(peer_obj);
    }
    else {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "ESP-NOW module is not initialized!"));
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mod_espnow_del_peer_obj, mod_espnow_del_peer);

STATIC mp_obj_t mod_espnow_send(mp_obj_t addr, mp_obj_t msg) {

    if(initialized == true) {
        mp_uint_t addr_len;
        const uint8_t *addr_buf;
        mp_uint_t msg_len;
        const uint8_t *msg_buf = (const uint8_t *)mp_obj_str_get_data(msg, &msg_len);
        if (msg_len > ESP_NOW_MAX_DATA_LEN) mp_raise_ValueError("msg too long");

        if (addr == mp_const_none) {
            // Send to all peers
            mod_esp_espnow_exceptions(esp_now_send(NULL, msg_buf, msg_len));
        } else {
            // Send to the specified peer
            addr_buf = (const uint8_t *)mp_obj_str_get_data(addr, &addr_len);
            if (addr_len != ESP_NOW_ETH_ALEN) mp_raise_ValueError("addr invalid");
            mod_esp_espnow_exceptions(esp_now_send(addr_buf, msg_buf, msg_len));
        }
    }
    else {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "ESP-NOW module is not initialized!"));
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mod_espnow_send_obj, mod_espnow_send);

STATIC mp_obj_t mod_espnow_peer_count() {
    if(initialized == true) {
        esp_now_peer_num_t peer_num = {0};
        mod_esp_espnow_exceptions(esp_now_get_peer_num(&peer_num));

        mp_obj_t tuple[2];
        tuple[0] = mp_obj_new_int(peer_num.total_num);
        tuple[1] = mp_obj_new_int(peer_num.encrypt_num);
        return mp_obj_new_tuple(2, tuple);
    }
    else {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "ESP-NOW module is not initialized!"));
    }

    return mp_const_none;

}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mod_espnow_peer_count_obj, mod_espnow_peer_count);

STATIC mp_obj_t mod_espnow_version() {
    if(initialized == true) {
        uint32_t version;
        mod_esp_espnow_exceptions(esp_now_get_version(&version));
        return mp_obj_new_int(version);
    }
    else {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_RuntimeError, "ESP-NOW module is not initialized!"));
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mod_espnow_version_obj, mod_espnow_version);

STATIC const mp_map_elem_t mod_espnow_peer_locals_dict_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR_addr),                    (mp_obj_t)&mod_espnow_peer_addr_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_lmk),                     (mp_obj_t)&mod_espnow_peer_lmk_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_send),                    (mp_obj_t)&mod_espnow_peer_send_obj },
};
STATIC MP_DEFINE_CONST_DICT(mod_espnow_peer_locals_dict, mod_espnow_peer_locals_dict_table);

static const mp_obj_type_t mod_espnow_peer_type = {
    { &mp_type_type },
    .name = MP_QSTR_ESPNOW_Peer,
    .locals_dict = (mp_obj_t)&mod_espnow_peer_locals_dict,
};

STATIC const mp_rom_map_elem_t mod_espnow_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),        MP_ROM_QSTR(MP_QSTR_espnow) },
    { MP_ROM_QSTR(MP_QSTR_init),            (mp_obj_t)&mod_espnow_init_obj },
    { MP_ROM_QSTR(MP_QSTR_deinit),          (mp_obj_t)&mod_espnow_deinit_obj },
    { MP_ROM_QSTR(MP_QSTR_pmk),             (mp_obj_t)&mod_espnow_pmk_obj },
    { MP_ROM_QSTR(MP_QSTR_add_peer),        (mp_obj_t)&mod_espnow_add_peer_obj },
    { MP_ROM_QSTR(MP_QSTR_del_peer),        (mp_obj_t)&mod_espnow_del_peer_obj },
    { MP_ROM_QSTR(MP_QSTR_send),            (mp_obj_t)&mod_espnow_send_obj },
    { MP_ROM_QSTR(MP_QSTR_on_send),         (mp_obj_t)&mod_espnow_on_send_obj },
    { MP_ROM_QSTR(MP_QSTR_on_recv),         (mp_obj_t)&mod_espnow_on_recv_obj },
    { MP_ROM_QSTR(MP_QSTR_peer_count),      (mp_obj_t)&mod_espnow_peer_count_obj },
    { MP_ROM_QSTR(MP_QSTR_version),         (mp_obj_t)&mod_espnow_version_obj },
};

STATIC MP_DEFINE_CONST_DICT(mod_espnow_module_globals, mod_espnow_module_globals_table);

const mp_obj_module_t mod_espnow = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&mod_espnow_module_globals,
};
