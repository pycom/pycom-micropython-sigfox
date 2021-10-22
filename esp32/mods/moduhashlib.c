/*
 * Copyright (c) 2021, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include <assert.h>
#include <string.h>

#define MBEDTLS_SHA1_ALT
#define MBEDTLS_SHA256_ALT
#define MBEDTLS_SHA512_ALT

#include "py/mpconfig.h"
#include "py/nlr.h"
#include "py/runtime.h"
#include "sha1_alt.h"
#include "sha256_alt.h"
#include "sha512_alt.h"
#include "rom/md5_hash.h"
#include "hwcrypto/sha.h"
#include "mpexception.h"
#include "mbedtls/sha1.h"
#include "mbedtls/sha256.h"
#include "mbedtls/sha512.h"

STATIC bool hash_busy = false;

/******************************************************************************
 DEFINE PRIVATE TYPES
 ******************************************************************************/
typedef struct _mp_obj_hash_t {
    mp_obj_base_t base;
    uint8_t  b_size;
    uint8_t  h_size;
    uint8_t  buf_count;
    bool  digested;
    uint8_t buffer[128];
    union {
        struct MD5Context md5_context;
        mbedtls_sha1_context sha1_context;
        mbedtls_sha256_context sha256_context;
        mbedtls_sha512_context sha512_context;
    }u;
} mp_obj_hash_t;


/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/
STATIC void hash_update_internal(mp_obj_t self_in, mp_obj_t data, bool digest);
STATIC mp_obj_t hash_read (mp_obj_t self_in);

/******************************************************************************
 DEFINE PRIVATE FUNCTIONS
 ******************************************************************************/


STATIC void generic_hash_update(mp_obj_hash_t *self, void *data, uint32_t len) {
    switch (self->base.type->name) {
    case MP_QSTR_sha1:
        mbedtls_sha1_update_ret(&self->u.sha1_context, data, len);
        break;

    case MP_QSTR_sha224:
    case MP_QSTR_sha256:
        mbedtls_sha256_update_ret(&self->u.sha256_context, data, len);
        break;

    case MP_QSTR_sha384:
    case MP_QSTR_sha512:
        mbedtls_sha512_update_ret(&self->u.sha512_context, data, len);
        break;

    case MP_QSTR_md5:
        MD5Update(&self->u.md5_context, data, len);
        break;
    }
}

STATIC void hash_update_internal(mp_obj_t self_in, mp_obj_t data, bool digest) {
    mp_obj_hash_t *self = self_in;
    mp_buffer_info_t bufinfo;
    uint32_t len;

    if (digest == true) {
        generic_hash_update(self, self->buffer, self->buf_count);
        return;
    }

    if (!data) {
        return;
    }

    mp_get_buffer_raise(data, &bufinfo, MP_BUFFER_READ);

    // Try to fill the buffer to the top
    len = self->b_size - self->buf_count;
    len = MIN(len, bufinfo.len);

    memcpy(self->buffer + self->buf_count, bufinfo.buf, len);

    self->buf_count += len;

    // check if there is a complete block
    if (self->buf_count != self->b_size && digest == false) {
        return;
    }

    // process it
    generic_hash_update(self, self->buffer, self->buf_count);
    self->buf_count = 0;

    // update the input string information
    bufinfo.buf = ((uint8_t *) bufinfo.buf) + len;
    bufinfo.len -= len;

    // see if there is at least another complete block
    len = bufinfo.len / self->b_size;
    if (len != 0) {
        // process it (them)
        len *= self->b_size;
        generic_hash_update(self, bufinfo.buf, len);

        // update the input string information
        bufinfo.buf = ((uint8_t *) bufinfo.buf) + len;
        bufinfo.len -= len;
    }

    // and copy any remaining bytes to the buffer
    memcpy(self->buffer, bufinfo.buf, bufinfo.len);
    self->buf_count = bufinfo.len;
}

STATIC mp_obj_t hash_read(mp_obj_t self_in) {
    mp_obj_hash_t *self = self_in;

    if (!self->digested) {
        // process any remaining data in the buffer
        hash_update_internal(self, MP_OBJ_NULL, true);

        switch (self->base.type->name) {
        case MP_QSTR_sha1:
            mbedtls_sha1_finish_ret(&self->u.sha1_context, (uint8_t *)self->buffer);
            mbedtls_sha1_free(&self->u.sha1_context);
            break;

        case MP_QSTR_sha224:
        case MP_QSTR_sha256:
            mbedtls_sha256_finish_ret(&self->u.sha256_context, (uint8_t *)self->buffer);
            mbedtls_sha256_free(&self->u.sha256_context);
            break;

        case MP_QSTR_sha384:
        case MP_QSTR_sha512:
            mbedtls_sha512_finish_ret(&self->u.sha512_context, (uint8_t *)self->buffer);
            mbedtls_sha512_free(&self->u.sha512_context);
            break;

        case MP_QSTR_md5:
            MD5Final((uint8_t *)self->buffer, &self->u.md5_context);
            break;
        }

        self->digested = true;
        hash_busy = false;
    }

    return mp_obj_new_bytes(self->buffer, self->h_size);
}

/******************************************************************************/
// Micro Python bindings

/// \classmethod \constructor([data])
/// initial data must be given if block_size wants to be passed
STATIC mp_obj_t hash_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 0, 1, false);


    if (hash_busy == true) {
        mp_raise_msg(&mp_type_OSError, "only one active hash operation is permitted at a time");
    }

    hash_busy = true;

    mp_obj_hash_t *self = m_new_obj(mp_obj_hash_t);

    memset(self, 0, sizeof(mp_obj_hash_t));

    self->digested = false;
    self->base.type = type;

    switch (self->base.type->name) {
    case MP_QSTR_sha1:
        self->h_size = 20;
        self->b_size = 64;
        mbedtls_sha1_init(&self->u.sha1_context);
        mbedtls_sha1_starts_ret(&self->u.sha1_context);
        break;

    case MP_QSTR_sha224:
        self->h_size = 28;
        self->b_size = 64;
        mbedtls_sha256_init(&self->u.sha256_context);
        mbedtls_sha256_starts_ret(&self->u.sha256_context, 1);
        break;

    case MP_QSTR_sha256:
        self->h_size = 32;
        self->b_size = 64;
        mbedtls_sha256_init(&self->u.sha256_context);
        mbedtls_sha256_starts_ret(&self->u.sha256_context, 0);
        break;

    case MP_QSTR_sha384:
        self->h_size = 48;
        self->b_size = 128;
        mbedtls_sha512_init(&self->u.sha512_context);
        mbedtls_sha512_starts_ret(&self->u.sha512_context, 1);
        break;

    case MP_QSTR_sha512:
        self->h_size = 64;
        self->b_size = 128;
        mbedtls_sha512_init(&self->u.sha512_context);
        mbedtls_sha512_starts_ret(&self->u.sha512_context, 0);
        break;

    case MP_QSTR_md5:
        self->h_size = 16;
        self->b_size = 64;
        MD5Init(&self->u.md5_context);
        break;
    }

    if (n_args) {
        hash_update_internal(self, args[0], false);
    }

    return self;
}

STATIC mp_obj_t hash_update(mp_obj_t self_in, mp_obj_t arg) {
    mp_obj_hash_t *self = self_in;
    if (self->digested == false) {
        hash_update_internal(self, arg, false);
    }
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

STATIC const mp_obj_type_t sha1_type = {
   { &mp_type_type },
   .name = MP_QSTR_sha1,
   .make_new = hash_make_new,
   .locals_dict = (mp_obj_t)&hash_locals_dict,
};

STATIC const mp_obj_type_t sha224_type = {
   { &mp_type_type },
   .name = MP_QSTR_sha224,
   .make_new = hash_make_new,
   .locals_dict = (mp_obj_t)&hash_locals_dict,
};

STATIC const mp_obj_type_t sha256_type = {
   { &mp_type_type },
   .name = MP_QSTR_sha256,
   .make_new = hash_make_new,
   .locals_dict = (mp_obj_t)&hash_locals_dict,
};

STATIC const mp_obj_type_t sha384_type = {
   { &mp_type_type },
   .name = MP_QSTR_sha384,
   .make_new = hash_make_new,
   .locals_dict = (mp_obj_t)&hash_locals_dict,
};

STATIC const mp_obj_type_t sha512_type = {
   { &mp_type_type },
   .name = MP_QSTR_sha512,
   .make_new = hash_make_new,
   .locals_dict = (mp_obj_t)&hash_locals_dict,
};

STATIC const mp_map_elem_t mp_module_hashlib_globals_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR___name__),    MP_OBJ_NEW_QSTR(MP_QSTR_uhashlib) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_md5),         (mp_obj_t)&md5_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_sha1),        (mp_obj_t)&sha1_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_sha224),      (mp_obj_t)&sha224_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_sha256),      (mp_obj_t)&sha256_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_sha384),      (mp_obj_t)&sha384_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_sha512),      (mp_obj_t)&sha512_type },
};

STATIC MP_DEFINE_CONST_DICT(mp_module_hashlib_globals, mp_module_hashlib_globals_table);

const mp_obj_module_t mp_module_uhashlib = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&mp_module_hashlib_globals,
};

