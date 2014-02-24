/*
 *  Public Key abstraction layer
 *
 *  Copyright (C) 2006-2013, Brainspark B.V.
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

#if defined(POLARSSL_PK_C)

#include "polarssl/pk.h"
#include "polarssl/pk_wrap.h"

#if defined(POLARSSL_RSA_C)
#include "polarssl/rsa.h"
#endif
#if defined(POLARSSL_ECP_C)
#include "polarssl/ecp.h"
#endif
#if defined(POLARSSL_ECDSA_C)
#include "polarssl/ecdsa.h"
#endif

/*
 * Initialise a pk_context
 */
void pk_init( pk_context *ctx )
{
    if( ctx == NULL )
        return;

    ctx->pk_info = NULL;
    ctx->pk_ctx = NULL;
}

/*
 * Free (the components of) a pk_context
 */
void pk_free( pk_context *ctx )
{
    if( ctx == NULL || ctx->pk_info == NULL)
        return;

    ctx->pk_info->ctx_free_func( ctx->pk_ctx );
    ctx->pk_ctx = NULL;

    ctx->pk_info = NULL;
}

/*
 * Get pk_info structure from type
 */
const pk_info_t * pk_info_from_type( pk_type_t pk_type )
{
    switch( pk_type ) {
#if defined(POLARSSL_RSA_C)
        case POLARSSL_PK_RSA:
            return &rsa_info;
#endif
#if defined(POLARSSL_ECP_C)
        case POLARSSL_PK_ECKEY:
            return &eckey_info;
        case POLARSSL_PK_ECKEY_DH:
            return &eckeydh_info;
#endif
#if defined(POLARSSL_ECDSA_C)
        case POLARSSL_PK_ECDSA:
            return &ecdsa_info;
#endif
        /* POLARSSL_PK_RSA_ALT ommited on purpose */
        default:
            return NULL;
    }
}

/*
 * Initialise context
 */
int pk_init_ctx( pk_context *ctx, const pk_info_t *info )
{
    if( ctx == NULL || info == NULL || ctx->pk_info != NULL )
        return( POLARSSL_ERR_PK_BAD_INPUT_DATA );

    if( ( ctx->pk_ctx = info->ctx_alloc_func() ) == NULL )
        return( POLARSSL_ERR_PK_MALLOC_FAILED );

    ctx->pk_info = info;

    return( 0 );
}

/*
 * Initialize an RSA-alt context
 */
int pk_init_ctx_rsa_alt( pk_context *ctx, void * key,
                         pk_rsa_alt_decrypt_func decrypt_func,
                         pk_rsa_alt_sign_func sign_func,
                         pk_rsa_alt_key_len_func key_len_func )
{
    rsa_alt_context *rsa_alt;
    const pk_info_t *info = &rsa_alt_info;

    if( ctx == NULL || ctx->pk_info != NULL )
        return( POLARSSL_ERR_PK_BAD_INPUT_DATA );

    if( ( ctx->pk_ctx = info->ctx_alloc_func() ) == NULL )
        return( POLARSSL_ERR_PK_MALLOC_FAILED );

    ctx->pk_info = info;

    rsa_alt = (rsa_alt_context *) ctx->pk_ctx;

    rsa_alt->key = key;
    rsa_alt->decrypt_func = decrypt_func;
    rsa_alt->sign_func = sign_func;
    rsa_alt->key_len_func = key_len_func;

    return( 0 );
}

/*
 * Tell if a PK can do the operations of the given type
 */
int pk_can_do( pk_context *ctx, pk_type_t type )
{
    /* null or NONE context can't do anything */
    if( ctx == NULL || ctx->pk_info == NULL )
        return( 0 );

    return( ctx->pk_info->can_do( type ) );
}

/*
 * Helper for pk_sign and pk_verify
 */
static inline int pk_hashlen_helper( md_type_t md_alg, size_t *hash_len )
{
    const md_info_t *md_info;

    if( *hash_len != 0 )
        return( 0 );

    if( ( md_info = md_info_from_type( md_alg ) ) == NULL )
        return( -1 );

    *hash_len = md_info->size;
    return( 0 );
}

/*
 * Verify a signature
 */
int pk_verify( pk_context *ctx, md_type_t md_alg,
               const unsigned char *hash, size_t hash_len,
               const unsigned char *sig, size_t sig_len )
{
    if( ctx == NULL || ctx->pk_info == NULL ||
        pk_hashlen_helper( md_alg, &hash_len ) != 0 )
        return( POLARSSL_ERR_PK_BAD_INPUT_DATA );

    if( ctx->pk_info->verify_func == NULL )
        return( POLARSSL_ERR_PK_TYPE_MISMATCH );

    return( ctx->pk_info->verify_func( ctx->pk_ctx, md_alg, hash, hash_len,
                                       sig, sig_len ) );
}

/*
 * Make a signature
 */
int pk_sign( pk_context *ctx, md_type_t md_alg,
             const unsigned char *hash, size_t hash_len,
             unsigned char *sig, size_t *sig_len,
             int (*f_rng)(void *, unsigned char *, size_t), void *p_rng )
{
    if( ctx == NULL || ctx->pk_info == NULL ||
        pk_hashlen_helper( md_alg, &hash_len ) != 0 )
        return( POLARSSL_ERR_PK_BAD_INPUT_DATA );

    if( ctx->pk_info->sign_func == NULL )
        return( POLARSSL_ERR_PK_TYPE_MISMATCH );

    return( ctx->pk_info->sign_func( ctx->pk_ctx, md_alg, hash, hash_len,
                                     sig, sig_len, f_rng, p_rng ) );
}

/*
 * Decrypt message
 */
int pk_decrypt( pk_context *ctx,
                const unsigned char *input, size_t ilen,
                unsigned char *output, size_t *olen, size_t osize,
                int (*f_rng)(void *, unsigned char *, size_t), void *p_rng )
{
    if( ctx == NULL || ctx->pk_info == NULL )
        return( POLARSSL_ERR_PK_BAD_INPUT_DATA );

    if( ctx->pk_info->decrypt_func == NULL )
        return( POLARSSL_ERR_PK_TYPE_MISMATCH );

    return( ctx->pk_info->decrypt_func( ctx->pk_ctx, input, ilen,
                output, olen, osize, f_rng, p_rng ) );
}

/*
 * Encrypt message
 */
int pk_encrypt( pk_context *ctx,
                const unsigned char *input, size_t ilen,
                unsigned char *output, size_t *olen, size_t osize,
                int (*f_rng)(void *, unsigned char *, size_t), void *p_rng )
{
    if( ctx == NULL || ctx->pk_info == NULL )
        return( POLARSSL_ERR_PK_BAD_INPUT_DATA );

    if( ctx->pk_info->encrypt_func == NULL )
        return( POLARSSL_ERR_PK_TYPE_MISMATCH );

    return( ctx->pk_info->encrypt_func( ctx->pk_ctx, input, ilen,
                output, olen, osize, f_rng, p_rng ) );
}

/*
 * Get key size in bits
 */
size_t pk_get_size( const pk_context *ctx )
{
    if( ctx == NULL || ctx->pk_info == NULL )
        return( 0 );

    return( ctx->pk_info->get_size( ctx->pk_ctx ) );
}

/*
 * Export debug information
 */
int pk_debug( const pk_context *ctx, pk_debug_item *items )
{
    if( ctx == NULL || ctx->pk_info == NULL )
        return( POLARSSL_ERR_PK_BAD_INPUT_DATA );

    ctx->pk_info->debug_func( ctx->pk_ctx, items );
    return( 0 );
}

/*
 * Access the PK type name
 */
const char * pk_get_name( const pk_context *ctx )
{
    if( ctx == NULL || ctx->pk_info == NULL )
        return( "invalid PK" );

    return( ctx->pk_info->name );
}

/*
 * Access the PK type
 */
pk_type_t pk_get_type( const pk_context *ctx )
{
    if( ctx == NULL || ctx->pk_info == NULL )
        return( POLARSSL_PK_NONE );

    return( ctx->pk_info->type );
}

#endif /* POLARSSL_PK_C */
