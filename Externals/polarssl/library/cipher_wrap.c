/**
 * \file md_wrap.c
 * 
 * \brief Generic cipher wrapper for PolarSSL
 *
 * \author Adriaan de Jong <dejong@fox-it.com>
 *
 *  Copyright (C) 2006-2012, Brainspark B.V.
 *
 *  This file is part of PolarSSL (http://www.polarssl.org)
 *  Lead Maintainer: Paul Bakker <polarssl_maintainer at polarssl.org>
 *
 *  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "polarssl/config.h"

#if defined(POLARSSL_CIPHER_C)

#include "polarssl/cipher_wrap.h"

#if defined(POLARSSL_AES_C)
#include "polarssl/aes.h"
#endif

#if defined(POLARSSL_CAMELLIA_C)
#include "polarssl/camellia.h"
#endif

#if defined(POLARSSL_DES_C)
#include "polarssl/des.h"
#endif

#if defined(POLARSSL_BLOWFISH_C)
#include "polarssl/blowfish.h"
#endif

#include <stdlib.h>

#if defined(POLARSSL_AES_C)

int aes_crypt_cbc_wrap( void *ctx, operation_t operation, size_t length,
        unsigned char *iv, const unsigned char *input, unsigned char *output )
{
    return aes_crypt_cbc( (aes_context *) ctx, operation, length, iv, input, output );
}

int aes_crypt_cfb128_wrap( void *ctx, operation_t operation, size_t length,
        size_t *iv_off, unsigned char *iv, const unsigned char *input, unsigned char *output )
{
#if defined(POLARSSL_CIPHER_MODE_CFB)
    return aes_crypt_cfb128( (aes_context *) ctx, operation, length, iv_off, iv, input, output );
#else
    ((void) ctx);
    ((void) operation);
    ((void) length);
    ((void) iv_off);
    ((void) iv);
    ((void) input);
    ((void) output);

    return POLARSSL_ERR_CIPHER_FEATURE_UNAVAILABLE;
#endif
}

int aes_crypt_ctr_wrap( void *ctx, size_t length,
        size_t *nc_off, unsigned char *nonce_counter, unsigned char *stream_block,
        const unsigned char *input, unsigned char *output )
{
#if defined(POLARSSL_CIPHER_MODE_CTR)
    return aes_crypt_ctr( (aes_context *) ctx, length, nc_off, nonce_counter,
                          stream_block, input, output );
#else
    ((void) ctx);
    ((void) length);
    ((void) nc_off);
    ((void) nonce_counter);
    ((void) stream_block);
    ((void) input);
    ((void) output);

    return POLARSSL_ERR_CIPHER_FEATURE_UNAVAILABLE;
#endif
}

int aes_setkey_dec_wrap( void *ctx, const unsigned char *key, unsigned int key_length )
{
    return aes_setkey_dec( (aes_context *) ctx, key, key_length );
}

int aes_setkey_enc_wrap( void *ctx, const unsigned char *key, unsigned int key_length )
{
    return aes_setkey_enc( (aes_context *) ctx, key, key_length );
}

static void * aes_ctx_alloc( void )
{
    return malloc( sizeof( aes_context ) );
}

static void aes_ctx_free( void *ctx )
{
    free( ctx );
}

const cipher_base_t aes_info = {
    POLARSSL_CIPHER_ID_AES,
    aes_crypt_cbc_wrap,
    aes_crypt_cfb128_wrap,
    aes_crypt_ctr_wrap,
    aes_setkey_enc_wrap,
    aes_setkey_dec_wrap,
    aes_ctx_alloc,
    aes_ctx_free
};

const cipher_info_t aes_128_cbc_info = {
    POLARSSL_CIPHER_AES_128_CBC,
    POLARSSL_MODE_CBC,
    128,
    "AES-128-CBC",
    16,
    16,
    &aes_info
};

const cipher_info_t aes_192_cbc_info = {
    POLARSSL_CIPHER_AES_192_CBC,
    POLARSSL_MODE_CBC,
    192,
    "AES-192-CBC",
    16,
    16,
    &aes_info
};

const cipher_info_t aes_256_cbc_info = {
    POLARSSL_CIPHER_AES_256_CBC,
    POLARSSL_MODE_CBC,
    256,
    "AES-256-CBC",
    16,
    16,
    &aes_info
};

#if defined(POLARSSL_CIPHER_MODE_CFB)
const cipher_info_t aes_128_cfb128_info = {
    POLARSSL_CIPHER_AES_128_CFB128,
    POLARSSL_MODE_CFB,
    128,
    "AES-128-CFB128",
    16,
    16,
    &aes_info
};

const cipher_info_t aes_192_cfb128_info = {
    POLARSSL_CIPHER_AES_192_CFB128,
    POLARSSL_MODE_CFB,
    192,
    "AES-192-CFB128",
    16,
    16,
    &aes_info
};

const cipher_info_t aes_256_cfb128_info = {
    POLARSSL_CIPHER_AES_256_CFB128,
    POLARSSL_MODE_CFB,
    256,
    "AES-256-CFB128",
    16,
    16,
    &aes_info
};
#endif /* POLARSSL_CIPHER_MODE_CFB */

#if defined(POLARSSL_CIPHER_MODE_CTR)
const cipher_info_t aes_128_ctr_info = {
    POLARSSL_CIPHER_AES_128_CTR,
    POLARSSL_MODE_CTR,
    128,
    "AES-128-CTR",
    16,
    16,
    &aes_info
};

const cipher_info_t aes_192_ctr_info = {
    POLARSSL_CIPHER_AES_192_CTR,
    POLARSSL_MODE_CTR,
    192,
    "AES-192-CTR",
    16,
    16,
    &aes_info
};

const cipher_info_t aes_256_ctr_info = {
    POLARSSL_CIPHER_AES_256_CTR,
    POLARSSL_MODE_CTR,
    256,
    "AES-256-CTR",
    16,
    16,
    &aes_info
};
#endif /* POLARSSL_CIPHER_MODE_CTR */

#endif

#if defined(POLARSSL_CAMELLIA_C)

int camellia_crypt_cbc_wrap( void *ctx, operation_t operation, size_t length,
        unsigned char *iv, const unsigned char *input, unsigned char *output )
{
    return camellia_crypt_cbc( (camellia_context *) ctx, operation, length, iv, input, output );
}

int camellia_crypt_cfb128_wrap( void *ctx, operation_t operation, size_t length,
        size_t *iv_off, unsigned char *iv, const unsigned char *input, unsigned char *output )
{
#if defined(POLARSSL_CIPHER_MODE_CFB)
    return camellia_crypt_cfb128( (camellia_context *) ctx, operation, length, iv_off, iv, input, output );
#else
    ((void) ctx);
    ((void) operation);
    ((void) length);
    ((void) iv_off);
    ((void) iv);
    ((void) input);
    ((void) output);

    return POLARSSL_ERR_CIPHER_FEATURE_UNAVAILABLE;
#endif
}

int camellia_crypt_ctr_wrap( void *ctx, size_t length,
        size_t *nc_off, unsigned char *nonce_counter, unsigned char *stream_block,
        const unsigned char *input, unsigned char *output )
{
#if defined(POLARSSL_CIPHER_MODE_CTR)
    return camellia_crypt_ctr( (camellia_context *) ctx, length, nc_off, nonce_counter,
                          stream_block, input, output );
#else
    ((void) ctx);
    ((void) length);
    ((void) nc_off);
    ((void) nonce_counter);
    ((void) stream_block);
    ((void) input);
    ((void) output);

    return POLARSSL_ERR_CIPHER_FEATURE_UNAVAILABLE;
#endif
}

int camellia_setkey_dec_wrap( void *ctx, const unsigned char *key, unsigned int key_length )
{
    return camellia_setkey_dec( (camellia_context *) ctx, key, key_length );
}

int camellia_setkey_enc_wrap( void *ctx, const unsigned char *key, unsigned int key_length )
{
    return camellia_setkey_enc( (camellia_context *) ctx, key, key_length );
}

static void * camellia_ctx_alloc( void )
{
    return malloc( sizeof( camellia_context ) );
}

static void camellia_ctx_free( void *ctx )
{
    free( ctx );
}

const cipher_base_t camellia_info = {
    POLARSSL_CIPHER_ID_CAMELLIA,
    camellia_crypt_cbc_wrap,
    camellia_crypt_cfb128_wrap,
    camellia_crypt_ctr_wrap,
    camellia_setkey_enc_wrap,
    camellia_setkey_dec_wrap,
    camellia_ctx_alloc,
    camellia_ctx_free
};

const cipher_info_t camellia_128_cbc_info = {
    POLARSSL_CIPHER_CAMELLIA_128_CBC,
    POLARSSL_MODE_CBC,
    128,
    "CAMELLIA-128-CBC",
    16,
    16,
    &camellia_info
};

const cipher_info_t camellia_192_cbc_info = {
    POLARSSL_CIPHER_CAMELLIA_192_CBC,
    POLARSSL_MODE_CBC,
    192,
    "CAMELLIA-192-CBC",
    16,
    16,
    &camellia_info
};

const cipher_info_t camellia_256_cbc_info = {
    POLARSSL_CIPHER_CAMELLIA_256_CBC,
    POLARSSL_MODE_CBC,
    256,
    "CAMELLIA-256-CBC",
    16,
    16,
    &camellia_info
};

#if defined(POLARSSL_CIPHER_MODE_CFB)
const cipher_info_t camellia_128_cfb128_info = {
    POLARSSL_CIPHER_CAMELLIA_128_CFB128,
    POLARSSL_MODE_CFB,
    128,
    "CAMELLIA-128-CFB128",
    16,
    16,
    &camellia_info
};

const cipher_info_t camellia_192_cfb128_info = {
    POLARSSL_CIPHER_CAMELLIA_192_CFB128,
    POLARSSL_MODE_CFB,
    192,
    "CAMELLIA-192-CFB128",
    16,
    16,
    &camellia_info
};

const cipher_info_t camellia_256_cfb128_info = {
    POLARSSL_CIPHER_CAMELLIA_256_CFB128,
    POLARSSL_MODE_CFB,
    256,
    "CAMELLIA-256-CFB128",
    16,
    16,
    &camellia_info
};
#endif /* POLARSSL_CIPHER_MODE_CFB */

#if defined(POLARSSL_CIPHER_MODE_CTR)
const cipher_info_t camellia_128_ctr_info = {
    POLARSSL_CIPHER_CAMELLIA_128_CTR,
    POLARSSL_MODE_CTR,
    128,
    "CAMELLIA-128-CTR",
    16,
    16,
    &camellia_info
};

const cipher_info_t camellia_192_ctr_info = {
    POLARSSL_CIPHER_CAMELLIA_192_CTR,
    POLARSSL_MODE_CTR,
    192,
    "CAMELLIA-192-CTR",
    16,
    16,
    &camellia_info
};

const cipher_info_t camellia_256_ctr_info = {
    POLARSSL_CIPHER_CAMELLIA_256_CTR,
    POLARSSL_MODE_CTR,
    256,
    "CAMELLIA-256-CTR",
    16,
    16,
    &camellia_info
};
#endif /* POLARSSL_CIPHER_MODE_CTR */

#endif

#if defined(POLARSSL_DES_C)

int des_crypt_cbc_wrap( void *ctx, operation_t operation, size_t length,
        unsigned char *iv, const unsigned char *input, unsigned char *output )
{
    return des_crypt_cbc( (des_context *) ctx, operation, length, iv, input, output );
}

int des3_crypt_cbc_wrap( void *ctx, operation_t operation, size_t length,
        unsigned char *iv, const unsigned char *input, unsigned char *output )
{
    return des3_crypt_cbc( (des3_context *) ctx, operation, length, iv, input, output );
}

int des_crypt_cfb128_wrap( void *ctx, operation_t operation, size_t length,
        size_t *iv_off, unsigned char *iv, const unsigned char *input, unsigned char *output )
{
    ((void) ctx);
    ((void) operation);
    ((void) length);
    ((void) iv_off);
    ((void) iv);
    ((void) input);
    ((void) output);

    return POLARSSL_ERR_CIPHER_FEATURE_UNAVAILABLE;
}

int des_crypt_ctr_wrap( void *ctx, size_t length,
        size_t *nc_off, unsigned char *nonce_counter, unsigned char *stream_block,
        const unsigned char *input, unsigned char *output )
{
    ((void) ctx);
    ((void) length);
    ((void) nc_off);
    ((void) nonce_counter);
    ((void) stream_block);
    ((void) input);
    ((void) output);

    return POLARSSL_ERR_CIPHER_FEATURE_UNAVAILABLE;
}


int des_setkey_dec_wrap( void *ctx, const unsigned char *key, unsigned int key_length )
{
    ((void) key_length);

    return des_setkey_dec( (des_context *) ctx, key );
}

int des_setkey_enc_wrap( void *ctx, const unsigned char *key, unsigned int key_length )
{
    ((void) key_length);

    return des_setkey_enc( (des_context *) ctx, key );
}

int des3_set2key_dec_wrap( void *ctx, const unsigned char *key, unsigned int key_length )
{
    ((void) key_length);

    return des3_set2key_dec( (des3_context *) ctx, key );
}

int des3_set2key_enc_wrap( void *ctx, const unsigned char *key, unsigned int key_length )
{
    ((void) key_length);

    return des3_set2key_enc( (des3_context *) ctx, key );
}

int des3_set3key_dec_wrap( void *ctx, const unsigned char *key, unsigned int key_length )
{
    ((void) key_length);

    return des3_set3key_dec( (des3_context *) ctx, key );
}

int des3_set3key_enc_wrap( void *ctx, const unsigned char *key, unsigned int key_length )
{
    ((void) key_length);

    return des3_set3key_enc( (des3_context *) ctx, key );
}

static void * des_ctx_alloc( void )
{
    return malloc( sizeof( des_context ) );
}

static void * des3_ctx_alloc( void )
{
    return malloc( sizeof( des3_context ) );
}

static void des_ctx_free( void *ctx )
{
    free( ctx );
}

const cipher_base_t des_info = {
    POLARSSL_CIPHER_ID_DES,
    des_crypt_cbc_wrap,
    des_crypt_cfb128_wrap,
    des_crypt_ctr_wrap,
    des_setkey_enc_wrap,
    des_setkey_dec_wrap,
    des_ctx_alloc,
    des_ctx_free
};

const cipher_info_t des_cbc_info = {
    POLARSSL_CIPHER_DES_CBC,
    POLARSSL_MODE_CBC,
    POLARSSL_KEY_LENGTH_DES,
    "DES-CBC",
    8,
    8,
    &des_info
};

const cipher_base_t des_ede_info = {
    POLARSSL_CIPHER_ID_DES,
    des3_crypt_cbc_wrap,
    des_crypt_cfb128_wrap,
    des_crypt_ctr_wrap,
    des3_set2key_enc_wrap,
    des3_set2key_dec_wrap,
    des3_ctx_alloc,
    des_ctx_free
};

const cipher_info_t des_ede_cbc_info = {
    POLARSSL_CIPHER_DES_EDE_CBC,
    POLARSSL_MODE_CBC,
    POLARSSL_KEY_LENGTH_DES_EDE,
    "DES-EDE-CBC",
    8,
    8,
    &des_ede_info
};

const cipher_base_t des_ede3_info = {
    POLARSSL_CIPHER_ID_DES,
    des3_crypt_cbc_wrap,
    des_crypt_cfb128_wrap,
    des_crypt_ctr_wrap,
    des3_set3key_enc_wrap,
    des3_set3key_dec_wrap,
    des3_ctx_alloc,
    des_ctx_free
};

const cipher_info_t des_ede3_cbc_info = {
    POLARSSL_CIPHER_DES_EDE3_CBC,
    POLARSSL_MODE_CBC,
    POLARSSL_KEY_LENGTH_DES_EDE3,
    "DES-EDE3-CBC",
    8,
    8,
    &des_ede3_info
};
#endif

#if defined(POLARSSL_BLOWFISH_C)

int blowfish_crypt_cbc_wrap( void *ctx, operation_t operation, size_t length,
        unsigned char *iv, const unsigned char *input, unsigned char *output )
{
    return blowfish_crypt_cbc( (blowfish_context *) ctx, operation, length, iv, input, output );
}

int blowfish_crypt_cfb64_wrap( void *ctx, operation_t operation, size_t length,
        size_t *iv_off, unsigned char *iv, const unsigned char *input, unsigned char *output )
{
#if defined(POLARSSL_CIPHER_MODE_CFB)
    return blowfish_crypt_cfb64( (blowfish_context *) ctx, operation, length, iv_off, iv, input, output );
#else
    ((void) ctx);
    ((void) operation);
    ((void) length);
    ((void) iv_off);
    ((void) iv);
    ((void) input);
    ((void) output);

    return POLARSSL_ERR_CIPHER_FEATURE_UNAVAILABLE;
#endif
}

int blowfish_crypt_ctr_wrap( void *ctx, size_t length,
        size_t *nc_off, unsigned char *nonce_counter, unsigned char *stream_block,
        const unsigned char *input, unsigned char *output )
{
#if defined(POLARSSL_CIPHER_MODE_CTR)
    return blowfish_crypt_ctr( (blowfish_context *) ctx, length, nc_off, nonce_counter,
                          stream_block, input, output );
#else
    ((void) ctx);
    ((void) length);
    ((void) nc_off);
    ((void) nonce_counter);
    ((void) stream_block);
    ((void) input);
    ((void) output);

    return POLARSSL_ERR_CIPHER_FEATURE_UNAVAILABLE;
#endif
}

int blowfish_setkey_dec_wrap( void *ctx, const unsigned char *key, unsigned int key_length )
{
    return blowfish_setkey( (blowfish_context *) ctx, key, key_length );
}

int blowfish_setkey_enc_wrap( void *ctx, const unsigned char *key, unsigned int key_length )
{
    return blowfish_setkey( (blowfish_context *) ctx, key, key_length );
}

static void * blowfish_ctx_alloc( void )
{
    return malloc( sizeof( blowfish_context ) );
}

static void blowfish_ctx_free( void *ctx )
{
    free( ctx );
}

const cipher_base_t blowfish_info = {
    POLARSSL_CIPHER_ID_BLOWFISH,
    blowfish_crypt_cbc_wrap,
    blowfish_crypt_cfb64_wrap,
    blowfish_crypt_ctr_wrap,
    blowfish_setkey_enc_wrap,
    blowfish_setkey_dec_wrap,
    blowfish_ctx_alloc,
    blowfish_ctx_free
};

const cipher_info_t blowfish_cbc_info = {
    POLARSSL_CIPHER_BLOWFISH_CBC,
    POLARSSL_MODE_CBC,
    128,
    "BLOWFISH-CBC",
    8,
    8,
    &blowfish_info
};

#if defined(POLARSSL_CIPHER_MODE_CFB)
const cipher_info_t blowfish_cfb64_info = {
    POLARSSL_CIPHER_BLOWFISH_CFB64,
    POLARSSL_MODE_CFB,
    128,
    "BLOWFISH-CFB64",
    8,
    8,
    &blowfish_info
};
#endif /* POLARSSL_CIPHER_MODE_CFB */

#if defined(POLARSSL_CIPHER_MODE_CTR)
const cipher_info_t blowfish_ctr_info = {
    POLARSSL_CIPHER_BLOWFISH_CTR,
    POLARSSL_MODE_CTR,
    128,
    "BLOWFISH-CTR",
    8,
    8,
    &blowfish_info
};
#endif /* POLARSSL_CIPHER_MODE_CTR */
#endif /* POLARSSL_BLOWFISH_C */

#if defined(POLARSSL_CIPHER_NULL_CIPHER)
static void * null_ctx_alloc( void )
{
    return (void *) 1;
}


static void null_ctx_free( void *ctx )
{
    ((void) ctx);
}

const cipher_base_t null_base_info = {
    POLARSSL_CIPHER_ID_NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    null_ctx_alloc,
    null_ctx_free
};

const cipher_info_t null_cipher_info = {
    POLARSSL_CIPHER_NULL,
    POLARSSL_MODE_NULL,
    0,
    "NULL",
    1,
    1,
    &null_base_info
};
#endif /* defined(POLARSSL_CIPHER_NULL_CIPHER) */

#endif
