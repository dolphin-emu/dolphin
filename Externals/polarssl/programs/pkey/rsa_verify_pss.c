/*
 *  RSASSA-PSS/SHA-1 signature verification program
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

#include "polarssl/md.h"
#include "polarssl/pem.h"
#include "polarssl/rsa.h"
#include "polarssl/sha1.h"
#include "polarssl/x509.h"

#if defined _MSC_VER && !defined snprintf
#define snprintf _snprintf
#endif

#if !defined(POLARSSL_BIGNUM_C) || !defined(POLARSSL_RSA_C) ||      \
    !defined(POLARSSL_SHA1_C) || !defined(POLARSSL_X509_PARSE_C) || \
    !defined(POLARSSL_FS_IO)
int main( int argc, char *argv[] )
{
    ((void) argc);
    ((void) argv);

    printf("POLARSSL_BIGNUM_C and/or POLARSSL_RSA_C and/or "
           "POLARSSL_SHA1_C and/or POLARSSL_X509_PARSE_C and/or "
           "POLARSSL_FS_IO not defined.\n");
    return( 0 );
}
#else
int main( int argc, char *argv[] )
{
    FILE *f;
    int ret;
    size_t i;
    rsa_context rsa;
    unsigned char hash[20];
    unsigned char buf[POLARSSL_MPI_MAX_SIZE];
    char filename[512];

    ret = 1;
    if( argc != 3 )
    {
        printf( "usage: rsa_verify_pss <key_file> <filename>\n" );

#if defined(_WIN32)
        printf( "\n" );
#endif

        goto exit;
    }

    printf( "\n  . Reading public key from '%s'", argv[1] );
    fflush( stdout );

    rsa_init( &rsa, RSA_PKCS_V21, POLARSSL_MD_SHA1 );

    if( ( ret = x509parse_public_keyfile( &rsa, argv[1] ) ) != 0 )
    {
        printf( " failed\n  ! x509parse_public_key returned %d\n\n", ret );
        goto exit;
    }

    /*
     * Extract the RSA signature from the text file
     */
    ret = 1;
    snprintf( filename, 512, "%s.sig", argv[2] );

    if( ( f = fopen( filename, "rb" ) ) == NULL )
    {
        printf( "\n  ! Could not open %s\n\n", filename );
        goto exit;
    }

    i = fread( buf, 1, rsa.len, f );

    fclose( f );

    if( i != rsa.len )
    {
        printf( "\n  ! Invalid RSA signature format\n\n" );
        goto exit;
    }

    /*
     * Compute the SHA-1 hash of the input file and compare
     * it with the hash decrypted from the RSA signature.
     */
    printf( "\n  . Verifying the RSA/SHA-1 signature" );
    fflush( stdout );

    if( ( ret = sha1_file( argv[2], hash ) ) != 0 )
    {
        printf( " failed\n  ! Could not open or read %s\n\n", argv[2] );
        goto exit;
    }

    if( ( ret = rsa_pkcs1_verify( &rsa, RSA_PUBLIC, SIG_RSA_SHA1,
                                  20, hash, buf ) ) != 0 )
    {
        printf( " failed\n  ! rsa_pkcs1_verify returned %d\n\n", ret );
        goto exit;
    }

    printf( "\n  . OK (the decrypted SHA-1 hash matches)\n\n" );

    ret = 0;

exit:

#if defined(_WIN32)
    printf( "  + Press Enter to exit this program.\n" );
    fflush( stdout ); getchar();
#endif

    return( ret );
}
#endif /* POLARSSL_BIGNUM_C && POLARSSL_RSA_C && POLARSSL_SHA1_C &&
          POLARSSL_X509_PARSE_C && POLARSSL_FS_IO */
