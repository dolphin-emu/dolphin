/*
 *  SSL server demonstration program using fork() for handling multiple clients
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

#if defined(_WIN32)
#include <windows.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

#include "polarssl/config.h"

#include "polarssl/entropy.h"
#include "polarssl/ctr_drbg.h"
#include "polarssl/certs.h"
#include "polarssl/x509.h"
#include "polarssl/ssl.h"
#include "polarssl/net.h"
#include "polarssl/timing.h"

#define HTTP_RESPONSE \
    "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\n" \
    "<h2>PolarSSL Test Server</h2>\r\n" \
    "<p>Successful connection using: %s</p>\r\n"

#if !defined(POLARSSL_BIGNUM_C) || !defined(POLARSSL_CERTS_C) ||    \
    !defined(POLARSSL_ENTROPY_C) || !defined(POLARSSL_SSL_TLS_C) || \
    !defined(POLARSSL_SSL_SRV_C) || !defined(POLARSSL_NET_C) ||     \
    !defined(POLARSSL_RSA_C) || !defined(POLARSSL_CTR_DRBG_C)
int main( int argc, char *argv[] )
{
    ((void) argc);
    ((void) argv);

    printf("POLARSSL_BIGNUM_C and/or POLARSSL_CERTS_C and/or POLARSSL_ENTROPY_C "
           "and/or POLARSSL_SSL_TLS_C and/or POLARSSL_SSL_SRV_C and/or "
           "POLARSSL_NET_C and/or POLARSSL_RSA_C and/or "
           "POLARSSL_CTR_DRBG_C not defined.\n");
    return( 0 );
}
#elif defined(_WIN32)
int main( int argc, char *argv[] )
{
    ((void) argc);
    ((void) argv);

    printf("_WIN32 defined. This application requires fork() and signals "
           "to work correctly.\n");
    return( 0 );
}
#else

#define DEBUG_LEVEL 0

void my_debug( void *ctx, int level, const char *str )
{
    if( level < DEBUG_LEVEL )
    {
        fprintf( (FILE *) ctx, "%s", str );
        fflush(  (FILE *) ctx  );
    }
}

int main( int argc, char *argv[] )
{
    int ret, len, cnt = 0, pid;
    int listen_fd;
    int client_fd;
    unsigned char buf[1024];
    const char *pers = "ssl_fork_server";

    entropy_context entropy;
    ctr_drbg_context ctr_drbg;
    ssl_context ssl;
    x509_cert srvcert;
    rsa_context rsa;

    ((void) argc);
    ((void) argv);

    signal( SIGCHLD, SIG_IGN );

    /*
     * 0. Initial seeding of the RNG
     */
    printf( "\n  . Initial seeding of the random generator..." );
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
     * 1. Load the certificates and private RSA key
     */
    printf( "  . Loading the server cert. and key..." );
    fflush( stdout );

    memset( &srvcert, 0, sizeof( x509_cert ) );

    /*
     * This demonstration program uses embedded test certificates.
     * Instead, you may want to use x509parse_crtfile() to read the
     * server and CA certificates, as well as x509parse_keyfile().
     */
    ret = x509parse_crt( &srvcert, (const unsigned char *) test_srv_crt,
                         strlen( test_srv_crt ) );
    if( ret != 0 )
    {
        printf( " failed\n  !  x509parse_crt returned %d\n\n", ret );
        goto exit;
    }

    ret = x509parse_crt( &srvcert, (const unsigned char *) test_ca_crt,
                         strlen( test_ca_crt ) );
    if( ret != 0 )
    {
        printf( " failed\n  !  x509parse_crt returned %d\n\n", ret );
        goto exit;
    }

    rsa_init( &rsa, RSA_PKCS_V15, 0 );
    ret =  x509parse_key( &rsa, (const unsigned char *) test_srv_key,
                          strlen( test_srv_key ), NULL, 0 );
    if( ret != 0 )
    {
        printf( " failed\n  !  x509parse_key returned %d\n\n", ret );
        goto exit;
    }

    printf( " ok\n" );

    /*
     * 2. Setup the listening TCP socket
     */
    printf( "  . Bind on https://localhost:4433/ ..." );
    fflush( stdout );

    if( ( ret = net_bind( &listen_fd, NULL, 4433 ) ) != 0 )
    {
        printf( " failed\n  ! net_bind returned %d\n\n", ret );
        goto exit;
    }

    printf( " ok\n" );

    while( 1 )
    {
        /*
         * 3. Wait until a client connects
         */
        client_fd = -1;
        memset( &ssl, 0, sizeof( ssl ) );

        printf( "  . Waiting for a remote connection ..." );
        fflush( stdout );

        if( ( ret = net_accept( listen_fd, &client_fd, NULL ) ) != 0 )
        {
            printf( " failed\n  ! net_accept returned %d\n\n", ret );
            goto exit;
        }

        printf( " ok\n" );

        /*
         * 3.5. Forking server thread
         */

        pid = fork();

        printf( "  . Forking to handle connection ..." );
        fflush( stdout );

        if( pid < 0 )
        {
            printf(" failed\n  ! fork returned %d\n\n", pid );
            goto exit;
        }

        printf( " ok\n" );

        if( pid != 0 )
        {
            if( ( ret = ctr_drbg_reseed( &ctr_drbg,
                                         (const unsigned char *) "parent",
                                         6 ) ) != 0 )
            {
                printf( " failed\n  ! ctr_drbg_reseed returned %d\n", ret );
                goto exit;
            }

            close( client_fd );
            continue;
        }

        close( listen_fd );

        /*
         * 4. Setup stuff
         */
        printf( "  . Setting up the SSL data...." );
        fflush( stdout );

        if( ( ret = ctr_drbg_reseed( &ctr_drbg,
                                     (const unsigned char *) "child",
                                     5 ) ) != 0 )
        {
            printf( " failed\n  ! ctr_drbg_reseed returned %d\n", ret );
            goto exit;
        }
        
        if( ( ret = ssl_init( &ssl ) ) != 0 )
        {
            printf( " failed\n  ! ssl_init returned %d\n\n", ret );
            goto exit;
        }

        printf( " ok\n" );

        ssl_set_endpoint( &ssl, SSL_IS_SERVER );
        ssl_set_authmode( &ssl, SSL_VERIFY_NONE );

        ssl_set_rng( &ssl, ctr_drbg_random, &ctr_drbg );
        ssl_set_dbg( &ssl, my_debug, stdout );
        ssl_set_bio( &ssl, net_recv, &client_fd,
                           net_send, &client_fd );

        ssl_set_ca_chain( &ssl, srvcert.next, NULL, NULL );
        ssl_set_own_cert( &ssl, &srvcert, &rsa );

        /*
         * 5. Handshake
         */
        printf( "  . Performing the SSL/TLS handshake..." );
        fflush( stdout );

        while( ( ret = ssl_handshake( &ssl ) ) != 0 )
        {
            if( ret != POLARSSL_ERR_NET_WANT_READ && ret != POLARSSL_ERR_NET_WANT_WRITE )
            {
                printf( " failed\n  ! ssl_handshake returned %d\n\n", ret );
                goto exit;
            }
        }

        printf( " ok\n" );

        /*
         * 6. Read the HTTP Request
         */
        printf( "  < Read from client:" );
        fflush( stdout );

        do
        {
            len = sizeof( buf ) - 1;
            memset( buf, 0, sizeof( buf ) );
            ret = ssl_read( &ssl, buf, len );

            if( ret == POLARSSL_ERR_NET_WANT_READ || ret == POLARSSL_ERR_NET_WANT_WRITE )
                continue;

            if( ret <= 0 )
            {
                switch( ret )
                {
                    case POLARSSL_ERR_SSL_PEER_CLOSE_NOTIFY:
                        printf( " connection was closed gracefully\n" );
                        break;

                    case POLARSSL_ERR_NET_CONN_RESET:
                        printf( " connection was reset by peer\n" );
                        break;

                    default:
                        printf( " ssl_read returned %d\n", ret );
                        break;
                }

                break;
            }

            len = ret;
            printf( " %d bytes read\n\n%s", len, (char *) buf );
        }
        while( 0 );

        /*
         * 7. Write the 200 Response
         */
        printf( "  > Write to client:" );
        fflush( stdout );

        len = sprintf( (char *) buf, HTTP_RESPONSE,
                ssl_get_ciphersuite( &ssl ) );

        while( cnt < 100 )
        {
            while( ( ret = ssl_write( &ssl, buf, len ) ) <= 0 )
            {
                if( ret == POLARSSL_ERR_NET_CONN_RESET )
                {
                    printf( " failed\n  ! peer closed the connection\n\n" );
                    goto exit;
                }

                if( ret != POLARSSL_ERR_NET_WANT_READ && ret != POLARSSL_ERR_NET_WANT_WRITE )
                {
                    printf( " failed\n  ! ssl_write returned %d\n\n", ret );
                    goto exit;
                }
            }
            len = ret;
            printf( " %d bytes written\n\n%s\n", len, (char *) buf );

            m_sleep( 1000 );
        }

        ssl_close_notify( &ssl );
        goto exit;
    }

exit:

    net_close( client_fd );
    x509_free( &srvcert );
    rsa_free( &rsa );
    ssl_free( &ssl );

#if defined(_WIN32)
    printf( "  Press Enter to exit this program.\n" );
    fflush( stdout ); getchar();
#endif

    return( ret );
}
#endif /* POLARSSL_BIGNUM_C && POLARSSL_CERTS_C && POLARSSL_ENTROPY_C &&
          POLARSSL_SSL_TLS_C && POLARSSL_SSL_SRV_C && POLARSSL_NET_C &&
          POLARSSL_RSA_C && POLARSSL_CTR_DRBG_C */
