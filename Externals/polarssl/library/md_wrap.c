/**
 * \file md_wrap.c

 * \brief Generic message digest wrapper for PolarSSL
 *
 * \author Adriaan de Jong <dejong@fox-it.com>
 *
 *  Copyright (C) 2006-2010, Brainspark B.V.
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

#if defined(POLARSSL_MD_C)

#include "polarssl/md_wrap.h"

#if defined(POLARSSL_MD2_C)
#include "polarssl/md2.h"
#endif

#if defined(POLARSSL_MD4_C)
#include "polarssl/md4.h"
#endif

#if defined(POLARSSL_MD5_C)
#include "polarssl/md5.h"
#endif

#if defined(POLARSSL_SHA1_C)
#include "polarssl/sha1.h"
#endif

#if defined(POLARSSL_SHA2_C)
#include "polarssl/sha2.h"
#endif

#if defined(POLARSSL_SHA4_C)
#include "polarssl/sha4.h"
#endif

#include <stdlib.h>

#if defined(POLARSSL_MD2_C)

static void md2_starts_wrap( void *ctx )
{
    md2_starts( (md2_context *) ctx );
}

static void md2_update_wrap( void *ctx, const unsigned char *input, size_t ilen )
{
    md2_update( (md2_context *) ctx, input, ilen );
}

static void md2_finish_wrap( void *ctx, unsigned char *output )
{
    md2_finish( (md2_context *) ctx, output );
}

int md2_file_wrap( const char *path, unsigned char *output )
{
#if defined(POLARSSL_FS_IO)
    return md2_file( path, output );
#else
    ((void) path);
    ((void) output);
    return POLARSSL_ERR_MD_FEATURE_UNAVAILABLE;
#endif
}

static void md2_hmac_starts_wrap( void *ctx, const unsigned char *key, size_t keylen )
{
    md2_hmac_starts( (md2_context *) ctx, key, keylen );
}

static void md2_hmac_update_wrap( void *ctx, const unsigned char *input, size_t ilen )
{
    md2_hmac_update( (md2_context *) ctx, input, ilen );
}

static void md2_hmac_finish_wrap( void *ctx, unsigned char *output )
{
    md2_hmac_finish( (md2_context *) ctx, output );
}

static void md2_hmac_reset_wrap( void *ctx )
{
    md2_hmac_reset( (md2_context *) ctx );
}

static void * md2_ctx_alloc( void )
{
    return malloc( sizeof( md2_context ) );
}

static void md2_ctx_free( void *ctx )
{
    free( ctx );
}

const md_info_t md2_info = {
    POLARSSL_MD_MD2,
    "MD2",
    16,
    md2_starts_wrap,
    md2_update_wrap,
    md2_finish_wrap,
    md2,
    md2_file_wrap,
    md2_hmac_starts_wrap,
    md2_hmac_update_wrap,
    md2_hmac_finish_wrap,
    md2_hmac_reset_wrap,
    md2_hmac,
    md2_ctx_alloc,
    md2_ctx_free,
};

#endif

#if defined(POLARSSL_MD4_C)

void md4_starts_wrap( void *ctx )
{
    md4_starts( (md4_context *) ctx );
}

void md4_update_wrap( void *ctx, const unsigned char *input, size_t ilen )
{
    md4_update( (md4_context *) ctx, input, ilen );
}

void md4_finish_wrap( void *ctx, unsigned char *output )
{
    md4_finish( (md4_context *) ctx, output );
}

int md4_file_wrap( const char *path, unsigned char *output )
{
#if defined(POLARSSL_FS_IO)
    return md4_file( path, output );
#else
    ((void) path);
    ((void) output);
    return POLARSSL_ERR_MD_FEATURE_UNAVAILABLE;
#endif
}

void md4_hmac_starts_wrap( void *ctx, const unsigned char *key, size_t keylen )
{
    md4_hmac_starts( (md4_context *) ctx, key, keylen );
}

void md4_hmac_update_wrap( void *ctx, const unsigned char *input, size_t ilen )
{
    md4_hmac_update( (md4_context *) ctx, input, ilen );
}

void md4_hmac_finish_wrap( void *ctx, unsigned char *output )
{
    md4_hmac_finish( (md4_context *) ctx, output );
}

void md4_hmac_reset_wrap( void *ctx )
{
    md4_hmac_reset( (md4_context *) ctx );
}

void *md4_ctx_alloc( void )
{
    return malloc( sizeof( md4_context ) );
}

void md4_ctx_free( void *ctx )
{
    free( ctx );
}

const md_info_t md4_info = {
    POLARSSL_MD_MD4,
    "MD4",
    16,
    md4_starts_wrap,
    md4_update_wrap,
    md4_finish_wrap,
    md4,
    md4_file_wrap,
    md4_hmac_starts_wrap,
    md4_hmac_update_wrap,
    md4_hmac_finish_wrap,
    md4_hmac_reset_wrap,
    md4_hmac,
    md4_ctx_alloc,
    md4_ctx_free,
};

#endif

#if defined(POLARSSL_MD5_C)

static void md5_starts_wrap( void *ctx )
{
    md5_starts( (md5_context *) ctx );
}

static void md5_update_wrap( void *ctx, const unsigned char *input, size_t ilen )
{
    md5_update( (md5_context *) ctx, input, ilen );
}

static void md5_finish_wrap( void *ctx, unsigned char *output )
{
    md5_finish( (md5_context *) ctx, output );
}

int md5_file_wrap( const char *path, unsigned char *output )
{
#if defined(POLARSSL_FS_IO)
    return md5_file( path, output );
#else
    ((void) path);
    ((void) output);
    return POLARSSL_ERR_MD_FEATURE_UNAVAILABLE;
#endif
}

static void md5_hmac_starts_wrap( void *ctx, const unsigned char *key, size_t keylen )
{
    md5_hmac_starts( (md5_context *) ctx, key, keylen );
}

static void md5_hmac_update_wrap( void *ctx, const unsigned char *input, size_t ilen )
{
    md5_hmac_update( (md5_context *) ctx, input, ilen );
}

static void md5_hmac_finish_wrap( void *ctx, unsigned char *output )
{
    md5_hmac_finish( (md5_context *) ctx, output );
}

static void md5_hmac_reset_wrap( void *ctx )
{
    md5_hmac_reset( (md5_context *) ctx );
}

static void * md5_ctx_alloc( void )
{
    return malloc( sizeof( md5_context ) );
}

static void md5_ctx_free( void *ctx )
{
    free( ctx );
}

const md_info_t md5_info = {
    POLARSSL_MD_MD5,
    "MD5",
    16,
    md5_starts_wrap,
    md5_update_wrap,
    md5_finish_wrap,
    md5,
    md5_file_wrap,
    md5_hmac_starts_wrap,
    md5_hmac_update_wrap,
    md5_hmac_finish_wrap,
    md5_hmac_reset_wrap,
    md5_hmac,
    md5_ctx_alloc,
    md5_ctx_free,
};

#endif

#if defined(POLARSSL_SHA1_C)

void sha1_starts_wrap( void *ctx )
{
    sha1_starts( (sha1_context *) ctx );
}

void sha1_update_wrap( void *ctx, const unsigned char *input, size_t ilen )
{
    sha1_update( (sha1_context *) ctx, input, ilen );
}

void sha1_finish_wrap( void *ctx, unsigned char *output )
{
    sha1_finish( (sha1_context *) ctx, output );
}

int sha1_file_wrap( const char *path, unsigned char *output )
{
#if defined(POLARSSL_FS_IO)
    return sha1_file( path, output );
#else
    ((void) path);
    ((void) output);
    return POLARSSL_ERR_MD_FEATURE_UNAVAILABLE;
#endif
}

void sha1_hmac_starts_wrap( void *ctx, const unsigned char *key, size_t keylen )
{
    sha1_hmac_starts( (sha1_context *) ctx, key, keylen );
}

void sha1_hmac_update_wrap( void *ctx, const unsigned char *input, size_t ilen )
{
    sha1_hmac_update( (sha1_context *) ctx, input, ilen );
}

void sha1_hmac_finish_wrap( void *ctx, unsigned char *output )
{
    sha1_hmac_finish( (sha1_context *) ctx, output );
}

void sha1_hmac_reset_wrap( void *ctx )
{
    sha1_hmac_reset( (sha1_context *) ctx );
}

void * sha1_ctx_alloc( void )
{
    return malloc( sizeof( sha1_context ) );
}

void sha1_ctx_free( void *ctx )
{
    free( ctx );
}

const md_info_t sha1_info = {
    POLARSSL_MD_SHA1,
    "SHA1",
    20,
    sha1_starts_wrap,
    sha1_update_wrap,
    sha1_finish_wrap,
    sha1,
    sha1_file_wrap,
    sha1_hmac_starts_wrap,
    sha1_hmac_update_wrap,
    sha1_hmac_finish_wrap,
    sha1_hmac_reset_wrap,
    sha1_hmac,
    sha1_ctx_alloc,
    sha1_ctx_free,
};

#endif

/*
 * Wrappers for generic message digests
 */
#if defined(POLARSSL_SHA2_C)

void sha224_starts_wrap( void *ctx )
{
    sha2_starts( (sha2_context *) ctx, 1 );
}

void sha224_update_wrap( void *ctx, const unsigned char *input, size_t ilen )
{
    sha2_update( (sha2_context *) ctx, input, ilen );
}

void sha224_finish_wrap( void *ctx, unsigned char *output )
{
    sha2_finish( (sha2_context *) ctx, output );
}

void sha224_wrap( const unsigned char *input, size_t ilen,
                    unsigned char *output )
{
    sha2( input, ilen, output, 1 );
}

int sha224_file_wrap( const char *path, unsigned char *output )
{
#if defined(POLARSSL_FS_IO)
    return sha2_file( path, output, 1 );
#else
    ((void) path);
    ((void) output);
    return POLARSSL_ERR_MD_FEATURE_UNAVAILABLE;
#endif
}

void sha224_hmac_starts_wrap( void *ctx, const unsigned char *key, size_t keylen )
{
    sha2_hmac_starts( (sha2_context *) ctx, key, keylen, 1 );
}

void sha224_hmac_update_wrap( void *ctx, const unsigned char *input, size_t ilen )
{
    sha2_hmac_update( (sha2_context *) ctx, input, ilen );
}

void sha224_hmac_finish_wrap( void *ctx, unsigned char *output )
{
    sha2_hmac_finish( (sha2_context *) ctx, output );
}

void sha224_hmac_reset_wrap( void *ctx )
{
    sha2_hmac_reset( (sha2_context *) ctx );
}

void sha224_hmac_wrap( const unsigned char *key, size_t keylen,
        const unsigned char *input, size_t ilen,
        unsigned char *output )
{
    sha2_hmac( key, keylen, input, ilen, output, 1 );
}

void * sha224_ctx_alloc( void )
{
    return malloc( sizeof( sha2_context ) );
}

void sha224_ctx_free( void *ctx )
{
    free( ctx );
}

const md_info_t sha224_info = {
    POLARSSL_MD_SHA224,
    "SHA224",
    28,
    sha224_starts_wrap,
    sha224_update_wrap,
    sha224_finish_wrap,
    sha224_wrap,
    sha224_file_wrap,
    sha224_hmac_starts_wrap,
    sha224_hmac_update_wrap,
    sha224_hmac_finish_wrap,
    sha224_hmac_reset_wrap,
    sha224_hmac_wrap,
    sha224_ctx_alloc,
    sha224_ctx_free,
};

void sha256_starts_wrap( void *ctx )
{
    sha2_starts( (sha2_context *) ctx, 0 );
}

void sha256_update_wrap( void *ctx, const unsigned char *input, size_t ilen )
{
    sha2_update( (sha2_context *) ctx, input, ilen );
}

void sha256_finish_wrap( void *ctx, unsigned char *output )
{
    sha2_finish( (sha2_context *) ctx, output );
}

void sha256_wrap( const unsigned char *input, size_t ilen,
                    unsigned char *output )
{
    sha2( input, ilen, output, 0 );
}

int sha256_file_wrap( const char *path, unsigned char *output )
{
#if defined(POLARSSL_FS_IO)
    return sha2_file( path, output, 0 );
#else
    ((void) path);
    ((void) output);
    return POLARSSL_ERR_MD_FEATURE_UNAVAILABLE;
#endif
}

void sha256_hmac_starts_wrap( void *ctx, const unsigned char *key, size_t keylen )
{
    sha2_hmac_starts( (sha2_context *) ctx, key, keylen, 0 );
}

void sha256_hmac_update_wrap( void *ctx, const unsigned char *input, size_t ilen )
{
    sha2_hmac_update( (sha2_context *) ctx, input, ilen );
}

void sha256_hmac_finish_wrap( void *ctx, unsigned char *output )
{
    sha2_hmac_finish( (sha2_context *) ctx, output );
}

void sha256_hmac_reset_wrap( void *ctx )
{
    sha2_hmac_reset( (sha2_context *) ctx );
}

void sha256_hmac_wrap( const unsigned char *key, size_t keylen,
        const unsigned char *input, size_t ilen,
        unsigned char *output )
{
    sha2_hmac( key, keylen, input, ilen, output, 0 );
}

void * sha256_ctx_alloc( void )
{
    return malloc( sizeof( sha2_context ) );
}

void sha256_ctx_free( void *ctx )
{
    free( ctx );
}

const md_info_t sha256_info = {
    POLARSSL_MD_SHA256,
    "SHA256",
    32,
    sha256_starts_wrap,
    sha256_update_wrap,
    sha256_finish_wrap,
    sha256_wrap,
    sha256_file_wrap,
    sha256_hmac_starts_wrap,
    sha256_hmac_update_wrap,
    sha256_hmac_finish_wrap,
    sha256_hmac_reset_wrap,
    sha256_hmac_wrap,
    sha256_ctx_alloc,
    sha256_ctx_free,
};

#endif

#if defined(POLARSSL_SHA4_C)

void sha384_starts_wrap( void *ctx )
{
    sha4_starts( (sha4_context *) ctx, 1 );
}

void sha384_update_wrap( void *ctx, const unsigned char *input, size_t ilen )
{
    sha4_update( (sha4_context *) ctx, input, ilen );
}

void sha384_finish_wrap( void *ctx, unsigned char *output )
{
    sha4_finish( (sha4_context *) ctx, output );
}

void sha384_wrap( const unsigned char *input, size_t ilen,
                    unsigned char *output )
{
    sha4( input, ilen, output, 1 );
}

int sha384_file_wrap( const char *path, unsigned char *output )
{
#if defined(POLARSSL_FS_IO)
    return sha4_file( path, output, 1 );
#else
    ((void) path);
    ((void) output);
    return POLARSSL_ERR_MD_FEATURE_UNAVAILABLE;
#endif
}

void sha384_hmac_starts_wrap( void *ctx, const unsigned char *key, size_t keylen )
{
    sha4_hmac_starts( (sha4_context *) ctx, key, keylen, 1 );
}

void sha384_hmac_update_wrap( void *ctx, const unsigned char *input, size_t ilen )
{
    sha4_hmac_update( (sha4_context *) ctx, input, ilen );
}

void sha384_hmac_finish_wrap( void *ctx, unsigned char *output )
{
    sha4_hmac_finish( (sha4_context *) ctx, output );
}

void sha384_hmac_reset_wrap( void *ctx )
{
    sha4_hmac_reset( (sha4_context *) ctx );
}

void sha384_hmac_wrap( const unsigned char *key, size_t keylen,
        const unsigned char *input, size_t ilen,
        unsigned char *output )
{
    sha4_hmac( key, keylen, input, ilen, output, 1 );
}

void * sha384_ctx_alloc( void )
{
    return malloc( sizeof( sha4_context ) );
}

void sha384_ctx_free( void *ctx )
{
    free( ctx );
}

const md_info_t sha384_info = {
    POLARSSL_MD_SHA384,
    "SHA384",
    48,
    sha384_starts_wrap,
    sha384_update_wrap,
    sha384_finish_wrap,
    sha384_wrap,
    sha384_file_wrap,
    sha384_hmac_starts_wrap,
    sha384_hmac_update_wrap,
    sha384_hmac_finish_wrap,
    sha384_hmac_reset_wrap,
    sha384_hmac_wrap,
    sha384_ctx_alloc,
    sha384_ctx_free,
};

void sha512_starts_wrap( void *ctx )
{
    sha4_starts( (sha4_context *) ctx, 0 );
}

void sha512_update_wrap( void *ctx, const unsigned char *input, size_t ilen )
{
    sha4_update( (sha4_context *) ctx, input, ilen );
}

void sha512_finish_wrap( void *ctx, unsigned char *output )
{
    sha4_finish( (sha4_context *) ctx, output );
}

void sha512_wrap( const unsigned char *input, size_t ilen,
                    unsigned char *output )
{
    sha4( input, ilen, output, 0 );
}

int sha512_file_wrap( const char *path, unsigned char *output )
{
#if defined(POLARSSL_FS_IO)
    return sha4_file( path, output, 0 );
#else
    ((void) path);
    ((void) output);
    return POLARSSL_ERR_MD_FEATURE_UNAVAILABLE;
#endif
}

void sha512_hmac_starts_wrap( void *ctx, const unsigned char *key, size_t keylen )
{
    sha4_hmac_starts( (sha4_context *) ctx, key, keylen, 0 );
}

void sha512_hmac_update_wrap( void *ctx, const unsigned char *input, size_t ilen )
{
    sha4_hmac_update( (sha4_context *) ctx, input, ilen );
}

void sha512_hmac_finish_wrap( void *ctx, unsigned char *output )
{
    sha4_hmac_finish( (sha4_context *) ctx, output );
}

void sha512_hmac_reset_wrap( void *ctx )
{
    sha4_hmac_reset( (sha4_context *) ctx );
}

void sha512_hmac_wrap( const unsigned char *key, size_t keylen,
        const unsigned char *input, size_t ilen,
        unsigned char *output )
{
    sha4_hmac( key, keylen, input, ilen, output, 0 );
}

void * sha512_ctx_alloc( void )
{
    return malloc( sizeof( sha4_context ) );
}

void sha512_ctx_free( void *ctx )
{
    free( ctx );
}

const md_info_t sha512_info = {
    POLARSSL_MD_SHA512,
    "SHA512",
    64,
    sha512_starts_wrap,
    sha512_update_wrap,
    sha512_finish_wrap,
    sha512_wrap,
    sha512_file_wrap,
    sha512_hmac_starts_wrap,
    sha512_hmac_update_wrap,
    sha512_hmac_finish_wrap,
    sha512_hmac_reset_wrap,
    sha512_hmac_wrap,
    sha512_ctx_alloc,
    sha512_ctx_free,
};

#endif

#endif
