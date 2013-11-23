/**
 * \file cipher.c
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

#include "polarssl/cipher.h"
#include "polarssl/cipher_wrap.h"

#include <stdlib.h>

#if defined _MSC_VER && !defined strcasecmp
#define strcasecmp _stricmp
#endif

static const int supported_ciphers[] = {

#if defined(POLARSSL_AES_C)
        POLARSSL_CIPHER_AES_128_CBC,
        POLARSSL_CIPHER_AES_192_CBC,
        POLARSSL_CIPHER_AES_256_CBC,

#if defined(POLARSSL_CIPHER_MODE_CFB)
        POLARSSL_CIPHER_AES_128_CFB128,
        POLARSSL_CIPHER_AES_192_CFB128,
        POLARSSL_CIPHER_AES_256_CFB128,
#endif /* defined(POLARSSL_CIPHER_MODE_CFB) */

#if defined(POLARSSL_CIPHER_MODE_CTR)
        POLARSSL_CIPHER_AES_128_CTR,
        POLARSSL_CIPHER_AES_192_CTR,
        POLARSSL_CIPHER_AES_256_CTR,
#endif /* defined(POLARSSL_CIPHER_MODE_CTR) */

#endif /* defined(POLARSSL_AES_C) */

#if defined(POLARSSL_CAMELLIA_C)
        POLARSSL_CIPHER_CAMELLIA_128_CBC,
        POLARSSL_CIPHER_CAMELLIA_192_CBC,
        POLARSSL_CIPHER_CAMELLIA_256_CBC,

#if defined(POLARSSL_CIPHER_MODE_CFB)
        POLARSSL_CIPHER_CAMELLIA_128_CFB128,
        POLARSSL_CIPHER_CAMELLIA_192_CFB128,
        POLARSSL_CIPHER_CAMELLIA_256_CFB128,
#endif /* defined(POLARSSL_CIPHER_MODE_CFB) */

#if defined(POLARSSL_CIPHER_MODE_CTR)
        POLARSSL_CIPHER_CAMELLIA_128_CTR,
        POLARSSL_CIPHER_CAMELLIA_192_CTR,
        POLARSSL_CIPHER_CAMELLIA_256_CTR,
#endif /* defined(POLARSSL_CIPHER_MODE_CTR) */

#endif /* defined(POLARSSL_CAMELLIA_C) */

#if defined(POLARSSL_DES_C)
        POLARSSL_CIPHER_DES_CBC,
        POLARSSL_CIPHER_DES_EDE_CBC,
        POLARSSL_CIPHER_DES_EDE3_CBC,
#endif /* defined(POLARSSL_DES_C) */

#if defined(POLARSSL_BLOWFISH_C)
        POLARSSL_CIPHER_BLOWFISH_CBC,

#if defined(POLARSSL_CIPHER_MODE_CFB)
        POLARSSL_CIPHER_BLOWFISH_CFB64,
#endif /* defined(POLARSSL_CIPHER_MODE_CFB) */

#if defined(POLARSSL_CIPHER_MODE_CTR)
        POLARSSL_CIPHER_BLOWFISH_CTR,
#endif /* defined(POLARSSL_CIPHER_MODE_CTR) */

#endif /* defined(POLARSSL_BLOWFISH_C) */

#if defined(POLARSSL_CIPHER_NULL_CIPHER)
        POLARSSL_CIPHER_NULL,
#endif /* defined(POLARSSL_CIPHER_NULL_CIPHER) */

        0
};

const int *cipher_list( void )
{
    return supported_ciphers;
}

const cipher_info_t *cipher_info_from_type( const cipher_type_t cipher_type )
{
    /* Find static cipher information */
    switch ( cipher_type )
    {
#if defined(POLARSSL_AES_C)
        case POLARSSL_CIPHER_AES_128_CBC:
            return &aes_128_cbc_info;
        case POLARSSL_CIPHER_AES_192_CBC:
            return &aes_192_cbc_info;
        case POLARSSL_CIPHER_AES_256_CBC:
            return &aes_256_cbc_info;

#if defined(POLARSSL_CIPHER_MODE_CFB)
        case POLARSSL_CIPHER_AES_128_CFB128:
            return &aes_128_cfb128_info;
        case POLARSSL_CIPHER_AES_192_CFB128:
            return &aes_192_cfb128_info;
        case POLARSSL_CIPHER_AES_256_CFB128:
            return &aes_256_cfb128_info;
#endif /* defined(POLARSSL_CIPHER_MODE_CFB) */

#if defined(POLARSSL_CIPHER_MODE_CTR)
        case POLARSSL_CIPHER_AES_128_CTR:
            return &aes_128_ctr_info;
        case POLARSSL_CIPHER_AES_192_CTR:
            return &aes_192_ctr_info;
        case POLARSSL_CIPHER_AES_256_CTR:
            return &aes_256_ctr_info;
#endif /* defined(POLARSSL_CIPHER_MODE_CTR) */

#endif

#if defined(POLARSSL_CAMELLIA_C)
        case POLARSSL_CIPHER_CAMELLIA_128_CBC:
            return &camellia_128_cbc_info;
        case POLARSSL_CIPHER_CAMELLIA_192_CBC:
            return &camellia_192_cbc_info;
        case POLARSSL_CIPHER_CAMELLIA_256_CBC:
            return &camellia_256_cbc_info;

#if defined(POLARSSL_CIPHER_MODE_CFB)
        case POLARSSL_CIPHER_CAMELLIA_128_CFB128:
            return &camellia_128_cfb128_info;
        case POLARSSL_CIPHER_CAMELLIA_192_CFB128:
            return &camellia_192_cfb128_info;
        case POLARSSL_CIPHER_CAMELLIA_256_CFB128:
            return &camellia_256_cfb128_info;
#endif /* defined(POLARSSL_CIPHER_MODE_CFB) */

#if defined(POLARSSL_CIPHER_MODE_CTR)
        case POLARSSL_CIPHER_CAMELLIA_128_CTR:
            return &camellia_128_ctr_info;
        case POLARSSL_CIPHER_CAMELLIA_192_CTR:
            return &camellia_192_ctr_info;
        case POLARSSL_CIPHER_CAMELLIA_256_CTR:
            return &camellia_256_ctr_info;
#endif /* defined(POLARSSL_CIPHER_MODE_CTR) */

#endif

#if defined(POLARSSL_DES_C)
        case POLARSSL_CIPHER_DES_CBC:
            return &des_cbc_info;
        case POLARSSL_CIPHER_DES_EDE_CBC:
            return &des_ede_cbc_info;
        case POLARSSL_CIPHER_DES_EDE3_CBC:
            return &des_ede3_cbc_info;
#endif

#if defined(POLARSSL_BLOWFISH_C)
        case POLARSSL_CIPHER_BLOWFISH_CBC:
            return &blowfish_cbc_info;

#if defined(POLARSSL_CIPHER_MODE_CFB)
        case POLARSSL_CIPHER_BLOWFISH_CFB64:
            return &blowfish_cfb64_info;
#endif /* defined(POLARSSL_CIPHER_MODE_CFB) */

#if defined(POLARSSL_CIPHER_MODE_CTR)
        case POLARSSL_CIPHER_BLOWFISH_CTR:
            return &blowfish_ctr_info;
#endif /* defined(POLARSSL_CIPHER_MODE_CTR) */

#endif

#if defined(POLARSSL_CIPHER_NULL_CIPHER)
        case POLARSSL_CIPHER_NULL:
            return &null_cipher_info;
#endif /* defined(POLARSSL_CIPHER_NULL_CIPHER) */

        default:
            return NULL;
    }
}

const cipher_info_t *cipher_info_from_string( const char *cipher_name )
{
    if( NULL == cipher_name )
        return NULL;

    /* Get the appropriate cipher information */
#if defined(POLARSSL_CAMELLIA_C)
    if( !strcasecmp( "CAMELLIA-128-CBC", cipher_name ) )
        return cipher_info_from_type( POLARSSL_CIPHER_CAMELLIA_128_CBC );
    if( !strcasecmp( "CAMELLIA-192-CBC", cipher_name ) )
        return cipher_info_from_type( POLARSSL_CIPHER_CAMELLIA_192_CBC );
    if( !strcasecmp( "CAMELLIA-256-CBC", cipher_name ) )
        return cipher_info_from_type( POLARSSL_CIPHER_CAMELLIA_256_CBC );

#if defined(POLARSSL_CIPHER_MODE_CFB)
    if( !strcasecmp( "CAMELLIA-128-CFB128", cipher_name ) )
        return cipher_info_from_type( POLARSSL_CIPHER_CAMELLIA_128_CFB128 );
    if( !strcasecmp( "CAMELLIA-192-CFB128", cipher_name ) )
        return cipher_info_from_type( POLARSSL_CIPHER_CAMELLIA_192_CFB128 );
    if( !strcasecmp( "CAMELLIA-256-CFB128", cipher_name ) )
        return cipher_info_from_type( POLARSSL_CIPHER_CAMELLIA_256_CFB128 );
#endif /* defined(POLARSSL_CIPHER_MODE_CFB) */

#if defined(POLARSSL_CIPHER_MODE_CTR)
    if( !strcasecmp( "CAMELLIA-128-CTR", cipher_name ) )
        return cipher_info_from_type( POLARSSL_CIPHER_CAMELLIA_128_CTR );
    if( !strcasecmp( "CAMELLIA-192-CTR", cipher_name ) )
        return cipher_info_from_type( POLARSSL_CIPHER_CAMELLIA_192_CTR );
    if( !strcasecmp( "CAMELLIA-256-CTR", cipher_name ) )
        return cipher_info_from_type( POLARSSL_CIPHER_CAMELLIA_256_CTR );
#endif /* defined(POLARSSL_CIPHER_MODE_CTR) */
#endif

#if defined(POLARSSL_AES_C)
    if( !strcasecmp( "AES-128-CBC", cipher_name ) )
        return cipher_info_from_type( POLARSSL_CIPHER_AES_128_CBC );
    if( !strcasecmp( "AES-192-CBC", cipher_name ) )
        return cipher_info_from_type( POLARSSL_CIPHER_AES_192_CBC );
    if( !strcasecmp( "AES-256-CBC", cipher_name ) )
        return cipher_info_from_type( POLARSSL_CIPHER_AES_256_CBC );

#if defined(POLARSSL_CIPHER_MODE_CFB)
    if( !strcasecmp( "AES-128-CFB128", cipher_name ) )
        return cipher_info_from_type( POLARSSL_CIPHER_AES_128_CFB128 );
    if( !strcasecmp( "AES-192-CFB128", cipher_name ) )
        return cipher_info_from_type( POLARSSL_CIPHER_AES_192_CFB128 );
    if( !strcasecmp( "AES-256-CFB128", cipher_name ) )
        return cipher_info_from_type( POLARSSL_CIPHER_AES_256_CFB128 );
#endif /* defined(POLARSSL_CIPHER_MODE_CFB) */

#if defined(POLARSSL_CIPHER_MODE_CTR)
    if( !strcasecmp( "AES-128-CTR", cipher_name ) )
        return cipher_info_from_type( POLARSSL_CIPHER_AES_128_CTR );
    if( !strcasecmp( "AES-192-CTR", cipher_name ) )
        return cipher_info_from_type( POLARSSL_CIPHER_AES_192_CTR );
    if( !strcasecmp( "AES-256-CTR", cipher_name ) )
        return cipher_info_from_type( POLARSSL_CIPHER_AES_256_CTR );
#endif /* defined(POLARSSL_CIPHER_MODE_CTR) */
#endif

#if defined(POLARSSL_DES_C)
    if( !strcasecmp( "DES-CBC", cipher_name ) )
        return cipher_info_from_type( POLARSSL_CIPHER_DES_CBC );
    if( !strcasecmp( "DES-EDE-CBC", cipher_name ) )
        return cipher_info_from_type( POLARSSL_CIPHER_DES_EDE_CBC );
    if( !strcasecmp( "DES-EDE3-CBC", cipher_name ) )
        return cipher_info_from_type( POLARSSL_CIPHER_DES_EDE3_CBC );
#endif

#if defined(POLARSSL_BLOWFISH_C)
    if( !strcasecmp( "BLOWFISH-CBC", cipher_name ) )
        return cipher_info_from_type( POLARSSL_CIPHER_BLOWFISH_CBC );

#if defined(POLARSSL_CIPHER_MODE_CFB)
    if( !strcasecmp( "BLOWFISH-CFB64", cipher_name ) )
        return cipher_info_from_type( POLARSSL_CIPHER_BLOWFISH_CFB64 );
#endif /* defined(POLARSSL_CIPHER_MODE_CFB) */

#if defined(POLARSSL_CIPHER_MODE_CTR)
    if( !strcasecmp( "BLOWFISH-CTR", cipher_name ) )
        return cipher_info_from_type( POLARSSL_CIPHER_BLOWFISH_CTR );
#endif /* defined(POLARSSL_CIPHER_MODE_CTR) */
#endif

#if defined(POLARSSL_CIPHER_NULL_CIPHER)
    if( !strcasecmp( "NULL", cipher_name ) )
        return cipher_info_from_type( POLARSSL_CIPHER_NULL );
#endif /* defined(POLARSSL_CIPHER_NULL_CIPHER) */

    return NULL;
}

int cipher_init_ctx( cipher_context_t *ctx, const cipher_info_t *cipher_info )
{
    if( NULL == cipher_info || NULL == ctx )
        return POLARSSL_ERR_CIPHER_BAD_INPUT_DATA;

    memset( ctx, 0, sizeof( cipher_context_t ) );

    if( NULL == ( ctx->cipher_ctx = cipher_info->base->ctx_alloc_func() ) )
        return POLARSSL_ERR_CIPHER_ALLOC_FAILED;

    ctx->cipher_info = cipher_info;

    return 0;
}

int cipher_free_ctx( cipher_context_t *ctx )
{
    if( ctx == NULL || ctx->cipher_info == NULL )
        return POLARSSL_ERR_CIPHER_BAD_INPUT_DATA;

    ctx->cipher_info->base->ctx_free_func( ctx->cipher_ctx );

    return 0;
}

int cipher_setkey( cipher_context_t *ctx, const unsigned char *key,
        int key_length, const operation_t operation )
{
    if( NULL == ctx || NULL == ctx->cipher_info )
        return POLARSSL_ERR_CIPHER_BAD_INPUT_DATA;

    ctx->key_length = key_length;
    ctx->operation = operation;

#if defined(POLARSSL_CIPHER_NULL_CIPHER)
    if( ctx->cipher_info->mode == POLARSSL_MODE_NULL )
        return 0;
#endif /* defined(POLARSSL_CIPHER_NULL_CIPHER) */

    /*
     * For CFB and CTR mode always use the encryption key schedule
     */
    if( POLARSSL_ENCRYPT == operation ||
        POLARSSL_MODE_CFB == ctx->cipher_info->mode ||
        POLARSSL_MODE_CTR == ctx->cipher_info->mode )
    {
        return ctx->cipher_info->base->setkey_enc_func( ctx->cipher_ctx, key,
                ctx->key_length );
    }

    if( POLARSSL_DECRYPT == operation )
        return ctx->cipher_info->base->setkey_dec_func( ctx->cipher_ctx, key,
                ctx->key_length );

    return POLARSSL_ERR_CIPHER_BAD_INPUT_DATA;
}

int cipher_reset( cipher_context_t *ctx, const unsigned char *iv )
{
    if( NULL == ctx || NULL == ctx->cipher_info || NULL == iv )
        return POLARSSL_ERR_CIPHER_BAD_INPUT_DATA;

    ctx->unprocessed_len = 0;

    memcpy( ctx->iv, iv, cipher_get_iv_size( ctx ) );

    return 0;
}

int cipher_update( cipher_context_t *ctx, const unsigned char *input, size_t ilen,
        unsigned char *output, size_t *olen )
{
    int ret;
    size_t copy_len = 0;

    if( NULL == ctx || NULL == ctx->cipher_info || NULL == olen ||
        input == output )
    {
        return POLARSSL_ERR_CIPHER_BAD_INPUT_DATA;
    }

    *olen = 0;

#if defined(POLARSSL_CIPHER_NULL_CIPHER)
    if( ctx->cipher_info->mode == POLARSSL_MODE_NULL )
    {
        memcpy( output, input, ilen );
        *olen = ilen;
        return 0;
    }
#endif /* defined(POLARSSL_CIPHER_NULL_CIPHER) */

    if( ctx->cipher_info->mode == POLARSSL_MODE_CBC )
    {
        /*
         * If there is not enough data for a full block, cache it.
         */
        if( ( ctx->operation == POLARSSL_DECRYPT &&
                ilen + ctx->unprocessed_len <= cipher_get_block_size( ctx ) ) ||
             ( ctx->operation == POLARSSL_ENCRYPT &&
                ilen + ctx->unprocessed_len < cipher_get_block_size( ctx ) ) )
        {
            memcpy( &( ctx->unprocessed_data[ctx->unprocessed_len] ), input,
                    ilen );

            ctx->unprocessed_len += ilen;
            return 0;
        }

        /*
         * Process cached data first
         */
        if( ctx->unprocessed_len != 0 )
        {
            copy_len = cipher_get_block_size( ctx ) - ctx->unprocessed_len;

            memcpy( &( ctx->unprocessed_data[ctx->unprocessed_len] ), input,
                    copy_len );

            if( 0 != ( ret = ctx->cipher_info->base->cbc_func( ctx->cipher_ctx,
                    ctx->operation, cipher_get_block_size( ctx ), ctx->iv,
                    ctx->unprocessed_data, output ) ) )
            {
                return ret;
            }

            *olen += cipher_get_block_size( ctx );
            output += cipher_get_block_size( ctx );
            ctx->unprocessed_len = 0;

            input += copy_len;
            ilen -= copy_len;
        }

        /*
         * Cache final, incomplete block
         */
        if( 0 != ilen )
        {
            copy_len = ilen % cipher_get_block_size( ctx );
            if( copy_len == 0 && ctx->operation == POLARSSL_DECRYPT )
                copy_len = cipher_get_block_size(ctx);

            memcpy( ctx->unprocessed_data, &( input[ilen - copy_len] ),
                    copy_len );

            ctx->unprocessed_len += copy_len;
            ilen -= copy_len;
        }

        /*
         * Process remaining full blocks
         */
        if( ilen )
        {
            if( 0 != ( ret = ctx->cipher_info->base->cbc_func( ctx->cipher_ctx,
                    ctx->operation, ilen, ctx->iv, input, output ) ) )
            {
                return ret;
            }
            *olen += ilen;
        }

        return 0;
    }

    if( ctx->cipher_info->mode == POLARSSL_MODE_CFB )
    {
        if( 0 != ( ret = ctx->cipher_info->base->cfb_func( ctx->cipher_ctx,
                ctx->operation, ilen, &ctx->unprocessed_len, ctx->iv,
                input, output ) ) )
        {
            return ret;
        }

        *olen = ilen;

        return 0;
    }

    if( ctx->cipher_info->mode == POLARSSL_MODE_CTR )
    {
        if( 0 != ( ret = ctx->cipher_info->base->ctr_func( ctx->cipher_ctx,
                ilen, &ctx->unprocessed_len, ctx->iv,
                ctx->unprocessed_data, input, output ) ) )
        {
            return ret;
        }

        *olen = ilen;

        return 0;
    }

    return POLARSSL_ERR_CIPHER_FEATURE_UNAVAILABLE;
}

static void add_pkcs_padding( unsigned char *output, size_t output_len,
        size_t data_len )
{
    size_t padding_len = output_len - data_len;
    unsigned char i = 0;

    for( i = 0; i < padding_len; i++ )
        output[data_len + i] = (unsigned char) padding_len;
}

static int get_pkcs_padding( unsigned char *input, unsigned int input_len,
        size_t *data_len)
{
    unsigned int i, padding_len = 0;

    if( NULL == input || NULL == data_len )
        return POLARSSL_ERR_CIPHER_BAD_INPUT_DATA;

    padding_len = input[input_len - 1];

    if( padding_len > input_len )
        return POLARSSL_ERR_CIPHER_INVALID_PADDING;

    for( i = input_len - padding_len; i < input_len; i++ )
        if( input[i] != padding_len )
            return POLARSSL_ERR_CIPHER_INVALID_PADDING;

    *data_len = input_len - padding_len;

    return 0;
}

int cipher_finish( cipher_context_t *ctx, unsigned char *output, size_t *olen)
{
    int ret = 0;

    if( NULL == ctx || NULL == ctx->cipher_info || NULL == olen )
        return POLARSSL_ERR_CIPHER_BAD_INPUT_DATA;

    *olen = 0;

    if( POLARSSL_MODE_CFB == ctx->cipher_info->mode ||
        POLARSSL_MODE_CTR == ctx->cipher_info->mode ||
        POLARSSL_MODE_NULL == ctx->cipher_info->mode )
    {
        return 0;
    }

    if( POLARSSL_MODE_CBC == ctx->cipher_info->mode )
    {
        if( POLARSSL_ENCRYPT == ctx->operation )
        {
            add_pkcs_padding( ctx->unprocessed_data, cipher_get_iv_size( ctx ),
                    ctx->unprocessed_len );
        }
        else if ( cipher_get_block_size( ctx ) != ctx->unprocessed_len )
        {
            /* For decrypt operations, expect a full block */
            return POLARSSL_ERR_CIPHER_FULL_BLOCK_EXPECTED;
        }

        /* cipher block */
        if( 0 != ( ret = ctx->cipher_info->base->cbc_func( ctx->cipher_ctx,
                ctx->operation, cipher_get_block_size( ctx ), ctx->iv,
                ctx->unprocessed_data, output ) ) )
        {
            return ret;
        }

        /* Set output size for decryption */
        if( POLARSSL_DECRYPT == ctx->operation )
            return get_pkcs_padding( output, cipher_get_block_size( ctx ), olen );

        /* Set output size for encryption */
        *olen = cipher_get_block_size( ctx );
        return 0;
    }

    return POLARSSL_ERR_CIPHER_FEATURE_UNAVAILABLE;
}

#if defined(POLARSSL_SELF_TEST)

#include <stdio.h>

#define ASSERT(x) if (!(x)) { \
        printf( "failed with %i at %s\n", value, (#x) ); \
    return( 1 ); \
}
/*
 * Checkup routine
 */

int cipher_self_test( int verbose )
{
    ((void) verbose);

    return( 0 );
}

#endif

#endif
