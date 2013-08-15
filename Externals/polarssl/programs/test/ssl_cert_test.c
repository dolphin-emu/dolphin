/*
 *  SSL certificate functionality tests
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
#include <stdio.h>

#include "polarssl/config.h"

#include "polarssl/certs.h"
#include "polarssl/x509.h"

#if defined _MSC_VER && !defined snprintf
#define snprintf _snprintf
#endif

#define MAX_CLIENT_CERTS    8

const char *client_certificates[MAX_CLIENT_CERTS] =
{
    "client1.crt",
    "client2.crt",
    "server1.crt",
    "server2.crt",
    "cert_sha224.crt",
    "cert_sha256.crt",
    "cert_sha384.crt",
    "cert_sha512.crt"
};

const char *client_private_keys[MAX_CLIENT_CERTS] =
{
    "client1.key",
    "client2.key",
    "server1.key",
    "server2.key",
    "cert_digest.key",
    "cert_digest.key",
    "cert_digest.key",
    "cert_digest.key"
};

#if !defined(POLARSSL_BIGNUM_C) || !defined(POLARSSL_RSA_C) ||  \
    !defined(POLARSSL_X509_PARSE_C) || !defined(POLARSSL_FS_IO)
int main( int argc, char *argv[] )
{
    ((void) argc);
    ((void) argv);

    printf("POLARSSL_BIGNUM_C and/or POLARSSL_RSA_C and/or "
           "POLARSSL_X509_PARSE_C and/or POLARSSL_FS_IO not defined.\n");
    return( 0 );
}
#else
int main( int argc, char *argv[] )
{
    int ret, i;
    x509_cert cacert;
    x509_crl crl;
    char buf[10240];

    ((void) argc);
    ((void) argv);

    memset( &cacert, 0, sizeof( x509_cert ) );
    memset( &crl, 0, sizeof( x509_crl ) );

    /*
     * 1.1. Load the trusted CA
     */
    printf( "\n  . Loading the CA root certificate ..." );
    fflush( stdout );

    /*
     * Alternatively, you may load the CA certificates from a .pem or
     * .crt file by calling x509parse_crtfile( &cacert, "myca.crt" ).
     */
    ret = x509parse_crtfile( &cacert, "ssl/test-ca/test-ca.crt" );
    if( ret != 0 )
    {
        printf( " failed\n  !  x509parse_crtfile returned %d\n\n", ret );
        goto exit;
    }

    printf( " ok\n" );

    x509parse_cert_info( buf, 1024, "CRT: ", &cacert );
    printf("%s\n", buf );

    /*
     * 1.2. Load the CRL
     */
    printf( "  . Loading the CRL ..." );
    fflush( stdout );

    ret = x509parse_crlfile( &crl, "ssl/test-ca/crl.pem" );
    if( ret != 0 )
    {
        printf( " failed\n  !  x509parse_crlfile returned %d\n\n", ret );
        goto exit;
    }

    printf( " ok\n" );

    x509parse_crl_info( buf, 1024, "CRL: ", &crl );
    printf("%s\n", buf );

    for( i = 0; i < MAX_CLIENT_CERTS; i++ )
    {
        /*
         * 1.3. Load own certificate
         */
        char    name[512];
        int flags;
        x509_cert clicert;
        rsa_context rsa;

        memset( &clicert, 0, sizeof( x509_cert ) );
        memset( &rsa, 0, sizeof( rsa_context ) );

        snprintf(name, 512, "ssl/test-ca/%s", client_certificates[i]);

        printf( "  . Loading the client certificate %s...", name );
        fflush( stdout );

        ret = x509parse_crtfile( &clicert, name );
        if( ret != 0 )
        {
            printf( " failed\n  !  x509parse_crt returned %d\n\n", ret );
            goto exit;
        }

        printf( " ok\n" );

        /*
         * 1.4. Verify certificate validity with CA certificate
         */
        printf( "  . Verify the client certificate with CA certificate..." );
        fflush( stdout );

        ret = x509parse_verify( &clicert, &cacert, &crl, NULL, &flags, NULL, NULL );
        if( ret != 0 )
        {
            if( ret == POLARSSL_ERR_X509_CERT_VERIFY_FAILED )
            {
                if( flags & BADCERT_CN_MISMATCH )
                    printf( " CN_MISMATCH " );
                if( flags & BADCERT_EXPIRED )
                    printf( " EXPIRED " );
                if( flags & BADCERT_REVOKED )
                    printf( " REVOKED " );
                if( flags & BADCERT_NOT_TRUSTED )
                    printf( " NOT_TRUSTED " );
                if( flags & BADCRL_NOT_TRUSTED )
                    printf( " CRL_NOT_TRUSTED " );
                if( flags & BADCRL_EXPIRED )
                    printf( " CRL_EXPIRED " );
            } else {
                printf( " failed\n  !  x509parse_verify returned %d\n\n", ret );
                goto exit;
            }
        }

        printf( " ok\n" );

        /*
         * 1.5. Load own private key
         */
        snprintf(name, 512, "ssl/test-ca/%s", client_private_keys[i]);

        printf( "  . Loading the client private key %s...", name );
        fflush( stdout );

        ret = x509parse_keyfile( &rsa, name, NULL );
        if( ret != 0 )
        {
            printf( " failed\n  !  x509parse_key returned %d\n\n", ret );
            goto exit;
        }

        printf( " ok\n" );

        /*
         * 1.5. Verify certificate validity with private key
         */
        printf( "  . Verify the client certificate with private key..." );
        fflush( stdout );

        ret = mpi_cmp_mpi(&rsa.N, &clicert.rsa.N);
        if( ret != 0 )
        {
            printf( " failed\n  !  mpi_cmp_mpi for N returned %d\n\n", ret );
            goto exit;
        }

        ret = mpi_cmp_mpi(&rsa.E, &clicert.rsa.E);
        if( ret != 0 )
        {
            printf( " failed\n  !  mpi_cmp_mpi for E returned %d\n\n", ret );
            goto exit;
        }

        ret = rsa_check_privkey( &rsa );
        if( ret != 0 )
        {
            printf( " failed\n  !  rsa_check_privkey returned %d\n\n", ret );
            goto exit;
        }

        printf( " ok\n" );

        x509_free( &clicert );
        rsa_free( &rsa );
    }

exit:
    x509_free( &cacert );
    x509_crl_free( &crl );

#if defined(_WIN32)
    printf( "  + Press Enter to exit this program.\n" );
    fflush( stdout ); getchar();
#endif

    return( ret );
}
#endif /* POLARSSL_BIGNUM_C && POLARSSL_RSA_C && POLARSSL_X509_PARSE_C &&
          POLARSSL_FS_IO */
