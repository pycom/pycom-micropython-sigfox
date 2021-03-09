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

#include "py/mpconfig.h"
#include "py/nlr.h"
#include "py/runtime.h"
#include "esp_system.h"
#include "hwcrypto/aes.h"
#include "hwcrypto/sha.h"
#include "mpexception.h"
#include "mbedtls/entropy.h"
#include "mbedtls/pk.h"
#include "mbedtls/ctr_drbg.h"

/******************************************************************************
 DEFINE CONSTANTS
 ******************************************************************************/

/******************************************************************************
 DEFINE PRIVATE TYPES
 ******************************************************************************/
typedef enum {
    CRYPT_MODE_ECB = 1,
    CRYPT_MODE_CBC = 2,
    CRYPT_MODE_CFB = 3,
    CRYPT_MODE_CTR = 6,
} crypt_mode_t;

typedef enum {
    CRYPT_SEGMENT_8 = 8,
    CRYPT_SEGMENT_128 = 128,
} crypt_segment_size_t;

typedef struct _mp_obj_AES_t mp_obj_AES_t;

typedef mp_obj_t(*crypt_func_t)(mp_obj_AES_t *, uint32_t, const unsigned char *, uint32_t);

typedef struct _mp_obj_AES_t {
    mp_obj_base_t base;
    esp_aes_context ctx;
    crypt_segment_size_t segment_size;
    union {
        uint8_t IV[16];
        uint8_t counter[16]; // used only in CTR
    }u;
    uint8_t stream[16]; // used only in CTR
    uint32_t offset;
    crypt_func_t crypt_func;
} mp_obj_AES_t;

/******************************************************************************
 DECLARE PRIVATE FUNCTIONS
 ******************************************************************************/
STATIC mp_obj_t aes_do_ecb(mp_obj_AES_t *self, uint32_t operation, const unsigned char *input, uint32_t len);
STATIC mp_obj_t aes_do_cbc(mp_obj_AES_t *self, uint32_t operation, const unsigned char *input, uint32_t len);
STATIC mp_obj_t aes_do_cfb(mp_obj_AES_t *self, uint32_t operation, const unsigned char *input, uint32_t len);
STATIC mp_obj_t aes_do_ctr(mp_obj_AES_t *self, uint32_t operation, const unsigned char *input, uint32_t len);

/******************************************************************************
 DEFINE PRIVATE FUNCTIONS
 ******************************************************************************/

STATIC mp_obj_t aes_do_ecb(mp_obj_AES_t *self, uint32_t operation, const unsigned char *input, uint32_t len) {
    int i;
    uint8_t *output = m_new(uint8_t, len);
    uint8_t *tmp = output;

    if ((len % 16) != 0) {
        mp_raise_ValueError("Input strings must be a multiple of 16 in length");
    }

    for (i = len / 16; i > 0; i--) {
        esp_aes_crypt_ecb(&self->ctx, operation, input, tmp);
        input += 16;
        tmp += 16;
    }

    return mp_obj_new_bytes(output, len);
}

STATIC mp_obj_t aes_do_cbc(mp_obj_AES_t *self, uint32_t operation, const unsigned char *input, uint32_t len) {
    int result;
    uint8_t *output = m_new(uint8_t, len);

    result = esp_aes_crypt_cbc(&self->ctx, operation, len, self->u.IV, input, output);

    if (result == ERR_ESP_AES_INVALID_INPUT_LENGTH) {
        mp_raise_ValueError("Input strings must be a multiple of 16 in length");
    }

    return mp_obj_new_bytes(output, len);
}

STATIC mp_obj_t aes_do_cfb(mp_obj_AES_t *self, uint32_t operation, const unsigned char *input, uint32_t len) {
    int result;
    uint8_t *output = m_new(uint8_t, len);

    if (self->segment_size == CRYPT_SEGMENT_128) {
        result = esp_aes_crypt_cfb128(&self->ctx, operation, len, &self->offset, self->u.IV, input, output);
    } else {
        result = esp_aes_crypt_cfb8(&self->ctx, operation, len, self->u.IV, input, output);
    }

    if (result == ERR_ESP_AES_INVALID_INPUT_LENGTH) {
        mp_raise_ValueError("Input strings must be a multiple of 16 in length");
    }

    return mp_obj_new_bytes(output, len);
}

STATIC mp_obj_t aes_do_ctr(mp_obj_AES_t *self, uint32_t operation, const unsigned char *input, uint32_t len) {
    uint8_t *output = m_new(uint8_t, len);

    esp_aes_crypt_ctr(&self->ctx, len, &self->offset, self->u.counter, self->stream, input, output);

    return mp_obj_new_bytes(output, len);
}

STATIC mp_obj_t AES_decrypt(mp_obj_t self_in, mp_obj_t ciphertext) {
    mp_obj_AES_t *self = self_in;
    mp_buffer_info_t bufinfo;

    mp_get_buffer_raise(ciphertext, &bufinfo, MP_BUFFER_READ);
    return self->crypt_func(self, ESP_AES_DECRYPT, bufinfo.buf, bufinfo.len);
}
MP_DEFINE_CONST_FUN_OBJ_2(AES_decrypt_obj, AES_decrypt);

STATIC mp_obj_t AES_encrypt(mp_obj_t self_in, mp_obj_t plaintext) {
    mp_obj_AES_t *self = self_in;
    mp_buffer_info_t bufinfo;

    mp_get_buffer_raise(plaintext, &bufinfo, MP_BUFFER_READ);
    return self->crypt_func(self, ESP_AES_ENCRYPT, bufinfo.buf, bufinfo.len);
}
MP_DEFINE_CONST_FUN_OBJ_2(AES_encrypt_obj, AES_encrypt);

STATIC const mp_map_elem_t AES_locals_dict_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR_decrypt),    (mp_obj_t) &AES_decrypt_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_encrypt),    (mp_obj_t) &AES_encrypt_obj },
};

STATIC MP_DEFINE_CONST_DICT(AES_locals_dict, AES_locals_dict_table);

STATIC const mp_obj_type_t AESCipher_type = {
    { &mp_type_type },
    .name = MP_QSTR_AESCipher,
    // .make_new = AES_make_new,
    .locals_dict = (mp_obj_t)&AES_locals_dict,
};

/******************************************************************************/
// Micro Python bindings

/// \classmethod \constructor([data])
/// initial data must be given if block_size wants to be passed

STATIC mp_obj_t AES_make_new(const mp_obj_type_t *type, mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *all_args) {

    STATIC const mp_arg_t allowed_args[] = {
        { MP_QSTR_key,          MP_ARG_REQUIRED | MP_ARG_OBJ,   },
        { MP_QSTR_mode,         MP_ARG_INT,                     {.u_int = CRYPT_MODE_ECB} },
        { MP_QSTR_IV,           MP_ARG_OBJ,                     {.u_obj = mp_const_none} },
        { MP_QSTR_counter,      MP_ARG_OBJ,                     {.u_obj = mp_const_none} },
        { MP_QSTR_segment_size, MP_ARG_INT,                     {.u_int = -1} },
    };

    // parse arguments
    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, all_args + n_args);
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, all_args, &kw_args, MP_ARRAY_SIZE(args), allowed_args, args);

    // and store them
    mp_obj_AES_t *self = m_new_obj(mp_obj_AES_t);
    mp_buffer_info_t bufinfo;

    self->base.type = &AESCipher_type;
    self->offset = 0;

    // store the key
    mp_get_buffer_raise(args[0].u_obj, &bufinfo, MP_BUFFER_READ);

    if (bufinfo.len != 16 && bufinfo.len != 24 && bufinfo.len != 32) {
        mp_raise_ValueError("AES key must be either 16, 24, or 32 bytes long");
    }

    esp_aes_setkey(&self->ctx, bufinfo.buf, bufinfo.len * 8);

    // store the mode
    crypt_mode_t mode;
    mode = args[1].u_int;

    switch (mode) {
    case CRYPT_MODE_ECB:
        self->crypt_func = &aes_do_ecb;
        break;

    case CRYPT_MODE_CBC:
        self->crypt_func = &aes_do_cbc;
        break;

    case CRYPT_MODE_CFB:
        self->crypt_func = &aes_do_cfb;
        break;

    case CRYPT_MODE_CTR:
        self->crypt_func = &aes_do_ctr;
        break;

    default:
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError,
            "Unknown cipher feedback mode %d", mode));
        break;
    }

    // store the IV (ignored in ECB & CTR)
    if (mode != CRYPT_MODE_ECB &&
        mode != CRYPT_MODE_CTR &&
        args[2].u_obj != mp_const_none
    ) {
        mp_get_buffer_raise(args[2].u_obj, &bufinfo, MP_BUFFER_READ);
        if(bufinfo.len == sizeof(self->u.IV)) {
            memcpy(self->u.IV, bufinfo.buf, sizeof(self->u.IV));
        } else {
            mp_raise_ValueError("IV must be 16 bytes long");
        }
    }

    // store the counter (only valid in CTR mode)
    if (mode == CRYPT_MODE_CTR) {
        mp_get_buffer_raise(args[3].u_obj, &bufinfo, MP_BUFFER_READ);
        if (bufinfo.len != 16) {
            mp_raise_ValueError("Initial counter value must be 16 bytes long");
        }
        memcpy(self->u.counter, bufinfo.buf, sizeof(self->u.counter));
        memset(self->stream, 0, sizeof(self->stream));
    } else {
        if(args[3].u_obj != mp_const_none) {
            mp_raise_ValueError("'counter' parameter only useful with CTR mode");
        }
    }

    // store the segment sizes
    self->segment_size = (args[4].u_int == -1 ? CRYPT_SEGMENT_8 : args[4].u_int);
    if (mode == CRYPT_MODE_CFB &&
        self->segment_size != CRYPT_SEGMENT_8 &&
        self->segment_size != CRYPT_SEGMENT_128
    ) {
        mp_raise_ValueError("segment_size must be 8 or 128"); // for CFB
    }

    return self;
}

STATIC mp_obj_t getrandbits(mp_obj_t bits) {
    uint32_t num_cycles, i;
    vstr_t vstr;

    num_cycles = mp_obj_get_int(bits);
    num_cycles += 0x20 * ((num_cycles & 0x1F) != 0);  // round the bits to a multiple of 32
    num_cycles >>= 5;

    vstr_init_len(&vstr, num_cycles << 2); // going to get 32 bit integers (4 bytes)
    for (i = 0; i < num_cycles; i++) {
        *((uint32_t *) (vstr.buf + (i << 2))) = esp_random();
    }

    return mp_obj_new_str_from_vstr(&mp_type_bytes, &vstr);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(getrandbits_obj, getrandbits);

STATIC mp_obj_t mod_crypt_generate_rsa_signature(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    STATIC const mp_arg_t mod_pycom_generate_rsa_signature_args[] = {
        { MP_QSTR_message,                MP_ARG_OBJ | MP_ARG_REQUIRED, {} },
        { MP_QSTR_private_key,            MP_ARG_OBJ | MP_ARG_REQUIRED, {} },
        { MP_QSTR_pers,                   MP_ARG_OBJ | MP_ARG_KW_ONLY,  {.u_obj = MP_OBJ_NULL} }
    };

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(mod_pycom_generate_rsa_signature_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(mod_pycom_generate_rsa_signature_args), mod_pycom_generate_rsa_signature_args, args);

    const char* message = mp_obj_str_get_str(args[0].u_obj);
    const char* private_key = mp_obj_str_get_str(args[1].u_obj);

    char* pers="esp32-tls";
    if(args[2].u_obj != MP_OBJ_NULL) {
        pers = (char*)mp_obj_str_get_str(args[2].u_obj);
    }

    mbedtls_pk_context pk_context;
    mbedtls_pk_init(&pk_context);

    int rc = mbedtls_pk_parse_key(&pk_context, (const unsigned char*)private_key, strlen(private_key)+1, NULL, 0);
    if (rc != 0) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "Invalid Private Key, error code: %d", rc));
    }

    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);

    mbedtls_ctr_drbg_seed(
        &ctr_drbg,
        mbedtls_entropy_func,
        &entropy,
        (const unsigned char*)pers,
        strlen(pers));

    uint8_t digest[32];
    rc = mbedtls_md(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), (const unsigned char*)message, strlen(message), digest);
    if (rc != 0) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_RuntimeError, "Message Digest operation failed, error code: %d", rc));
    }

    unsigned char *signature = malloc(5000);
    size_t signature_length;

    rc = mbedtls_pk_sign(&pk_context, MBEDTLS_MD_SHA256, digest, sizeof(digest), signature, &signature_length, mbedtls_ctr_drbg_random, &ctr_drbg);
    if (rc != 0) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_RuntimeError, "Signing failed, error code: %d!", rc));
    }

    mp_obj_t ret_signature = mp_obj_new_bytes((const byte*)signature, signature_length);

    mbedtls_pk_free(&pk_context);
    free((char*)signature);

    return ret_signature;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mod_crypt_generate_rsa_signature_obj, 2, mod_crypt_generate_rsa_signature);

STATIC mp_obj_t mod_crypt_rsa_encrypt(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    STATIC const mp_arg_t mod_pycom_generate_rsa_signature_args[] = {
        { MP_QSTR_message,                  MP_ARG_OBJ | MP_ARG_REQUIRED, {} },
        { MP_QSTR_key,                      MP_ARG_OBJ | MP_ARG_REQUIRED, {} },
    };

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(mod_pycom_generate_rsa_signature_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(mod_pycom_generate_rsa_signature_args), mod_pycom_generate_rsa_signature_args, args);

    const char* public_key = mp_obj_str_get_str(args[1].u_obj);

    mp_buffer_info_t message;
    mp_get_buffer_raise(args[0].u_obj, &message, MP_BUFFER_READ);

    char* pers="esp32-tls";

    mbedtls_pk_context pk_context;
    mbedtls_pk_init(&pk_context);

    int32_t rc = 0;
    rc = mbedtls_pk_parse_public_key(&pk_context, (const unsigned char*)public_key, strlen(public_key)+1);

    if (rc != 0) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "Invalid public key, mbedtls error code: 0x%X", -rc));
    }

    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);

    mbedtls_ctr_drbg_seed(
        &ctr_drbg,
        mbedtls_entropy_func,
        &entropy,
        (const unsigned char*)pers,
        strlen(pers));

    size_t output_len = message.len + 256;
    unsigned char *output = malloc(output_len);
    size_t output_actual_length = 0;

    rc = mbedtls_pk_encrypt(&pk_context,
            (const unsigned char*)message.buf,
            message.len,
            output,
            &output_actual_length,
            output_len,
            mbedtls_ctr_drbg_random,
            &ctr_drbg);

    if (rc != 0) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_RuntimeError, "Encrypt failed, mbedtls error code: 0x%X!", -rc));
    }

    mp_obj_t ret_output = mp_obj_new_bytes((const byte*)output, output_actual_length);

    mbedtls_pk_free(&pk_context);
    free((char*)output);

    return ret_output;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mod_crypt_rsa_encrypt_obj, 2, mod_crypt_rsa_encrypt);

STATIC mp_obj_t mod_crypt_rsa_decrypt(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    STATIC const mp_arg_t mod_pycom_generate_rsa_signature_args[] = {
        { MP_QSTR_message,                  MP_ARG_OBJ | MP_ARG_REQUIRED, {} },
        { MP_QSTR_key,                      MP_ARG_OBJ | MP_ARG_REQUIRED, {} },
    };

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(mod_pycom_generate_rsa_signature_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(mod_pycom_generate_rsa_signature_args), mod_pycom_generate_rsa_signature_args, args);

    const char* private_key = mp_obj_str_get_str(args[1].u_obj);

    mp_buffer_info_t message;
    mp_get_buffer_raise(args[0].u_obj, &message, MP_BUFFER_READ);

    char* pers="esp32-tls";

    mbedtls_pk_context pk_context;
    mbedtls_pk_init(&pk_context);

    int32_t rc = 0;
    rc = mbedtls_pk_parse_key(&pk_context, (const unsigned char*)private_key, strlen(private_key)+1, NULL, 0);

    if (rc != 0) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "Invalid private key, mbedtls error code: 0x%X", -rc));
    }

    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);

    mbedtls_ctr_drbg_seed(
        &ctr_drbg,
        mbedtls_entropy_func,
        &entropy,
        (const unsigned char*)pers,
        strlen(pers));

    size_t output_len = message.len + 256;
    unsigned char *output = malloc(output_len);
    size_t output_actual_length = 0;

    rc = mbedtls_pk_decrypt(&pk_context,
            (const unsigned char*)message.buf,
            message.len,
            output,
            &output_actual_length,
            output_len,
            mbedtls_ctr_drbg_random,
            &ctr_drbg);

    if (rc != 0) {
        nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_RuntimeError, "Decrypt failed, mbedtls error code: 0x%X!", -rc));
    }

    mp_obj_t ret_output = mp_obj_new_bytes((const byte*)output, output_actual_length);

    mbedtls_pk_free(&pk_context);
    free((char*)output);

    return ret_output;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mod_crypt_rsa_decrypt_obj, 2, mod_crypt_rsa_decrypt);

STATIC const mp_map_elem_t mp_module_AES_dict_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR___name__),            MP_OBJ_NEW_QSTR(MP_QSTR_uAES) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_MODE_ECB),            MP_OBJ_NEW_SMALL_INT(CRYPT_MODE_ECB) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_MODE_CBC),            MP_OBJ_NEW_SMALL_INT(CRYPT_MODE_CBC) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_MODE_CFB),            MP_OBJ_NEW_SMALL_INT(CRYPT_MODE_CFB) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_MODE_CTR),            MP_OBJ_NEW_SMALL_INT(CRYPT_MODE_CTR) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_SEGMENT_8),           MP_OBJ_NEW_SMALL_INT(CRYPT_SEGMENT_8) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_SEGMENT_128),         MP_OBJ_NEW_SMALL_INT(CRYPT_SEGMENT_128) },
};

STATIC MP_DEFINE_CONST_DICT(mp_module_AES_dict, mp_module_AES_dict_table);

STATIC const mp_obj_type_t mod_crypt_aes = {
   { &mp_type_type },
   .name = MP_QSTR_AES,
   .make_new = AES_make_new,
   .locals_dict = (mp_obj_t)&mp_module_AES_dict,
};


STATIC const mp_map_elem_t module_ucrypto_globals_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR___name__),                        MP_OBJ_NEW_QSTR(MP_QSTR_ucrypto) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_AES),                             (mp_obj_t)&mod_crypt_aes },
    { MP_OBJ_NEW_QSTR(MP_QSTR_getrandbits),                     (mp_obj_t)&getrandbits_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_generate_rsa_signature),          (mp_obj_t)&mod_crypt_generate_rsa_signature_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_rsa_encrypt),                     (mp_obj_t)&mod_crypt_rsa_encrypt_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_rsa_decrypt),                     (mp_obj_t)&mod_crypt_rsa_decrypt_obj },
};

STATIC MP_DEFINE_CONST_DICT(module_ucrypto_globals, module_ucrypto_globals_table);

const mp_obj_module_t module_ucrypto = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&module_ucrypto_globals,
};
