/*
 * Copyright (c) 2016, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include <assert.h>
#include <string.h>

#include "py/mpconfig.h"
#include "py/nlr.h"
#include "py/runtime.h"
#include "rom/md5_hash.h"
#include "mpexception.h"

/******************************************************************************
 DEFINE PRIVATE TYPES
 ******************************************************************************/
typedef struct _mp_obj_hash_t {
    mp_obj_base_t base;
    uint8_t *buffer;
    uint32_t b_size;
    uint32_t c_size;
    uint8_t  algo;
    uint8_t  h_size;
    bool  fixedlen;
    bool  digested;
    bool  inprogress;
    uint8_t hash[32];
} mp_obj_hash_t;

STATIC mp_obj_hash_t mp_obj_hash = {.inprogress = false};

static struct MD5Context md5_context;
/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/
STATIC void hash_update_internal(mp_obj_t self_in, mp_obj_t data, bool digest);
STATIC mp_obj_t hash_read (mp_obj_t self_in);

/******************************************************************************
 DEFINE PRIVATE FUNCTIONS
 ******************************************************************************/
STATIC void hash_update_internal(mp_obj_t self_in, mp_obj_t data, bool digest) {
    mp_obj_hash_t *self = self_in;
    mp_buffer_info_t bufinfo;

    if (data) {
        mp_get_buffer_raise(data, &bufinfo, MP_BUFFER_READ);
    }

    if (digest) {
        self->inprogress = true;
//        CRYPTOHASH_SHAMD5Start (self->algo, self->b_size);
    }

    if (self->c_size < self->b_size || !data || !self->fixedlen) {
//        if (digest || self->fixedlen) {
//            // no data means we want to process our internal buffer
//            CRYPTOHASH_SHAMD5Update (data ? bufinfo.buf : self->buffer, data ? bufinfo.len : self->b_size);
            MD5Update(&md5_context, bufinfo.buf, bufinfo.len);
            self->c_size += data ? bufinfo.len : 0;
//        } else {
//            self->buffer = m_renew(byte, self->buffer, self->b_size, self->b_size + bufinfo.len);
//            MP_STATE_PORT(hash_buffer) = self->buffer;
//            mp_seq_copy((byte*)self->buffer + self->b_size, bufinfo.buf, bufinfo.len, byte);
//            self->b_size += bufinfo.len;
//            self->digested = false;
//        }
    } else {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));
    }
}

STATIC mp_obj_t hash_read (mp_obj_t self_in) {
    mp_obj_hash_t *self = self_in;

//    if (!self->fixedlen) {
//        if (!self->digested) {
//            hash_update_internal(self, MP_OBJ_NULL, true);
//        }
//    } else if (self->c_size < self->b_size) {
//        // it's a fixed len block which is still incomplete
//        nlr_raise(mp_obj_new_exception_msg(&mp_type_OSError, mpexception_os_request_not_possible));
//    }

    if (!self->digested) {
//        CRYPTOHASH_SHAMD5Read ((uint8_t *)self->hash);
        MD5Final((uint8_t *)self->hash, &md5_context);
        self->digested = true;
        self->inprogress = false;
    }
    return mp_obj_new_bytes(self->hash, self->h_size);
}

/******************************************************************************/
// Micro Python bindings

/// \classmethod \constructor([data[, block_size]])
/// initial data must be given if block_size wants to be passed
STATIC mp_obj_t hash_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 0, 2, false);
    // this is needed to abort a previous operation
    if (mp_obj_hash.inprogress) {
        if (mp_obj_hash.c_size < mp_obj_hash.b_size) {
//            CRYPTOHASH_SHAMD5Update ((uint8_t *)0x20000000, mp_obj_hash.b_size - mp_obj_hash.c_size);
        }
//        MAP_SHAMD5ResultRead(SHAMD5_BASE, (uint8_t *)mp_obj_hash.hash);
        mp_obj_hash.inprogress = false;
    }
    mp_obj_hash_t *self = &mp_obj_hash;
    memset(&mp_obj_hash, 0, sizeof(mp_obj_hash));
    self->base.type = type;
//    if (self->base.type->name == MP_QSTR_sha1) {
//        self->algo = SHAMD5_ALGO_SHA1;
//        self->h_size = 20;
//    } else /* if (self->base.type->name == MP_QSTR_sha256) */ {
//        self->algo = SHAMD5_ALGO_SHA256;
//        self->h_size = 32;
//    } /* else {
//        self->algo = SHAMD5_ALGO_MD5;
//        self->h_size = 32;
//    } */

    self->h_size = 16;
    MD5Init(&md5_context);
    if (n_args) {
        // CPython extension to avoid buffering the data before digesting it
        // Note: care must be taken to provide all intermediate blocks as multiple
        //       of four bytes, otherwise the resulting hash will be incorrect.
        //       the final block can be of any length
        if (n_args > 1) {
            uint32_t b_size = mp_obj_get_int(args[1]);
            if (b_size > 0) {
                // block size given and > 0, we will feed the data directly into the hash engine
                self->fixedlen = true;
                self->b_size = b_size;
                hash_update_internal(self, args[0], true);
            }
        } else {
            hash_update_internal(self, args[0], false);
        }
    }
    return self;
}

STATIC mp_obj_t hash_update(mp_obj_t self_in, mp_obj_t arg) {
    mp_obj_hash_t *self = self_in;
    hash_update_internal(self, arg, false);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_2(hash_update_obj, hash_update);

STATIC mp_obj_t hash_digest(mp_obj_t self_in) {
    return hash_read(self_in);
}
MP_DEFINE_CONST_FUN_OBJ_1(hash_digest_obj, hash_digest);

STATIC const mp_map_elem_t hash_locals_dict_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR_update),    (mp_obj_t) &hash_update_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_digest),    (mp_obj_t) &hash_digest_obj },
};

STATIC MP_DEFINE_CONST_DICT(hash_locals_dict, hash_locals_dict_table);

STATIC const mp_obj_type_t md5_type = {
    { &mp_type_type },
    .name = MP_QSTR_md5,
    .make_new = hash_make_new,
    .locals_dict = (mp_obj_t)&hash_locals_dict,
};

//STATIC const mp_obj_type_t sha1_type = {
//    { &mp_type_type },
//    .name = MP_QSTR_sha1,
//    .make_new = hash_make_new,
//    .locals_dict = (mp_obj_t)&hash_locals_dict,
//};
//
//STATIC const mp_obj_type_t sha256_type = {
//    { &mp_type_type },
//    .name = MP_QSTR_sha256,
//    .make_new = hash_make_new,
//    .locals_dict = (mp_obj_t)&hash_locals_dict,
//};

STATIC const mp_map_elem_t mp_module_hashlib_globals_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR___name__),    MP_OBJ_NEW_QSTR(MP_QSTR_uhashlib) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_md5),         (mp_obj_t)&md5_type },
//    { MP_OBJ_NEW_QSTR(MP_QSTR_sha1),        (mp_obj_t)&sha1_type },
//    { MP_OBJ_NEW_QSTR(MP_QSTR_sha256),      (mp_obj_t)&sha256_type },
};

STATIC MP_DEFINE_CONST_DICT(mp_module_hashlib_globals, mp_module_hashlib_globals_table);

const mp_obj_module_t mp_module_uhashlib = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&mp_module_hashlib_globals,
};

