/*
 *  SSLv3/TLSv1 shared functions
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
/*
 *  The SSL 3.0 specification was drafted by Netscape in 1996,
 *  and became an IETF standard in 1999.
 *
 *  http://wp.netscape.com/eng/ssl3/
 *  http://www.ietf.org/rfc/rfc2246.txt
 *  http://www.ietf.org/rfc/rfc4346.txt
 */

#include "polarssl/config.h"

#if defined(POLARSSL_SSL_TLS_C)

#include "polarssl/aes.h"
#include "polarssl/arc4.h"
#include "polarssl/camellia.h"
#include "polarssl/des.h"
#include "polarssl/debug.h"
#include "polarssl/ssl.h"
#include "polarssl/sha2.h"

#if defined(POLARSSL_GCM_C)
#include "polarssl/gcm.h"
#endif

#include <stdlib.h>
#include <time.h>

#if defined _MSC_VER && !defined strcasecmp
#define strcasecmp _stricmp
#endif

#if defined(POLARSSL_SSL_HW_RECORD_ACCEL)
int (*ssl_hw_record_init)(ssl_context *ssl,
                       const unsigned char *key_enc, const unsigned char *key_dec,
                       const unsigned char *iv_enc,  const unsigned char *iv_dec,
                       const unsigned char *mac_enc, const unsigned char *mac_dec) = NULL;
int (*ssl_hw_record_reset)(ssl_context *ssl) = NULL;
int (*ssl_hw_record_write)(ssl_context *ssl) = NULL;
int (*ssl_hw_record_read)(ssl_context *ssl) = NULL;
int (*ssl_hw_record_finish)(ssl_context *ssl) = NULL;
#endif

static int ssl_rsa_decrypt( void *ctx, int mode, size_t *olen,
                        const unsigned char *input, unsigned char *output,
                        size_t output_max_len )
{
    return rsa_pkcs1_decrypt( (rsa_context *) ctx, mode, olen, input, output,
                              output_max_len );
}

static int ssl_rsa_sign( void *ctx,
                    int (*f_rng)(void *, unsigned char *, size_t), void *p_rng,
                    int mode, int hash_id, unsigned int hashlen,
                    const unsigned char *hash, unsigned char *sig )
{
    return rsa_pkcs1_sign( (rsa_context *) ctx, f_rng, p_rng, mode, hash_id,
                           hashlen, hash, sig );
}

static size_t ssl_rsa_key_len( void *ctx )
{
    return ( (rsa_context *) ctx )->len;
}

/*
 * Key material generation
 */
static int ssl3_prf( unsigned char *secret, size_t slen, char *label,
                     unsigned char *random, size_t rlen,
                     unsigned char *dstbuf, size_t dlen )
{
    size_t i;
    md5_context md5;
    sha1_context sha1;
    unsigned char padding[16];
    unsigned char sha1sum[20];
    ((void)label);

    /*
     *  SSLv3:
     *    block =
     *      MD5( secret + SHA1( 'A'    + secret + random ) ) +
     *      MD5( secret + SHA1( 'BB'   + secret + random ) ) +
     *      MD5( secret + SHA1( 'CCC'  + secret + random ) ) +
     *      ...
     */
    for( i = 0; i < dlen / 16; i++ )
    {
        memset( padding, 'A' + i, 1 + i );

        sha1_starts( &sha1 );
        sha1_update( &sha1, padding, 1 + i );
        sha1_update( &sha1, secret, slen );
        sha1_update( &sha1, random, rlen );
        sha1_finish( &sha1, sha1sum );

        md5_starts( &md5 );
        md5_update( &md5, secret, slen );
        md5_update( &md5, sha1sum, 20 );
        md5_finish( &md5, dstbuf + i * 16 );
    }

    memset( &md5,  0, sizeof( md5  ) );
    memset( &sha1, 0, sizeof( sha1 ) );

    memset( padding, 0, sizeof( padding ) );
    memset( sha1sum, 0, sizeof( sha1sum ) );

    return( 0 );
}

static int tls1_prf( unsigned char *secret, size_t slen, char *label,
                     unsigned char *random, size_t rlen,
                     unsigned char *dstbuf, size_t dlen )
{
    size_t nb, hs;
    size_t i, j, k;
    unsigned char *S1, *S2;
    unsigned char tmp[128];
    unsigned char h_i[20];

    if( sizeof( tmp ) < 20 + strlen( label ) + rlen )
        return( POLARSSL_ERR_SSL_BAD_INPUT_DATA );

    hs = ( slen + 1 ) / 2;
    S1 = secret;
    S2 = secret + slen - hs;

    nb = strlen( label );
    memcpy( tmp + 20, label, nb );
    memcpy( tmp + 20 + nb, random, rlen );
    nb += rlen;

    /*
     * First compute P_md5(secret,label+random)[0..dlen]
     */
    md5_hmac( S1, hs, tmp + 20, nb, 4 + tmp );

    for( i = 0; i < dlen; i += 16 )
    {
        md5_hmac( S1, hs, 4 + tmp, 16 + nb, h_i );
        md5_hmac( S1, hs, 4 + tmp, 16,  4 + tmp );

        k = ( i + 16 > dlen ) ? dlen % 16 : 16;

        for( j = 0; j < k; j++ )
            dstbuf[i + j]  = h_i[j];
    }

    /*
     * XOR out with P_sha1(secret,label+random)[0..dlen]
     */
    sha1_hmac( S2, hs, tmp + 20, nb, tmp );

    for( i = 0; i < dlen; i += 20 )
    {
        sha1_hmac( S2, hs, tmp, 20 + nb, h_i );
        sha1_hmac( S2, hs, tmp, 20,      tmp );

        k = ( i + 20 > dlen ) ? dlen % 20 : 20;

        for( j = 0; j < k; j++ )
            dstbuf[i + j] = (unsigned char)( dstbuf[i + j] ^ h_i[j] );
    }

    memset( tmp, 0, sizeof( tmp ) );
    memset( h_i, 0, sizeof( h_i ) );

    return( 0 );
}

static int tls_prf_sha256( unsigned char *secret, size_t slen, char *label,
                           unsigned char *random, size_t rlen,
                           unsigned char *dstbuf, size_t dlen )
{
    size_t nb;
    size_t i, j, k;
    unsigned char tmp[128];
    unsigned char h_i[32];

    if( sizeof( tmp ) < 32 + strlen( label ) + rlen )
        return( POLARSSL_ERR_SSL_BAD_INPUT_DATA );

    nb = strlen( label );
    memcpy( tmp + 32, label, nb );
    memcpy( tmp + 32 + nb, random, rlen );
    nb += rlen;

    /*
     * Compute P_<hash>(secret, label + random)[0..dlen]
     */
    sha2_hmac( secret, slen, tmp + 32, nb, tmp, 0 );

    for( i = 0; i < dlen; i += 32 )
    {
        sha2_hmac( secret, slen, tmp, 32 + nb, h_i, 0 );
        sha2_hmac( secret, slen, tmp, 32,      tmp, 0 );

        k = ( i + 32 > dlen ) ? dlen % 32 : 32;

        for( j = 0; j < k; j++ )
            dstbuf[i + j]  = h_i[j];
    }

    memset( tmp, 0, sizeof( tmp ) );
    memset( h_i, 0, sizeof( h_i ) );

    return( 0 );
}

#if defined(POLARSSL_SHA4_C)
static int tls_prf_sha384( unsigned char *secret, size_t slen, char *label,
                           unsigned char *random, size_t rlen,
                           unsigned char *dstbuf, size_t dlen )
{
    size_t nb;
    size_t i, j, k;
    unsigned char tmp[128];
    unsigned char h_i[48];

    if( sizeof( tmp ) < 48 + strlen( label ) + rlen )
        return( POLARSSL_ERR_SSL_BAD_INPUT_DATA );

    nb = strlen( label );
    memcpy( tmp + 48, label, nb );
    memcpy( tmp + 48 + nb, random, rlen );
    nb += rlen;

    /*
     * Compute P_<hash>(secret, label + random)[0..dlen]
     */
    sha4_hmac( secret, slen, tmp + 48, nb, tmp, 1 );

    for( i = 0; i < dlen; i += 48 )
    {
        sha4_hmac( secret, slen, tmp, 48 + nb, h_i, 1 );
        sha4_hmac( secret, slen, tmp, 48,      tmp, 1 );

        k = ( i + 48 > dlen ) ? dlen % 48 : 48;

        for( j = 0; j < k; j++ )
            dstbuf[i + j]  = h_i[j];
    }

    memset( tmp, 0, sizeof( tmp ) );
    memset( h_i, 0, sizeof( h_i ) );

    return( 0 );
}
#endif

static void ssl_update_checksum_start(ssl_context *, unsigned char *, size_t);
static void ssl_update_checksum_md5sha1(ssl_context *, unsigned char *, size_t);
static void ssl_update_checksum_sha256(ssl_context *, unsigned char *, size_t);

static void ssl_calc_verify_ssl(ssl_context *,unsigned char *);
static void ssl_calc_verify_tls(ssl_context *,unsigned char *);
static void ssl_calc_verify_tls_sha256(ssl_context *,unsigned char *);

static void ssl_calc_finished_ssl(ssl_context *,unsigned char *,int);
static void ssl_calc_finished_tls(ssl_context *,unsigned char *,int);
static void ssl_calc_finished_tls_sha256(ssl_context *,unsigned char *,int);

#if defined(POLARSSL_SHA4_C)
static void ssl_update_checksum_sha384(ssl_context *, unsigned char *, size_t);
static void ssl_calc_verify_tls_sha384(ssl_context *,unsigned char *);
static void ssl_calc_finished_tls_sha384(ssl_context *,unsigned char *,int);
#endif

int ssl_derive_keys( ssl_context *ssl )
{
    unsigned char tmp[64];
    unsigned char keyblk[256];
    unsigned char *key1;
    unsigned char *key2;
    unsigned int iv_copy_len;
    ssl_session *session = ssl->session_negotiate;
    ssl_transform *transform = ssl->transform_negotiate;
    ssl_handshake_params *handshake = ssl->handshake;

    SSL_DEBUG_MSG( 2, ( "=> derive keys" ) );

    /*
     * Set appropriate PRF function and other SSL / TLS / TLS1.2 functions
     */
    if( ssl->minor_ver == SSL_MINOR_VERSION_0 )
    {
        handshake->tls_prf = ssl3_prf;
        handshake->calc_verify = ssl_calc_verify_ssl;
        handshake->calc_finished = ssl_calc_finished_ssl;
    }
    else if( ssl->minor_ver < SSL_MINOR_VERSION_3 )
    {
        handshake->tls_prf = tls1_prf;
        handshake->calc_verify = ssl_calc_verify_tls;
        handshake->calc_finished = ssl_calc_finished_tls;
    }
#if defined(POLARSSL_SHA4_C)
    else if( session->ciphersuite == TLS_RSA_WITH_AES_256_GCM_SHA384 ||
             session->ciphersuite == TLS_DHE_RSA_WITH_AES_256_GCM_SHA384 )
    {
        handshake->tls_prf = tls_prf_sha384;
        handshake->calc_verify = ssl_calc_verify_tls_sha384;
        handshake->calc_finished = ssl_calc_finished_tls_sha384;
    }
#endif
    else
    {
        handshake->tls_prf = tls_prf_sha256;
        handshake->calc_verify = ssl_calc_verify_tls_sha256;
        handshake->calc_finished = ssl_calc_finished_tls_sha256;
    }

    /*
     * SSLv3:
     *   master =
     *     MD5( premaster + SHA1( 'A'   + premaster + randbytes ) ) +
     *     MD5( premaster + SHA1( 'BB'  + premaster + randbytes ) ) +
     *     MD5( premaster + SHA1( 'CCC' + premaster + randbytes ) )
     *   
     * TLSv1:
     *   master = PRF( premaster, "master secret", randbytes )[0..47]
     */
    if( handshake->resume == 0 )
    {
        SSL_DEBUG_BUF( 3, "premaster secret", handshake->premaster,
                       handshake->pmslen );

        handshake->tls_prf( handshake->premaster, handshake->pmslen,
                            "master secret",
                            handshake->randbytes, 64, session->master, 48 );

        memset( handshake->premaster, 0, sizeof( handshake->premaster ) );
    }
    else
        SSL_DEBUG_MSG( 3, ( "no premaster (session resumed)" ) );

    /*
     * Swap the client and server random values.
     */
    memcpy( tmp, handshake->randbytes, 64 );
    memcpy( handshake->randbytes, tmp + 32, 32 );
    memcpy( handshake->randbytes + 32, tmp, 32 );
    memset( tmp, 0, sizeof( tmp ) );

    /*
     *  SSLv3:
     *    key block =
     *      MD5( master + SHA1( 'A'    + master + randbytes ) ) +
     *      MD5( master + SHA1( 'BB'   + master + randbytes ) ) +
     *      MD5( master + SHA1( 'CCC'  + master + randbytes ) ) +
     *      MD5( master + SHA1( 'DDDD' + master + randbytes ) ) +
     *      ...
     *
     *  TLSv1:
     *    key block = PRF( master, "key expansion", randbytes )
     */
    handshake->tls_prf( session->master, 48, "key expansion",
                        handshake->randbytes, 64, keyblk, 256 );

    SSL_DEBUG_MSG( 3, ( "ciphersuite = %s",
                   ssl_get_ciphersuite_name( session->ciphersuite ) ) );
    SSL_DEBUG_BUF( 3, "master secret", session->master, 48 );
    SSL_DEBUG_BUF( 4, "random bytes", handshake->randbytes, 64 );
    SSL_DEBUG_BUF( 4, "key block", keyblk, 256 );

    memset( handshake->randbytes, 0, sizeof( handshake->randbytes ) );

    /*
     * Determine the appropriate key, IV and MAC length.
     */
    switch( session->ciphersuite )
    {
#if defined(POLARSSL_ARC4_C)
        case TLS_RSA_WITH_RC4_128_MD5:
            transform->keylen = 16; transform->minlen = 16;
            transform->ivlen  =  0; transform->maclen = 16;
            break;

        case TLS_RSA_WITH_RC4_128_SHA:
            transform->keylen = 16; transform->minlen = 20;
            transform->ivlen  =  0; transform->maclen = 20;
            break;
#endif

#if defined(POLARSSL_DES_C)
        case TLS_RSA_WITH_3DES_EDE_CBC_SHA:
        case TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA:
            transform->keylen = 24; transform->minlen = 24;
            transform->ivlen  =  8; transform->maclen = 20;
            break;
#endif

#if defined(POLARSSL_AES_C)
        case TLS_RSA_WITH_AES_128_CBC_SHA:
        case TLS_DHE_RSA_WITH_AES_128_CBC_SHA:
            transform->keylen = 16; transform->minlen = 32;
            transform->ivlen  = 16; transform->maclen = 20;
            break;

        case TLS_RSA_WITH_AES_256_CBC_SHA:
        case TLS_DHE_RSA_WITH_AES_256_CBC_SHA:
            transform->keylen = 32; transform->minlen = 32;
            transform->ivlen  = 16; transform->maclen = 20;
            break;

#if defined(POLARSSL_SHA2_C)
        case TLS_RSA_WITH_AES_128_CBC_SHA256:
        case TLS_DHE_RSA_WITH_AES_128_CBC_SHA256:
            transform->keylen = 16; transform->minlen = 32;
            transform->ivlen  = 16; transform->maclen = 32;
            break;

        case TLS_RSA_WITH_AES_256_CBC_SHA256:
        case TLS_DHE_RSA_WITH_AES_256_CBC_SHA256:
            transform->keylen = 32; transform->minlen = 32;
            transform->ivlen  = 16; transform->maclen = 32;
            break;
#endif
#if defined(POLARSSL_GCM_C)
        case TLS_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_DHE_RSA_WITH_AES_128_GCM_SHA256:
            transform->keylen = 16; transform->minlen = 1;
            transform->ivlen  = 12; transform->maclen = 0;
            transform->fixed_ivlen = 4;
            break;

        case TLS_RSA_WITH_AES_256_GCM_SHA384:
        case TLS_DHE_RSA_WITH_AES_256_GCM_SHA384:
            transform->keylen = 32; transform->minlen = 1;
            transform->ivlen  = 12; transform->maclen = 0;
            transform->fixed_ivlen = 4;
            break;
#endif
#endif

#if defined(POLARSSL_CAMELLIA_C)
        case TLS_RSA_WITH_CAMELLIA_128_CBC_SHA:
        case TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA:
            transform->keylen = 16; transform->minlen = 32;
            transform->ivlen  = 16; transform->maclen = 20;
            break;

        case TLS_RSA_WITH_CAMELLIA_256_CBC_SHA:
        case TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA:
            transform->keylen = 32; transform->minlen = 32;
            transform->ivlen  = 16; transform->maclen = 20;
            break;

#if defined(POLARSSL_SHA2_C)
        case TLS_RSA_WITH_CAMELLIA_128_CBC_SHA256:
        case TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA256:
            transform->keylen = 16; transform->minlen = 32;
            transform->ivlen  = 16; transform->maclen = 32;
            break;

        case TLS_RSA_WITH_CAMELLIA_256_CBC_SHA256:
        case TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA256:
            transform->keylen = 32; transform->minlen = 32;
            transform->ivlen  = 16; transform->maclen = 32;
            break;
#endif
#endif

#if defined(POLARSSL_ENABLE_WEAK_CIPHERSUITES)
#if defined(POLARSSL_CIPHER_NULL_CIPHER)
        case TLS_RSA_WITH_NULL_MD5:
            transform->keylen = 0; transform->minlen = 0;
            transform->ivlen  = 0; transform->maclen = 16;
            break;

        case TLS_RSA_WITH_NULL_SHA:
            transform->keylen = 0; transform->minlen = 0;
            transform->ivlen  = 0; transform->maclen = 20;
            break;

        case TLS_RSA_WITH_NULL_SHA256:
            transform->keylen = 0; transform->minlen = 0;
            transform->ivlen  = 0; transform->maclen = 32;
            break;
#endif /* defined(POLARSSL_CIPHER_NULL_CIPHER) */

#if defined(POLARSSL_DES_C)
        case TLS_RSA_WITH_DES_CBC_SHA:
        case TLS_DHE_RSA_WITH_DES_CBC_SHA:
            transform->keylen =  8; transform->minlen = 8;
            transform->ivlen  =  8; transform->maclen = 20;
            break;
#endif
#endif /* defined(POLARSSL_ENABLE_WEAK_CIPHERSUITES) */

        default:
            SSL_DEBUG_MSG( 1, ( "ciphersuite %s is not available",
                           ssl_get_ciphersuite_name( session->ciphersuite ) ) );
            return( POLARSSL_ERR_SSL_FEATURE_UNAVAILABLE );
    }

    SSL_DEBUG_MSG( 3, ( "keylen: %d, minlen: %d, ivlen: %d, maclen: %d",
                   transform->keylen, transform->minlen, transform->ivlen,
                   transform->maclen ) );

    /*
     * Finally setup the cipher contexts, IVs and MAC secrets.
     */
    if( ssl->endpoint == SSL_IS_CLIENT )
    {
        key1 = keyblk + transform->maclen * 2;
        key2 = keyblk + transform->maclen * 2 + transform->keylen;

        memcpy( transform->mac_enc, keyblk,  transform->maclen );
        memcpy( transform->mac_dec, keyblk + transform->maclen,
                transform->maclen );

        /*
         * This is not used in TLS v1.1.
         */
        iv_copy_len = ( transform->fixed_ivlen ) ?
                            transform->fixed_ivlen : transform->ivlen;
        memcpy( transform->iv_enc, key2 + transform->keylen,  iv_copy_len );
        memcpy( transform->iv_dec, key2 + transform->keylen + iv_copy_len,
                iv_copy_len );
    }
    else
    {
        key1 = keyblk + transform->maclen * 2 + transform->keylen;
        key2 = keyblk + transform->maclen * 2;

        memcpy( transform->mac_dec, keyblk,  transform->maclen );
        memcpy( transform->mac_enc, keyblk + transform->maclen,
                transform->maclen );

        /*
         * This is not used in TLS v1.1.
         */
        iv_copy_len = ( transform->fixed_ivlen ) ?
                            transform->fixed_ivlen : transform->ivlen;
        memcpy( transform->iv_dec, key1 + transform->keylen,  iv_copy_len );
        memcpy( transform->iv_enc, key1 + transform->keylen + iv_copy_len,
                iv_copy_len );
    }

#if defined(POLARSSL_SSL_HW_RECORD_ACCEL)
    if( ssl_hw_record_init != NULL)
    {
        int ret = 0;

        SSL_DEBUG_MSG( 2, ( "going for ssl_hw_record_init()" ) );

        if( ( ret = ssl_hw_record_init( ssl, key1, key2, transform->iv_enc,
                                        transform->iv_dec, transform->mac_enc,
                                        transform->mac_dec ) ) != 0 )
        {
            SSL_DEBUG_RET( 1, "ssl_hw_record_init", ret );
            return POLARSSL_ERR_SSL_HW_ACCEL_FAILED;
        }
    }
#endif

    switch( session->ciphersuite )
    {
#if defined(POLARSSL_ARC4_C)
        case TLS_RSA_WITH_RC4_128_MD5:
        case TLS_RSA_WITH_RC4_128_SHA:
            arc4_setup( (arc4_context *) transform->ctx_enc, key1,
                        transform->keylen );
            arc4_setup( (arc4_context *) transform->ctx_dec, key2,
                        transform->keylen );
            break;
#endif

#if defined(POLARSSL_DES_C)
        case TLS_RSA_WITH_3DES_EDE_CBC_SHA:
        case TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA:
            des3_set3key_enc( (des3_context *) transform->ctx_enc, key1 );
            des3_set3key_dec( (des3_context *) transform->ctx_dec, key2 );
            break;
#endif

#if defined(POLARSSL_AES_C)
        case TLS_RSA_WITH_AES_128_CBC_SHA:
        case TLS_DHE_RSA_WITH_AES_128_CBC_SHA:
        case TLS_RSA_WITH_AES_128_CBC_SHA256:
        case TLS_DHE_RSA_WITH_AES_128_CBC_SHA256:
            aes_setkey_enc( (aes_context *) transform->ctx_enc, key1, 128 );
            aes_setkey_dec( (aes_context *) transform->ctx_dec, key2, 128 );
            break;

        case TLS_RSA_WITH_AES_256_CBC_SHA:
        case TLS_DHE_RSA_WITH_AES_256_CBC_SHA:
        case TLS_RSA_WITH_AES_256_CBC_SHA256:
        case TLS_DHE_RSA_WITH_AES_256_CBC_SHA256:
            aes_setkey_enc( (aes_context *) transform->ctx_enc, key1, 256 );
            aes_setkey_dec( (aes_context *) transform->ctx_dec, key2, 256 );
            break;

#if defined(POLARSSL_GCM_C)
        case TLS_RSA_WITH_AES_128_GCM_SHA256:
        case TLS_DHE_RSA_WITH_AES_128_GCM_SHA256:
            gcm_init( (gcm_context *) transform->ctx_enc, key1, 128 );
            gcm_init( (gcm_context *) transform->ctx_dec, key2, 128 );
            break;

        case TLS_RSA_WITH_AES_256_GCM_SHA384:
        case TLS_DHE_RSA_WITH_AES_256_GCM_SHA384:
            gcm_init( (gcm_context *) transform->ctx_enc, key1, 256 );
            gcm_init( (gcm_context *) transform->ctx_dec, key2, 256 );
            break;
#endif
#endif

#if defined(POLARSSL_CAMELLIA_C)
        case TLS_RSA_WITH_CAMELLIA_128_CBC_SHA:
        case TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA:
        case TLS_RSA_WITH_CAMELLIA_128_CBC_SHA256:
        case TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA256:
            camellia_setkey_enc( (camellia_context *) transform->ctx_enc, key1, 128 );
            camellia_setkey_dec( (camellia_context *) transform->ctx_dec, key2, 128 );
            break;

        case TLS_RSA_WITH_CAMELLIA_256_CBC_SHA:
        case TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA:
        case TLS_RSA_WITH_CAMELLIA_256_CBC_SHA256:
        case TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA256:
            camellia_setkey_enc( (camellia_context *) transform->ctx_enc, key1, 256 );
            camellia_setkey_dec( (camellia_context *) transform->ctx_dec, key2, 256 );
            break;
#endif

#if defined(POLARSSL_ENABLE_WEAK_CIPHERSUITES)
#if defined(POLARSSL_CIPHER_NULL_CIPHER)
        case TLS_RSA_WITH_NULL_MD5:
        case TLS_RSA_WITH_NULL_SHA:
        case TLS_RSA_WITH_NULL_SHA256:
            break;
#endif /* defined(POLARSSL_CIPHER_NULL_CIPHER) */

#if defined(POLARSSL_DES_C)
        case TLS_RSA_WITH_DES_CBC_SHA:
        case TLS_DHE_RSA_WITH_DES_CBC_SHA:
            des_setkey_enc( (des_context *) transform->ctx_enc, key1 );
            des_setkey_dec( (des_context *) transform->ctx_dec, key2 );
            break;
#endif
#endif /* defined(POLARSSL_ENABLE_WEAK_CIPHERSUITES) */

        default:
            return( POLARSSL_ERR_SSL_FEATURE_UNAVAILABLE );
    }

    memset( keyblk, 0, sizeof( keyblk ) );

#if defined(POLARSSL_ZLIB_SUPPORT)
    // Initialize compression
    //
    if( session->compression == SSL_COMPRESS_DEFLATE )
    {
        SSL_DEBUG_MSG( 3, ( "Initializing zlib states" ) );

        memset( &transform->ctx_deflate, 0, sizeof( transform->ctx_deflate ) );
        memset( &transform->ctx_inflate, 0, sizeof( transform->ctx_inflate ) );

        if( deflateInit( &transform->ctx_deflate, Z_DEFAULT_COMPRESSION ) != Z_OK ||
            inflateInit( &transform->ctx_inflate ) != Z_OK )
        {
            SSL_DEBUG_MSG( 1, ( "Failed to initialize compression" ) );
            return( POLARSSL_ERR_SSL_COMPRESSION_FAILED );
        }
    }
#endif /* POLARSSL_ZLIB_SUPPORT */

    SSL_DEBUG_MSG( 2, ( "<= derive keys" ) );

    return( 0 );
}

void ssl_calc_verify_ssl( ssl_context *ssl, unsigned char hash[36] )
{
    md5_context md5;
    sha1_context sha1;
    unsigned char pad_1[48];
    unsigned char pad_2[48];

    SSL_DEBUG_MSG( 2, ( "=> calc verify ssl" ) );

    memcpy( &md5 , &ssl->handshake->fin_md5 , sizeof(md5_context)  );
    memcpy( &sha1, &ssl->handshake->fin_sha1, sizeof(sha1_context) );

    memset( pad_1, 0x36, 48 );
    memset( pad_2, 0x5C, 48 );

    md5_update( &md5, ssl->session_negotiate->master, 48 );
    md5_update( &md5, pad_1, 48 );
    md5_finish( &md5, hash );

    md5_starts( &md5 );
    md5_update( &md5, ssl->session_negotiate->master, 48 );
    md5_update( &md5, pad_2, 48 );
    md5_update( &md5, hash,  16 );
    md5_finish( &md5, hash );

    sha1_update( &sha1, ssl->session_negotiate->master, 48 );
    sha1_update( &sha1, pad_1, 40 );
    sha1_finish( &sha1, hash + 16 );

    sha1_starts( &sha1 );
    sha1_update( &sha1, ssl->session_negotiate->master, 48 );
    sha1_update( &sha1, pad_2, 40 );
    sha1_update( &sha1, hash + 16, 20 );
    sha1_finish( &sha1, hash + 16 );

    SSL_DEBUG_BUF( 3, "calculated verify result", hash, 36 );
    SSL_DEBUG_MSG( 2, ( "<= calc verify" ) );

    return;
}

void ssl_calc_verify_tls( ssl_context *ssl, unsigned char hash[36] )
{
    md5_context md5;
    sha1_context sha1;

    SSL_DEBUG_MSG( 2, ( "=> calc verify tls" ) );

    memcpy( &md5 , &ssl->handshake->fin_md5 , sizeof(md5_context)  );
    memcpy( &sha1, &ssl->handshake->fin_sha1, sizeof(sha1_context) );

     md5_finish( &md5,  hash );
    sha1_finish( &sha1, hash + 16 );

    SSL_DEBUG_BUF( 3, "calculated verify result", hash, 36 );
    SSL_DEBUG_MSG( 2, ( "<= calc verify" ) );

    return;
}

void ssl_calc_verify_tls_sha256( ssl_context *ssl, unsigned char hash[32] )
{
    sha2_context sha2;

    SSL_DEBUG_MSG( 2, ( "=> calc verify sha256" ) );

    memcpy( &sha2, &ssl->handshake->fin_sha2, sizeof(sha2_context) );
    sha2_finish( &sha2, hash );

    SSL_DEBUG_BUF( 3, "calculated verify result", hash, 32 );
    SSL_DEBUG_MSG( 2, ( "<= calc verify" ) );

    return;
}

#if defined(POLARSSL_SHA4_C)
void ssl_calc_verify_tls_sha384( ssl_context *ssl, unsigned char hash[48] )
{
    sha4_context sha4;

    SSL_DEBUG_MSG( 2, ( "=> calc verify sha384" ) );

    memcpy( &sha4, &ssl->handshake->fin_sha4, sizeof(sha4_context) );
    sha4_finish( &sha4, hash );

    SSL_DEBUG_BUF( 3, "calculated verify result", hash, 48 );
    SSL_DEBUG_MSG( 2, ( "<= calc verify" ) );

    return;
}
#endif

/*
 * SSLv3.0 MAC functions
 */
static void ssl_mac_md5( unsigned char *secret,
                         unsigned char *buf, size_t len,
                         unsigned char *ctr, int type )
{
    unsigned char header[11];
    unsigned char padding[48];
    md5_context md5;

    memcpy( header, ctr, 8 );
    header[ 8] = (unsigned char)  type;
    header[ 9] = (unsigned char)( len >> 8 );
    header[10] = (unsigned char)( len      );

    memset( padding, 0x36, 48 );
    md5_starts( &md5 );
    md5_update( &md5, secret,  16 );
    md5_update( &md5, padding, 48 );
    md5_update( &md5, header,  11 );
    md5_update( &md5, buf,  len );
    md5_finish( &md5, buf + len );

    memset( padding, 0x5C, 48 );
    md5_starts( &md5 );
    md5_update( &md5, secret,  16 );
    md5_update( &md5, padding, 48 );
    md5_update( &md5, buf + len, 16 );
    md5_finish( &md5, buf + len );
}

static void ssl_mac_sha1( unsigned char *secret,
                          unsigned char *buf, size_t len,
                          unsigned char *ctr, int type )
{
    unsigned char header[11];
    unsigned char padding[40];
    sha1_context sha1;

    memcpy( header, ctr, 8 );
    header[ 8] = (unsigned char)  type;
    header[ 9] = (unsigned char)( len >> 8 );
    header[10] = (unsigned char)( len      );

    memset( padding, 0x36, 40 );
    sha1_starts( &sha1 );
    sha1_update( &sha1, secret,  20 );
    sha1_update( &sha1, padding, 40 );
    sha1_update( &sha1, header,  11 );
    sha1_update( &sha1, buf,  len );
    sha1_finish( &sha1, buf + len );

    memset( padding, 0x5C, 40 );
    sha1_starts( &sha1 );
    sha1_update( &sha1, secret,  20 );
    sha1_update( &sha1, padding, 40 );
    sha1_update( &sha1, buf + len, 20 );
    sha1_finish( &sha1, buf + len );
}

static void ssl_mac_sha2( unsigned char *secret,
                          unsigned char *buf, size_t len,
                          unsigned char *ctr, int type )
{
    unsigned char header[11];
    unsigned char padding[32];
    sha2_context sha2;

    memcpy( header, ctr, 8 );
    header[ 8] = (unsigned char)  type;
    header[ 9] = (unsigned char)( len >> 8 );
    header[10] = (unsigned char)( len      );

    memset( padding, 0x36, 32 );
    sha2_starts( &sha2, 0 );
    sha2_update( &sha2, secret,  32 );
    sha2_update( &sha2, padding, 32 );
    sha2_update( &sha2, header,  11 );
    sha2_update( &sha2, buf,  len );
    sha2_finish( &sha2, buf + len );

    memset( padding, 0x5C, 32 );
    sha2_starts( &sha2, 0 );
    sha2_update( &sha2, secret,  32 );
    sha2_update( &sha2, padding, 32 );
    sha2_update( &sha2, buf + len, 32 );
    sha2_finish( &sha2, buf + len );
}

/*
 * Encryption/decryption functions
 */ 
static int ssl_encrypt_buf( ssl_context *ssl )
{
    size_t i, padlen;

    SSL_DEBUG_MSG( 2, ( "=> encrypt buf" ) );

    /*
     * Add MAC then encrypt
     */
    if( ssl->minor_ver == SSL_MINOR_VERSION_0 )
    {
        if( ssl->transform_out->maclen == 16 )
             ssl_mac_md5( ssl->transform_out->mac_enc,
                          ssl->out_msg, ssl->out_msglen,
                          ssl->out_ctr, ssl->out_msgtype );
        else if( ssl->transform_out->maclen == 20 )
            ssl_mac_sha1( ssl->transform_out->mac_enc,
                          ssl->out_msg, ssl->out_msglen,
                          ssl->out_ctr, ssl->out_msgtype );
        else if( ssl->transform_out->maclen == 32 )
            ssl_mac_sha2( ssl->transform_out->mac_enc,
                          ssl->out_msg, ssl->out_msglen,
                          ssl->out_ctr, ssl->out_msgtype );
        else if( ssl->transform_out->maclen != 0 )
        {
            SSL_DEBUG_MSG( 1, ( "invalid MAC len: %d",
                                ssl->transform_out->maclen ) );
            return( POLARSSL_ERR_SSL_FEATURE_UNAVAILABLE );
        }
    }
    else
    {
        if( ssl->transform_out->maclen == 16 )
        {
            md5_context ctx;
            md5_hmac_starts( &ctx, ssl->transform_out->mac_enc, 16 );
            md5_hmac_update( &ctx, ssl->out_ctr, 13 );
            md5_hmac_update( &ctx, ssl->out_msg, ssl->out_msglen );
            md5_hmac_finish( &ctx, ssl->out_msg + ssl->out_msglen );
            memset( &ctx, 0, sizeof(md5_context));
        }
        else if( ssl->transform_out->maclen == 20 )
        {
            sha1_context ctx;
            sha1_hmac_starts( &ctx, ssl->transform_out->mac_enc, 20 );
            sha1_hmac_update( &ctx, ssl->out_ctr, 13 );
            sha1_hmac_update( &ctx, ssl->out_msg, ssl->out_msglen );
            sha1_hmac_finish( &ctx, ssl->out_msg + ssl->out_msglen );
            memset( &ctx, 0, sizeof(sha1_context));
        }
        else if( ssl->transform_out->maclen == 32 )
        {
            sha2_context ctx;
            sha2_hmac_starts( &ctx, ssl->transform_out->mac_enc, 32, 0 );
            sha2_hmac_update( &ctx, ssl->out_ctr, 13 );
            sha2_hmac_update( &ctx, ssl->out_msg, ssl->out_msglen );
            sha2_hmac_finish( &ctx, ssl->out_msg + ssl->out_msglen );
            memset( &ctx, 0, sizeof(sha2_context));
        }
        else if( ssl->transform_out->maclen != 0 )
        {
            SSL_DEBUG_MSG( 1, ( "invalid MAC len: %d",
                                ssl->transform_out->maclen ) );
            return( POLARSSL_ERR_SSL_FEATURE_UNAVAILABLE );
        }
    }

    SSL_DEBUG_BUF( 4, "computed mac",
                   ssl->out_msg + ssl->out_msglen, ssl->transform_out->maclen );

    ssl->out_msglen += ssl->transform_out->maclen;

    if( ssl->transform_out->ivlen == 0 )
    {
        padlen = 0;

        SSL_DEBUG_MSG( 3, ( "before encrypt: msglen = %d, "
                            "including %d bytes of padding",
                       ssl->out_msglen, 0 ) );

        SSL_DEBUG_BUF( 4, "before encrypt: output payload",
                       ssl->out_msg, ssl->out_msglen );

#if defined(POLARSSL_ARC4_C)
        if( ssl->session_out->ciphersuite == TLS_RSA_WITH_RC4_128_MD5 ||
            ssl->session_out->ciphersuite == TLS_RSA_WITH_RC4_128_SHA )
        {
            arc4_crypt( (arc4_context *) ssl->transform_out->ctx_enc,
                        ssl->out_msglen, ssl->out_msg,
                        ssl->out_msg );
        } else
#endif
#if defined(POLARSSL_CIPHER_NULL_CIPHER)
        if( ssl->session_out->ciphersuite == TLS_RSA_WITH_NULL_MD5 ||
            ssl->session_out->ciphersuite == TLS_RSA_WITH_NULL_SHA ||
            ssl->session_out->ciphersuite == TLS_RSA_WITH_NULL_SHA256 )
        {
        } else
#endif
        return( POLARSSL_ERR_SSL_FEATURE_UNAVAILABLE );
    }
    else if( ssl->transform_out->ivlen == 12 )
    {
        size_t enc_msglen;
        unsigned char *enc_msg;
        unsigned char add_data[13];
        int ret = POLARSSL_ERR_SSL_FEATURE_UNAVAILABLE;

        padlen = 0;
        enc_msglen = ssl->out_msglen;

        memcpy( add_data, ssl->out_ctr, 8 );
        add_data[8]  = ssl->out_msgtype;
        add_data[9]  = ssl->major_ver;
        add_data[10] = ssl->minor_ver;
        add_data[11] = ( ssl->out_msglen >> 8 ) & 0xFF;
        add_data[12] = ssl->out_msglen & 0xFF;

        SSL_DEBUG_BUF( 4, "additional data used for AEAD",
                       add_data, 13 );

#if defined(POLARSSL_AES_C) && defined(POLARSSL_GCM_C)

        if( ssl->session_out->ciphersuite == TLS_RSA_WITH_AES_128_GCM_SHA256 ||
            ssl->session_out->ciphersuite == TLS_RSA_WITH_AES_256_GCM_SHA384 ||
            ssl->session_out->ciphersuite == TLS_DHE_RSA_WITH_AES_128_GCM_SHA256 ||
            ssl->session_out->ciphersuite == TLS_DHE_RSA_WITH_AES_256_GCM_SHA384 )
        {
            /*
             * Generate IV
             */
            ret = ssl->f_rng( ssl->p_rng,
                        ssl->transform_out->iv_enc + ssl->transform_out->fixed_ivlen,
                        ssl->transform_out->ivlen - ssl->transform_out->fixed_ivlen );
            if( ret != 0 )
                return( ret );

            /*
             * Shift message for ivlen bytes and prepend IV
             */
            memmove( ssl->out_msg + ssl->transform_out->ivlen -
                     ssl->transform_out->fixed_ivlen,
                     ssl->out_msg, ssl->out_msglen );
            memcpy( ssl->out_msg,
                    ssl->transform_out->iv_enc + ssl->transform_out->fixed_ivlen,
                    ssl->transform_out->ivlen  - ssl->transform_out->fixed_ivlen );

            /*
             * Fix pointer positions and message length with added IV
             */
            enc_msg = ssl->out_msg + ssl->transform_out->ivlen -
                      ssl->transform_out->fixed_ivlen;
            enc_msglen = ssl->out_msglen;
            ssl->out_msglen += ssl->transform_out->ivlen -
                               ssl->transform_out->fixed_ivlen;

            SSL_DEBUG_MSG( 3, ( "before encrypt: msglen = %d, "
                                "including %d bytes of padding",
                           ssl->out_msglen, 0 ) );

            SSL_DEBUG_BUF( 4, "before encrypt: output payload",
                           ssl->out_msg, ssl->out_msglen );

            /*
             * Adjust for tag
             */
            ssl->out_msglen += 16;
            
            gcm_crypt_and_tag( (gcm_context *) ssl->transform_out->ctx_enc,
                    GCM_ENCRYPT, enc_msglen,
                    ssl->transform_out->iv_enc, ssl->transform_out->ivlen,
                    add_data, 13,
                    enc_msg, enc_msg,
                    16, enc_msg + enc_msglen );

            SSL_DEBUG_BUF( 4, "after encrypt: tag",
                           enc_msg + enc_msglen, 16 );

        } else
#endif
        return( ret );
    }
    else
    {
        unsigned char *enc_msg;
        size_t enc_msglen;

        padlen = ssl->transform_out->ivlen - ( ssl->out_msglen + 1 ) %
                 ssl->transform_out->ivlen;
        if( padlen == ssl->transform_out->ivlen )
            padlen = 0;

        for( i = 0; i <= padlen; i++ )
            ssl->out_msg[ssl->out_msglen + i] = (unsigned char) padlen;

        ssl->out_msglen += padlen + 1;

        enc_msglen = ssl->out_msglen;
        enc_msg = ssl->out_msg;

        /*
         * Prepend per-record IV for block cipher in TLS v1.1 and up as per
         * Method 1 (6.2.3.2. in RFC4346 and RFC5246)
         */
        if( ssl->minor_ver >= SSL_MINOR_VERSION_2 )
        {
            /*
             * Generate IV
             */
            int ret = ssl->f_rng( ssl->p_rng, ssl->transform_out->iv_enc,
                                  ssl->transform_out->ivlen );
            if( ret != 0 )
                return( ret );

            /*
             * Shift message for ivlen bytes and prepend IV
             */
            memmove( ssl->out_msg + ssl->transform_out->ivlen, ssl->out_msg,
                     ssl->out_msglen );
            memcpy( ssl->out_msg, ssl->transform_out->iv_enc,
                    ssl->transform_out->ivlen );

            /*
             * Fix pointer positions and message length with added IV
             */
            enc_msg = ssl->out_msg + ssl->transform_out->ivlen;
            enc_msglen = ssl->out_msglen;
            ssl->out_msglen += ssl->transform_out->ivlen;
        }

        SSL_DEBUG_MSG( 3, ( "before encrypt: msglen = %d, "
                            "including %d bytes of IV and %d bytes of padding",
                       ssl->out_msglen, ssl->transform_out->ivlen, padlen + 1 ) );

        SSL_DEBUG_BUF( 4, "before encrypt: output payload",
                       ssl->out_msg, ssl->out_msglen );

        switch( ssl->transform_out->ivlen )
        {
#if defined(POLARSSL_DES_C)
            case  8:
#if defined(POLARSSL_ENABLE_WEAK_CIPHERSUITES)
                if( ssl->session_out->ciphersuite == TLS_RSA_WITH_DES_CBC_SHA ||
                    ssl->session_out->ciphersuite == TLS_DHE_RSA_WITH_DES_CBC_SHA )
                {
                    des_crypt_cbc( (des_context *) ssl->transform_out->ctx_enc,
                                   DES_ENCRYPT, enc_msglen,
                                   ssl->transform_out->iv_enc, enc_msg, enc_msg );
                }
                else
#endif
                    des3_crypt_cbc( (des3_context *) ssl->transform_out->ctx_enc,
                                    DES_ENCRYPT, enc_msglen,
                                    ssl->transform_out->iv_enc, enc_msg, enc_msg );
                break;
#endif

            case 16:
#if defined(POLARSSL_AES_C)
        if ( ssl->session_out->ciphersuite == TLS_RSA_WITH_AES_128_CBC_SHA ||
             ssl->session_out->ciphersuite == TLS_DHE_RSA_WITH_AES_128_CBC_SHA ||
             ssl->session_out->ciphersuite == TLS_RSA_WITH_AES_256_CBC_SHA ||
             ssl->session_out->ciphersuite == TLS_DHE_RSA_WITH_AES_256_CBC_SHA ||
             ssl->session_out->ciphersuite == TLS_RSA_WITH_AES_128_CBC_SHA256 ||
             ssl->session_out->ciphersuite == TLS_RSA_WITH_AES_256_CBC_SHA256 ||
             ssl->session_out->ciphersuite == TLS_DHE_RSA_WITH_AES_128_CBC_SHA256 ||
             ssl->session_out->ciphersuite == TLS_DHE_RSA_WITH_AES_256_CBC_SHA256 )
        {
                    aes_crypt_cbc( (aes_context *) ssl->transform_out->ctx_enc,
                        AES_ENCRYPT, enc_msglen,
                        ssl->transform_out->iv_enc, enc_msg, enc_msg);
                    break;
        }
#endif

#if defined(POLARSSL_CAMELLIA_C)
        if ( ssl->session_out->ciphersuite == TLS_RSA_WITH_CAMELLIA_128_CBC_SHA ||
             ssl->session_out->ciphersuite == TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA ||
             ssl->session_out->ciphersuite == TLS_RSA_WITH_CAMELLIA_256_CBC_SHA ||
             ssl->session_out->ciphersuite == TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA ||
             ssl->session_out->ciphersuite == TLS_RSA_WITH_CAMELLIA_128_CBC_SHA256 ||
             ssl->session_out->ciphersuite == TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA256 ||
             ssl->session_out->ciphersuite == TLS_RSA_WITH_CAMELLIA_256_CBC_SHA256 ||
             ssl->session_out->ciphersuite == TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA256 )
        {
                    camellia_crypt_cbc( (camellia_context *) ssl->transform_out->ctx_enc,
                        CAMELLIA_ENCRYPT, enc_msglen,
                        ssl->transform_out->iv_enc, enc_msg, enc_msg );
                    break;
        }
#endif

            default:
                return( POLARSSL_ERR_SSL_FEATURE_UNAVAILABLE );
        }
    }

    for( i = 8; i > 0; i-- )
        if( ++ssl->out_ctr[i - 1] != 0 )
            break;

    SSL_DEBUG_MSG( 2, ( "<= encrypt buf" ) );

    return( 0 );
}

/*
 * TODO: Use digest version when integrated!
 */
#define POLARSSL_SSL_MAX_MAC_SIZE   32

static int ssl_decrypt_buf( ssl_context *ssl )
{
    size_t i, padlen = 0, correct = 1;
    unsigned char tmp[POLARSSL_SSL_MAX_MAC_SIZE];

    SSL_DEBUG_MSG( 2, ( "=> decrypt buf" ) );

    if( ssl->in_msglen < ssl->transform_in->minlen )
    {
        SSL_DEBUG_MSG( 1, ( "in_msglen (%d) < minlen (%d)",
                       ssl->in_msglen, ssl->transform_in->minlen ) );
        return( POLARSSL_ERR_SSL_INVALID_MAC );
    }

    if( ssl->transform_in->ivlen == 0 )
    {
#if defined(POLARSSL_ARC4_C)
        if( ssl->session_in->ciphersuite == TLS_RSA_WITH_RC4_128_MD5 ||
            ssl->session_in->ciphersuite == TLS_RSA_WITH_RC4_128_SHA )
        {
            arc4_crypt( (arc4_context *) ssl->transform_in->ctx_dec,
                    ssl->in_msglen, ssl->in_msg,
                    ssl->in_msg );
        } else
#endif
#if defined(POLARSSL_CIPHER_NULL_CIPHER)
        if( ssl->session_in->ciphersuite == TLS_RSA_WITH_NULL_MD5 ||
            ssl->session_in->ciphersuite == TLS_RSA_WITH_NULL_SHA ||
            ssl->session_in->ciphersuite == TLS_RSA_WITH_NULL_SHA256 )
        {
        } else
#endif
        return( POLARSSL_ERR_SSL_FEATURE_UNAVAILABLE );
    }
    else if( ssl->transform_in->ivlen == 12 )
    {
        unsigned char *dec_msg;
        unsigned char *dec_msg_result;
        size_t dec_msglen;
        unsigned char add_data[13];
        int ret = POLARSSL_ERR_SSL_FEATURE_UNAVAILABLE;

#if defined(POLARSSL_AES_C) && defined(POLARSSL_GCM_C)
        if( ssl->session_in->ciphersuite == TLS_RSA_WITH_AES_128_GCM_SHA256 ||
            ssl->session_in->ciphersuite == TLS_RSA_WITH_AES_256_GCM_SHA384 ||
            ssl->session_in->ciphersuite == TLS_DHE_RSA_WITH_AES_128_GCM_SHA256 ||
            ssl->session_in->ciphersuite == TLS_DHE_RSA_WITH_AES_256_GCM_SHA384 )
        {
            dec_msglen = ssl->in_msglen - ( ssl->transform_in->ivlen -
                                            ssl->transform_in->fixed_ivlen );
            dec_msglen -= 16;
            dec_msg = ssl->in_msg + ( ssl->transform_in->ivlen -
                                      ssl->transform_in->fixed_ivlen );
            dec_msg_result = ssl->in_msg;
            ssl->in_msglen = dec_msglen;

            memcpy( add_data, ssl->in_ctr, 8 );
            add_data[8]  = ssl->in_msgtype;
            add_data[9]  = ssl->major_ver;
            add_data[10] = ssl->minor_ver;
            add_data[11] = ( ssl->in_msglen >> 8 ) & 0xFF;
            add_data[12] = ssl->in_msglen & 0xFF;

            SSL_DEBUG_BUF( 4, "additional data used for AEAD",
                           add_data, 13 );

            memcpy( ssl->transform_in->iv_dec + ssl->transform_in->fixed_ivlen,
                    ssl->in_msg,
                    ssl->transform_in->ivlen - ssl->transform_in->fixed_ivlen );

            SSL_DEBUG_BUF( 4, "IV used", ssl->transform_in->iv_dec,
                                         ssl->transform_in->ivlen );
            SSL_DEBUG_BUF( 4, "TAG used", dec_msg + dec_msglen, 16 );

            memcpy( ssl->transform_in->iv_dec + ssl->transform_in->fixed_ivlen,
                    ssl->in_msg,
                    ssl->transform_in->ivlen - ssl->transform_in->fixed_ivlen );

            ret = gcm_auth_decrypt( (gcm_context *) ssl->transform_in->ctx_dec,
                                     dec_msglen,
                                     ssl->transform_in->iv_dec,
                                     ssl->transform_in->ivlen,
                                     add_data, 13,
                                     dec_msg + dec_msglen, 16,
                                     dec_msg, dec_msg_result );
            
            if( ret != 0 )
            {
                SSL_DEBUG_MSG( 1, ( "AEAD decrypt failed on validation (ret = -0x%02x)",
                                    -ret ) );

                return( POLARSSL_ERR_SSL_INVALID_MAC );
            }
        } else
#endif
        return( ret );
    }
    else
    {
        /*
         * Decrypt and check the padding
         */
        unsigned char *dec_msg;
        unsigned char *dec_msg_result;
        size_t dec_msglen;
        size_t minlen = 0;

        /*
         * Check immediate ciphertext sanity
         */
        if( ssl->in_msglen % ssl->transform_in->ivlen != 0 )
        {
            SSL_DEBUG_MSG( 1, ( "msglen (%d) %% ivlen (%d) != 0",
                           ssl->in_msglen, ssl->transform_in->ivlen ) );
            return( POLARSSL_ERR_SSL_INVALID_MAC );
        }

        if( ssl->minor_ver >= SSL_MINOR_VERSION_2 )
            minlen += ssl->transform_in->ivlen;

        if( ssl->in_msglen < minlen + ssl->transform_in->ivlen ||
            ssl->in_msglen < minlen + ssl->transform_in->maclen + 1 )
        {
            SSL_DEBUG_MSG( 1, ( "msglen (%d) < max( ivlen(%d), maclen (%d) + 1 ) ( + expl IV )",
                           ssl->in_msglen, ssl->transform_in->ivlen, ssl->transform_in->maclen ) );
            return( POLARSSL_ERR_SSL_INVALID_MAC );
        }

        dec_msglen = ssl->in_msglen;
        dec_msg = ssl->in_msg;
        dec_msg_result = ssl->in_msg;

        /*
         * Initialize for prepended IV for block cipher in TLS v1.1 and up
         */
        if( ssl->minor_ver >= SSL_MINOR_VERSION_2 )
        {
            dec_msg += ssl->transform_in->ivlen;
            dec_msglen -= ssl->transform_in->ivlen;
            ssl->in_msglen -= ssl->transform_in->ivlen;

            for( i = 0; i < ssl->transform_in->ivlen; i++ )
                ssl->transform_in->iv_dec[i] = ssl->in_msg[i];
        }

        switch( ssl->transform_in->ivlen )
        {
#if defined(POLARSSL_DES_C)
            case  8:
#if defined(POLARSSL_ENABLE_WEAK_CIPHERSUITES)
                if( ssl->session_in->ciphersuite == TLS_RSA_WITH_DES_CBC_SHA ||
                    ssl->session_in->ciphersuite == TLS_DHE_RSA_WITH_DES_CBC_SHA )
                {
                    des_crypt_cbc( (des_context *) ssl->transform_in->ctx_dec,
                                   DES_DECRYPT, dec_msglen,
                                   ssl->transform_in->iv_dec, dec_msg, dec_msg_result );
                }
                else
#endif
                    des3_crypt_cbc( (des3_context *) ssl->transform_in->ctx_dec,
                        DES_DECRYPT, dec_msglen,
                        ssl->transform_in->iv_dec, dec_msg, dec_msg_result );
                break;
#endif

            case 16:
#if defined(POLARSSL_AES_C)
        if ( ssl->session_in->ciphersuite == TLS_RSA_WITH_AES_128_CBC_SHA ||
             ssl->session_in->ciphersuite == TLS_DHE_RSA_WITH_AES_128_CBC_SHA ||
             ssl->session_in->ciphersuite == TLS_RSA_WITH_AES_256_CBC_SHA ||
             ssl->session_in->ciphersuite == TLS_DHE_RSA_WITH_AES_256_CBC_SHA ||
             ssl->session_in->ciphersuite == TLS_RSA_WITH_AES_128_CBC_SHA256 ||
             ssl->session_in->ciphersuite == TLS_RSA_WITH_AES_256_CBC_SHA256 ||
             ssl->session_in->ciphersuite == TLS_DHE_RSA_WITH_AES_128_CBC_SHA256 ||
             ssl->session_in->ciphersuite == TLS_DHE_RSA_WITH_AES_256_CBC_SHA256 )
        {
                    aes_crypt_cbc( (aes_context *) ssl->transform_in->ctx_dec,
                       AES_DECRYPT, dec_msglen,
                       ssl->transform_in->iv_dec, dec_msg, dec_msg_result );
                    break;
        }
#endif

#if defined(POLARSSL_CAMELLIA_C)
        if ( ssl->session_in->ciphersuite == TLS_RSA_WITH_CAMELLIA_128_CBC_SHA ||
             ssl->session_in->ciphersuite == TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA ||
             ssl->session_in->ciphersuite == TLS_RSA_WITH_CAMELLIA_256_CBC_SHA ||
             ssl->session_in->ciphersuite == TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA ||
             ssl->session_in->ciphersuite == TLS_RSA_WITH_CAMELLIA_128_CBC_SHA256 ||
             ssl->session_in->ciphersuite == TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA256 ||
             ssl->session_in->ciphersuite == TLS_RSA_WITH_CAMELLIA_256_CBC_SHA256 ||
             ssl->session_in->ciphersuite == TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA256 )
        {
                    camellia_crypt_cbc( (camellia_context *) ssl->transform_in->ctx_dec,
                       CAMELLIA_DECRYPT, dec_msglen,
                       ssl->transform_in->iv_dec, dec_msg, dec_msg_result );
                    break;
        }
#endif

            default:
                return( POLARSSL_ERR_SSL_FEATURE_UNAVAILABLE );
        }

        padlen = 1 + ssl->in_msg[ssl->in_msglen - 1];

        if( ssl->in_msglen < ssl->transform_in->maclen + padlen )
        {
#if defined(POLARSSL_SSL_DEBUG_ALL)
            SSL_DEBUG_MSG( 1, ( "msglen (%d) < maclen (%d) + padlen (%d)",
                        ssl->in_msglen, ssl->transform_in->maclen, padlen ) );
#endif
            padlen = 0;
            correct = 0;
        }

        if( ssl->minor_ver == SSL_MINOR_VERSION_0 )
        {
            if( padlen > ssl->transform_in->ivlen )
            {
#if defined(POLARSSL_SSL_DEBUG_ALL)
                SSL_DEBUG_MSG( 1, ( "bad padding length: is %d, "
                                    "should be no more than %d",
                               padlen, ssl->transform_in->ivlen ) );
#endif
                correct = 0;
            }
        }
        else
        {
            /*
             * TLSv1+: always check the padding up to the first failure
             * and fake check up to 256 bytes of padding
             */
            size_t pad_count = 0, fake_pad_count = 0;
            size_t padding_idx = ssl->in_msglen - padlen - 1;

            for( i = 1; i <= padlen; i++ )
                pad_count += ( ssl->in_msg[padding_idx + i] == padlen - 1 );

            for( ; i <= 256; i++ )
                fake_pad_count += ( ssl->in_msg[padding_idx + i] == padlen - 1 );

            correct &= ( pad_count == padlen ); /* Only 1 on correct padding */
            correct &= ( pad_count + fake_pad_count < 512 ); /* Always 1 */

#if defined(POLARSSL_SSL_DEBUG_ALL)
            if( padlen > 0 && correct == 0)
                SSL_DEBUG_MSG( 1, ( "bad padding byte detected" ) );
#endif
            padlen &= correct * 0x1FF;
        }
    }

    SSL_DEBUG_BUF( 4, "raw buffer after decryption",
                   ssl->in_msg, ssl->in_msglen );

    /*
     * Always compute the MAC (RFC4346, CBCTIME).
     */
    ssl->in_msglen -= ( ssl->transform_in->maclen + padlen );

    ssl->in_hdr[3] = (unsigned char)( ssl->in_msglen >> 8 );
    ssl->in_hdr[4] = (unsigned char)( ssl->in_msglen      );

    memcpy( tmp, ssl->in_msg + ssl->in_msglen, ssl->transform_in->maclen );

    if( ssl->minor_ver == SSL_MINOR_VERSION_0 )
    {
        if( ssl->transform_in->maclen == 16 )
             ssl_mac_md5( ssl->transform_in->mac_dec,
                          ssl->in_msg, ssl->in_msglen,
                          ssl->in_ctr, ssl->in_msgtype );
        else if( ssl->transform_in->maclen == 20 )
            ssl_mac_sha1( ssl->transform_in->mac_dec,
                          ssl->in_msg, ssl->in_msglen,
                          ssl->in_ctr, ssl->in_msgtype );
        else if( ssl->transform_in->maclen == 32 )
            ssl_mac_sha2( ssl->transform_in->mac_dec,
                          ssl->in_msg, ssl->in_msglen,
                          ssl->in_ctr, ssl->in_msgtype );
        else if( ssl->transform_in->maclen != 0 )
        {
            SSL_DEBUG_MSG( 1, ( "invalid MAC len: %d",
                                ssl->transform_in->maclen ) );
            return( POLARSSL_ERR_SSL_FEATURE_UNAVAILABLE );
        }
    }
    else
    {
        /*
         * Process MAC and always update for padlen afterwards to make
         * total time independent of padlen
         *
         * extra_run compensates MAC check for padlen 
         *
         * Known timing attacks:
         *  - Lucky Thirteen (http://www.isg.rhul.ac.uk/tls/TLStiming.pdf)
         *
         * We use ( ( Lx + 8 ) / 64 ) to handle 'negative Lx' values
         * correctly. (We round down instead of up, so -56 is the correct
         * value for our calculations instead of -55)
         */
        int j, extra_run = 0;
        extra_run = ( 13 + ssl->in_msglen + padlen + 8 ) / 64 -
                    ( 13 + ssl->in_msglen          + 8 ) / 64;

        extra_run &= correct * 0xFF;

        if( ssl->transform_in->maclen == 16 )
        {
            md5_context ctx;
            md5_hmac_starts( &ctx, ssl->transform_in->mac_dec, 16 );
            md5_hmac_update( &ctx, ssl->in_ctr,  ssl->in_msglen + 13 );
            md5_hmac_finish( &ctx, ssl->in_msg + ssl->in_msglen );

            for( j = 0; j < extra_run; j++ )
                md5_process( &ctx, ssl->in_msg ); 
        }
        else if( ssl->transform_in->maclen == 20 )
        {
            sha1_context ctx;
            sha1_hmac_starts( &ctx, ssl->transform_in->mac_dec, 20 );
            sha1_hmac_update( &ctx, ssl->in_ctr,  ssl->in_msglen + 13 );
            sha1_hmac_finish( &ctx, ssl->in_msg + ssl->in_msglen );

            for( j = 0; j < extra_run; j++ )
                sha1_process( &ctx, ssl->in_msg ); 
        }
        else if( ssl->transform_in->maclen == 32 )
        {
            sha2_context ctx;
            sha2_hmac_starts( &ctx, ssl->transform_in->mac_dec, 32, 0 );
            sha2_hmac_update( &ctx, ssl->in_ctr,  ssl->in_msglen + 13 );
            sha2_hmac_finish( &ctx, ssl->in_msg + ssl->in_msglen );

            for( j = 0; j < extra_run; j++ )
                sha2_process( &ctx, ssl->in_msg ); 
        }
        else if( ssl->transform_in->maclen != 0 )
        {
            SSL_DEBUG_MSG( 1, ( "invalid MAC len: %d",
                                ssl->transform_in->maclen ) );
            return( POLARSSL_ERR_SSL_FEATURE_UNAVAILABLE );
        }
    }

    SSL_DEBUG_BUF( 4, "message  mac", tmp, ssl->transform_in->maclen );
    SSL_DEBUG_BUF( 4, "computed mac", ssl->in_msg + ssl->in_msglen,
                   ssl->transform_in->maclen );

    if( memcmp( tmp, ssl->in_msg + ssl->in_msglen,
                     ssl->transform_in->maclen ) != 0 )
    {
#if defined(POLARSSL_SSL_DEBUG_ALL)
        SSL_DEBUG_MSG( 1, ( "message mac does not match" ) );
#endif
        correct = 0;
    }

    /*
     * Finally check the correct flag
     */
    if( correct == 0 )
        return( POLARSSL_ERR_SSL_INVALID_MAC );

    if( ssl->in_msglen == 0 )
    {
        ssl->nb_zero++;

        /*
         * Three or more empty messages may be a DoS attack
         * (excessive CPU consumption).
         */
        if( ssl->nb_zero > 3 )
        {
            SSL_DEBUG_MSG( 1, ( "received four consecutive empty "
                                "messages, possible DoS attack" ) );
            return( POLARSSL_ERR_SSL_INVALID_MAC );
        }
    }
    else
        ssl->nb_zero = 0;
            
    for( i = 8; i > 0; i-- )
        if( ++ssl->in_ctr[i - 1] != 0 )
            break;

    SSL_DEBUG_MSG( 2, ( "<= decrypt buf" ) );

    return( 0 );
}

#if defined(POLARSSL_ZLIB_SUPPORT)
/*
 * Compression/decompression functions
 */
static int ssl_compress_buf( ssl_context *ssl )
{
    int ret;
    unsigned char *msg_post = ssl->out_msg;
    size_t len_pre = ssl->out_msglen;
    unsigned char *msg_pre;

    SSL_DEBUG_MSG( 2, ( "=> compress buf" ) );

    msg_pre = (unsigned char*) malloc( len_pre );
    if( msg_pre == NULL )
    {
        SSL_DEBUG_MSG( 1, ( "malloc(%d bytes) failed", len_pre ) );
        return( POLARSSL_ERR_SSL_MALLOC_FAILED );
    }

    memcpy( msg_pre, ssl->out_msg, len_pre );

    SSL_DEBUG_MSG( 3, ( "before compression: msglen = %d, ",
                   ssl->out_msglen ) );

    SSL_DEBUG_BUF( 4, "before compression: output payload",
                   ssl->out_msg, ssl->out_msglen );

    ssl->transform_out->ctx_deflate.next_in = msg_pre;
    ssl->transform_out->ctx_deflate.avail_in = len_pre;
    ssl->transform_out->ctx_deflate.next_out = msg_post;
    ssl->transform_out->ctx_deflate.avail_out = SSL_BUFFER_LEN;

    ret = deflate( &ssl->transform_out->ctx_deflate, Z_SYNC_FLUSH );
    if( ret != Z_OK )
    {
        SSL_DEBUG_MSG( 1, ( "failed to perform compression (%d)", ret ) );
        return( POLARSSL_ERR_SSL_COMPRESSION_FAILED );
    }

    ssl->out_msglen = SSL_BUFFER_LEN - ssl->transform_out->ctx_deflate.avail_out;

    free( msg_pre );

    SSL_DEBUG_MSG( 3, ( "after compression: msglen = %d, ",
                   ssl->out_msglen ) );

    SSL_DEBUG_BUF( 4, "after compression: output payload",
                   ssl->out_msg, ssl->out_msglen );

    SSL_DEBUG_MSG( 2, ( "<= compress buf" ) );

    return( 0 );
}

static int ssl_decompress_buf( ssl_context *ssl )
{
    int ret;
    unsigned char *msg_post = ssl->in_msg;
    size_t len_pre = ssl->in_msglen;
    unsigned char *msg_pre;

    SSL_DEBUG_MSG( 2, ( "=> decompress buf" ) );

    msg_pre = (unsigned char*) malloc( len_pre );
    if( msg_pre == NULL )
    {
        SSL_DEBUG_MSG( 1, ( "malloc(%d bytes) failed", len_pre ) );
        return( POLARSSL_ERR_SSL_MALLOC_FAILED );
    }

    memcpy( msg_pre, ssl->in_msg, len_pre );

    SSL_DEBUG_MSG( 3, ( "before decompression: msglen = %d, ",
                   ssl->in_msglen ) );

    SSL_DEBUG_BUF( 4, "before decompression: input payload",
                   ssl->in_msg, ssl->in_msglen );

    ssl->transform_in->ctx_inflate.next_in = msg_pre;
    ssl->transform_in->ctx_inflate.avail_in = len_pre;
    ssl->transform_in->ctx_inflate.next_out = msg_post;
    ssl->transform_in->ctx_inflate.avail_out = SSL_MAX_CONTENT_LEN;

    ret = inflate( &ssl->transform_in->ctx_inflate, Z_SYNC_FLUSH );
    if( ret != Z_OK )
    {
        SSL_DEBUG_MSG( 1, ( "failed to perform decompression (%d)", ret ) );
        return( POLARSSL_ERR_SSL_COMPRESSION_FAILED );
    }

    ssl->in_msglen = SSL_MAX_CONTENT_LEN - ssl->transform_in->ctx_inflate.avail_out;

    free( msg_pre );

    SSL_DEBUG_MSG( 3, ( "after decompression: msglen = %d, ",
                   ssl->in_msglen ) );

    SSL_DEBUG_BUF( 4, "after decompression: input payload",
                   ssl->in_msg, ssl->in_msglen );

    SSL_DEBUG_MSG( 2, ( "<= decompress buf" ) );

    return( 0 );
}
#endif /* POLARSSL_ZLIB_SUPPORT */

/*
 * Fill the input message buffer
 */
int ssl_fetch_input( ssl_context *ssl, size_t nb_want )
{
    int ret;
    size_t len;

    SSL_DEBUG_MSG( 2, ( "=> fetch input" ) );

    while( ssl->in_left < nb_want )
    {
        len = nb_want - ssl->in_left;
        ret = ssl->f_recv( ssl->p_recv, ssl->in_hdr + ssl->in_left, len );

        SSL_DEBUG_MSG( 2, ( "in_left: %d, nb_want: %d",
                       ssl->in_left, nb_want ) );
        SSL_DEBUG_RET( 2, "ssl->f_recv", ret );

        if( ret == 0 )
            return( POLARSSL_ERR_SSL_CONN_EOF );

        if( ret < 0 )
            return( ret );

        ssl->in_left += ret;
    }

    SSL_DEBUG_MSG( 2, ( "<= fetch input" ) );

    return( 0 );
}

/*
 * Flush any data not yet written
 */
int ssl_flush_output( ssl_context *ssl )
{
    int ret;
    unsigned char *buf;

    SSL_DEBUG_MSG( 2, ( "=> flush output" ) );

    while( ssl->out_left > 0 )
    {
        SSL_DEBUG_MSG( 2, ( "message length: %d, out_left: %d",
                       5 + ssl->out_msglen, ssl->out_left ) );

        if( ssl->out_msglen < ssl->out_left )
        {
            size_t header_left = ssl->out_left - ssl->out_msglen;

            buf = ssl->out_hdr + 5 - header_left;
            ret = ssl->f_send( ssl->p_send, buf, header_left );
            
            SSL_DEBUG_RET( 2, "ssl->f_send (header)", ret );

            if( ret <= 0 )
                return( ret );

            ssl->out_left -= ret;
        }
        
        buf = ssl->out_msg + ssl->out_msglen - ssl->out_left;
        ret = ssl->f_send( ssl->p_send, buf, ssl->out_left );

        SSL_DEBUG_RET( 2, "ssl->f_send", ret );

        if( ret <= 0 )
            return( ret );

        ssl->out_left -= ret;
    }

    SSL_DEBUG_MSG( 2, ( "<= flush output" ) );

    return( 0 );
}

/*
 * Record layer functions
 */
int ssl_write_record( ssl_context *ssl )
{
    int ret, done = 0;
    size_t len = ssl->out_msglen;

    SSL_DEBUG_MSG( 2, ( "=> write record" ) );

    if( ssl->out_msgtype == SSL_MSG_HANDSHAKE )
    {
        ssl->out_msg[1] = (unsigned char)( ( len - 4 ) >> 16 );
        ssl->out_msg[2] = (unsigned char)( ( len - 4 ) >>  8 );
        ssl->out_msg[3] = (unsigned char)( ( len - 4 )       );

        ssl->handshake->update_checksum( ssl, ssl->out_msg, len );
    }

#if defined(POLARSSL_ZLIB_SUPPORT)
    if( ssl->transform_out != NULL &&
        ssl->session_out->compression == SSL_COMPRESS_DEFLATE )
    {
        if( ( ret = ssl_compress_buf( ssl ) ) != 0 )
        {
            SSL_DEBUG_RET( 1, "ssl_compress_buf", ret );
            return( ret );
        }

        len = ssl->out_msglen;
    }
#endif /*POLARSSL_ZLIB_SUPPORT */

#if defined(POLARSSL_SSL_HW_RECORD_ACCEL)
    if( ssl_hw_record_write != NULL)
    {
        SSL_DEBUG_MSG( 2, ( "going for ssl_hw_record_write()" ) );

        ret = ssl_hw_record_write( ssl );
        if( ret != 0 && ret != POLARSSL_ERR_SSL_HW_ACCEL_FALLTHROUGH )
        {
            SSL_DEBUG_RET( 1, "ssl_hw_record_write", ret );
            return POLARSSL_ERR_SSL_HW_ACCEL_FAILED;
        }
        done = 1;
    }
#endif
    if( !done )
    {
        ssl->out_hdr[0] = (unsigned char) ssl->out_msgtype;
        ssl->out_hdr[1] = (unsigned char) ssl->major_ver;
        ssl->out_hdr[2] = (unsigned char) ssl->minor_ver;
        ssl->out_hdr[3] = (unsigned char)( len >> 8 );
        ssl->out_hdr[4] = (unsigned char)( len      );

        if( ssl->transform_out != NULL )
        {
            if( ( ret = ssl_encrypt_buf( ssl ) ) != 0 )
            {
                SSL_DEBUG_RET( 1, "ssl_encrypt_buf", ret );
                return( ret );
            }

            len = ssl->out_msglen;
            ssl->out_hdr[3] = (unsigned char)( len >> 8 );
            ssl->out_hdr[4] = (unsigned char)( len      );
        }

        ssl->out_left = 5 + ssl->out_msglen;

        SSL_DEBUG_MSG( 3, ( "output record: msgtype = %d, "
                            "version = [%d:%d], msglen = %d",
                       ssl->out_hdr[0], ssl->out_hdr[1], ssl->out_hdr[2],
                     ( ssl->out_hdr[3] << 8 ) | ssl->out_hdr[4] ) );

        SSL_DEBUG_BUF( 4, "output record header sent to network",
                       ssl->out_hdr, 5 );
        SSL_DEBUG_BUF( 4, "output record sent to network",
                       ssl->out_hdr + 32, ssl->out_msglen );
    }

    if( ( ret = ssl_flush_output( ssl ) ) != 0 )
    {
        SSL_DEBUG_RET( 1, "ssl_flush_output", ret );
        return( ret );
    }

    SSL_DEBUG_MSG( 2, ( "<= write record" ) );

    return( 0 );
}

int ssl_read_record( ssl_context *ssl )
{
    int ret, done = 0;

    SSL_DEBUG_MSG( 2, ( "=> read record" ) );

    if( ssl->in_hslen != 0 &&
        ssl->in_hslen < ssl->in_msglen )
    {
        /*
         * Get next Handshake message in the current record
         */
        ssl->in_msglen -= ssl->in_hslen;

        memmove( ssl->in_msg, ssl->in_msg + ssl->in_hslen,
                 ssl->in_msglen );

        ssl->in_hslen  = 4;
        ssl->in_hslen += ( ssl->in_msg[2] << 8 ) | ssl->in_msg[3];

        SSL_DEBUG_MSG( 3, ( "handshake message: msglen ="
                            " %d, type = %d, hslen = %d",
                       ssl->in_msglen, ssl->in_msg[0], ssl->in_hslen ) );

        if( ssl->in_msglen < 4 || ssl->in_msg[1] != 0 )
        {
            SSL_DEBUG_MSG( 1, ( "bad handshake length" ) );
            return( POLARSSL_ERR_SSL_INVALID_RECORD );
        }

        if( ssl->in_msglen < ssl->in_hslen )
        {
            SSL_DEBUG_MSG( 1, ( "bad handshake length" ) );
            return( POLARSSL_ERR_SSL_INVALID_RECORD );
        }

        ssl->handshake->update_checksum( ssl, ssl->in_msg, ssl->in_hslen );

        return( 0 );
    }

    ssl->in_hslen = 0;

    /*
     * Read the record header and validate it
     */
    if( ( ret = ssl_fetch_input( ssl, 5 ) ) != 0 )
    {
        SSL_DEBUG_RET( 1, "ssl_fetch_input", ret );
        return( ret );
    }

    ssl->in_msgtype =  ssl->in_hdr[0];
    ssl->in_msglen = ( ssl->in_hdr[3] << 8 ) | ssl->in_hdr[4];

    SSL_DEBUG_MSG( 3, ( "input record: msgtype = %d, "
                        "version = [%d:%d], msglen = %d",
                     ssl->in_hdr[0], ssl->in_hdr[1], ssl->in_hdr[2],
                   ( ssl->in_hdr[3] << 8 ) | ssl->in_hdr[4] ) );

    if( ssl->in_hdr[1] != ssl->major_ver )
    {
        SSL_DEBUG_MSG( 1, ( "major version mismatch" ) );
        return( POLARSSL_ERR_SSL_INVALID_RECORD );
    }

    if( ssl->in_hdr[2] > ssl->max_minor_ver )
    {
        SSL_DEBUG_MSG( 1, ( "minor version mismatch" ) );
        return( POLARSSL_ERR_SSL_INVALID_RECORD );
    }

    /*
     * Make sure the message length is acceptable
     */
    if( ssl->transform_in == NULL )
    {
        if( ssl->in_msglen < 1 ||
            ssl->in_msglen > SSL_MAX_CONTENT_LEN )
        {
            SSL_DEBUG_MSG( 1, ( "bad message length" ) );
            return( POLARSSL_ERR_SSL_INVALID_RECORD );
        }
    }
    else
    {
        if( ssl->in_msglen < ssl->transform_in->minlen )
        {
            SSL_DEBUG_MSG( 1, ( "bad message length" ) );
            return( POLARSSL_ERR_SSL_INVALID_RECORD );
        }

        if( ssl->minor_ver == SSL_MINOR_VERSION_0 &&
            ssl->in_msglen > ssl->transform_in->minlen + SSL_MAX_CONTENT_LEN )
        {
            SSL_DEBUG_MSG( 1, ( "bad message length" ) );
            return( POLARSSL_ERR_SSL_INVALID_RECORD );
        }

        /*
         * TLS encrypted messages can have up to 256 bytes of padding
         */
        if( ssl->minor_ver >= SSL_MINOR_VERSION_1 &&
            ssl->in_msglen > ssl->transform_in->minlen + SSL_MAX_CONTENT_LEN + 256 )
        {
            SSL_DEBUG_MSG( 1, ( "bad message length" ) );
            return( POLARSSL_ERR_SSL_INVALID_RECORD );
        }
    }

    /*
     * Read and optionally decrypt the message contents
     */
    if( ( ret = ssl_fetch_input( ssl, 5 + ssl->in_msglen ) ) != 0 )
    {
        SSL_DEBUG_RET( 1, "ssl_fetch_input", ret );
        return( ret );
    }

    SSL_DEBUG_BUF( 4, "input record from network",
                   ssl->in_hdr, 5 + ssl->in_msglen );

#if defined(POLARSSL_SSL_HW_RECORD_ACCEL)
    if( ssl_hw_record_read != NULL)
    {
        SSL_DEBUG_MSG( 2, ( "going for ssl_hw_record_read()" ) );

        ret = ssl_hw_record_read( ssl );
        if( ret != 0 && ret != POLARSSL_ERR_SSL_HW_ACCEL_FALLTHROUGH )
        {
            SSL_DEBUG_RET( 1, "ssl_hw_record_read", ret );
            return POLARSSL_ERR_SSL_HW_ACCEL_FAILED;
        }
        done = 1;
    }
#endif
    if( !done && ssl->transform_in != NULL )
    {
        if( ( ret = ssl_decrypt_buf( ssl ) ) != 0 )
        {
#if defined(POLARSSL_SSL_ALERT_MESSAGES)
            if( ret == POLARSSL_ERR_SSL_INVALID_MAC )
            {
                ssl_send_alert_message( ssl,
                                        SSL_ALERT_LEVEL_FATAL,
                                        SSL_ALERT_MSG_BAD_RECORD_MAC );
            }
#endif
            SSL_DEBUG_RET( 1, "ssl_decrypt_buf", ret );
            return( ret );
        }

        SSL_DEBUG_BUF( 4, "input payload after decrypt",
                       ssl->in_msg, ssl->in_msglen );

        if( ssl->in_msglen > SSL_MAX_CONTENT_LEN )
        {
            SSL_DEBUG_MSG( 1, ( "bad message length" ) );
            return( POLARSSL_ERR_SSL_INVALID_RECORD );
        }
    }

#if defined(POLARSSL_ZLIB_SUPPORT)
    if( ssl->transform_in != NULL &&
        ssl->session_in->compression == SSL_COMPRESS_DEFLATE )
    {
        if( ( ret = ssl_decompress_buf( ssl ) ) != 0 )
        {
            SSL_DEBUG_RET( 1, "ssl_decompress_buf", ret );
            return( ret );
        }

        ssl->in_hdr[3] = (unsigned char)( ssl->in_msglen >> 8 );
        ssl->in_hdr[4] = (unsigned char)( ssl->in_msglen      );
    }
#endif /* POLARSSL_ZLIB_SUPPORT */

    if( ssl->in_msgtype != SSL_MSG_HANDSHAKE &&
        ssl->in_msgtype != SSL_MSG_ALERT &&
        ssl->in_msgtype != SSL_MSG_CHANGE_CIPHER_SPEC &&
        ssl->in_msgtype != SSL_MSG_APPLICATION_DATA )
    {
        SSL_DEBUG_MSG( 1, ( "unknown record type" ) );

        if( ( ret = ssl_send_alert_message( ssl,
                        SSL_ALERT_LEVEL_FATAL,
                        SSL_ALERT_MSG_UNEXPECTED_MESSAGE ) ) != 0 )
        {
            return( ret );
        }

        return( POLARSSL_ERR_SSL_INVALID_RECORD );
    }

    if( ssl->in_msgtype == SSL_MSG_HANDSHAKE )
    {
        ssl->in_hslen  = 4;
        ssl->in_hslen += ( ssl->in_msg[2] << 8 ) | ssl->in_msg[3];

        SSL_DEBUG_MSG( 3, ( "handshake message: msglen ="
                            " %d, type = %d, hslen = %d",
                       ssl->in_msglen, ssl->in_msg[0], ssl->in_hslen ) );

        /*
         * Additional checks to validate the handshake header
         */
        if( ssl->in_msglen < 4 || ssl->in_msg[1] != 0 )
        {
            SSL_DEBUG_MSG( 1, ( "bad handshake length" ) );
            return( POLARSSL_ERR_SSL_INVALID_RECORD );
        }

        if( ssl->in_msglen < ssl->in_hslen )
        {
            SSL_DEBUG_MSG( 1, ( "bad handshake length" ) );
            return( POLARSSL_ERR_SSL_INVALID_RECORD );
        }

        if( ssl->state != SSL_HANDSHAKE_OVER )
            ssl->handshake->update_checksum( ssl, ssl->in_msg, ssl->in_hslen );
    }

    if( ssl->in_msgtype == SSL_MSG_ALERT )
    {
        SSL_DEBUG_MSG( 2, ( "got an alert message, type: [%d:%d]",
                       ssl->in_msg[0], ssl->in_msg[1] ) );

        /*
         * Ignore non-fatal alerts, except close_notify
         */
        if( ssl->in_msg[0] == SSL_ALERT_LEVEL_FATAL )
        {
            SSL_DEBUG_MSG( 1, ( "is a fatal alert message (msg %d)",
                           ssl->in_msg[1] ) );
            /**
             * Subtract from error code as ssl->in_msg[1] is 7-bit positive
             * error identifier.
             */
            return( POLARSSL_ERR_SSL_FATAL_ALERT_MESSAGE );
        }

        if( ssl->in_msg[0] == SSL_ALERT_LEVEL_WARNING &&
            ssl->in_msg[1] == SSL_ALERT_MSG_CLOSE_NOTIFY )
        {
            SSL_DEBUG_MSG( 2, ( "is a close notify message" ) );
            return( POLARSSL_ERR_SSL_PEER_CLOSE_NOTIFY );
        }
    }

    ssl->in_left = 0;

    SSL_DEBUG_MSG( 2, ( "<= read record" ) );

    return( 0 );
}

int ssl_send_fatal_handshake_failure( ssl_context *ssl )
{
    int ret;

    if( ( ret = ssl_send_alert_message( ssl,
                    SSL_ALERT_LEVEL_FATAL,
                    SSL_ALERT_MSG_HANDSHAKE_FAILURE ) ) != 0 )
    {
        return( ret );
    }

    return( 0 );
}

int ssl_send_alert_message( ssl_context *ssl,
                            unsigned char level,
                            unsigned char message )
{
    int ret;

    SSL_DEBUG_MSG( 2, ( "=> send alert message" ) );

    ssl->out_msgtype = SSL_MSG_ALERT;
    ssl->out_msglen = 2;
    ssl->out_msg[0] = level;
    ssl->out_msg[1] = message;

    if( ( ret = ssl_write_record( ssl ) ) != 0 )
    {
        SSL_DEBUG_RET( 1, "ssl_write_record", ret );
        return( ret );
    }

    SSL_DEBUG_MSG( 2, ( "<= send alert message" ) );

    return( 0 );
}

/*
 * Handshake functions
 */
int ssl_write_certificate( ssl_context *ssl )
{
    int ret;
    size_t i, n;
    const x509_cert *crt;

    SSL_DEBUG_MSG( 2, ( "=> write certificate" ) );

    if( ssl->endpoint == SSL_IS_CLIENT )
    {
        if( ssl->client_auth == 0 )
        {
            SSL_DEBUG_MSG( 2, ( "<= skip write certificate" ) );
            ssl->state++;
            return( 0 );
        }

        /*
         * If using SSLv3 and got no cert, send an Alert message
         * (otherwise an empty Certificate message will be sent).
         */
        if( ssl->own_cert  == NULL &&
            ssl->minor_ver == SSL_MINOR_VERSION_0 )
        {
            ssl->out_msglen  = 2;
            ssl->out_msgtype = SSL_MSG_ALERT;
            ssl->out_msg[0]  = SSL_ALERT_LEVEL_WARNING;
            ssl->out_msg[1]  = SSL_ALERT_MSG_NO_CERT;

            SSL_DEBUG_MSG( 2, ( "got no certificate to send" ) );
            goto write_msg;
        }
    }
    else /* SSL_IS_SERVER */
    {
        if( ssl->own_cert == NULL )
        {
            SSL_DEBUG_MSG( 1, ( "got no certificate to send" ) );
            return( POLARSSL_ERR_SSL_CERTIFICATE_REQUIRED );
        }
    }

    SSL_DEBUG_CRT( 3, "own certificate", ssl->own_cert );

    /*
     *     0  .  0    handshake type
     *     1  .  3    handshake length
     *     4  .  6    length of all certs
     *     7  .  9    length of cert. 1
     *    10  . n-1   peer certificate
     *     n  . n+2   length of cert. 2
     *    n+3 . ...   upper level cert, etc.
     */
    i = 7;
    crt = ssl->own_cert;

    while( crt != NULL )
    {
        n = crt->raw.len;
        if( i + 3 + n > SSL_MAX_CONTENT_LEN )
        {
            SSL_DEBUG_MSG( 1, ( "certificate too large, %d > %d",
                           i + 3 + n, SSL_MAX_CONTENT_LEN ) );
            return( POLARSSL_ERR_SSL_CERTIFICATE_TOO_LARGE );
        }

        ssl->out_msg[i    ] = (unsigned char)( n >> 16 );
        ssl->out_msg[i + 1] = (unsigned char)( n >>  8 );
        ssl->out_msg[i + 2] = (unsigned char)( n       );

        i += 3; memcpy( ssl->out_msg + i, crt->raw.p, n );
        i += n; crt = crt->next;
    }

    ssl->out_msg[4]  = (unsigned char)( ( i - 7 ) >> 16 );
    ssl->out_msg[5]  = (unsigned char)( ( i - 7 ) >>  8 );
    ssl->out_msg[6]  = (unsigned char)( ( i - 7 )       );

    ssl->out_msglen  = i;
    ssl->out_msgtype = SSL_MSG_HANDSHAKE;
    ssl->out_msg[0]  = SSL_HS_CERTIFICATE;

write_msg:

    ssl->state++;

    if( ( ret = ssl_write_record( ssl ) ) != 0 )
    {
        SSL_DEBUG_RET( 1, "ssl_write_record", ret );
        return( ret );
    }

    SSL_DEBUG_MSG( 2, ( "<= write certificate" ) );

    return( 0 );
}

int ssl_parse_certificate( ssl_context *ssl )
{
    int ret;
    size_t i, n;

    SSL_DEBUG_MSG( 2, ( "=> parse certificate" ) );

    if( ssl->endpoint == SSL_IS_SERVER &&
        ssl->authmode == SSL_VERIFY_NONE )
    {
        ssl->verify_result = BADCERT_SKIP_VERIFY;
        SSL_DEBUG_MSG( 2, ( "<= skip parse certificate" ) );
        ssl->state++;
        return( 0 );
    }

    if( ( ret = ssl_read_record( ssl ) ) != 0 )
    {
        SSL_DEBUG_RET( 1, "ssl_read_record", ret );
        return( ret );
    }

    ssl->state++;

    /*
     * Check if the client sent an empty certificate
     */
    if( ssl->endpoint  == SSL_IS_SERVER &&
        ssl->minor_ver == SSL_MINOR_VERSION_0 )
    {
        if( ssl->in_msglen  == 2                        &&
            ssl->in_msgtype == SSL_MSG_ALERT            &&
            ssl->in_msg[0]  == SSL_ALERT_LEVEL_WARNING  &&
            ssl->in_msg[1]  == SSL_ALERT_MSG_NO_CERT )
        {
            SSL_DEBUG_MSG( 1, ( "SSLv3 client has no certificate" ) );

            ssl->verify_result = BADCERT_MISSING;
            if( ssl->authmode == SSL_VERIFY_OPTIONAL )
                return( 0 );
            else
                return( POLARSSL_ERR_SSL_NO_CLIENT_CERTIFICATE );
        }
    }

    if( ssl->endpoint  == SSL_IS_SERVER &&
        ssl->minor_ver != SSL_MINOR_VERSION_0 )
    {
        if( ssl->in_hslen   == 7                    &&
            ssl->in_msgtype == SSL_MSG_HANDSHAKE    &&
            ssl->in_msg[0]  == SSL_HS_CERTIFICATE   &&
            memcmp( ssl->in_msg + 4, "\0\0\0", 3 ) == 0 )
        {
            SSL_DEBUG_MSG( 1, ( "TLSv1 client has no certificate" ) );

            ssl->verify_result = BADCERT_MISSING;
            if( ssl->authmode == SSL_VERIFY_REQUIRED )
                return( POLARSSL_ERR_SSL_NO_CLIENT_CERTIFICATE );
            else
                return( 0 );
        }
    }

    if( ssl->in_msgtype != SSL_MSG_HANDSHAKE )
    {
        SSL_DEBUG_MSG( 1, ( "bad certificate message" ) );
        return( POLARSSL_ERR_SSL_UNEXPECTED_MESSAGE );
    }

    if( ssl->in_msg[0] != SSL_HS_CERTIFICATE || ssl->in_hslen < 10 )
    {
        SSL_DEBUG_MSG( 1, ( "bad certificate message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_CERTIFICATE );
    }

    /*
     * Same message structure as in ssl_write_certificate()
     */
    n = ( ssl->in_msg[5] << 8 ) | ssl->in_msg[6];

    if( ssl->in_msg[4] != 0 || ssl->in_hslen != 7 + n )
    {
        SSL_DEBUG_MSG( 1, ( "bad certificate message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_CERTIFICATE );
    }

    if( ( ssl->session_negotiate->peer_cert = (x509_cert *) malloc(
                    sizeof( x509_cert ) ) ) == NULL )
    {
        SSL_DEBUG_MSG( 1, ( "malloc(%d bytes) failed",
                       sizeof( x509_cert ) ) );
        return( POLARSSL_ERR_SSL_MALLOC_FAILED );
    }

    memset( ssl->session_negotiate->peer_cert, 0, sizeof( x509_cert ) );

    i = 7;

    while( i < ssl->in_hslen )
    {
        if( ssl->in_msg[i] != 0 )
        {
            SSL_DEBUG_MSG( 1, ( "bad certificate message" ) );
            return( POLARSSL_ERR_SSL_BAD_HS_CERTIFICATE );
        }

        n = ( (unsigned int) ssl->in_msg[i + 1] << 8 )
            | (unsigned int) ssl->in_msg[i + 2];
        i += 3;

        if( n < 128 || i + n > ssl->in_hslen )
        {
            SSL_DEBUG_MSG( 1, ( "bad certificate message" ) );
            return( POLARSSL_ERR_SSL_BAD_HS_CERTIFICATE );
        }

        ret = x509parse_crt_der( ssl->session_negotiate->peer_cert,
                                 ssl->in_msg + i, n );
        if( ret != 0 )
        {
            SSL_DEBUG_RET( 1, " x509parse_crt", ret );
            return( ret );
        }

        i += n;
    }

    SSL_DEBUG_CRT( 3, "peer certificate", ssl->session_negotiate->peer_cert );

    if( ssl->authmode != SSL_VERIFY_NONE )
    {
        if( ssl->ca_chain == NULL )
        {
            SSL_DEBUG_MSG( 1, ( "got no CA chain" ) );
            return( POLARSSL_ERR_SSL_CA_CHAIN_REQUIRED );
        }

        ret = x509parse_verify( ssl->session_negotiate->peer_cert,
                                ssl->ca_chain, ssl->ca_crl,
                                ssl->peer_cn,  &ssl->verify_result,
                                ssl->f_vrfy, ssl->p_vrfy );

        if( ret != 0 )
            SSL_DEBUG_RET( 1, "x509_verify_cert", ret );

        if( ssl->authmode != SSL_VERIFY_REQUIRED )
            ret = 0;
    }

    SSL_DEBUG_MSG( 2, ( "<= parse certificate" ) );

    return( ret );
}

int ssl_write_change_cipher_spec( ssl_context *ssl )
{
    int ret;

    SSL_DEBUG_MSG( 2, ( "=> write change cipher spec" ) );

    ssl->out_msgtype = SSL_MSG_CHANGE_CIPHER_SPEC;
    ssl->out_msglen  = 1;
    ssl->out_msg[0]  = 1;

    ssl->state++;

    if( ( ret = ssl_write_record( ssl ) ) != 0 )
    {
        SSL_DEBUG_RET( 1, "ssl_write_record", ret );
        return( ret );
    }

    SSL_DEBUG_MSG( 2, ( "<= write change cipher spec" ) );

    return( 0 );
}

int ssl_parse_change_cipher_spec( ssl_context *ssl )
{
    int ret;

    SSL_DEBUG_MSG( 2, ( "=> parse change cipher spec" ) );

    if( ( ret = ssl_read_record( ssl ) ) != 0 )
    {
        SSL_DEBUG_RET( 1, "ssl_read_record", ret );
        return( ret );
    }

    if( ssl->in_msgtype != SSL_MSG_CHANGE_CIPHER_SPEC )
    {
        SSL_DEBUG_MSG( 1, ( "bad change cipher spec message" ) );
        return( POLARSSL_ERR_SSL_UNEXPECTED_MESSAGE );
    }

    if( ssl->in_msglen != 1 || ssl->in_msg[0] != 1 )
    {
        SSL_DEBUG_MSG( 1, ( "bad change cipher spec message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_CHANGE_CIPHER_SPEC );
    }

    ssl->state++;

    SSL_DEBUG_MSG( 2, ( "<= parse change cipher spec" ) );

    return( 0 );
}

void ssl_optimize_checksum( ssl_context *ssl, int ciphersuite )
{
#if !defined(POLARSSL_SHA4_C)
    ((void) ciphersuite);
#endif

    if( ssl->minor_ver < SSL_MINOR_VERSION_3 )
        ssl->handshake->update_checksum = ssl_update_checksum_md5sha1;
#if defined(POLARSSL_SHA4_C)
    else if ( ciphersuite == TLS_RSA_WITH_AES_256_GCM_SHA384 ||
              ciphersuite == TLS_DHE_RSA_WITH_AES_256_GCM_SHA384 )
    {
        ssl->handshake->update_checksum = ssl_update_checksum_sha384;
    }
#endif
    else
        ssl->handshake->update_checksum = ssl_update_checksum_sha256;
}
    
static void ssl_update_checksum_start( ssl_context *ssl, unsigned char *buf,
                                       size_t len )
{
     md5_update( &ssl->handshake->fin_md5 , buf, len );
    sha1_update( &ssl->handshake->fin_sha1, buf, len );
    sha2_update( &ssl->handshake->fin_sha2, buf, len );
#if defined(POLARSSL_SHA4_C)
    sha4_update( &ssl->handshake->fin_sha4, buf, len );
#endif
}

static void ssl_update_checksum_md5sha1( ssl_context *ssl, unsigned char *buf,
                                         size_t len )
{
     md5_update( &ssl->handshake->fin_md5 , buf, len );
    sha1_update( &ssl->handshake->fin_sha1, buf, len );
}

static void ssl_update_checksum_sha256( ssl_context *ssl, unsigned char *buf,
                                        size_t len )
{
    sha2_update( &ssl->handshake->fin_sha2, buf, len );
}

#if defined(POLARSSL_SHA4_C)
static void ssl_update_checksum_sha384( ssl_context *ssl, unsigned char *buf,
                                        size_t len )
{
    sha4_update( &ssl->handshake->fin_sha4, buf, len );
}
#endif

static void ssl_calc_finished_ssl(
                ssl_context *ssl, unsigned char *buf, int from )
{
    const char *sender;
    md5_context  md5;
    sha1_context sha1;

    unsigned char padbuf[48];
    unsigned char md5sum[16];
    unsigned char sha1sum[20];

    ssl_session *session = ssl->session_negotiate;
    if( !session )
        session = ssl->session;

    SSL_DEBUG_MSG( 2, ( "=> calc  finished ssl" ) );

    memcpy( &md5 , &ssl->handshake->fin_md5 , sizeof(md5_context)  );
    memcpy( &sha1, &ssl->handshake->fin_sha1, sizeof(sha1_context) );

    /*
     * SSLv3:
     *   hash =
     *      MD5( master + pad2 +
     *          MD5( handshake + sender + master + pad1 ) )
     *   + SHA1( master + pad2 +
     *         SHA1( handshake + sender + master + pad1 ) )
     */

#if !defined(POLARSSL_MD5_ALT)
    SSL_DEBUG_BUF( 4, "finished  md5 state", (unsigned char *)
                    md5.state, sizeof(  md5.state ) );
#endif

#if !defined(POLARSSL_SHA1_ALT)
    SSL_DEBUG_BUF( 4, "finished sha1 state", (unsigned char *)
                   sha1.state, sizeof( sha1.state ) );
#endif

    sender = ( from == SSL_IS_CLIENT ) ? "CLNT"
                                       : "SRVR";

    memset( padbuf, 0x36, 48 );

    md5_update( &md5, (const unsigned char *) sender, 4 );
    md5_update( &md5, session->master, 48 );
    md5_update( &md5, padbuf, 48 );
    md5_finish( &md5, md5sum );

    sha1_update( &sha1, (const unsigned char *) sender, 4 );
    sha1_update( &sha1, session->master, 48 );
    sha1_update( &sha1, padbuf, 40 );
    sha1_finish( &sha1, sha1sum );

    memset( padbuf, 0x5C, 48 );

    md5_starts( &md5 );
    md5_update( &md5, session->master, 48 );
    md5_update( &md5, padbuf, 48 );
    md5_update( &md5, md5sum, 16 );
    md5_finish( &md5, buf );

    sha1_starts( &sha1 );
    sha1_update( &sha1, session->master, 48 );
    sha1_update( &sha1, padbuf , 40 );
    sha1_update( &sha1, sha1sum, 20 );
    sha1_finish( &sha1, buf + 16 );

    SSL_DEBUG_BUF( 3, "calc finished result", buf, 36 );

    memset(  &md5, 0, sizeof(  md5_context ) );
    memset( &sha1, 0, sizeof( sha1_context ) );

    memset(  padbuf, 0, sizeof(  padbuf ) );
    memset(  md5sum, 0, sizeof(  md5sum ) );
    memset( sha1sum, 0, sizeof( sha1sum ) );

    SSL_DEBUG_MSG( 2, ( "<= calc  finished" ) );
}

static void ssl_calc_finished_tls(
                ssl_context *ssl, unsigned char *buf, int from )
{
    int len = 12;
    const char *sender;
    md5_context  md5;
    sha1_context sha1;
    unsigned char padbuf[36];

    ssl_session *session = ssl->session_negotiate;
    if( !session )
        session = ssl->session;

    SSL_DEBUG_MSG( 2, ( "=> calc  finished tls" ) );

    memcpy( &md5 , &ssl->handshake->fin_md5 , sizeof(md5_context)  );
    memcpy( &sha1, &ssl->handshake->fin_sha1, sizeof(sha1_context) );

    /*
     * TLSv1:
     *   hash = PRF( master, finished_label,
     *               MD5( handshake ) + SHA1( handshake ) )[0..11]
     */

#if !defined(POLARSSL_MD5_ALT)
    SSL_DEBUG_BUF( 4, "finished  md5 state", (unsigned char *)
                    md5.state, sizeof(  md5.state ) );
#endif

#if !defined(POLARSSL_SHA1_ALT)
    SSL_DEBUG_BUF( 4, "finished sha1 state", (unsigned char *)
                   sha1.state, sizeof( sha1.state ) );
#endif

    sender = ( from == SSL_IS_CLIENT )
             ? "client finished"
             : "server finished";

    md5_finish(  &md5, padbuf );
    sha1_finish( &sha1, padbuf + 16 );

    ssl->handshake->tls_prf( session->master, 48, (char *) sender,
                             padbuf, 36, buf, len );

    SSL_DEBUG_BUF( 3, "calc finished result", buf, len );

    memset(  &md5, 0, sizeof(  md5_context ) );
    memset( &sha1, 0, sizeof( sha1_context ) );

    memset(  padbuf, 0, sizeof(  padbuf ) );

    SSL_DEBUG_MSG( 2, ( "<= calc  finished" ) );
}

static void ssl_calc_finished_tls_sha256(
                ssl_context *ssl, unsigned char *buf, int from )
{
    int len = 12;
    const char *sender;
    sha2_context sha2;
    unsigned char padbuf[32];

    ssl_session *session = ssl->session_negotiate;
    if( !session )
        session = ssl->session;

    SSL_DEBUG_MSG( 2, ( "=> calc  finished tls sha256" ) );

    memcpy( &sha2, &ssl->handshake->fin_sha2, sizeof(sha2_context) );

    /*
     * TLSv1.2:
     *   hash = PRF( master, finished_label,
     *               Hash( handshake ) )[0.11]
     */

#if !defined(POLARSSL_SHA2_ALT)
    SSL_DEBUG_BUF( 4, "finished sha2 state", (unsigned char *)
                   sha2.state, sizeof( sha2.state ) );
#endif

    sender = ( from == SSL_IS_CLIENT )
             ? "client finished"
             : "server finished";

    sha2_finish( &sha2, padbuf );

    ssl->handshake->tls_prf( session->master, 48, (char *) sender,
                             padbuf, 32, buf, len );

    SSL_DEBUG_BUF( 3, "calc finished result", buf, len );

    memset( &sha2, 0, sizeof( sha2_context ) );

    memset(  padbuf, 0, sizeof(  padbuf ) );

    SSL_DEBUG_MSG( 2, ( "<= calc  finished" ) );
}

#if defined(POLARSSL_SHA4_C)
static void ssl_calc_finished_tls_sha384(
                ssl_context *ssl, unsigned char *buf, int from )
{
    int len = 12;
    const char *sender;
    sha4_context sha4;
    unsigned char padbuf[48];

    ssl_session *session = ssl->session_negotiate;
    if( !session )
        session = ssl->session;

    SSL_DEBUG_MSG( 2, ( "=> calc  finished tls sha384" ) );

    memcpy( &sha4, &ssl->handshake->fin_sha4, sizeof(sha4_context) );

    /*
     * TLSv1.2:
     *   hash = PRF( master, finished_label,
     *               Hash( handshake ) )[0.11]
     */

#if !defined(POLARSSL_SHA4_ALT)
    SSL_DEBUG_BUF( 4, "finished sha4 state", (unsigned char *)
                   sha4.state, sizeof( sha4.state ) );
#endif

    sender = ( from == SSL_IS_CLIENT )
             ? "client finished"
             : "server finished";

    sha4_finish( &sha4, padbuf );

    ssl->handshake->tls_prf( session->master, 48, (char *) sender,
                             padbuf, 48, buf, len );

    SSL_DEBUG_BUF( 3, "calc finished result", buf, len );

    memset( &sha4, 0, sizeof( sha4_context ) );

    memset(  padbuf, 0, sizeof(  padbuf ) );

    SSL_DEBUG_MSG( 2, ( "<= calc  finished" ) );
}
#endif

void ssl_handshake_wrapup( ssl_context *ssl )
{
    SSL_DEBUG_MSG( 3, ( "=> handshake wrapup" ) );

    /*
     * Free our handshake params
     */
    ssl_handshake_free( ssl->handshake );
    free( ssl->handshake );
    ssl->handshake = NULL;

    /*
     * Switch in our now active transform context
     */
    if( ssl->transform )
    {
        ssl_transform_free( ssl->transform );
        free( ssl->transform );
    }
    ssl->transform = ssl->transform_negotiate;
    ssl->transform_negotiate = NULL;

    if( ssl->session )
    {
        ssl_session_free( ssl->session );
        free( ssl->session );
    }
    ssl->session = ssl->session_negotiate;
    ssl->session_negotiate = NULL;

    /*
     * Add cache entry
     */
    if( ssl->f_set_cache != NULL )
        if( ssl->f_set_cache( ssl->p_set_cache, ssl->session ) != 0 )
            SSL_DEBUG_MSG( 1, ( "cache did not store session" ) );

    ssl->state++;

    SSL_DEBUG_MSG( 3, ( "<= handshake wrapup" ) );
}

int ssl_write_finished( ssl_context *ssl )
{
    int ret, hash_len;

    SSL_DEBUG_MSG( 2, ( "=> write finished" ) );

    ssl->handshake->calc_finished( ssl, ssl->out_msg + 4, ssl->endpoint );

    // TODO TLS/1.2 Hash length is determined by cipher suite (Page 63)
    hash_len = ( ssl->minor_ver == SSL_MINOR_VERSION_0 ) ? 36 : 12;

    ssl->verify_data_len = hash_len;
    memcpy( ssl->own_verify_data, ssl->out_msg + 4, hash_len );

    ssl->out_msglen  = 4 + hash_len;
    ssl->out_msgtype = SSL_MSG_HANDSHAKE;
    ssl->out_msg[0]  = SSL_HS_FINISHED;

    /*
     * In case of session resuming, invert the client and server
     * ChangeCipherSpec messages order.
     */
    if( ssl->handshake->resume != 0 )
    {
        if( ssl->endpoint == SSL_IS_CLIENT )
            ssl->state = SSL_HANDSHAKE_WRAPUP;
        else
            ssl->state = SSL_CLIENT_CHANGE_CIPHER_SPEC;
    }
    else
        ssl->state++;

    /*
     * Switch to our negotiated transform and session parameters for outbound data.
     */
    SSL_DEBUG_MSG( 3, ( "switching to new transform spec for outbound data" ) );
    ssl->transform_out = ssl->transform_negotiate;
    ssl->session_out = ssl->session_negotiate;
    memset( ssl->out_ctr, 0, 8 );

    if( ( ret = ssl_write_record( ssl ) ) != 0 )
    {
        SSL_DEBUG_RET( 1, "ssl_write_record", ret );
        return( ret );
    }

    SSL_DEBUG_MSG( 2, ( "<= write finished" ) );

    return( 0 );
}

int ssl_parse_finished( ssl_context *ssl )
{
    int ret;
    unsigned int hash_len;
    unsigned char buf[36];

    SSL_DEBUG_MSG( 2, ( "=> parse finished" ) );

    ssl->handshake->calc_finished( ssl, buf, ssl->endpoint ^ 1 );

    /*
     * Switch to our negotiated transform and session parameters for inbound data.
     */
    SSL_DEBUG_MSG( 3, ( "switching to new transform spec for inbound data" ) );
    ssl->transform_in = ssl->transform_negotiate;
    ssl->session_in = ssl->session_negotiate;
    memset( ssl->in_ctr, 0, 8 );

    if( ( ret = ssl_read_record( ssl ) ) != 0 )
    {
        SSL_DEBUG_RET( 1, "ssl_read_record", ret );
        return( ret );
    }

    if( ssl->in_msgtype != SSL_MSG_HANDSHAKE )
    {
        SSL_DEBUG_MSG( 1, ( "bad finished message" ) );
        return( POLARSSL_ERR_SSL_UNEXPECTED_MESSAGE );
    }

    // TODO TLS/1.2 Hash length is determined by cipher suite (Page 63)
    hash_len = ( ssl->minor_ver == SSL_MINOR_VERSION_0 ) ? 36 : 12;

    if( ssl->in_msg[0] != SSL_HS_FINISHED ||
        ssl->in_hslen  != 4 + hash_len )
    {
        SSL_DEBUG_MSG( 1, ( "bad finished message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_FINISHED );
    }

    if( memcmp( ssl->in_msg + 4, buf, hash_len ) != 0 )
    {
        SSL_DEBUG_MSG( 1, ( "bad finished message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_FINISHED );
    }

    ssl->verify_data_len = hash_len;
    memcpy( ssl->peer_verify_data, buf, hash_len );

    if( ssl->handshake->resume != 0 )
    {
        if( ssl->endpoint == SSL_IS_CLIENT )
            ssl->state = SSL_CLIENT_CHANGE_CIPHER_SPEC;

        if( ssl->endpoint == SSL_IS_SERVER )
            ssl->state = SSL_HANDSHAKE_WRAPUP;
    }
    else
        ssl->state++;

    SSL_DEBUG_MSG( 2, ( "<= parse finished" ) );

    return( 0 );
}

int ssl_handshake_init( ssl_context *ssl )
{
    if( ssl->transform_negotiate )
        ssl_transform_free( ssl->transform_negotiate );
    else
        ssl->transform_negotiate = malloc( sizeof(ssl_transform) );

    if( ssl->session_negotiate )
        ssl_session_free( ssl->session_negotiate );
    else
        ssl->session_negotiate = malloc( sizeof(ssl_session) );

    if( ssl->handshake )
        ssl_handshake_free( ssl->handshake );
    else
        ssl->handshake = malloc( sizeof(ssl_handshake_params) );

    if( ssl->handshake == NULL ||
        ssl->transform_negotiate == NULL ||
        ssl->session_negotiate == NULL )
    {
        SSL_DEBUG_MSG( 1, ( "malloc() of ssl sub-contexts failed" ) );
        return( POLARSSL_ERR_SSL_MALLOC_FAILED );
    }

    memset( ssl->handshake, 0, sizeof(ssl_handshake_params) );
    memset( ssl->transform_negotiate, 0, sizeof(ssl_transform) );
    memset( ssl->session_negotiate, 0, sizeof(ssl_session) );

     md5_starts( &ssl->handshake->fin_md5 );
    sha1_starts( &ssl->handshake->fin_sha1 );
    sha2_starts( &ssl->handshake->fin_sha2, 0 );
#if defined(POLARSSL_SHA4_C)
    sha4_starts( &ssl->handshake->fin_sha4, 1 );
#endif

    ssl->handshake->update_checksum = ssl_update_checksum_start;
    ssl->handshake->sig_alg = SSL_HASH_SHA1;
    
    return( 0 );
}

/*
 * Initialize an SSL context
 */
int ssl_init( ssl_context *ssl )
{
    int ret;
    int len = SSL_BUFFER_LEN;

    memset( ssl, 0, sizeof( ssl_context ) );

    /*
     * Sane defaults
     */
    ssl->rsa_decrypt = ssl_rsa_decrypt;
    ssl->rsa_sign = ssl_rsa_sign;
    ssl->rsa_key_len = ssl_rsa_key_len;

    ssl->min_major_ver = SSL_MAJOR_VERSION_3;
    ssl->min_minor_ver = SSL_MINOR_VERSION_0;

    ssl->ciphersuites = malloc( sizeof(int *) * 4 );
    ssl_set_ciphersuites( ssl, ssl_default_ciphersuites );

#if defined(POLARSSL_DHM_C)
    if( ( ret = mpi_read_string( &ssl->dhm_P, 16,
                                 POLARSSL_DHM_RFC5114_MODP_1024_P) ) != 0 ||
        ( ret = mpi_read_string( &ssl->dhm_G, 16,
                                 POLARSSL_DHM_RFC5114_MODP_1024_G) ) != 0 )
    {
        SSL_DEBUG_RET( 1, "mpi_read_string", ret );
        return( ret );
    }
#endif

    /*
     * Prepare base structures
     */
    ssl->in_ctr = (unsigned char *) malloc( len );
    ssl->in_hdr = ssl->in_ctr +  8;
    ssl->in_msg = ssl->in_ctr + 13;

    if( ssl->in_ctr == NULL )
    {
        SSL_DEBUG_MSG( 1, ( "malloc(%d bytes) failed", len ) );
        return( POLARSSL_ERR_SSL_MALLOC_FAILED );
    }

    ssl->out_ctr = (unsigned char *) malloc( len );
    ssl->out_hdr = ssl->out_ctr +  8;
    ssl->out_msg = ssl->out_ctr + 40;

    if( ssl->out_ctr == NULL )
    {
        SSL_DEBUG_MSG( 1, ( "malloc(%d bytes) failed", len ) );
        free( ssl-> in_ctr );
        return( POLARSSL_ERR_SSL_MALLOC_FAILED );
    }

    memset( ssl-> in_ctr, 0, SSL_BUFFER_LEN );
    memset( ssl->out_ctr, 0, SSL_BUFFER_LEN );

    ssl->hostname = NULL;
    ssl->hostname_len = 0;

    if( ( ret = ssl_handshake_init( ssl ) ) != 0 )
        return( ret );

    return( 0 );
}

/*
 * Reset an initialized and used SSL context for re-use while retaining
 * all application-set variables, function pointers and data.
 */
int ssl_session_reset( ssl_context *ssl )
{
    int ret;

    ssl->state = SSL_HELLO_REQUEST;
    ssl->renegotiation = SSL_INITIAL_HANDSHAKE;
    ssl->secure_renegotiation = SSL_LEGACY_RENEGOTIATION;

    ssl->verify_data_len = 0;
    memset( ssl->own_verify_data, 0, 36 );
    memset( ssl->peer_verify_data, 0, 36 );

    ssl->in_offt = NULL;

    ssl->in_msgtype = 0;
    ssl->in_msglen = 0;
    ssl->in_left = 0;

    ssl->in_hslen = 0;
    ssl->nb_zero = 0;

    ssl->out_msgtype = 0;
    ssl->out_msglen = 0;
    ssl->out_left = 0;

    ssl->transform_in = NULL;
    ssl->transform_out = NULL;

    memset( ssl->out_ctr, 0, SSL_BUFFER_LEN );
    memset( ssl->in_ctr, 0, SSL_BUFFER_LEN );

#if defined(POLARSSL_SSL_HW_RECORD_ACCEL)
    if( ssl_hw_record_reset != NULL)
    {
        SSL_DEBUG_MSG( 2, ( "going for ssl_hw_record_reset()" ) );
        if( ssl_hw_record_reset( ssl ) != 0 )
        {
            SSL_DEBUG_RET( 1, "ssl_hw_record_reset", ret );
            return( POLARSSL_ERR_SSL_HW_ACCEL_FAILED );
        }
    }
#endif

    if( ssl->transform )
    {
        ssl_transform_free( ssl->transform );
        free( ssl->transform );
        ssl->transform = NULL;
    }

    if( ssl->session )
    {
        ssl_session_free( ssl->session );
        free( ssl->session );
        ssl->session = NULL;
    }

    if( ( ret = ssl_handshake_init( ssl ) ) != 0 )
        return( ret );

    return( 0 );
}

/*
 * SSL set accessors
 */
void ssl_set_endpoint( ssl_context *ssl, int endpoint )
{
    ssl->endpoint   = endpoint;
}

void ssl_set_authmode( ssl_context *ssl, int authmode )
{
    ssl->authmode   = authmode;
}

void ssl_set_verify( ssl_context *ssl,
                     int (*f_vrfy)(void *, x509_cert *, int, int *),
                     void *p_vrfy )
{
    ssl->f_vrfy      = f_vrfy;
    ssl->p_vrfy      = p_vrfy;
}

void ssl_set_rng( ssl_context *ssl,
                  int (*f_rng)(void *, unsigned char *, size_t),
                  void *p_rng )
{
    ssl->f_rng      = f_rng;
    ssl->p_rng      = p_rng;
}

void ssl_set_dbg( ssl_context *ssl,
                  void (*f_dbg)(void *, int, const char *),
                  void  *p_dbg )
{
    ssl->f_dbg      = f_dbg;
    ssl->p_dbg      = p_dbg;
}

void ssl_set_bio( ssl_context *ssl,
            int (*f_recv)(void *, unsigned char *, size_t), void *p_recv,
            int (*f_send)(void *, const unsigned char *, size_t), void *p_send )
{
    ssl->f_recv     = f_recv;
    ssl->f_send     = f_send;
    ssl->p_recv     = p_recv;
    ssl->p_send     = p_send;
}

void ssl_set_session_cache( ssl_context *ssl,
        int (*f_get_cache)(void *, ssl_session *), void *p_get_cache,
        int (*f_set_cache)(void *, const ssl_session *), void *p_set_cache )
{
    ssl->f_get_cache = f_get_cache;
    ssl->p_get_cache = p_get_cache;
    ssl->f_set_cache = f_set_cache;
    ssl->p_set_cache = p_set_cache;
}

void ssl_set_session( ssl_context *ssl, const ssl_session *session )
{
    memcpy( ssl->session_negotiate, session, sizeof(ssl_session) );
    ssl->handshake->resume = 1;
}

void ssl_set_ciphersuites( ssl_context *ssl, const int *ciphersuites )
{
    ssl->ciphersuites[SSL_MINOR_VERSION_0] = ciphersuites;
    ssl->ciphersuites[SSL_MINOR_VERSION_1] = ciphersuites;
    ssl->ciphersuites[SSL_MINOR_VERSION_2] = ciphersuites;
    ssl->ciphersuites[SSL_MINOR_VERSION_3] = ciphersuites;
}

void ssl_set_ciphersuites_for_version( ssl_context *ssl, const int *ciphersuites,
                                       int major, int minor )
{
    if( major != SSL_MAJOR_VERSION_3 )
        return;

    if( minor < SSL_MINOR_VERSION_0 || minor > SSL_MINOR_VERSION_3 )
        return;

    ssl->ciphersuites[minor] = ciphersuites;
}

void ssl_set_ca_chain( ssl_context *ssl, x509_cert *ca_chain,
                       x509_crl *ca_crl, const char *peer_cn )
{
    ssl->ca_chain   = ca_chain;
    ssl->ca_crl     = ca_crl;
    ssl->peer_cn    = peer_cn;
}

void ssl_set_own_cert( ssl_context *ssl, x509_cert *own_cert,
                       rsa_context *rsa_key )
{
    ssl->own_cert   = own_cert;
    ssl->rsa_key    = rsa_key;
}

void ssl_set_own_cert_alt( ssl_context *ssl, x509_cert *own_cert,
                           void *rsa_key,
                           rsa_decrypt_func rsa_decrypt,
                           rsa_sign_func rsa_sign,
                           rsa_key_len_func rsa_key_len )
{
    ssl->own_cert   = own_cert;
    ssl->rsa_key    = rsa_key;
    ssl->rsa_decrypt = rsa_decrypt;
    ssl->rsa_sign = rsa_sign;
    ssl->rsa_key_len = rsa_key_len;
}


#if defined(POLARSSL_DHM_C)
int ssl_set_dh_param( ssl_context *ssl, const char *dhm_P, const char *dhm_G )
{
    int ret;

    if( ( ret = mpi_read_string( &ssl->dhm_P, 16, dhm_P ) ) != 0 )
    {
        SSL_DEBUG_RET( 1, "mpi_read_string", ret );
        return( ret );
    }

    if( ( ret = mpi_read_string( &ssl->dhm_G, 16, dhm_G ) ) != 0 )
    {
        SSL_DEBUG_RET( 1, "mpi_read_string", ret );
        return( ret );
    }

    return( 0 );
}

int ssl_set_dh_param_ctx( ssl_context *ssl, dhm_context *dhm_ctx )
{
    int ret;

    if( ( ret = mpi_copy(&ssl->dhm_P, &dhm_ctx->P) ) != 0 )
    {
        SSL_DEBUG_RET( 1, "mpi_copy", ret );
        return( ret );
    }

    if( ( ret = mpi_copy(&ssl->dhm_G, &dhm_ctx->G) ) != 0 )
    {
        SSL_DEBUG_RET( 1, "mpi_copy", ret );
        return( ret );
    }

    return( 0 );
}
#endif /* POLARSSL_DHM_C */

int ssl_set_hostname( ssl_context *ssl, const char *hostname )
{
    if( hostname == NULL )
        return( POLARSSL_ERR_SSL_BAD_INPUT_DATA );

    ssl->hostname_len = strlen( hostname );
    ssl->hostname = (unsigned char *) malloc( ssl->hostname_len + 1 );

    if( ssl->hostname == NULL )
        return( POLARSSL_ERR_SSL_MALLOC_FAILED );

    memcpy( ssl->hostname, (const unsigned char *) hostname,
            ssl->hostname_len );
    
    ssl->hostname[ssl->hostname_len] = '\0';

    return( 0 );
}

void ssl_set_sni( ssl_context *ssl,
                  int (*f_sni)(void *, ssl_context *,
                                const unsigned char *, size_t),
                  void *p_sni )
{
    ssl->f_sni = f_sni;
    ssl->p_sni = p_sni;
}

void ssl_set_max_version( ssl_context *ssl, int major, int minor )
{
    ssl->max_major_ver = major;
    ssl->max_minor_ver = minor;
}

void ssl_set_min_version( ssl_context *ssl, int major, int minor )
{
    ssl->min_major_ver = major;
    ssl->min_minor_ver = minor;
}

void ssl_set_renegotiation( ssl_context *ssl, int renegotiation )
{
    ssl->disable_renegotiation = renegotiation;
}

void ssl_legacy_renegotiation( ssl_context *ssl, int allow_legacy )
{
    ssl->allow_legacy_renegotiation = allow_legacy;
}

/*
 * SSL get accessors
 */
size_t ssl_get_bytes_avail( const ssl_context *ssl )
{
    return( ssl->in_offt == NULL ? 0 : ssl->in_msglen );
}

int ssl_get_verify_result( const ssl_context *ssl )
{
    return( ssl->verify_result );
}

const char *ssl_get_ciphersuite_name( const int ciphersuite_id )
{
    switch( ciphersuite_id )
    {
#if defined(POLARSSL_ARC4_C)
        case TLS_RSA_WITH_RC4_128_MD5:
            return( "TLS-RSA-WITH-RC4-128-MD5" );

        case TLS_RSA_WITH_RC4_128_SHA:
            return( "TLS-RSA-WITH-RC4-128-SHA" );
#endif

#if defined(POLARSSL_DES_C)
        case TLS_RSA_WITH_3DES_EDE_CBC_SHA:
            return( "TLS-RSA-WITH-3DES-EDE-CBC-SHA" );

        case TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA:
            return( "TLS-DHE-RSA-WITH-3DES-EDE-CBC-SHA" );
#endif

#if defined(POLARSSL_AES_C)
        case TLS_RSA_WITH_AES_128_CBC_SHA:
            return( "TLS-RSA-WITH-AES-128-CBC-SHA" );

        case TLS_DHE_RSA_WITH_AES_128_CBC_SHA:
            return( "TLS-DHE-RSA-WITH-AES-128-CBC-SHA" );

        case TLS_RSA_WITH_AES_256_CBC_SHA:
            return( "TLS-RSA-WITH-AES-256-CBC-SHA" );

        case TLS_DHE_RSA_WITH_AES_256_CBC_SHA:
            return( "TLS-DHE-RSA-WITH-AES-256-CBC-SHA" );

#if defined(POLARSSL_SHA2_C)
        case TLS_RSA_WITH_AES_128_CBC_SHA256:
            return( "TLS-RSA-WITH-AES-128-CBC-SHA256" );

        case TLS_RSA_WITH_AES_256_CBC_SHA256:
            return( "TLS-RSA-WITH-AES-256-CBC-SHA256" );

        case TLS_DHE_RSA_WITH_AES_128_CBC_SHA256:
            return( "TLS-DHE-RSA-WITH-AES-128-CBC-SHA256" );

        case TLS_DHE_RSA_WITH_AES_256_CBC_SHA256:
            return( "TLS-DHE-RSA-WITH-AES-256-CBC-SHA256" );
#endif

#if defined(POLARSSL_GCM_C) && defined(POLARSSL_SHA2_C)
        case TLS_RSA_WITH_AES_128_GCM_SHA256:
            return( "TLS-RSA-WITH-AES-128-GCM-SHA256" );

        case TLS_RSA_WITH_AES_256_GCM_SHA384:
            return( "TLS-RSA-WITH-AES-256-GCM-SHA384" );
#endif

#if defined(POLARSSL_GCM_C) && defined(POLARSSL_SHA4_C)
        case TLS_DHE_RSA_WITH_AES_128_GCM_SHA256:
            return( "TLS-DHE-RSA-WITH-AES-128-GCM-SHA256" );

        case TLS_DHE_RSA_WITH_AES_256_GCM_SHA384:
            return( "TLS-DHE-RSA-WITH-AES-256-GCM-SHA384" );
#endif
#endif /* POLARSSL_AES_C */

#if defined(POLARSSL_CAMELLIA_C)
        case TLS_RSA_WITH_CAMELLIA_128_CBC_SHA:
            return( "TLS-RSA-WITH-CAMELLIA-128-CBC-SHA" );

        case TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA:
            return( "TLS-DHE-RSA-WITH-CAMELLIA-128-CBC-SHA" );

        case TLS_RSA_WITH_CAMELLIA_256_CBC_SHA:
            return( "TLS-RSA-WITH-CAMELLIA-256-CBC-SHA" );

        case TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA:
            return( "TLS-DHE-RSA-WITH-CAMELLIA-256-CBC-SHA" );

#if defined(POLARSSL_SHA2_C)
        case TLS_RSA_WITH_CAMELLIA_128_CBC_SHA256:
            return( "TLS-RSA-WITH-CAMELLIA-128-CBC-SHA256" );

        case TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA256:
            return( "TLS-DHE-RSA-WITH-CAMELLIA-128-CBC-SHA256" );

        case TLS_RSA_WITH_CAMELLIA_256_CBC_SHA256:
            return( "TLS-RSA-WITH-CAMELLIA-256-CBC-SHA256" );

        case TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA256:
            return( "TLS-DHE-RSA-WITH-CAMELLIA-256-CBC-SHA256" );
#endif
#endif

#if defined(POLARSSL_ENABLE_WEAK_CIPHERSUITES)
#if defined(POLARSSL_CIPHER_NULL_CIPHER)
        case TLS_RSA_WITH_NULL_MD5:
            return( "TLS-RSA-WITH-NULL-MD5" );
        case TLS_RSA_WITH_NULL_SHA:
            return( "TLS-RSA-WITH-NULL-SHA" );
        case TLS_RSA_WITH_NULL_SHA256:
            return( "TLS-RSA-WITH-NULL-SHA256" );
#endif /* defined(POLARSSL_CIPHER_NULL_CIPHER) */

#if defined(POLARSSL_DES_C)
        case TLS_RSA_WITH_DES_CBC_SHA:
            return( "TLS-RSA-WITH-DES-CBC-SHA" );
        case TLS_DHE_RSA_WITH_DES_CBC_SHA:
            return( "TLS-DHE-RSA-WITH-DES-CBC-SHA" );
#endif
#endif /* defined(POLARSSL_ENABLE_WEAK_CIPHERSUITES) */

    default:
        break;
    }

    return( "unknown" );
}

int ssl_get_ciphersuite_id( const char *ciphersuite_name )
{
#if defined(POLARSSL_ARC4_C)
    if (0 == strcasecmp(ciphersuite_name, "TLS-RSA-WITH-RC4-128-MD5"))
        return( TLS_RSA_WITH_RC4_128_MD5 );
    if (0 == strcasecmp(ciphersuite_name, "TLS-RSA-WITH-RC4-128-SHA"))
        return( TLS_RSA_WITH_RC4_128_SHA );
#endif

#if defined(POLARSSL_DES_C)
    if (0 == strcasecmp(ciphersuite_name, "TLS-RSA-WITH-3DES-EDE-CBC-SHA"))
        return( TLS_RSA_WITH_3DES_EDE_CBC_SHA );
    if (0 == strcasecmp(ciphersuite_name, "TLS-DHE-RSA-WITH-3DES-EDE-CBC-SHA"))
        return( TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA );
#endif

#if defined(POLARSSL_AES_C)
    if (0 == strcasecmp(ciphersuite_name, "TLS-RSA-WITH-AES-128-CBC-SHA"))
        return( TLS_RSA_WITH_AES_128_CBC_SHA );
    if (0 == strcasecmp(ciphersuite_name, "TLS-DHE-RSA-WITH-AES-128-CBC-SHA"))
        return( TLS_DHE_RSA_WITH_AES_128_CBC_SHA );
    if (0 == strcasecmp(ciphersuite_name, "TLS-RSA-WITH-AES-256-CBC-SHA"))
        return( TLS_RSA_WITH_AES_256_CBC_SHA );
    if (0 == strcasecmp(ciphersuite_name, "TLS-DHE-RSA-WITH-AES-256-CBC-SHA"))
        return( TLS_DHE_RSA_WITH_AES_256_CBC_SHA );

#if defined(POLARSSL_SHA2_C)
    if (0 == strcasecmp(ciphersuite_name, "TLS-RSA-WITH-AES-128-CBC-SHA256"))
        return( TLS_RSA_WITH_AES_128_CBC_SHA256 );
    if (0 == strcasecmp(ciphersuite_name, "TLS-RSA-WITH-AES-256-CBC-SHA256"))
        return( TLS_RSA_WITH_AES_256_CBC_SHA256 );
    if (0 == strcasecmp(ciphersuite_name, "TLS-DHE-RSA-WITH-AES-128-CBC-SHA256"))
        return( TLS_DHE_RSA_WITH_AES_128_CBC_SHA256 );
    if (0 == strcasecmp(ciphersuite_name, "TLS-DHE-RSA-WITH-AES-256-CBC-SHA256"))
        return( TLS_DHE_RSA_WITH_AES_256_CBC_SHA256 );
#endif

#if defined(POLARSSL_GCM_C) && defined(POLARSSL_SHA2_C)
    if (0 == strcasecmp(ciphersuite_name, "TLS-RSA-WITH-AES-128-GCM-SHA256"))
        return( TLS_RSA_WITH_AES_128_GCM_SHA256 );
    if (0 == strcasecmp(ciphersuite_name, "TLS-RSA-WITH-AES-256-GCM-SHA384"))
        return( TLS_RSA_WITH_AES_256_GCM_SHA384 );
#endif

#if defined(POLARSSL_GCM_C) && defined(POLARSSL_SHA2_C)
    if (0 == strcasecmp(ciphersuite_name, "TLS-DHE-RSA-WITH-AES-128-GCM-SHA256"))
        return( TLS_DHE_RSA_WITH_AES_128_GCM_SHA256 );
    if (0 == strcasecmp(ciphersuite_name, "TLS-DHE-RSA-WITH-AES-256-GCM-SHA384"))
        return( TLS_DHE_RSA_WITH_AES_256_GCM_SHA384 );
#endif
#endif

#if defined(POLARSSL_CAMELLIA_C)
    if (0 == strcasecmp(ciphersuite_name, "TLS-RSA-WITH-CAMELLIA-128-CBC-SHA"))
        return( TLS_RSA_WITH_CAMELLIA_128_CBC_SHA );
    if (0 == strcasecmp(ciphersuite_name, "TLS-DHE-RSA-WITH-CAMELLIA-128-CBC-SHA"))
        return( TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA );
    if (0 == strcasecmp(ciphersuite_name, "TLS-RSA-WITH-CAMELLIA-256-CBC-SHA"))
        return( TLS_RSA_WITH_CAMELLIA_256_CBC_SHA );
    if (0 == strcasecmp(ciphersuite_name, "TLS-DHE-RSA-WITH-CAMELLIA-256-CBC-SHA"))
        return( TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA );

#if defined(POLARSSL_SHA2_C)
    if (0 == strcasecmp(ciphersuite_name, "TLS-RSA-WITH-CAMELLIA-128-CBC-SHA256"))
        return( TLS_RSA_WITH_CAMELLIA_128_CBC_SHA256 );
    if (0 == strcasecmp(ciphersuite_name, "TLS-DHE-RSA-WITH-CAMELLIA-128-CBC-SHA256"))
        return( TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA256 );
    if (0 == strcasecmp(ciphersuite_name, "TLS-RSA-WITH-CAMELLIA-256-CBC-SHA256"))
        return( TLS_RSA_WITH_CAMELLIA_256_CBC_SHA256 );
    if (0 == strcasecmp(ciphersuite_name, "TLS-DHE-RSA-WITH-CAMELLIA-256-CBC-SHA256"))
        return( TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA256 );
#endif
#endif

#if defined(POLARSSL_ENABLE_WEAK_CIPHERSUITES)
#if defined(POLARSSL_CIPHER_NULL_CIPHER)
    if (0 == strcasecmp(ciphersuite_name, "TLS-RSA-WITH-NULL-MD5"))
        return( TLS_RSA_WITH_NULL_MD5 );
    if (0 == strcasecmp(ciphersuite_name, "TLS-RSA-WITH-NULL-SHA"))
        return( TLS_RSA_WITH_NULL_SHA );
    if (0 == strcasecmp(ciphersuite_name, "TLS-RSA-WITH-NULL-SHA256"))
        return( TLS_RSA_WITH_NULL_SHA256 );
#endif /* defined(POLARSSL_CIPHER_NULL_CIPHER) */

#if defined(POLARSSL_DES_C)
    if (0 == strcasecmp(ciphersuite_name, "TLS-RSA-WITH-DES-CBC-SHA"))
        return( TLS_RSA_WITH_DES_CBC_SHA );
    if (0 == strcasecmp(ciphersuite_name, "TLS-DHE-RSA-WITH-DES-CBC-SHA"))
        return( TLS_DHE_RSA_WITH_DES_CBC_SHA );
#endif
#endif /* defined(POLARSSL_ENABLE_WEAK_CIPHERSUITES) */

    return( 0 );
}

const char *ssl_get_ciphersuite( const ssl_context *ssl )
{
    if( ssl == NULL || ssl->session == NULL )
        return NULL;

    return ssl_get_ciphersuite_name( ssl->session->ciphersuite );
}

const char *ssl_get_version( const ssl_context *ssl )
{
    switch( ssl->minor_ver )
    {
        case SSL_MINOR_VERSION_0:
            return( "SSLv3.0" );

        case SSL_MINOR_VERSION_1:
            return( "TLSv1.0" );

        case SSL_MINOR_VERSION_2:
            return( "TLSv1.1" );

        case SSL_MINOR_VERSION_3:
            return( "TLSv1.2" );

        default:
            break;
    }
    return( "unknown" );
}

const x509_cert *ssl_get_peer_cert( const ssl_context *ssl )
{
    if( ssl == NULL || ssl->session == NULL )
        return NULL;

    return ssl->session->peer_cert;
}

const int ssl_default_ciphersuites[] =
{
#if defined(POLARSSL_DHM_C)
#if defined(POLARSSL_AES_C)
#if defined(POLARSSL_SHA2_C)
    TLS_DHE_RSA_WITH_AES_256_CBC_SHA256,
#endif /* POLARSSL_SHA2_C */
#if defined(POLARSSL_GCM_C) && defined(POLARSSL_SHA4_C)
    TLS_DHE_RSA_WITH_AES_256_GCM_SHA384,
#endif
    TLS_DHE_RSA_WITH_AES_256_CBC_SHA,
#if defined(POLARSSL_SHA2_C)
    TLS_DHE_RSA_WITH_AES_128_CBC_SHA256,
#endif
#if defined(POLARSSL_GCM_C) && defined(POLARSSL_SHA2_C)
    TLS_DHE_RSA_WITH_AES_128_GCM_SHA256,
#endif
    TLS_DHE_RSA_WITH_AES_128_CBC_SHA,
#endif
#if defined(POLARSSL_CAMELLIA_C)
#if defined(POLARSSL_SHA2_C)
    TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA256,
#endif /* POLARSSL_SHA2_C */
    TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA,
#if defined(POLARSSL_SHA2_C)
    TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA256,
#endif /* POLARSSL_SHA2_C */
    TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA,
#endif
#if defined(POLARSSL_DES_C)
    TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA,
#endif
#endif

#if defined(POLARSSL_AES_C)
#if defined(POLARSSL_SHA2_C)
    TLS_RSA_WITH_AES_256_CBC_SHA256,
#endif /* POLARSSL_SHA2_C */
#if defined(POLARSSL_GCM_C) && defined(POLARSSL_SHA4_C)
    TLS_RSA_WITH_AES_256_GCM_SHA384,
#endif /* POLARSSL_SHA2_C */
    TLS_RSA_WITH_AES_256_CBC_SHA,
#endif
#if defined(POLARSSL_CAMELLIA_C)
#if defined(POLARSSL_SHA2_C)
    TLS_RSA_WITH_CAMELLIA_256_CBC_SHA256,
#endif /* POLARSSL_SHA2_C */
    TLS_RSA_WITH_CAMELLIA_256_CBC_SHA,
#endif
#if defined(POLARSSL_AES_C)
#if defined(POLARSSL_SHA2_C)
    TLS_RSA_WITH_AES_128_CBC_SHA256,
#endif /* POLARSSL_SHA2_C */
#if defined(POLARSSL_GCM_C) && defined(POLARSSL_SHA2_C)
    TLS_RSA_WITH_AES_128_GCM_SHA256,
#endif /* POLARSSL_SHA2_C */
    TLS_RSA_WITH_AES_128_CBC_SHA,
#endif
#if defined(POLARSSL_CAMELLIA_C)
#if defined(POLARSSL_SHA2_C)
    TLS_RSA_WITH_CAMELLIA_128_CBC_SHA256,
#endif /* POLARSSL_SHA2_C */
    TLS_RSA_WITH_CAMELLIA_128_CBC_SHA,
#endif
#if defined(POLARSSL_DES_C)
    TLS_RSA_WITH_3DES_EDE_CBC_SHA,
#endif
#if defined(POLARSSL_ARC4_C)
    TLS_RSA_WITH_RC4_128_SHA,
    TLS_RSA_WITH_RC4_128_MD5,
#endif
    0
};

/*
 * Perform a single step of the SSL handshake
 */
int ssl_handshake_step( ssl_context *ssl )
{
    int ret = POLARSSL_ERR_SSL_FEATURE_UNAVAILABLE;

#if defined(POLARSSL_SSL_CLI_C)
    if( ssl->endpoint == SSL_IS_CLIENT )
        ret = ssl_handshake_client_step( ssl );
#endif

#if defined(POLARSSL_SSL_SRV_C)
    if( ssl->endpoint == SSL_IS_SERVER )
        ret = ssl_handshake_server_step( ssl );
#endif

    return( ret );
}

/*
 * Perform the SSL handshake
 */
int ssl_handshake( ssl_context *ssl )
{
    int ret = 0;

    SSL_DEBUG_MSG( 2, ( "=> handshake" ) );

    while( ssl->state != SSL_HANDSHAKE_OVER )
    {
        ret = ssl_handshake_step( ssl );

        if( ret != 0 )
            break;
    }

    SSL_DEBUG_MSG( 2, ( "<= handshake" ) );

    return( ret );
}

/*
 * Renegotiate current connection
 */
int ssl_renegotiate( ssl_context *ssl )
{
    int ret;

    SSL_DEBUG_MSG( 2, ( "=> renegotiate" ) );

    if( ssl->state != SSL_HANDSHAKE_OVER )
        return( POLARSSL_ERR_SSL_BAD_INPUT_DATA );

    ssl->state = SSL_HELLO_REQUEST;
    ssl->renegotiation = SSL_RENEGOTIATION;

    if( ( ret = ssl_handshake_init( ssl ) ) != 0 )
        return( ret );

    if( ( ret = ssl_handshake( ssl ) ) != 0 )
    {
        SSL_DEBUG_RET( 1, "ssl_handshake", ret );
        return( ret );
    }

    SSL_DEBUG_MSG( 2, ( "<= renegotiate" ) );

    return( 0 );
}

/*
 * Receive application data decrypted from the SSL layer
 */
int ssl_read( ssl_context *ssl, unsigned char *buf, size_t len )
{
    int ret;
    size_t n;

    SSL_DEBUG_MSG( 2, ( "=> read" ) );

    if( ssl->state != SSL_HANDSHAKE_OVER )
    {
        if( ( ret = ssl_handshake( ssl ) ) != 0 )
        {
            SSL_DEBUG_RET( 1, "ssl_handshake", ret );
            return( ret );
        }
    }

    if( ssl->in_offt == NULL )
    {
        if( ( ret = ssl_read_record( ssl ) ) != 0 )
        {
            if( ret == POLARSSL_ERR_SSL_CONN_EOF )
                return( 0 );

            SSL_DEBUG_RET( 1, "ssl_read_record", ret );
            return( ret );
        }

        if( ssl->in_msglen  == 0 &&
            ssl->in_msgtype == SSL_MSG_APPLICATION_DATA )
        {
            /*
             * OpenSSL sends empty messages to randomize the IV
             */
            if( ( ret = ssl_read_record( ssl ) ) != 0 )
            {
                if( ret == POLARSSL_ERR_SSL_CONN_EOF )
                    return( 0 );

                SSL_DEBUG_RET( 1, "ssl_read_record", ret );
                return( ret );
            }
        }

        if( ssl->in_msgtype == SSL_MSG_HANDSHAKE )
        {
            SSL_DEBUG_MSG( 1, ( "received handshake message" ) );

            if( ssl->endpoint == SSL_IS_CLIENT &&
                ( ssl->in_msg[0] != SSL_HS_HELLO_REQUEST ||
                  ssl->in_hslen != 4 ) )
            {
                SSL_DEBUG_MSG( 1, ( "handshake received (not HelloRequest)" ) );
                return( POLARSSL_ERR_SSL_UNEXPECTED_MESSAGE );
            }

            if( ssl->disable_renegotiation == SSL_RENEGOTIATION_DISABLED ||
                ( ssl->secure_renegotiation == SSL_LEGACY_RENEGOTIATION &&
                  ssl->allow_legacy_renegotiation == SSL_LEGACY_NO_RENEGOTIATION ) )
            {
                SSL_DEBUG_MSG( 3, ( "ignoring renegotiation, sending alert" ) );

                if( ssl->minor_ver == SSL_MINOR_VERSION_0 )
                {
                    /*
                     * SSLv3 does not have a "no_renegotiation" alert
                     */
                    if( ( ret = ssl_send_fatal_handshake_failure( ssl ) ) != 0 )
                        return( ret );
                }
                else
                {
                    if( ( ret = ssl_send_alert_message( ssl,
                                    SSL_ALERT_LEVEL_WARNING,
                                    SSL_ALERT_MSG_NO_RENEGOTIATION ) ) != 0 )
                    {
                        return( ret );
                    }
                }
            }
            else
            {
                if( ( ret = ssl_renegotiate( ssl ) ) != 0 )
                {
                    SSL_DEBUG_RET( 1, "ssl_renegotiate", ret );
                    return( ret );
                }

                return( POLARSSL_ERR_NET_WANT_READ );
            }
        }
        else if( ssl->in_msgtype != SSL_MSG_APPLICATION_DATA )
        {
            SSL_DEBUG_MSG( 1, ( "bad application data message" ) );
            return( POLARSSL_ERR_SSL_UNEXPECTED_MESSAGE );
        }

        ssl->in_offt = ssl->in_msg;
    }

    n = ( len < ssl->in_msglen )
        ? len : ssl->in_msglen;

    memcpy( buf, ssl->in_offt, n );
    ssl->in_msglen -= n;

    if( ssl->in_msglen == 0 )
        /* all bytes consumed  */
        ssl->in_offt = NULL;
    else
        /* more data available */
        ssl->in_offt += n;

    SSL_DEBUG_MSG( 2, ( "<= read" ) );

    return( (int) n );
}

/*
 * Send application data to be encrypted by the SSL layer
 */
int ssl_write( ssl_context *ssl, const unsigned char *buf, size_t len )
{
    int ret;
    size_t n;

    SSL_DEBUG_MSG( 2, ( "=> write" ) );

    if( ssl->state != SSL_HANDSHAKE_OVER )
    {
        if( ( ret = ssl_handshake( ssl ) ) != 0 )
        {
            SSL_DEBUG_RET( 1, "ssl_handshake", ret );
            return( ret );
        }
    }

    n = ( len < SSL_MAX_CONTENT_LEN )
        ? len : SSL_MAX_CONTENT_LEN;

    if( ssl->out_left != 0 )
    {
        if( ( ret = ssl_flush_output( ssl ) ) != 0 )
        {
            SSL_DEBUG_RET( 1, "ssl_flush_output", ret );
            return( ret );
        }
    }
    else
    {
        ssl->out_msglen  = n;
        ssl->out_msgtype = SSL_MSG_APPLICATION_DATA;
        memcpy( ssl->out_msg, buf, n );

        if( ( ret = ssl_write_record( ssl ) ) != 0 )
        {
            SSL_DEBUG_RET( 1, "ssl_write_record", ret );
            return( ret );
        }
    }

    SSL_DEBUG_MSG( 2, ( "<= write" ) );

    return( (int) n );
}

/*
 * Notify the peer that the connection is being closed
 */
int ssl_close_notify( ssl_context *ssl )
{
    int ret;

    SSL_DEBUG_MSG( 2, ( "=> write close notify" ) );

    if( ( ret = ssl_flush_output( ssl ) ) != 0 )
    {
        SSL_DEBUG_RET( 1, "ssl_flush_output", ret );
        return( ret );
    }

    if( ssl->state == SSL_HANDSHAKE_OVER )
    {
        if( ( ret = ssl_send_alert_message( ssl,
                        SSL_ALERT_LEVEL_WARNING,
                        SSL_ALERT_MSG_CLOSE_NOTIFY ) ) != 0 )
        {
            return( ret );
        }
    }

    SSL_DEBUG_MSG( 2, ( "<= write close notify" ) );

    return( ret );
}

void ssl_transform_free( ssl_transform *transform )
{
#if defined(POLARSSL_ZLIB_SUPPORT)
    deflateEnd( &transform->ctx_deflate );
    inflateEnd( &transform->ctx_inflate );
#endif

    memset( transform, 0, sizeof( ssl_transform ) );
}

void ssl_handshake_free( ssl_handshake_params *handshake )
{
#if defined(POLARSSL_DHM_C)
    dhm_free( &handshake->dhm_ctx );
#endif
    memset( handshake, 0, sizeof( ssl_handshake_params ) );
}

void ssl_session_free( ssl_session *session )
{
    if( session->peer_cert != NULL )
    {
        x509_free( session->peer_cert );
        free( session->peer_cert );
    }

    memset( session, 0, sizeof( ssl_session ) );
}

/*
 * Free an SSL context
 */
void ssl_free( ssl_context *ssl )
{
    SSL_DEBUG_MSG( 2, ( "=> free" ) );

    free( ssl->ciphersuites );

    if( ssl->out_ctr != NULL )
    {
        memset( ssl->out_ctr, 0, SSL_BUFFER_LEN );
          free( ssl->out_ctr );
    }

    if( ssl->in_ctr != NULL )
    {
        memset( ssl->in_ctr, 0, SSL_BUFFER_LEN );
          free( ssl->in_ctr );
    }

#if defined(POLARSSL_DHM_C)
    mpi_free( &ssl->dhm_P );
    mpi_free( &ssl->dhm_G );
#endif

    if( ssl->transform )
    {
        ssl_transform_free( ssl->transform );
        free( ssl->transform );
    }

    if( ssl->handshake )
    {
        ssl_handshake_free( ssl->handshake );
        ssl_transform_free( ssl->transform_negotiate );
        ssl_session_free( ssl->session_negotiate );

        free( ssl->handshake );
        free( ssl->transform_negotiate );
        free( ssl->session_negotiate );
    }

    if( ssl->session )
    {
        ssl_session_free( ssl->session );
        free( ssl->session );
    }

    if ( ssl->hostname != NULL)
    {
        memset( ssl->hostname, 0, ssl->hostname_len );
        free( ssl->hostname );
        ssl->hostname_len = 0;
    }

#if defined(POLARSSL_SSL_HW_RECORD_ACCEL)
    if( ssl_hw_record_finish != NULL )
    {
        SSL_DEBUG_MSG( 2, ( "going for ssl_hw_record_finish()" ) );
        ssl_hw_record_finish( ssl );
    }
#endif

    SSL_DEBUG_MSG( 2, ( "<= free" ) );

    /* Actually clear after last debug message */
    memset( ssl, 0, sizeof( ssl_context ) );
}

#endif
