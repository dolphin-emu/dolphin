/*
 *  Example RSA key generation program
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

#include <stdio.h>

#include "polarssl/config.h"

#include "polarssl/entropy.h"
#include "polarssl/ctr_drbg.h"
#include "polarssl/bignum.h"
#include "polarssl/x509.h"
#include "polarssl/rsa.h"

#define KEY_SIZE 1024
#define EXPONENT 65537

#if !defined(POLARSSL_BIGNUM_C) || !defined(POLARSSL_ENTROPY_C) ||   \
    !defined(POLARSSL_RSA_C) || !defined(POLARSSL_GENPRIME) ||      \
    !defined(POLARSSL_FS_IO) || !defined(POLARSSL_CTR_DRBG_C)
int main( int argc, char *argv[] )
{
    ((void) argc);
    ((void) argv);

    printf("POLARSSL_BIGNUM_C and/or POLARSSL_ENTROPY_C and/or "
           "POLARSSL_RSA_C and/or POLARSSL_GENPRIME and/or "
           "POLARSSL_FS_IO and/or POLARSSL_CTR_DRBG_C not defined.\n");
    return( 0 );
}
#else
int main( int argc, char *argv[] )
{
    int ret;
    rsa_context rsa;
    entropy_context entropy;
    ctr_drbg_context ctr_drbg;
    FILE *fpub  = NULL;
    FILE *fpriv = NULL;
    const char *pers = "rsa_genkey";

    ((void) argc);
    ((void) argv);

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

    printf( " ok\n  . Generating the RSA key [ %d-bit ]...", KEY_SIZE );
    fflush( stdout );

    rsa_init( &rsa, RSA_PKCS_V15, 0 );
    
    if( ( ret = rsa_gen_key( &rsa, ctr_drbg_random, &ctr_drbg, KEY_SIZE,
                             EXPONENT ) ) != 0 )
    {
        printf( " failed\n  ! rsa_gen_key returned %d\n\n", ret );
        goto exit;
    }

    printf( " ok\n  . Exporting the public  key in rsa_pub.txt...." );
    fflush( stdout );

    if( ( fpub = fopen( "rsa_pub.txt", "wb+" ) ) == NULL )
    {
        printf( " failed\n  ! could not open rsa_pub.txt for writing\n\n" );
        ret = 1;
        goto exit;
    }

    if( ( ret = mpi_write_file( "N = ", &rsa.N, 16, fpub ) ) != 0 ||
        ( ret = mpi_write_file( "E = ", &rsa.E, 16, fpub ) ) != 0 )
    {
        printf( " failed\n  ! mpi_write_file returned %d\n\n", ret );
        goto exit;
    }

    printf( " ok\n  . Exporting the private key in rsa_priv.txt..." );
    fflush( stdout );

    if( ( fpriv = fopen( "rsa_priv.txt", "wb+" ) ) == NULL )
    {
        printf( " failed\n  ! could not open rsa_priv.txt for writing\n" );
        ret = 1;
        goto exit;
    }

    if( ( ret = mpi_write_file( "N = " , &rsa.N , 16, fpriv ) ) != 0 ||
        ( ret = mpi_write_file( "E = " , &rsa.E , 16, fpriv ) ) != 0 ||
        ( ret = mpi_write_file( "D = " , &rsa.D , 16, fpriv ) ) != 0 ||
        ( ret = mpi_write_file( "P = " , &rsa.P , 16, fpriv ) ) != 0 ||
        ( ret = mpi_write_file( "Q = " , &rsa.Q , 16, fpriv ) ) != 0 ||
        ( ret = mpi_write_file( "DP = ", &rsa.DP, 16, fpriv ) ) != 0 ||
        ( ret = mpi_write_file( "DQ = ", &rsa.DQ, 16, fpriv ) ) != 0 ||
        ( ret = mpi_write_file( "QP = ", &rsa.QP, 16, fpriv ) ) != 0 )
    {
        printf( " failed\n  ! mpi_write_file returned %d\n\n", ret );
        goto exit;
    }
/*
    printf( " ok\n  . Generating the certificate..." );

    x509write_init_raw( &cert );
    x509write_add_pubkey( &cert, &rsa );
    x509write_add_subject( &cert, "CN='localhost'" );
    x509write_add_validity( &cert, "2007-09-06 17:00:32",
                                   "2010-09-06 17:00:32" );
    x509write_create_selfsign( &cert, &rsa );
    x509write_crtfile( &cert, "cert.der", X509_OUTPUT_DER );
    x509write_crtfile( &cert, "cert.pem", X509_OUTPUT_PEM );
    x509write_free_raw( &cert );
*/
    printf( " ok\n\n" );

exit:

    if( fpub  != NULL )
        fclose( fpub );

    if( fpriv != NULL )
        fclose( fpriv );

    rsa_free( &rsa );

#if defined(_WIN32)
    printf( "  Press Enter to exit this program.\n" );
    fflush( stdout ); getchar();
#endif

    return( ret );
}
#endif /* POLARSSL_BIGNUM_C && POLARSSL_ENTROPY_C && POLARSSL_RSA_C &&
          POLARSSL_GENPRIME && POLARSSL_FS_IO && POLARSSL_CTR_DRBG_C */
