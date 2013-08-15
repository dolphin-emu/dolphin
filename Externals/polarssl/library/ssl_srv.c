/*
 *  SSLv3/TLSv1 server-side functions
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

#if defined(POLARSSL_SSL_SRV_C)

#include "polarssl/debug.h"
#include "polarssl/ssl.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>

static int ssl_parse_servername_ext( ssl_context *ssl,
                                     const unsigned char *buf,
                                     size_t len )
{
    int ret;
    size_t servername_list_size, hostname_len;
    const unsigned char *p;

    servername_list_size = ( ( buf[0] << 8 ) | ( buf[1] ) );
    if( servername_list_size + 2 != len )
    {
        SSL_DEBUG_MSG( 1, ( "bad client hello message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_HELLO );
    }

    p = buf + 2;
    while( servername_list_size > 0 )
    {
        hostname_len = ( ( p[1] << 8 ) | p[2] );
        if( hostname_len + 3 > servername_list_size )
        {
            SSL_DEBUG_MSG( 1, ( "bad client hello message" ) );
            return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_HELLO );
        }

        if( p[0] == TLS_EXT_SERVERNAME_HOSTNAME )
        {
            ret = ssl->f_sni( ssl->p_sni, ssl, p + 3, hostname_len );
            if( ret != 0 )
            {
                ssl_send_alert_message( ssl, SSL_ALERT_LEVEL_FATAL,
                        SSL_ALERT_MSG_UNRECOGNIZED_NAME );
                return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_HELLO );
            }
            return( 0 );
        }

        servername_list_size -= hostname_len + 3;
        p += hostname_len + 3;
    }

    if( servername_list_size != 0 )
    {
        SSL_DEBUG_MSG( 1, ( "bad client hello message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_HELLO );
    }

    return( 0 );
}

static int ssl_parse_renegotiation_info( ssl_context *ssl,
                                         const unsigned char *buf,
                                         size_t len )
{
    int ret;

    if( ssl->renegotiation == SSL_INITIAL_HANDSHAKE )
    {
        if( len != 1 || buf[0] != 0x0 )
        {
            SSL_DEBUG_MSG( 1, ( "non-zero length renegotiated connection field" ) );

            if( ( ret = ssl_send_fatal_handshake_failure( ssl ) ) != 0 )
                return( ret );

            return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_HELLO );
        }

        ssl->secure_renegotiation = SSL_SECURE_RENEGOTIATION;
    }
    else
    {
        if( len    != 1 + ssl->verify_data_len ||
            buf[0] !=     ssl->verify_data_len ||
            memcmp( buf + 1, ssl->peer_verify_data, ssl->verify_data_len ) != 0 )
        {
            SSL_DEBUG_MSG( 1, ( "non-matching renegotiated connection field" ) );

            if( ( ret = ssl_send_fatal_handshake_failure( ssl ) ) != 0 )
                return( ret );

            return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_HELLO );
        }
    }

    return( 0 );
}

static int ssl_parse_signature_algorithms_ext( ssl_context *ssl,
                                               const unsigned char *buf,
                                               size_t len )
{
    size_t sig_alg_list_size;
    const unsigned char *p;

    sig_alg_list_size = ( ( buf[0] << 8 ) | ( buf[1] ) );
    if( sig_alg_list_size + 2 != len ||
        sig_alg_list_size %2 != 0 )
    {
        SSL_DEBUG_MSG( 1, ( "bad client hello message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_HELLO );
    }

    p = buf + 2;
    while( sig_alg_list_size > 0 )
    {
        if( p[1] != SSL_SIG_RSA )
        {
            sig_alg_list_size -= 2;
            p += 2;
            continue;
        }
#if defined(POLARSSL_SHA4_C)
        if( p[0] == SSL_HASH_SHA512 )
        {
            ssl->handshake->sig_alg = SSL_HASH_SHA512;
            break;
        }
        if( p[0] == SSL_HASH_SHA384 )
        {
            ssl->handshake->sig_alg = SSL_HASH_SHA384;
            break;
        }
#endif
#if defined(POLARSSL_SHA2_C)
        if( p[0] == SSL_HASH_SHA256 )
        {
            ssl->handshake->sig_alg = SSL_HASH_SHA256;
            break;
        }
        if( p[0] == SSL_HASH_SHA224 )
        {
            ssl->handshake->sig_alg = SSL_HASH_SHA224;
            break;
        }
#endif
        if( p[0] == SSL_HASH_SHA1 )
        {
            ssl->handshake->sig_alg = SSL_HASH_SHA1;
            break;
        }
        if( p[0] == SSL_HASH_MD5 )
        {
            ssl->handshake->sig_alg = SSL_HASH_MD5;
            break;
        }

        sig_alg_list_size -= 2;
        p += 2;
    }

    SSL_DEBUG_MSG( 3, ( "client hello v3, signature_algorithm ext: %d",
                   ssl->handshake->sig_alg ) );

    return( 0 );
}

#if defined(POLARSSL_SSL_SRV_SUPPORT_SSLV2_CLIENT_HELLO)
static int ssl_parse_client_hello_v2( ssl_context *ssl )
{
    int ret;
    unsigned int i, j;
    size_t n;
    unsigned int ciph_len, sess_len, chal_len;
    unsigned char *buf, *p;

    SSL_DEBUG_MSG( 2, ( "=> parse client hello v2" ) );

    if( ssl->renegotiation != SSL_INITIAL_HANDSHAKE )
    {
        SSL_DEBUG_MSG( 1, ( "client hello v2 illegal for renegotiation" ) );

        if( ( ret = ssl_send_fatal_handshake_failure( ssl ) ) != 0 )
            return( ret );

        return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_HELLO );
    }

    buf = ssl->in_hdr;

    SSL_DEBUG_BUF( 4, "record header", buf, 5 );

    SSL_DEBUG_MSG( 3, ( "client hello v2, message type: %d",
                   buf[2] ) );
    SSL_DEBUG_MSG( 3, ( "client hello v2, message len.: %d",
                   ( ( buf[0] & 0x7F ) << 8 ) | buf[1] ) );
    SSL_DEBUG_MSG( 3, ( "client hello v2, max. version: [%d:%d]",
                   buf[3], buf[4] ) );

    /*
     * SSLv2 Client Hello
     *
     * Record layer:
     *     0  .   1   message length
     *
     * SSL layer:
     *     2  .   2   message type
     *     3  .   4   protocol version
     */
    if( buf[2] != SSL_HS_CLIENT_HELLO ||
        buf[3] != SSL_MAJOR_VERSION_3 )
    {
        SSL_DEBUG_MSG( 1, ( "bad client hello message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_HELLO );
    }

    n = ( ( buf[0] << 8 ) | buf[1] ) & 0x7FFF;

    if( n < 17 || n > 512 )
    {
        SSL_DEBUG_MSG( 1, ( "bad client hello message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_HELLO );
    }

    ssl->major_ver = SSL_MAJOR_VERSION_3;
    ssl->minor_ver = ( buf[4] <= SSL_MINOR_VERSION_3 )
                     ? buf[4]  : SSL_MINOR_VERSION_3;

    if( ssl->minor_ver < ssl->min_minor_ver )
    {
        SSL_DEBUG_MSG( 1, ( "client only supports ssl smaller than minimum"
                            " [%d:%d] < [%d:%d]", ssl->major_ver, ssl->minor_ver,
                            ssl->min_major_ver, ssl->min_minor_ver ) );

        ssl_send_alert_message( ssl, SSL_ALERT_LEVEL_FATAL,
                                     SSL_ALERT_MSG_PROTOCOL_VERSION );
        return( POLARSSL_ERR_SSL_BAD_HS_PROTOCOL_VERSION );
    }

    ssl->max_major_ver = buf[3];
    ssl->max_minor_ver = buf[4];

    if( ( ret = ssl_fetch_input( ssl, 2 + n ) ) != 0 )
    {
        SSL_DEBUG_RET( 1, "ssl_fetch_input", ret );
        return( ret );
    }

    ssl->handshake->update_checksum( ssl, buf + 2, n );

    buf = ssl->in_msg;
    n = ssl->in_left - 5;

    /*
     *    0  .   1   ciphersuitelist length
     *    2  .   3   session id length
     *    4  .   5   challenge length
     *    6  .  ..   ciphersuitelist
     *   ..  .  ..   session id
     *   ..  .  ..   challenge
     */
    SSL_DEBUG_BUF( 4, "record contents", buf, n );

    ciph_len = ( buf[0] << 8 ) | buf[1];
    sess_len = ( buf[2] << 8 ) | buf[3];
    chal_len = ( buf[4] << 8 ) | buf[5];

    SSL_DEBUG_MSG( 3, ( "ciph_len: %d, sess_len: %d, chal_len: %d",
                   ciph_len, sess_len, chal_len ) );

    /*
     * Make sure each parameter length is valid
     */
    if( ciph_len < 3 || ( ciph_len % 3 ) != 0 )
    {
        SSL_DEBUG_MSG( 1, ( "bad client hello message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_HELLO );
    }

    if( sess_len > 32 )
    {
        SSL_DEBUG_MSG( 1, ( "bad client hello message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_HELLO );
    }

    if( chal_len < 8 || chal_len > 32 )
    {
        SSL_DEBUG_MSG( 1, ( "bad client hello message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_HELLO );
    }

    if( n != 6 + ciph_len + sess_len + chal_len )
    {
        SSL_DEBUG_MSG( 1, ( "bad client hello message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_HELLO );
    }

    SSL_DEBUG_BUF( 3, "client hello, ciphersuitelist",
                   buf + 6, ciph_len );
    SSL_DEBUG_BUF( 3, "client hello, session id",
                   buf + 6 + ciph_len, sess_len );
    SSL_DEBUG_BUF( 3, "client hello, challenge",
                   buf + 6 + ciph_len + sess_len, chal_len );

    p = buf + 6 + ciph_len;
    ssl->session_negotiate->length = sess_len;
    memset( ssl->session_negotiate->id, 0, sizeof( ssl->session_negotiate->id ) );
    memcpy( ssl->session_negotiate->id, p, ssl->session_negotiate->length );

    p += sess_len;
    memset( ssl->handshake->randbytes, 0, 64 );
    memcpy( ssl->handshake->randbytes + 32 - chal_len, p, chal_len );

    /*
     * Check for TLS_EMPTY_RENEGOTIATION_INFO_SCSV
     */
    for( i = 0, p = buf + 6; i < ciph_len; i += 3, p += 3 )
    {
        if( p[0] == 0 && p[1] == 0 && p[2] == SSL_EMPTY_RENEGOTIATION_INFO )
        {
            SSL_DEBUG_MSG( 3, ( "received TLS_EMPTY_RENEGOTIATION_INFO " ) );
            if( ssl->renegotiation == SSL_RENEGOTIATION )
            {
                SSL_DEBUG_MSG( 1, ( "received RENEGOTIATION SCSV during renegotiation" ) );

                if( ( ret = ssl_send_fatal_handshake_failure( ssl ) ) != 0 )
                    return( ret );

                return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_HELLO );
            }
            ssl->secure_renegotiation = SSL_SECURE_RENEGOTIATION;
            break;
        }
    }

    for( i = 0; ssl->ciphersuites[ssl->minor_ver][i] != 0; i++ )
    {
        for( j = 0, p = buf + 6; j < ciph_len; j += 3, p += 3 )
        {
            if( p[0] == 0 &&
                p[1] == 0 &&
                p[2] == ssl->ciphersuites[ssl->minor_ver][i] )
                goto have_ciphersuite_v2;
        }
    }

    SSL_DEBUG_MSG( 1, ( "got no ciphersuites in common" ) );

    return( POLARSSL_ERR_SSL_NO_CIPHER_CHOSEN );

have_ciphersuite_v2:
    ssl->session_negotiate->ciphersuite = ssl->ciphersuites[ssl->minor_ver][i];
    ssl_optimize_checksum( ssl, ssl->session_negotiate->ciphersuite );

    /*
     * SSLv2 Client Hello relevant renegotiation security checks
     */
    if( ssl->secure_renegotiation == SSL_LEGACY_RENEGOTIATION &&
        ssl->allow_legacy_renegotiation == SSL_LEGACY_BREAK_HANDSHAKE )
    {
        SSL_DEBUG_MSG( 1, ( "legacy renegotiation, breaking off handshake" ) );

        if( ( ret = ssl_send_fatal_handshake_failure( ssl ) ) != 0 )
            return( ret );

        return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_HELLO );
    }

    ssl->in_left = 0;
    ssl->state++;

    SSL_DEBUG_MSG( 2, ( "<= parse client hello v2" ) );

    return( 0 );
}
#endif /* POLARSSL_SSL_SRV_SUPPORT_SSLV2_CLIENT_HELLO */

static int ssl_parse_client_hello( ssl_context *ssl )
{
    int ret;
    unsigned int i, j;
    size_t n;
    unsigned int ciph_len, sess_len;
    unsigned int comp_len;
    unsigned int ext_len = 0;
    unsigned char *buf, *p, *ext;
    int renegotiation_info_seen = 0;
    int handshake_failure = 0;

    SSL_DEBUG_MSG( 2, ( "=> parse client hello" ) );

    if( ssl->renegotiation == SSL_INITIAL_HANDSHAKE &&
        ( ret = ssl_fetch_input( ssl, 5 ) ) != 0 )
    {
        SSL_DEBUG_RET( 1, "ssl_fetch_input", ret );
        return( ret );
    }

    buf = ssl->in_hdr;

#if defined(POLARSSL_SSL_SRV_SUPPORT_SSLV2_CLIENT_HELLO)
    if( ( buf[0] & 0x80 ) != 0 )
        return ssl_parse_client_hello_v2( ssl );
#endif

    SSL_DEBUG_BUF( 4, "record header", buf, 5 );

    SSL_DEBUG_MSG( 3, ( "client hello v3, message type: %d",
                   buf[0] ) );
    SSL_DEBUG_MSG( 3, ( "client hello v3, message len.: %d",
                   ( buf[3] << 8 ) | buf[4] ) );
    SSL_DEBUG_MSG( 3, ( "client hello v3, protocol ver: [%d:%d]",
                   buf[1], buf[2] ) );

    /*
     * SSLv3 Client Hello
     *
     * Record layer:
     *     0  .   0   message type
     *     1  .   2   protocol version
     *     3  .   4   message length
     */
    if( buf[0] != SSL_MSG_HANDSHAKE ||
        buf[1] != SSL_MAJOR_VERSION_3 )
    {
        SSL_DEBUG_MSG( 1, ( "bad client hello message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_HELLO );
    }

    n = ( buf[3] << 8 ) | buf[4];

    if( n < 45 || n > 512 )
    {
        SSL_DEBUG_MSG( 1, ( "bad client hello message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_HELLO );
    }

    if( ssl->renegotiation == SSL_INITIAL_HANDSHAKE &&
        ( ret = ssl_fetch_input( ssl, 5 + n ) ) != 0 )
    {
        SSL_DEBUG_RET( 1, "ssl_fetch_input", ret );
        return( ret );
    }

    buf = ssl->in_msg;
    if( !ssl->renegotiation )
        n = ssl->in_left - 5;
    else
        n = ssl->in_msglen;

    ssl->handshake->update_checksum( ssl, buf, n );

    /*
     * SSL layer:
     *     0  .   0   handshake type
     *     1  .   3   handshake length
     *     4  .   5   protocol version
     *     6  .   9   UNIX time()
     *    10  .  37   random bytes
     *    38  .  38   session id length
     *    39  . 38+x  session id
     *   39+x . 40+x  ciphersuitelist length
     *   41+x .  ..   ciphersuitelist
     *    ..  .  ..   compression alg.
     *    ..  .  ..   extensions
     */
    SSL_DEBUG_BUF( 4, "record contents", buf, n );

    SSL_DEBUG_MSG( 3, ( "client hello v3, handshake type: %d",
                   buf[0] ) );
    SSL_DEBUG_MSG( 3, ( "client hello v3, handshake len.: %d",
                   ( buf[1] << 16 ) | ( buf[2] << 8 ) | buf[3] ) );
    SSL_DEBUG_MSG( 3, ( "client hello v3, max. version: [%d:%d]",
                   buf[4], buf[5] ) );

    /*
     * Check the handshake type and protocol version
     */
    if( buf[0] != SSL_HS_CLIENT_HELLO ||
        buf[4] != SSL_MAJOR_VERSION_3 )
    {
        SSL_DEBUG_MSG( 1, ( "bad client hello message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_HELLO );
    }

    ssl->major_ver = SSL_MAJOR_VERSION_3;
    ssl->minor_ver = ( buf[5] <= SSL_MINOR_VERSION_3 )
                     ? buf[5]  : SSL_MINOR_VERSION_3;

    if( ssl->minor_ver < ssl->min_minor_ver )
    {
        SSL_DEBUG_MSG( 1, ( "client only supports ssl smaller than minimum"
                            " [%d:%d] < [%d:%d]", ssl->major_ver, ssl->minor_ver,
                            ssl->min_major_ver, ssl->min_minor_ver ) );

        ssl_send_alert_message( ssl, SSL_ALERT_LEVEL_FATAL,
                                     SSL_ALERT_MSG_PROTOCOL_VERSION );

        return( POLARSSL_ERR_SSL_BAD_HS_PROTOCOL_VERSION );
    }

    ssl->max_major_ver = buf[4];
    ssl->max_minor_ver = buf[5];

    memcpy( ssl->handshake->randbytes, buf + 6, 32 );

    /*
     * Check the handshake message length
     */
    if( buf[1] != 0 || n != (unsigned int) 4 + ( ( buf[2] << 8 ) | buf[3] ) )
    {
        SSL_DEBUG_MSG( 1, ( "bad client hello message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_HELLO );
    }

    /*
     * Check the session length
     */
    sess_len = buf[38];

    if( sess_len > 32 )
    {
        SSL_DEBUG_MSG( 1, ( "bad client hello message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_HELLO );
    }

    ssl->session_negotiate->length = sess_len;
    memset( ssl->session_negotiate->id, 0,
            sizeof( ssl->session_negotiate->id ) );
    memcpy( ssl->session_negotiate->id, buf + 39,
            ssl->session_negotiate->length );

    /*
     * Check the ciphersuitelist length
     */
    ciph_len = ( buf[39 + sess_len] << 8 )
             | ( buf[40 + sess_len]      );

    if( ciph_len < 2 || ciph_len > 256 || ( ciph_len % 2 ) != 0 )
    {
        SSL_DEBUG_MSG( 1, ( "bad client hello message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_HELLO );
    }

    /*
     * Check the compression algorithms length
     */
    comp_len = buf[41 + sess_len + ciph_len];

    if( comp_len < 1 || comp_len > 16 )
    {
        SSL_DEBUG_MSG( 1, ( "bad client hello message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_HELLO );
    }

    /*
     * Check the extension length
     */
    if( n > 42 + sess_len + ciph_len + comp_len )
    {
        ext_len = ( buf[42 + sess_len + ciph_len + comp_len] << 8 )
                | ( buf[43 + sess_len + ciph_len + comp_len]      );

        if( ( ext_len > 0 && ext_len < 4 ) ||
            n != 44 + sess_len + ciph_len + comp_len + ext_len )
        {
            SSL_DEBUG_MSG( 1, ( "bad client hello message" ) );
            SSL_DEBUG_BUF( 3, "Ext", buf + 44 + sess_len + ciph_len + comp_len, ext_len);
            return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_HELLO );
        }
    }

    ssl->session_negotiate->compression = SSL_COMPRESS_NULL;
#if defined(POLARSSL_ZLIB_SUPPORT)
    for( i = 0; i < comp_len; ++i )
    {
        if( buf[42 + sess_len + ciph_len + i] == SSL_COMPRESS_DEFLATE )
        {
            ssl->session_negotiate->compression = SSL_COMPRESS_DEFLATE;
            break;
        }
    }
#endif

    SSL_DEBUG_BUF( 3, "client hello, random bytes",
                   buf +  6,  32 );
    SSL_DEBUG_BUF( 3, "client hello, session id",
                   buf + 38,  sess_len );
    SSL_DEBUG_BUF( 3, "client hello, ciphersuitelist",
                   buf + 41 + sess_len,  ciph_len );
    SSL_DEBUG_BUF( 3, "client hello, compression",
                   buf + 42 + sess_len + ciph_len, comp_len );

    /*
     * Check for TLS_EMPTY_RENEGOTIATION_INFO_SCSV
     */
    for( i = 0, p = buf + 41 + sess_len; i < ciph_len; i += 2, p += 2 )
    {
        if( p[0] == 0 && p[1] == SSL_EMPTY_RENEGOTIATION_INFO )
        {
            SSL_DEBUG_MSG( 3, ( "received TLS_EMPTY_RENEGOTIATION_INFO " ) );
            if( ssl->renegotiation == SSL_RENEGOTIATION )
            {
                SSL_DEBUG_MSG( 1, ( "received RENEGOTIATION SCSV during renegotiation" ) );

                if( ( ret = ssl_send_fatal_handshake_failure( ssl ) ) != 0 )
                    return( ret );

                return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_HELLO );
            }
            ssl->secure_renegotiation = SSL_SECURE_RENEGOTIATION;
            break;
        }
    }

    /*
     * Search for a matching ciphersuite
     */
    for( i = 0; ssl->ciphersuites[ssl->minor_ver][i] != 0; i++ )
    {
        for( j = 0, p = buf + 41 + sess_len; j < ciph_len;
            j += 2, p += 2 )
        {
            if( p[0] == 0 && p[1] == ssl->ciphersuites[ssl->minor_ver][i] )
                goto have_ciphersuite;
        }
    }

    SSL_DEBUG_MSG( 1, ( "got no ciphersuites in common" ) );

    return( POLARSSL_ERR_SSL_NO_CIPHER_CHOSEN );

have_ciphersuite:
    ssl->session_negotiate->ciphersuite = ssl->ciphersuites[ssl->minor_ver][i];
    ssl_optimize_checksum( ssl, ssl->session_negotiate->ciphersuite );

    ext = buf + 44 + sess_len + ciph_len + comp_len;

    while( ext_len )
    {
        unsigned int ext_id   = ( ( ext[0] <<  8 )
                                | ( ext[1]       ) );
        unsigned int ext_size = ( ( ext[2] <<  8 )
                                | ( ext[3]       ) );

        if( ext_size + 4 > ext_len )
        {
            SSL_DEBUG_MSG( 1, ( "bad client hello message" ) );
            return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_HELLO );
        }
        switch( ext_id )
        {
        case TLS_EXT_SERVERNAME:
            SSL_DEBUG_MSG( 3, ( "found ServerName extension" ) );
            if( ssl->f_sni == NULL )
                break;

            ret = ssl_parse_servername_ext( ssl, ext + 4, ext_size );
            if( ret != 0 )
                return( ret );
            break;

        case TLS_EXT_RENEGOTIATION_INFO:
            SSL_DEBUG_MSG( 3, ( "found renegotiation extension" ) );
            renegotiation_info_seen = 1;

            ret = ssl_parse_renegotiation_info( ssl, ext + 4, ext_size );
            if( ret != 0 )
                return( ret );
            break;

        case TLS_EXT_SIG_ALG:
            SSL_DEBUG_MSG( 3, ( "found signature_algorithms extension" ) );
            if( ssl->renegotiation == SSL_RENEGOTIATION )
                break;

            ret = ssl_parse_signature_algorithms_ext( ssl, ext + 4, ext_size );
            if( ret != 0 )
                return( ret );
            break;

        default:
            SSL_DEBUG_MSG( 3, ( "unknown extension found: %d (ignoring)",
                           ext_id ) );
        }

        ext_len -= 4 + ext_size;
        ext += 4 + ext_size;

        if( ext_len > 0 && ext_len < 4 )
        {
            SSL_DEBUG_MSG( 1, ( "bad client hello message" ) );
            return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_HELLO );
        }
    }

    /*
     * Renegotiation security checks
     */
    if( ssl->secure_renegotiation == SSL_LEGACY_RENEGOTIATION &&
        ssl->allow_legacy_renegotiation == SSL_LEGACY_BREAK_HANDSHAKE )
    {
        SSL_DEBUG_MSG( 1, ( "legacy renegotiation, breaking off handshake" ) );
        handshake_failure = 1;
    }
    else if( ssl->renegotiation == SSL_RENEGOTIATION &&
             ssl->secure_renegotiation == SSL_SECURE_RENEGOTIATION &&
             renegotiation_info_seen == 0 )
    {
        SSL_DEBUG_MSG( 1, ( "renegotiation_info extension missing (secure)" ) );
        handshake_failure = 1;
    }
    else if( ssl->renegotiation == SSL_RENEGOTIATION &&
             ssl->secure_renegotiation == SSL_LEGACY_RENEGOTIATION &&
             ssl->allow_legacy_renegotiation == SSL_LEGACY_NO_RENEGOTIATION )
    {
        SSL_DEBUG_MSG( 1, ( "legacy renegotiation not allowed" ) );
        handshake_failure = 1;
    }
    else if( ssl->renegotiation == SSL_RENEGOTIATION &&
             ssl->secure_renegotiation == SSL_LEGACY_RENEGOTIATION &&
             renegotiation_info_seen == 1 )
    {
        SSL_DEBUG_MSG( 1, ( "renegotiation_info extension present (legacy)" ) );
        handshake_failure = 1;
    }

    if( handshake_failure == 1 )
    {
        if( ( ret = ssl_send_fatal_handshake_failure( ssl ) ) != 0 )
            return( ret );

        return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_HELLO );
    }

    ssl->in_left = 0;
    ssl->state++;

    SSL_DEBUG_MSG( 2, ( "<= parse client hello" ) );

    return( 0 );
}

static int ssl_write_server_hello( ssl_context *ssl )
{
    time_t t;
    int ret, n;
    size_t ext_len = 0;
    unsigned char *buf, *p;

    SSL_DEBUG_MSG( 2, ( "=> write server hello" ) );

    /*
     *     0  .   0   handshake type
     *     1  .   3   handshake length
     *     4  .   5   protocol version
     *     6  .   9   UNIX time()
     *    10  .  37   random bytes
     */
    buf = ssl->out_msg;
    p = buf + 4;

    *p++ = (unsigned char) ssl->major_ver;
    *p++ = (unsigned char) ssl->minor_ver;

    SSL_DEBUG_MSG( 3, ( "server hello, chosen version: [%d:%d]",
                   buf[4], buf[5] ) );

    t = time( NULL );
    *p++ = (unsigned char)( t >> 24 );
    *p++ = (unsigned char)( t >> 16 );
    *p++ = (unsigned char)( t >>  8 );
    *p++ = (unsigned char)( t       );

    SSL_DEBUG_MSG( 3, ( "server hello, current time: %lu", t ) );

    if( ( ret = ssl->f_rng( ssl->p_rng, p, 28 ) ) != 0 )
        return( ret );

    p += 28;

    memcpy( ssl->handshake->randbytes + 32, buf + 6, 32 );

    SSL_DEBUG_BUF( 3, "server hello, random bytes", buf + 6, 32 );

    /*
     *    38  .  38   session id length
     *    39  . 38+n  session id
     *   39+n . 40+n  chosen ciphersuite
     *   41+n . 41+n  chosen compression alg.
     */
    ssl->session_negotiate->length = n = 32;
    *p++ = (unsigned char) ssl->session_negotiate->length;

    if( ssl->renegotiation != SSL_INITIAL_HANDSHAKE ||
        ssl->f_get_cache == NULL ||
        ssl->f_get_cache( ssl->p_get_cache, ssl->session_negotiate ) != 0 )
    {
        /*
         * Not found, create a new session id
         */
        ssl->handshake->resume = 0;
        ssl->state++;

        if( ( ret = ssl->f_rng( ssl->p_rng, ssl->session_negotiate->id,
                                n ) ) != 0 )
            return( ret );
    }
    else
    {
        /*
         * Found a matching session, resuming it
         */
        ssl->handshake->resume = 1;
        ssl->state = SSL_SERVER_CHANGE_CIPHER_SPEC;

        if( ( ret = ssl_derive_keys( ssl ) ) != 0 )
        {
            SSL_DEBUG_RET( 1, "ssl_derive_keys", ret );
            return( ret );
        }
    }

    memcpy( p, ssl->session_negotiate->id, ssl->session_negotiate->length );
    p += ssl->session_negotiate->length;

    SSL_DEBUG_MSG( 3, ( "server hello, session id len.: %d", n ) );
    SSL_DEBUG_BUF( 3,   "server hello, session id", buf + 39, n );
    SSL_DEBUG_MSG( 3, ( "%s session has been resumed",
                   ssl->handshake->resume ? "a" : "no" ) );

    *p++ = (unsigned char)( ssl->session_negotiate->ciphersuite >> 8 );
    *p++ = (unsigned char)( ssl->session_negotiate->ciphersuite      );
    *p++ = (unsigned char)( ssl->session_negotiate->compression      );

    SSL_DEBUG_MSG( 3, ( "server hello, chosen ciphersuite: %d",
                   ssl->session_negotiate->ciphersuite ) );
    SSL_DEBUG_MSG( 3, ( "server hello, compress alg.: %d",
                   ssl->session_negotiate->compression ) );

    if( ssl->secure_renegotiation == SSL_SECURE_RENEGOTIATION )
    {
        SSL_DEBUG_MSG( 3, ( "server hello, prepping for secure renegotiation extension" ) );
        ext_len += 5 + ssl->verify_data_len * 2;

        SSL_DEBUG_MSG( 3, ( "server hello, total extension length: %d",
                       ext_len ) );

        *p++ = (unsigned char)( ( ext_len >> 8 ) & 0xFF );
        *p++ = (unsigned char)( ( ext_len      ) & 0xFF );

        /*
         * Secure renegotiation
         */
        SSL_DEBUG_MSG( 3, ( "client hello, secure renegotiation extension" ) );

        *p++ = (unsigned char)( ( TLS_EXT_RENEGOTIATION_INFO >> 8 ) & 0xFF );
        *p++ = (unsigned char)( ( TLS_EXT_RENEGOTIATION_INFO      ) & 0xFF );

        *p++ = 0x00;
        *p++ = ( ssl->verify_data_len * 2 + 1 ) & 0xFF;
        *p++ = ssl->verify_data_len * 2 & 0xFF;

        memcpy( p, ssl->peer_verify_data, ssl->verify_data_len );
        p += ssl->verify_data_len;
        memcpy( p, ssl->own_verify_data, ssl->verify_data_len );
        p += ssl->verify_data_len;
    }

    ssl->out_msglen  = p - buf;
    ssl->out_msgtype = SSL_MSG_HANDSHAKE;
    ssl->out_msg[0]  = SSL_HS_SERVER_HELLO;

    ret = ssl_write_record( ssl );

    SSL_DEBUG_MSG( 2, ( "<= write server hello" ) );

    return( ret );
}

static int ssl_write_certificate_request( ssl_context *ssl )
{
    int ret;
    size_t n = 0, dn_size, total_dn_size;
    unsigned char *buf, *p;
    const x509_cert *crt;

    SSL_DEBUG_MSG( 2, ( "=> write certificate request" ) );

    ssl->state++;

    if( ssl->authmode == SSL_VERIFY_NONE )
    {
        SSL_DEBUG_MSG( 2, ( "<= skip write certificate request" ) );
        return( 0 );
    }

    /*
     *     0  .   0   handshake type
     *     1  .   3   handshake length
     *     4  .   4   cert type count
     *     5  .. m-1  cert types
     *     m  .. m+1  sig alg length (TLS 1.2 only)
     *    m+1 .. n-1  SignatureAndHashAlgorithms (TLS 1.2 only) 
     *     n  .. n+1  length of all DNs
     *    n+2 .. n+3  length of DN 1
     *    n+4 .. ...  Distinguished Name #1
     *    ... .. ...  length of DN 2, etc.
     */
    buf = ssl->out_msg;
    p = buf + 4;

    /*
     * At the moment, only RSA certificates are supported
     */
    *p++ = 1;
    *p++ = SSL_CERT_TYPE_RSA_SIGN;

    /*
     * Add signature_algorithms for verify (TLS 1.2)
     * Only add current running algorithm that is already required for
     * requested ciphersuite.
     *
     * Length is always 2
     */
    if( ssl->minor_ver == SSL_MINOR_VERSION_3 )
    {
        ssl->handshake->verify_sig_alg = SSL_HASH_SHA256;

        *p++ = 0;
        *p++ = 2;

        if( ssl->session_negotiate->ciphersuite == TLS_RSA_WITH_AES_256_GCM_SHA384 ||
            ssl->session_negotiate->ciphersuite == TLS_DHE_RSA_WITH_AES_256_GCM_SHA384 )
        {
            ssl->handshake->verify_sig_alg = SSL_HASH_SHA384;
        }
        
        *p++ = ssl->handshake->verify_sig_alg;
        *p++ = SSL_SIG_RSA;

        n += 4;
    }

    p += 2;
    crt = ssl->ca_chain;

    total_dn_size = 0;
    while( crt != NULL && crt->version != 0)
    {
        if( p - buf > 4096 )
            break;

        dn_size = crt->subject_raw.len;
        *p++ = (unsigned char)( dn_size >> 8 );
        *p++ = (unsigned char)( dn_size      );
        memcpy( p, crt->subject_raw.p, dn_size );
        p += dn_size;

        SSL_DEBUG_BUF( 3, "requested DN", p, dn_size );

        total_dn_size += 2 + dn_size;
        crt = crt->next;
    }

    ssl->out_msglen  = p - buf;
    ssl->out_msgtype = SSL_MSG_HANDSHAKE;
    ssl->out_msg[0]  = SSL_HS_CERTIFICATE_REQUEST;
    ssl->out_msg[6 + n]  = (unsigned char)( total_dn_size  >> 8 );
    ssl->out_msg[7 + n]  = (unsigned char)( total_dn_size       );

    ret = ssl_write_record( ssl );

    SSL_DEBUG_MSG( 2, ( "<= write certificate request" ) );

    return( ret );
}

static int ssl_write_server_key_exchange( ssl_context *ssl )
{
#if defined(POLARSSL_DHM_C)
    int ret;
    size_t n, rsa_key_len = 0;
    unsigned char hash[64];
    int hash_id = 0;
    unsigned int hashlen = 0;
#endif

    SSL_DEBUG_MSG( 2, ( "=> write server key exchange" ) );

    if( ssl->session_negotiate->ciphersuite != TLS_DHE_RSA_WITH_DES_CBC_SHA &&
        ssl->session_negotiate->ciphersuite != TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA &&
        ssl->session_negotiate->ciphersuite != TLS_DHE_RSA_WITH_AES_128_CBC_SHA &&
        ssl->session_negotiate->ciphersuite != TLS_DHE_RSA_WITH_AES_256_CBC_SHA &&
        ssl->session_negotiate->ciphersuite != TLS_DHE_RSA_WITH_AES_128_CBC_SHA256 &&
        ssl->session_negotiate->ciphersuite != TLS_DHE_RSA_WITH_AES_256_CBC_SHA256 &&
        ssl->session_negotiate->ciphersuite != TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA &&
        ssl->session_negotiate->ciphersuite != TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA &&
        ssl->session_negotiate->ciphersuite != TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA256 &&
        ssl->session_negotiate->ciphersuite != TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA256 &&
        ssl->session_negotiate->ciphersuite != TLS_DHE_RSA_WITH_AES_128_GCM_SHA256 &&
        ssl->session_negotiate->ciphersuite != TLS_DHE_RSA_WITH_AES_256_GCM_SHA384 )
    {
        SSL_DEBUG_MSG( 2, ( "<= skip write server key exchange" ) );
        ssl->state++;
        return( 0 );
    }

#if !defined(POLARSSL_DHM_C)
    SSL_DEBUG_MSG( 1, ( "support for dhm is not available" ) );
    return( POLARSSL_ERR_SSL_FEATURE_UNAVAILABLE );
#else

    if( ssl->rsa_key == NULL )
    {
        SSL_DEBUG_MSG( 1, ( "got no private key" ) );
        return( POLARSSL_ERR_SSL_PRIVATE_KEY_REQUIRED );
    }

    /*
     * Ephemeral DH parameters:
     *
     * struct {
     *     opaque dh_p<1..2^16-1>;
     *     opaque dh_g<1..2^16-1>;
     *     opaque dh_Ys<1..2^16-1>;
     * } ServerDHParams;
     */
    if( ( ret = mpi_copy( &ssl->handshake->dhm_ctx.P, &ssl->dhm_P ) ) != 0 ||
        ( ret = mpi_copy( &ssl->handshake->dhm_ctx.G, &ssl->dhm_G ) ) != 0 )
    {
        SSL_DEBUG_RET( 1, "mpi_copy", ret );
        return( ret );
    }

    if( ( ret = dhm_make_params( &ssl->handshake->dhm_ctx,
                                  mpi_size( &ssl->handshake->dhm_ctx.P ),
                                  ssl->out_msg + 4,
                                  &n, ssl->f_rng, ssl->p_rng ) ) != 0 )
    {
        SSL_DEBUG_RET( 1, "dhm_make_params", ret );
        return( ret );
    }

    SSL_DEBUG_MPI( 3, "DHM: X ", &ssl->handshake->dhm_ctx.X  );
    SSL_DEBUG_MPI( 3, "DHM: P ", &ssl->handshake->dhm_ctx.P  );
    SSL_DEBUG_MPI( 3, "DHM: G ", &ssl->handshake->dhm_ctx.G  );
    SSL_DEBUG_MPI( 3, "DHM: GX", &ssl->handshake->dhm_ctx.GX );

    if( ssl->minor_ver != SSL_MINOR_VERSION_3 )
    {
        md5_context md5;
        sha1_context sha1;

        /*
         * digitally-signed struct {
         *     opaque md5_hash[16];
         *     opaque sha_hash[20];
         * };
         *
         * md5_hash
         *     MD5(ClientHello.random + ServerHello.random
         *                            + ServerParams);
         * sha_hash
         *     SHA(ClientHello.random + ServerHello.random
         *                            + ServerParams);
         */
        md5_starts( &md5 );
        md5_update( &md5, ssl->handshake->randbytes,  64 );
        md5_update( &md5, ssl->out_msg + 4, n );
        md5_finish( &md5, hash );

        sha1_starts( &sha1 );
        sha1_update( &sha1, ssl->handshake->randbytes,  64 );
        sha1_update( &sha1, ssl->out_msg + 4, n );
        sha1_finish( &sha1, hash + 16 );

        hashlen = 36;
        hash_id = SIG_RSA_RAW;
    }
    else
    {
        /*
         * digitally-signed struct {
         *     opaque client_random[32];
         *     opaque server_random[32];
         *     ServerDHParams params;
         * };
         */
#if defined(POLARSSL_SHA4_C)
        if( ssl->handshake->sig_alg == SSL_HASH_SHA512 )
        {
            sha4_context sha4;

            sha4_starts( &sha4, 0 );
            sha4_update( &sha4, ssl->handshake->randbytes, 64 );
            sha4_update( &sha4, ssl->out_msg + 4, n );
            sha4_finish( &sha4, hash );

            hashlen = 64;
            hash_id = SIG_RSA_SHA512;
        }
        else if( ssl->handshake->sig_alg == SSL_HASH_SHA384 )
        {
            sha4_context sha4;

            sha4_starts( &sha4, 1 );
            sha4_update( &sha4, ssl->handshake->randbytes, 64 );
            sha4_update( &sha4, ssl->out_msg + 4, n );
            sha4_finish( &sha4, hash );

            hashlen = 48;
            hash_id = SIG_RSA_SHA384;
        }
        else
#endif
#if defined(POLARSSL_SHA2_C)
        if( ssl->handshake->sig_alg == SSL_HASH_SHA256 )
        {
            sha2_context sha2;

            sha2_starts( &sha2, 0 );
            sha2_update( &sha2, ssl->handshake->randbytes, 64 );
            sha2_update( &sha2, ssl->out_msg + 4, n );
            sha2_finish( &sha2, hash );

            hashlen = 32;
            hash_id = SIG_RSA_SHA256;
        }
        else if( ssl->handshake->sig_alg == SSL_HASH_SHA224 )
        {
            sha2_context sha2;

            sha2_starts( &sha2, 1 );
            sha2_update( &sha2, ssl->handshake->randbytes, 64 );
            sha2_update( &sha2, ssl->out_msg + 4, n );
            sha2_finish( &sha2, hash );

            hashlen = 24;
            hash_id = SIG_RSA_SHA224;
        }
        else
#endif
        if( ssl->handshake->sig_alg == SSL_HASH_SHA1 )
        {
            sha1_context sha1;

            sha1_starts( &sha1 );
            sha1_update( &sha1, ssl->handshake->randbytes, 64 );
            sha1_update( &sha1, ssl->out_msg + 4, n );
            sha1_finish( &sha1, hash );

            hashlen = 20;
            hash_id = SIG_RSA_SHA1;
        }
        else if( ssl->handshake->sig_alg == SSL_HASH_MD5 )
        {
            md5_context md5;

            md5_starts( &md5 );
            md5_update( &md5, ssl->handshake->randbytes, 64 );
            md5_update( &md5, ssl->out_msg + 4, n );
            md5_finish( &md5, hash );

            hashlen = 16;
            hash_id = SIG_RSA_MD5;
        }
    }

    SSL_DEBUG_BUF( 3, "parameters hash", hash, hashlen );

    if ( ssl->rsa_key )
        rsa_key_len = ssl->rsa_key_len( ssl->rsa_key );

    if( ssl->minor_ver == SSL_MINOR_VERSION_3 )
    {
        ssl->out_msg[4 + n] = ssl->handshake->sig_alg;
        ssl->out_msg[5 + n] = SSL_SIG_RSA;

        n += 2;
    }

    ssl->out_msg[4 + n] = (unsigned char)( rsa_key_len >> 8 );
    ssl->out_msg[5 + n] = (unsigned char)( rsa_key_len      );

    if ( ssl->rsa_key )
    {
        ret = ssl->rsa_sign( ssl->rsa_key, ssl->f_rng, ssl->p_rng,
                             RSA_PRIVATE,
                             hash_id, hashlen, hash,
                             ssl->out_msg + 6 + n );
    }

    if( ret != 0 )
    {
        SSL_DEBUG_RET( 1, "pkcs1_sign", ret );
        return( ret );
    }

    SSL_DEBUG_BUF( 3, "my RSA sig", ssl->out_msg + 6 + n, rsa_key_len );

    ssl->out_msglen  = 6 + n + rsa_key_len;
    ssl->out_msgtype = SSL_MSG_HANDSHAKE;
    ssl->out_msg[0]  = SSL_HS_SERVER_KEY_EXCHANGE;

    ssl->state++;

    if( ( ret = ssl_write_record( ssl ) ) != 0 )
    {
        SSL_DEBUG_RET( 1, "ssl_write_record", ret );
        return( ret );
    }

    SSL_DEBUG_MSG( 2, ( "<= write server key exchange" ) );

    return( 0 );
#endif
}

static int ssl_write_server_hello_done( ssl_context *ssl )
{
    int ret;

    SSL_DEBUG_MSG( 2, ( "=> write server hello done" ) );

    ssl->out_msglen  = 4;
    ssl->out_msgtype = SSL_MSG_HANDSHAKE;
    ssl->out_msg[0]  = SSL_HS_SERVER_HELLO_DONE;

    ssl->state++;

    if( ( ret = ssl_write_record( ssl ) ) != 0 )
    {
        SSL_DEBUG_RET( 1, "ssl_write_record", ret );
        return( ret );
    }

    SSL_DEBUG_MSG( 2, ( "<= write server hello done" ) );

    return( 0 );
}

static int ssl_parse_client_key_exchange( ssl_context *ssl )
{
    int ret;
    size_t i, n = 0;

    SSL_DEBUG_MSG( 2, ( "=> parse client key exchange" ) );

    if( ( ret = ssl_read_record( ssl ) ) != 0 )
    {
        SSL_DEBUG_RET( 1, "ssl_read_record", ret );
        return( ret );
    }

    if( ssl->in_msgtype != SSL_MSG_HANDSHAKE )
    {
        SSL_DEBUG_MSG( 1, ( "bad client key exchange message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE );
    }

    if( ssl->in_msg[0] != SSL_HS_CLIENT_KEY_EXCHANGE )
    {
        SSL_DEBUG_MSG( 1, ( "bad client key exchange message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE );
    }

    if( ssl->session_negotiate->ciphersuite == TLS_DHE_RSA_WITH_DES_CBC_SHA ||
        ssl->session_negotiate->ciphersuite == TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA ||
        ssl->session_negotiate->ciphersuite == TLS_DHE_RSA_WITH_AES_128_CBC_SHA ||
        ssl->session_negotiate->ciphersuite == TLS_DHE_RSA_WITH_AES_256_CBC_SHA ||
        ssl->session_negotiate->ciphersuite == TLS_DHE_RSA_WITH_AES_128_CBC_SHA256 ||
        ssl->session_negotiate->ciphersuite == TLS_DHE_RSA_WITH_AES_256_CBC_SHA256 ||
        ssl->session_negotiate->ciphersuite == TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA ||
        ssl->session_negotiate->ciphersuite == TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA ||
        ssl->session_negotiate->ciphersuite == TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA256 ||
        ssl->session_negotiate->ciphersuite == TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA256 ||
        ssl->session_negotiate->ciphersuite == TLS_DHE_RSA_WITH_AES_128_GCM_SHA256 ||
        ssl->session_negotiate->ciphersuite == TLS_DHE_RSA_WITH_AES_256_GCM_SHA384 )
    {
#if !defined(POLARSSL_DHM_C)
        SSL_DEBUG_MSG( 1, ( "support for dhm is not available" ) );
        return( POLARSSL_ERR_SSL_FEATURE_UNAVAILABLE );
#else
        /*
         * Receive G^Y mod P, premaster = (G^Y)^X mod P
         */
        n = ( ssl->in_msg[4] << 8 ) | ssl->in_msg[5];

        if( n < 1 || n > ssl->handshake->dhm_ctx.len ||
            n + 6 != ssl->in_hslen )
        {
            SSL_DEBUG_MSG( 1, ( "bad client key exchange message" ) );
            return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE );
        }

        if( ( ret = dhm_read_public( &ssl->handshake->dhm_ctx,
                                      ssl->in_msg + 6, n ) ) != 0 )
        {
            SSL_DEBUG_RET( 1, "dhm_read_public", ret );
            return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE_DHM_RP );
        }

        SSL_DEBUG_MPI( 3, "DHM: GY", &ssl->handshake->dhm_ctx.GY );

        ssl->handshake->pmslen = ssl->handshake->dhm_ctx.len;

        if( ( ret = dhm_calc_secret( &ssl->handshake->dhm_ctx,
                                      ssl->handshake->premaster,
                                     &ssl->handshake->pmslen ) ) != 0 )
        {
            SSL_DEBUG_RET( 1, "dhm_calc_secret", ret );
            return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE_DHM_CS );
        }

        SSL_DEBUG_MPI( 3, "DHM: K ", &ssl->handshake->dhm_ctx.K  );
#endif
    }
    else
    {
        if( ssl->rsa_key == NULL )
        {
            SSL_DEBUG_MSG( 1, ( "got no private key" ) );
            return( POLARSSL_ERR_SSL_PRIVATE_KEY_REQUIRED );
        }

        /*
         * Decrypt the premaster using own private RSA key
         */
        i = 4;
        if( ssl->rsa_key )
            n = ssl->rsa_key_len( ssl->rsa_key );
        ssl->handshake->pmslen = 48;

        if( ssl->minor_ver != SSL_MINOR_VERSION_0 )
        {
            i += 2;
            if( ssl->in_msg[4] != ( ( n >> 8 ) & 0xFF ) ||
                ssl->in_msg[5] != ( ( n      ) & 0xFF ) )
            {
                SSL_DEBUG_MSG( 1, ( "bad client key exchange message" ) );
                return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE );
            }
        }

        if( ssl->in_hslen != i + n )
        {
            SSL_DEBUG_MSG( 1, ( "bad client key exchange message" ) );
            return( POLARSSL_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE );
        }

        if( ssl->rsa_key ) {
            ret = ssl->rsa_decrypt( ssl->rsa_key, RSA_PRIVATE,
                                   &ssl->handshake->pmslen,
                                    ssl->in_msg + i,
                                    ssl->handshake->premaster,
                                    sizeof(ssl->handshake->premaster) );
        }

        if( ret != 0 || ssl->handshake->pmslen != 48 ||
            ssl->handshake->premaster[0] != ssl->max_major_ver ||
            ssl->handshake->premaster[1] != ssl->max_minor_ver )
        {
            SSL_DEBUG_MSG( 1, ( "bad client key exchange message" ) );

            /*
             * Protection against Bleichenbacher's attack:
             * invalid PKCS#1 v1.5 padding must not cause
             * the connection to end immediately; instead,
             * send a bad_record_mac later in the handshake.
             */
            ssl->handshake->pmslen = 48;

            ret = ssl->f_rng( ssl->p_rng, ssl->handshake->premaster,
                              ssl->handshake->pmslen );
            if( ret != 0 )
                return( ret );
        }
    }

    if( ( ret = ssl_derive_keys( ssl ) ) != 0 )
    {
        SSL_DEBUG_RET( 1, "ssl_derive_keys", ret );
        return( ret );
    }

    ssl->state++;

    SSL_DEBUG_MSG( 2, ( "<= parse client key exchange" ) );

    return( 0 );
}

static int ssl_parse_certificate_verify( ssl_context *ssl )
{
    int ret;
    size_t n = 0, n1, n2;
    unsigned char hash[48];
    int hash_id;
    unsigned int hashlen;

    SSL_DEBUG_MSG( 2, ( "=> parse certificate verify" ) );

    if( ssl->session_negotiate->peer_cert == NULL )
    {
        SSL_DEBUG_MSG( 2, ( "<= skip parse certificate verify" ) );
        ssl->state++;
        return( 0 );
    }

    ssl->handshake->calc_verify( ssl, hash );

    if( ( ret = ssl_read_record( ssl ) ) != 0 )
    {
        SSL_DEBUG_RET( 1, "ssl_read_record", ret );
        return( ret );
    }

    ssl->state++;

    if( ssl->in_msgtype != SSL_MSG_HANDSHAKE )
    {
        SSL_DEBUG_MSG( 1, ( "bad certificate verify message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_CERTIFICATE_VERIFY );
    }

    if( ssl->in_msg[0] != SSL_HS_CERTIFICATE_VERIFY )
    {
        SSL_DEBUG_MSG( 1, ( "bad certificate verify message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_CERTIFICATE_VERIFY );
    }

    if( ssl->minor_ver == SSL_MINOR_VERSION_3 )
    {
        /*
         * As server we know we either have SSL_HASH_SHA384 or
         * SSL_HASH_SHA256
         */
        if( ssl->in_msg[4] != ssl->handshake->verify_sig_alg ||
            ssl->in_msg[5] != SSL_SIG_RSA )
        {
            SSL_DEBUG_MSG( 1, ( "peer not adhering to requested sig_alg for verify message" ) );
            return( POLARSSL_ERR_SSL_BAD_HS_CERTIFICATE_VERIFY );
        }

        if( ssl->handshake->verify_sig_alg == SSL_HASH_SHA384 )
        {
            hashlen = 48;
            hash_id = SIG_RSA_SHA384;
        }
        else
        {
            hashlen = 32;
            hash_id = SIG_RSA_SHA256;
        }

        n += 2;
    }
    else
    {
        hashlen = 36;
        hash_id = SIG_RSA_RAW;
    }

    n1 = ssl->session_negotiate->peer_cert->rsa.len;
    n2 = ( ssl->in_msg[4 + n] << 8 ) | ssl->in_msg[5 + n];

    if( n + n1 + 6 != ssl->in_hslen || n1 != n2 )
    {
        SSL_DEBUG_MSG( 1, ( "bad certificate verify message" ) );
        return( POLARSSL_ERR_SSL_BAD_HS_CERTIFICATE_VERIFY );
    }

    ret = rsa_pkcs1_verify( &ssl->session_negotiate->peer_cert->rsa, RSA_PUBLIC,
                            hash_id, hashlen, hash, ssl->in_msg + 6 + n );
    if( ret != 0 )
    {
        SSL_DEBUG_RET( 1, "rsa_pkcs1_verify", ret );
        return( ret );
    }

    SSL_DEBUG_MSG( 2, ( "<= parse certificate verify" ) );

    return( 0 );
}

/*
 * SSL handshake -- server side -- single step
 */
int ssl_handshake_server_step( ssl_context *ssl )
{
    int ret = 0;

    if( ssl->state == SSL_HANDSHAKE_OVER )
        return( POLARSSL_ERR_SSL_BAD_INPUT_DATA );

    SSL_DEBUG_MSG( 2, ( "server state: %d", ssl->state ) );

    if( ( ret = ssl_flush_output( ssl ) ) != 0 )
        return( ret );

    switch( ssl->state )
    {
        case SSL_HELLO_REQUEST:
            ssl->state = SSL_CLIENT_HELLO;
            break;

        /*
         *  <==   ClientHello
         */
        case SSL_CLIENT_HELLO:
            ret = ssl_parse_client_hello( ssl );
            break;

        /*
         *  ==>   ServerHello
         *        Certificate
         *      ( ServerKeyExchange  )
         *      ( CertificateRequest )
         *        ServerHelloDone
         */
        case SSL_SERVER_HELLO:
            ret = ssl_write_server_hello( ssl );
            break;

        case SSL_SERVER_CERTIFICATE:
            ret = ssl_write_certificate( ssl );
            break;

        case SSL_SERVER_KEY_EXCHANGE:
            ret = ssl_write_server_key_exchange( ssl );
            break;

        case SSL_CERTIFICATE_REQUEST:
            ret = ssl_write_certificate_request( ssl );
            break;

        case SSL_SERVER_HELLO_DONE:
            ret = ssl_write_server_hello_done( ssl );
            break;

        /*
         *  <== ( Certificate/Alert  )
         *        ClientKeyExchange
         *      ( CertificateVerify  )
         *        ChangeCipherSpec
         *        Finished
         */
        case SSL_CLIENT_CERTIFICATE:
            ret = ssl_parse_certificate( ssl );
            break;

        case SSL_CLIENT_KEY_EXCHANGE:
            ret = ssl_parse_client_key_exchange( ssl );
            break;

        case SSL_CERTIFICATE_VERIFY:
            ret = ssl_parse_certificate_verify( ssl );
            break;

        case SSL_CLIENT_CHANGE_CIPHER_SPEC:
            ret = ssl_parse_change_cipher_spec( ssl );
            break;

        case SSL_CLIENT_FINISHED:
            ret = ssl_parse_finished( ssl );
            break;

        /*
         *  ==>   ChangeCipherSpec
         *        Finished
         */
        case SSL_SERVER_CHANGE_CIPHER_SPEC:
            ret = ssl_write_change_cipher_spec( ssl );
            break;

        case SSL_SERVER_FINISHED:
            ret = ssl_write_finished( ssl );
            break;

        case SSL_FLUSH_BUFFERS:
            SSL_DEBUG_MSG( 2, ( "handshake: done" ) );
            ssl->state = SSL_HANDSHAKE_WRAPUP;
            break;

        case SSL_HANDSHAKE_WRAPUP:
            ssl_handshake_wrapup( ssl );
            break;

        default:
            SSL_DEBUG_MSG( 1, ( "invalid state %d", ssl->state ) );
            return( POLARSSL_ERR_SSL_BAD_INPUT_DATA );
    }

    return( ret );
}
#endif
