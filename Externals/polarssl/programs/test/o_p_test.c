/*
 *  Test application that shows some PolarSSL and OpenSSL compatibility
 *
 *  Copyright (C) 2011-2012 Brainspark B.V.
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
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

#include <openssl/rsa.h>
#include <openssl/engine.h>
#include <openssl/pem.h>
#include <openssl/bio.h>

#include "polarssl/config.h"

#include "polarssl/x509.h"
#include "polarssl/rsa.h"
#include "polarssl/entropy.h"
#include "polarssl/ctr_drbg.h"

int main( int argc, char *argv[] )
{
    int ret;
    FILE *key_file;
    size_t olen;
    rsa_context p_rsa;
    RSA *o_rsa;
    entropy_context entropy;
    ctr_drbg_context ctr_drbg;
    unsigned char input[1024];
    unsigned char p_pub_encrypted[512];
    unsigned char o_pub_encrypted[512];
    unsigned char p_pub_decrypted[512];
    unsigned char o_pub_decrypted[512];
    unsigned char p_priv_encrypted[512];
    unsigned char o_priv_encrypted[512];
    unsigned char p_priv_decrypted[512];
    unsigned char o_priv_decrypted[512];
    const char *pers = "o_p_test_example";

    entropy_init( &entropy );
    if( ( ret = ctr_drbg_init( &ctr_drbg, entropy_func, &entropy,
                    (const unsigned char *) pers,
                    strlen( pers ) ) ) != 0 )
    {
        printf( " failed\n  ! ctr_drbg_init returned %d\n", ret );
        goto exit;
    }
    ERR_load_crypto_strings();

    ret = 1;

    if( argc != 3 )
    {
        printf( "usage: o_p_test <keyfile with private_key> <string of max 100 characters>\n" );

#ifdef WIN32
        printf( "\n" );
#endif

        goto exit;
    }

    printf( "  . Reading private key from %s into PolarSSL ...", argv[1] );
    fflush( stdout );

    rsa_init( &p_rsa, RSA_PKCS_V15, 0 );
    if( x509parse_keyfile( &p_rsa, argv[1], NULL ) != 0 )
    {
        ret = 1;
        printf( " failed\n  ! Could not load key.\n\n" );
        goto exit;
    }

    printf( " passed\n");

    printf( "  . Reading private key from %s into OpenSSL  ...", argv[1] );
    fflush( stdout );

    key_file = fopen( argv[1], "r" );
    o_rsa = PEM_read_RSAPrivateKey(key_file, 0, 0, 0);
    fclose(key_file);
    if( o_rsa == NULL )
    {
        ret = 1;
        printf( " failed\n  ! Could not load key.\n\n" );
        goto exit;
    }

    printf( " passed\n");
    printf( "\n" );

    if( strlen( argv[1] ) > 100 )
    {
        printf( " Input data larger than 100 characters.\n\n" );
        goto exit;
    }

    memcpy( input, argv[2], strlen( argv[2] ) );

    /*
     * Calculate the RSA encryption with public key.
     */
    printf( "  . Generating the RSA encrypted value with PolarSSL (RSA_PUBLIC)  ..." );
    fflush( stdout );

    if( ( ret = rsa_pkcs1_encrypt( &p_rsa, ctr_drbg_random, &ctr_drbg, RSA_PUBLIC, strlen( argv[1] ), input, p_pub_encrypted ) ) != 0 )
    {
        printf( " failed\n  ! rsa_pkcs1_encrypt returned %d\n\n", ret );
        goto exit;
    }
    else
        printf( " passed\n");

    printf( "  . Generating the RSA encrypted value with OpenSSL (PUBLIC)       ..." );
    fflush( stdout );

    if( ( ret = RSA_public_encrypt( strlen( argv[1] ), input, o_pub_encrypted, o_rsa, RSA_PKCS1_PADDING ) ) == -1 )
    {
        unsigned long code = ERR_get_error();
        printf( " failed\n  ! RSA_public_encrypt returned %d %s\n\n", ret, ERR_error_string( code, NULL ) );
        goto exit;
    }
    else
        printf( " passed\n");

    /*
     * Calculate the RSA encryption with private key.
     */
    printf( "  . Generating the RSA encrypted value with PolarSSL (RSA_PRIVATE) ..." );
    fflush( stdout );

    if( ( ret = rsa_pkcs1_encrypt( &p_rsa, ctr_drbg_random, &ctr_drbg, RSA_PRIVATE, strlen( argv[1] ), input, p_priv_encrypted ) ) != 0 )
    {
        printf( " failed\n  ! rsa_pkcs1_encrypt returned %d\n\n", ret );
        goto exit;
    }
    else
        printf( " passed\n");

    printf( "  . Generating the RSA encrypted value with OpenSSL (PRIVATE)      ..." );
    fflush( stdout );

    if( ( ret = RSA_private_encrypt( strlen( argv[1] ), input, o_priv_encrypted, o_rsa, RSA_PKCS1_PADDING ) ) == -1 )
    {
        unsigned long code = ERR_get_error();
        printf( " failed\n  ! RSA_private_encrypt returned %d %s\n\n", ret, ERR_error_string( code, NULL ) );
        goto exit;
    }
    else
        printf( " passed\n");

    printf( "\n" );

    /*
     * Calculate the RSA decryption with private key.
     */
    printf( "  . Generating the RSA decrypted value for OpenSSL (PUBLIC) with PolarSSL (PRIVATE) ..." );
    fflush( stdout );

    if( ( ret = rsa_pkcs1_decrypt( &p_rsa, RSA_PRIVATE, &olen, o_pub_encrypted, p_pub_decrypted, 1024 ) ) != 0 )
    {
        printf( " failed\n  ! rsa_pkcs1_decrypt returned %d\n\n", ret );
    }
    else
        printf( " passed\n");

    printf( "  . Generating the RSA decrypted value for PolarSSL (PUBLIC) with OpenSSL (PRIVATE) ..." );
    fflush( stdout );

    if( ( ret = RSA_private_decrypt( p_rsa.len, p_pub_encrypted, o_pub_decrypted, o_rsa, RSA_PKCS1_PADDING ) ) == -1 )
    {
        unsigned long code = ERR_get_error();
        printf( " failed\n  ! RSA_private_decrypt returned %d %s\n\n", ret, ERR_error_string( code, NULL ) );
    }
    else
        printf( " passed\n");

    /*
     * Calculate the RSA decryption with public key.
     */
    printf( "  . Generating the RSA decrypted value for OpenSSL (PRIVATE) with PolarSSL (PUBLIC) ..." );
    fflush( stdout );

    if( ( ret = rsa_pkcs1_decrypt( &p_rsa, RSA_PUBLIC, &olen, o_priv_encrypted, p_priv_decrypted, 1024 ) ) != 0 )
    {
        printf( " failed\n  ! rsa_pkcs1_decrypt returned %d\n\n", ret );
    }
    else
        printf( " passed\n");

    printf( "  . Generating the RSA decrypted value for PolarSSL (PRIVATE) with OpenSSL (PUBLIC) ..." );
    fflush( stdout );

    if( ( ret = RSA_public_decrypt( p_rsa.len, p_priv_encrypted, o_priv_decrypted, o_rsa, RSA_PKCS1_PADDING ) ) == -1 )
    {
        unsigned long code = ERR_get_error();
        printf( " failed\n  ! RSA_public_decrypt returned %d %s\n\n", ret, ERR_error_string( code, NULL ) );
    }
    else
        printf( " passed\n");

    printf( "\n" );
    printf( "String value (OpenSSL Public Encrypt, PolarSSL Private Decrypt): '%s'\n", p_pub_decrypted );
    printf( "String value (PolarSSL Public Encrypt, OpenSSL Private Decrypt): '%s'\n", o_pub_decrypted );
    printf( "String value (OpenSSL Private Encrypt, PolarSSL Public Decrypt): '%s'\n", p_priv_decrypted );
    printf( "String value (PolarSSL Private Encrypt, OpenSSL Public Decrypt): '%s'\n", o_priv_decrypted );

exit:

#ifdef WIN32
    printf( "  + Press Enter to exit this program.\n" );
    fflush( stdout ); getchar();
#endif

    return( ret );
}
