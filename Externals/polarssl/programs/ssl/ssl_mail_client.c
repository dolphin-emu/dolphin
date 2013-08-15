/*
 *  SSL client for SMTP servers
 *
 *  Copyright (C) 2006-2011, Brainspark B.V.
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

#ifndef _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE 1
#endif

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#if defined(_WIN32) || defined(_WIN32_WCE)

#include <winsock2.h>
#include <windows.h>

#if defined(_WIN32_WCE)
#pragma comment( lib, "ws2.lib" )
#else
#pragma comment( lib, "ws2_32.lib" )
#endif
#endif

#include "polarssl/config.h"

#include "polarssl/base64.h"
#include "polarssl/error.h"
#include "polarssl/net.h"
#include "polarssl/ssl.h"
#include "polarssl/entropy.h"
#include "polarssl/ctr_drbg.h"
#include "polarssl/certs.h"
#include "polarssl/x509.h"

#define DFL_SERVER_NAME         "localhost"
#define DFL_SERVER_PORT         465
#define DFL_USER_NAME           "user"
#define DFL_USER_PWD            "password"
#define DFL_MAIL_FROM           ""
#define DFL_MAIL_TO             ""
#define DFL_DEBUG_LEVEL         0
#define DFL_CA_FILE             ""
#define DFL_CRT_FILE            ""
#define DFL_KEY_FILE            ""
#define DFL_FORCE_CIPHER        0
#define DFL_MODE                0
#define DFL_AUTHENTICATION      0

#define MODE_SSL_TLS            0
#define MODE_STARTTLS           0

/*
 * global options
 */
struct options
{
    const char *server_name;    /* hostname of the server (client only)     */
    int server_port;            /* port on which the ssl service runs       */
    int debug_level;            /* level of debugging                       */
    int authentication;         /* if authentication is required            */
    int mode;                   /* SSL/TLS (0) or STARTTLS (1)              */
    const char *user_name;      /* username to use for authentication       */
    const char *user_pwd;       /* password to use for authentication       */
    const char *mail_from;      /* E-Mail address to use as sender          */
    const char *mail_to;        /* E-Mail address to use as recipient       */
    const char *ca_file;        /* the file with the CA certificate(s)      */
    const char *crt_file;       /* the file with the client certificate     */
    const char *key_file;       /* the file with the client key             */
    int force_ciphersuite[2];   /* protocol/ciphersuite to use, or all      */
} opt;

void my_debug( void *ctx, int level, const char *str )
{
    if( level < opt.debug_level )
    {
        fprintf( (FILE *) ctx, "%s", str );
        fflush(  (FILE *) ctx  );
    }
}

#if !defined(POLARSSL_BIGNUM_C) || !defined(POLARSSL_ENTROPY_C) ||  \
    !defined(POLARSSL_SSL_TLS_C) || !defined(POLARSSL_SSL_CLI_C) || \
    !defined(POLARSSL_NET_C) || !defined(POLARSSL_RSA_C) ||         \
    !defined(POLARSSL_CTR_DRBG_C)
int main( int argc, char *argv[] )
{
    ((void) argc);
    ((void) argv);

    printf("POLARSSL_BIGNUM_C and/or POLARSSL_ENTROPY_C and/or "
           "POLARSSL_SSL_TLS_C and/or POLARSSL_SSL_CLI_C and/or "
           "POLARSSL_NET_C and/or POLARSSL_RSA_C and/or "
           "POLARSSL_CTR_DRBG_C not defined.\n");
    return( 0 );
}
#else
int do_handshake( ssl_context *ssl, struct options *opt )
{
    int ret;
    unsigned char buf[1024];
    memset(buf, 0, 1024);

    /*
     * 4. Handshake
     */
    printf( "  . Performing the SSL/TLS handshake..." );
    fflush( stdout );

    while( ( ret = ssl_handshake( ssl ) ) != 0 )
    {
        if( ret != POLARSSL_ERR_NET_WANT_READ && ret != POLARSSL_ERR_NET_WANT_WRITE )
        {
#if defined(POLARSSL_ERROR_C)
            error_strerror( ret, (char *) buf, 1024 );
#endif
            printf( " failed\n  ! ssl_handshake returned %d: %s\n\n", ret, buf );
            return( -1 );
        }
    }

    printf( " ok\n    [ Ciphersuite is %s ]\n",
            ssl_get_ciphersuite( ssl ) );

    /*
     * 5. Verify the server certificate
     */
    printf( "  . Verifying peer X.509 certificate..." );

    if( ( ret = ssl_get_verify_result( ssl ) ) != 0 )
    {
        printf( " failed\n" );

        if( ( ret & BADCERT_EXPIRED ) != 0 )
            printf( "  ! server certificate has expired\n" );

        if( ( ret & BADCERT_REVOKED ) != 0 )
            printf( "  ! server certificate has been revoked\n" );

        if( ( ret & BADCERT_CN_MISMATCH ) != 0 )
            printf( "  ! CN mismatch (expected CN=%s)\n", opt->server_name );

        if( ( ret & BADCERT_NOT_TRUSTED ) != 0 )
            printf( "  ! self-signed or not signed by a trusted CA\n" );

        printf( "\n" );
    }
    else
        printf( " ok\n" );

    printf( "  . Peer certificate information    ...\n" );
    x509parse_cert_info( (char *) buf, sizeof( buf ) - 1, "      ",
                         ssl_get_peer_cert( ssl ) );
    printf( "%s\n", buf );

    return( 0 );
}

int write_ssl_data( ssl_context *ssl, unsigned char *buf, size_t len )
{
    int ret;

    printf("\n%s", buf);
    while( len && ( ret = ssl_write( ssl, buf, len ) ) <= 0 )
    {
        if( ret != POLARSSL_ERR_NET_WANT_READ && ret != POLARSSL_ERR_NET_WANT_WRITE )
        {
            printf( " failed\n  ! ssl_write returned %d\n\n", ret );
            return -1;
        }
    }

    return( 0 );
}

int write_ssl_and_get_response( ssl_context *ssl, unsigned char *buf, size_t len )
{
    int ret;
    unsigned char data[128];
    char code[4];
    size_t i, idx = 0;

    printf("\n%s", buf);
    while( len && ( ret = ssl_write( ssl, buf, len ) ) <= 0 )
    {
        if( ret != POLARSSL_ERR_NET_WANT_READ && ret != POLARSSL_ERR_NET_WANT_WRITE )
        {
            printf( " failed\n  ! ssl_write returned %d\n\n", ret );
            return -1;
        }
    }

    do
    {
        len = sizeof( data ) - 1;
        memset( data, 0, sizeof( data ) );
        ret = ssl_read( ssl, data, len );

        if( ret == POLARSSL_ERR_NET_WANT_READ || ret == POLARSSL_ERR_NET_WANT_WRITE )
            continue;

        if( ret == POLARSSL_ERR_SSL_PEER_CLOSE_NOTIFY )
            return -1;

        if( ret <= 0 )
        {
            printf( "failed\n  ! ssl_read returned %d\n\n", ret );
            return -1;
        }

        printf("\n%s", data);
        len = ret;
        for( i = 0; i < len; i++ )
        {
            if( data[i] != '\n' )
            {
                if( idx < 4 )
                    code[ idx++ ] = data[i];
                continue;
            }

            if( idx == 4 && code[0] >= '0' && code[0] <= '9' && code[3] == ' ' )
            {
                code[3] = '\0';
                return atoi( code );
            }
            
            idx = 0;
        }
    }
    while( 1 );
}

int write_and_get_response( int sock_fd, unsigned char *buf, size_t len )
{
    int ret;
    unsigned char data[128];
    char code[4];
    size_t i, idx = 0;

    printf("\n%s", buf);
    if( len && ( ret = write( sock_fd, buf, len ) ) <= 0 )
    {
        printf( " failed\n  ! ssl_write returned %d\n\n", ret );
            return -1;
    }

    do
    {
        len = sizeof( data ) - 1;
        memset( data, 0, sizeof( data ) );
        ret = read( sock_fd, data, len );

        if( ret <= 0 )
        {
            printf( "failed\n  ! read returned %d\n\n", ret );
            return -1;
        }

        printf("\n%s", data);
        len = ret;
        for( i = 0; i < len; i++ )
        {
            if( data[i] != '\n' )
            {
                if( idx < 4 )
                    code[ idx++ ] = data[i];
                continue;
            }

            if( idx == 4 && code[0] >= '0' && code[0] <= '9' && code[3] == ' ' )
            {
                code[3] = '\0';
                return atoi( code );
            }
            
            idx = 0;
        }
    }
    while( 1 );
}

#if defined(POLARSSL_BASE64_C)
#define USAGE_AUTH \
    "    authentication=%%d   default: 0 (disabled)\n"      \
    "    user_name=%%s        default: \"user\"\n"          \
    "    user_pwd=%%s         default: \"password\"\n"      
#else
#define USAGE_AUTH \
    "    authentication options disabled. (Require POLARSSL_BASE64_C)\n"
#endif /* POLARSSL_BASE64_C */

#if defined(POLARSSL_FS_IO)
#define USAGE_IO \
    "    ca_file=%%s          default: \"\" (pre-loaded)\n" \
    "    crt_file=%%s         default: \"\" (pre-loaded)\n" \
    "    key_file=%%s         default: \"\" (pre-loaded)\n"
#else
#define USAGE_IO \
    "    No file operations available (POLARSSL_FS_IO not defined)\n"
#endif /* POLARSSL_FS_IO */

#define USAGE \
    "\n usage: ssl_mail_client param=<>...\n"               \
    "\n acceptable parameters:\n"                           \
    "    server_name=%%s      default: localhost\n"         \
    "    server_port=%%d      default: 4433\n"              \
    "    debug_level=%%d      default: 0 (disabled)\n"      \
    "    mode=%%d             default: 0 (SSL/TLS) (1 for STARTTLS)\n"  \
    USAGE_AUTH                                              \
    "    mail_from=%%s        default: \"\"\n"              \
    "    mail_to=%%s          default: \"\"\n"              \
    USAGE_IO                                                \
    "    force_ciphersuite=<name>    default: all enabled\n"\
    " acceptable ciphersuite names:\n"

int main( int argc, char *argv[] )
{
    int ret = 0, len, server_fd;
    unsigned char buf[1024];
#if defined(POLARSSL_BASE64_C)
    unsigned char base[1024];
#endif
    char hostname[32];
    const char *pers = "ssl_mail_client";

    entropy_context entropy;
    ctr_drbg_context ctr_drbg;
    ssl_context ssl;
    x509_cert cacert;
    x509_cert clicert;
    rsa_context rsa;
    int i;
    size_t n;
    char *p, *q;
    const int *list;

    /*
     * Make sure memory references are valid.
     */
    server_fd = 0;
    memset( &cacert, 0, sizeof( x509_cert ) );
    memset( &clicert, 0, sizeof( x509_cert ) );
    memset( &rsa, 0, sizeof( rsa_context ) );

    if( argc == 0 )
    {
    usage:
        printf( USAGE );

        list = ssl_list_ciphersuites();
        while( *list )
        {
            printf("    %s\n", ssl_get_ciphersuite_name( *list ) );
            list++;
        }
        printf("\n");
        goto exit;
    }

    opt.server_name         = DFL_SERVER_NAME;
    opt.server_port         = DFL_SERVER_PORT;
    opt.debug_level         = DFL_DEBUG_LEVEL;
    opt.authentication      = DFL_AUTHENTICATION;
    opt.mode                = DFL_MODE;
    opt.user_name           = DFL_USER_NAME;
    opt.user_pwd            = DFL_USER_PWD;
    opt.mail_from           = DFL_MAIL_FROM;
    opt.mail_to             = DFL_MAIL_TO;
    opt.ca_file             = DFL_CA_FILE;
    opt.crt_file            = DFL_CRT_FILE;
    opt.key_file            = DFL_KEY_FILE;
    opt.force_ciphersuite[0]= DFL_FORCE_CIPHER;

    for( i = 1; i < argc; i++ )
    {
        p = argv[i];
        if( ( q = strchr( p, '=' ) ) == NULL )
            goto usage;
        *q++ = '\0';

        if( strcmp( p, "server_name" ) == 0 )
            opt.server_name = q;
        else if( strcmp( p, "server_port" ) == 0 )
        {
            opt.server_port = atoi( q );
            if( opt.server_port < 1 || opt.server_port > 65535 )
                goto usage;
        }
        else if( strcmp( p, "debug_level" ) == 0 )
        {
            opt.debug_level = atoi( q );
            if( opt.debug_level < 0 || opt.debug_level > 65535 )
                goto usage;
        }
        else if( strcmp( p, "authentication" ) == 0 )
        {
            opt.authentication = atoi( q );
            if( opt.authentication < 0 || opt.authentication > 1 )
                goto usage;
        }
        else if( strcmp( p, "mode" ) == 0 )
        {
            opt.mode = atoi( q );
            if( opt.mode < 0 || opt.mode > 1 )
                goto usage;
        }
        else if( strcmp( p, "user_name" ) == 0 )
            opt.user_name = q;
        else if( strcmp( p, "user_pwd" ) == 0 )
            opt.user_pwd = q;
        else if( strcmp( p, "mail_from" ) == 0 )
            opt.mail_from = q;
        else if( strcmp( p, "mail_to" ) == 0 )
            opt.mail_to = q;
        else if( strcmp( p, "ca_file" ) == 0 )
            opt.ca_file = q;
        else if( strcmp( p, "crt_file" ) == 0 )
            opt.crt_file = q;
        else if( strcmp( p, "key_file" ) == 0 )
            opt.key_file = q;
        else if( strcmp( p, "force_ciphersuite" ) == 0 )
        {
            opt.force_ciphersuite[0] = -1;

            opt.force_ciphersuite[0] = ssl_get_ciphersuite_id( q );

            if( opt.force_ciphersuite[0] <= 0 )
                goto usage;

            opt.force_ciphersuite[1] = 0;
        }
        else
            goto usage;
    }

    /*
     * 0. Initialize the RNG and the session data
     */
    printf( "\n  . Seeding the random number generator..." );
    fflush( stdout );

    entropy_init( &entropy );
    if( ( ret = ctr_drbg_init( &ctr_drbg, entropy_func, &entropy,
                               (const unsigned char *) pers,
                               strlen( pers ) ) ) != 0 )
    {
        printf( " failed\n  ! ctr_drbg_init returned %d\n", ret );
        goto exit;
    }

    printf( " ok\n" );

    /*
     * 1.1. Load the trusted CA
     */
    printf( "  . Loading the CA root certificate ..." );
    fflush( stdout );

#if defined(POLARSSL_FS_IO)
    if( strlen( opt.ca_file ) )
        ret = x509parse_crtfile( &cacert, opt.ca_file );
    else
#endif
#if defined(POLARSSL_CERTS_C)
        ret = x509parse_crt( &cacert, (const unsigned char *) test_ca_crt,
                strlen( test_ca_crt ) );
#else
    {
        ret = 1;
        printf("POLARSSL_CERTS_C not defined.");
    }
#endif
    if( ret < 0 )
    {
        printf( " failed\n  !  x509parse_crt returned %d\n\n", ret );
        goto exit;
    }

    printf( " ok (%d skipped)\n", ret );

    /*
     * 1.2. Load own certificate and private key
     *
     * (can be skipped if client authentication is not required)
     */
    printf( "  . Loading the client cert. and key..." );
    fflush( stdout );

#if defined(POLARSSL_FS_IO)
    if( strlen( opt.crt_file ) )
        ret = x509parse_crtfile( &clicert, opt.crt_file );
    else 
#endif
#if defined(POLARSSL_CERTS_C)
        ret = x509parse_crt( &clicert, (const unsigned char *) test_cli_crt,
                strlen( test_cli_crt ) );
#else
    {
        ret = -1;
        printf("POLARSSL_CERTS_C not defined.");
    }
#endif
    if( ret != 0 )
    {
        printf( " failed\n  !  x509parse_crt returned %d\n\n", ret );
        goto exit;
    }

#if defined(POLARSSL_FS_IO)
    if( strlen( opt.key_file ) )
        ret = x509parse_keyfile( &rsa, opt.key_file, "" );
    else
#endif
#if defined(POLARSSL_CERTS_C)
        ret = x509parse_key( &rsa, (const unsigned char *) test_cli_key,
                strlen( test_cli_key ), NULL, 0 );
#else
    {
        ret = -1;
        printf("POLARSSL_CERTS_C not defined.");
    }
#endif
    if( ret != 0 )
    {
        printf( " failed\n  !  x509parse_key returned %d\n\n", ret );
        goto exit;
    }

    printf( " ok\n" );

    /*
     * 2. Start the connection
     */
    printf( "  . Connecting to tcp/%s/%-4d...", opt.server_name,
                                                opt.server_port );
    fflush( stdout );

    if( ( ret = net_connect( &server_fd, opt.server_name,
                                         opt.server_port ) ) != 0 )
    {
        printf( " failed\n  ! net_connect returned %d\n\n", ret );
        goto exit;
    }

    printf( " ok\n" );

    /*
     * 3. Setup stuff
     */
    printf( "  . Setting up the SSL/TLS structure..." );
    fflush( stdout );

    if( ( ret = ssl_init( &ssl ) ) != 0 )
    {
        printf( " failed\n  ! ssl_init returned %d\n\n", ret );
        goto exit;
    }

    printf( " ok\n" );

    ssl_set_endpoint( &ssl, SSL_IS_CLIENT );
    ssl_set_authmode( &ssl, SSL_VERIFY_OPTIONAL );

    ssl_set_rng( &ssl, ctr_drbg_random, &ctr_drbg );
    ssl_set_dbg( &ssl, my_debug, stdout );
    ssl_set_bio( &ssl, net_recv, &server_fd,
            net_send, &server_fd );

    if( opt.force_ciphersuite[0] != DFL_FORCE_CIPHER )
        ssl_set_ciphersuites( &ssl, opt.force_ciphersuite );

    ssl_set_ca_chain( &ssl, &cacert, NULL, opt.server_name );
    ssl_set_own_cert( &ssl, &clicert, &rsa );

    ssl_set_hostname( &ssl, opt.server_name );

    if( opt.mode == MODE_SSL_TLS )
    {
        if( do_handshake( &ssl, &opt ) != 0 )
            goto exit;

        printf( "  > Get header from server:" );
        fflush( stdout );

        ret = write_ssl_and_get_response( &ssl, buf, 0 );
        if( ret < 200 || ret > 299 )
        {
            printf( " failed\n  ! server responded with %d\n\n", ret );
            goto exit;
        }

        printf(" ok\n" );

        printf( "  > Write EHLO to server:" );
        fflush( stdout );

        gethostname( hostname, 32 );
        len = sprintf( (char *) buf, "EHLO %s\n", hostname );
        ret = write_ssl_and_get_response( &ssl, buf, len );
        if( ret < 200 || ret > 299 )
        {
            printf( " failed\n  ! server responded with %d\n\n", ret );
            goto exit;
        }
    }
    else
    {
        printf( "  > Get header from server:" );
        fflush( stdout );

        ret = write_and_get_response( server_fd, buf, 0 );
        if( ret < 200 || ret > 299 )
        {
            printf( " failed\n  ! server responded with %d\n\n", ret );
            goto exit;
        }

        printf(" ok\n" );

        printf( "  > Write EHLO to server:" );
        fflush( stdout );

        gethostname( hostname, 32 );
        len = sprintf( (char *) buf, "EHLO %s\n", hostname );
        ret = write_and_get_response( server_fd, buf, len );
        if( ret < 200 || ret > 299 )
        {
            printf( " failed\n  ! server responded with %d\n\n", ret );
            goto exit;
        }

        printf(" ok\n" );

        printf( "  > Write STARTTLS to server:" );
        fflush( stdout );

        gethostname( hostname, 32 );
        len = sprintf( (char *) buf, "STARTTLS\n" );
        ret = write_and_get_response( server_fd, buf, len );
        if( ret < 200 || ret > 299 )
        {
            printf( " failed\n  ! server responded with %d\n\n", ret );
            goto exit;
        }

        printf(" ok\n" );

        if( do_handshake( &ssl, &opt ) != 0 )
            goto exit;
    }

#if defined(POLARSSL_BASE64_C)
    if( opt.authentication )
    {
        printf( "  > Write AUTH LOGIN to server:" );
        fflush( stdout );

        len = sprintf( (char *) buf, "AUTH LOGIN\n" );
        ret = write_ssl_and_get_response( &ssl, buf, len );
        if( ret < 200 || ret > 399 )
        {
            printf( " failed\n  ! server responded with %d\n\n", ret );
            goto exit;
        }

        printf(" ok\n" );

        printf( "  > Write username to server: %s", opt.user_name );
        fflush( stdout );

        n = sizeof( buf );
        len = base64_encode( base, &n, (const unsigned char *) opt.user_name,
                             strlen( opt.user_name ) );
        len = sprintf( (char *) buf, "%s\n", base );
        ret = write_ssl_and_get_response( &ssl, buf, len );
        if( ret < 300 || ret > 399 )
        {
            printf( " failed\n  ! server responded with %d\n\n", ret );
            goto exit;
        }

        printf(" ok\n" );

        printf( "  > Write password to server: %s", opt.user_pwd );
        fflush( stdout );

        len = base64_encode( base, &n, (const unsigned char *) opt.user_pwd,
                             strlen( opt.user_pwd ) );
        len = sprintf( (char *) buf, "%s\n", base );
        ret = write_ssl_and_get_response( &ssl, buf, len );
        if( ret < 200 || ret > 399 )
        {
            printf( " failed\n  ! server responded with %d\n\n", ret );
            goto exit;
        }

        printf(" ok\n" );
    }
#endif

    printf( "  > Write MAIL FROM to server:" );
    fflush( stdout );

    len = sprintf( (char *) buf, "MAIL FROM:<%s>\n", opt.mail_from );
    ret = write_ssl_and_get_response( &ssl, buf, len );
    if( ret < 200 || ret > 299 )
    {
        printf( " failed\n  ! server responded with %d\n\n", ret );
        goto exit;
    }

    printf(" ok\n" );

    printf( "  > Write RCPT TO to server:" );
    fflush( stdout );

    len = sprintf( (char *) buf, "RCPT TO:<%s>\n", opt.mail_to );
    ret = write_ssl_and_get_response( &ssl, buf, len );
    if( ret < 200 || ret > 299 )
    {
        printf( " failed\n  ! server responded with %d\n\n", ret );
        goto exit;
    }

    printf(" ok\n" );

    printf( "  > Write DATA to server:" );
    fflush( stdout );

    len = sprintf( (char *) buf, "DATA\n" );
    ret = write_ssl_and_get_response( &ssl, buf, len );
    if( ret < 300 || ret > 399 )
    {
        printf( " failed\n  ! server responded with %d\n\n", ret );
        goto exit;
    }

    printf(" ok\n" );

    printf( "  > Write content to server:" );
    fflush( stdout );

    len = sprintf( (char *) buf, "From: %s\nSubject: PolarSSL Test mail\n\n"
            "This is a simple test mail from the "
            "PolarSSL mail client example.\n"
            "\n"
            "Enjoy!", opt.mail_from );
    ret = write_ssl_data( &ssl, buf, len );

    len = sprintf( (char *) buf, "\r\n.\r\n");
    ret = write_ssl_and_get_response( &ssl, buf, len );
    if( ret < 200 || ret > 299 )
    {
        printf( " failed\n  ! server responded with %d\n\n", ret );
        goto exit;
    }

    printf(" ok\n" );

    ssl_close_notify( &ssl );

exit:

    if( server_fd )
        net_close( server_fd );
    x509_free( &clicert );
    x509_free( &cacert );
    rsa_free( &rsa );
    ssl_free( &ssl );

#if defined(_WIN32)
    printf( "  + Press Enter to exit this program.\n" );
    fflush( stdout ); getchar();
#endif

    return( ret );
}
#endif /* POLARSSL_BIGNUM_C && POLARSSL_ENTROPY_C && POLARSSL_SSL_TLS_C &&
          POLARSSL_SSL_CLI_C && POLARSSL_NET_C && POLARSSL_RSA_C **
          POLARSSL_CTR_DRBG_C */
