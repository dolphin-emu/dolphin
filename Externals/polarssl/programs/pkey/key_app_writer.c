/*
 *  Key reading application
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

#include "polarssl/config.h"

#include "polarssl/error.h"
#include "polarssl/rsa.h"
#include "polarssl/x509.h"
#include "polarssl/base64.h"
#include "polarssl/x509write.h"

#define MODE_NONE               0
#define MODE_PRIVATE            1
#define MODE_PUBLIC             2

#define OUTPUT_MODE_NONE               0
#define OUTPUT_MODE_PRIVATE            1
#define OUTPUT_MODE_PUBLIC             2

#define DFL_MODE                MODE_NONE
#define DFL_FILENAME            "keyfile.key"
#define DFL_DEBUG_LEVEL         0
#define DFL_OUTPUT_MODE          OUTPUT_MODE_NONE
#define DFL_OUTPUT_FILENAME     "keyfile.pem"

/*
 * global options
 */
struct options
{
    int mode;                   /* the mode to run the application in   */
    const char *filename;       /* filename of the key file             */
    int debug_level;            /* level of debugging                   */
    int output_mode;            /* the output mode to use               */
    const char *output_file;    /* where to store the constructed key file  */
} opt;

void my_debug( void *ctx, int level, const char *str )
{
    if( level < opt.debug_level )
    {
        fprintf( (FILE *) ctx, "%s", str );
        fflush(  (FILE *) ctx  );
    }
}

void write_public_key( rsa_context *rsa, const char *output_file )
{
    FILE *f;
    unsigned char output_buf[16000];
    unsigned char base_buf[16000];
    unsigned char *c;
    int ret;
    size_t len = 0, olen = 16000;

    memset(output_buf, 0, 16000);
    ret = x509_write_pubkey_der( output_buf, 16000, rsa );

    if( ret < 0 )
        return;

    len = ret;
    c = output_buf + 15999 - len;

    base64_encode( base_buf, &olen, c, len );

    c = base_buf;

    f = fopen( output_file, "w" );
    fprintf(f, "-----BEGIN PUBLIC KEY-----\n");
    while (olen)
    {
        int use_len = olen;
        if (use_len > 64) use_len = 64;
        fwrite( c, 1, use_len, f );
        olen -= use_len;
        c += use_len;
        fprintf(f, "\n");
    }
    fprintf(f, "-----END PUBLIC KEY-----\n");
    fclose(f);
}

void write_private_key( rsa_context *rsa, const char *output_file )
{
    FILE *f;
    unsigned char output_buf[16000];
    unsigned char base_buf[16000];
    unsigned char *c;
    int ret;
    size_t len = 0, olen = 16000;

    memset(output_buf, 0, 16000);
    ret = x509_write_key_der( output_buf, 16000, rsa );
    if( ret < 0 )
        return;

    len = ret;
    c = output_buf + 15999 - len;

    base64_encode( base_buf, &olen, c, len );

    c = base_buf;

    f = fopen( output_file, "w" );
    fprintf(f, "-----BEGIN RSA PRIVATE KEY-----\n");
    while (olen)
    {
        int use_len = olen;
        if (use_len > 64) use_len = 64;
        fwrite( c, 1, use_len, f );
        olen -= use_len;
        c += use_len;
        fprintf(f, "\n");
    }
    fprintf(f, "-----END RSA PRIVATE KEY-----\n");
    fclose(f);
}

#define USAGE \
    "\n usage: key_app param=<>...\n"                   \
    "\n acceptable parameters:\n"                       \
    "    mode=private|public default: none\n"           \
    "    filename=%%s         default: keyfile.key\n"   \
    "    debug_level=%%d      default: 0 (disabled)\n"  \
    "    output_mode=private|public default: none\n"    \
    "    output_file=%%s      defeult: keyfile.pem\n"   \
    "\n"

#if !defined(POLARSSL_BIGNUM_C) || !defined(POLARSSL_RSA_C) ||         \
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
    int ret = 0;
    rsa_context rsa;
    char buf[1024];
    int i;
    char *p, *q;

    /*
     * Set to sane values
     */
    memset( &rsa, 0, sizeof( rsa_context ) );
    memset( buf, 0, 1024 );

    if( argc == 0 )
    {
    usage:
        printf( USAGE );
        goto exit;
    }

    opt.mode                = DFL_MODE;
    opt.filename            = DFL_FILENAME;
    opt.debug_level         = DFL_DEBUG_LEVEL;
    opt.output_mode         = DFL_OUTPUT_MODE;
    opt.output_file         = DFL_OUTPUT_FILENAME;

    for( i = 1; i < argc; i++ )
    {
        p = argv[i];
        if( ( q = strchr( p, '=' ) ) == NULL )
            goto usage;
        *q++ = '\0';

        if( strcmp( p, "mode" ) == 0 )
        {
            if( strcmp( q, "private" ) == 0 )
                opt.mode = MODE_PRIVATE;
            else if( strcmp( q, "public" ) == 0 )
                opt.mode = MODE_PUBLIC;
            else
                goto usage;
        }
        else if( strcmp( p, "output_mode" ) == 0 )
        {
            if( strcmp( q, "private" ) == 0 )
                opt.output_mode = OUTPUT_MODE_PRIVATE;
            else if( strcmp( q, "public" ) == 0 )
                opt.output_mode = OUTPUT_MODE_PUBLIC;
            else
                goto usage;
        }
        else if( strcmp( p, "filename" ) == 0 )
            opt.filename = q;
        else if( strcmp( p, "output_file" ) == 0 )
            opt.output_file = q;
        else if( strcmp( p, "debug_level" ) == 0 )
        {
            opt.debug_level = atoi( q );
            if( opt.debug_level < 0 || opt.debug_level > 65535 )
                goto usage;
        }
        else
            goto usage;
    }

    if( opt.mode == MODE_NONE && opt.output_mode != OUTPUT_MODE_NONE )
    {
        printf( "\nCannot output a key without reading one.\n");
        goto exit;
    }

    if( opt.mode == MODE_PUBLIC && opt.output_mode == OUTPUT_MODE_PRIVATE )
    {
        printf( "\nCannot output a private key from a public key.\n");
        goto exit;
    }

    if( opt.mode == MODE_PRIVATE )
    {
        /*
         * 1.1. Load the key
         */
        printf( "\n  . Loading the private key ..." );
        fflush( stdout );

        ret = x509parse_keyfile( &rsa, opt.filename, NULL );

        if( ret != 0 )
        {
#ifdef POLARSSL_ERROR_C
            error_strerror( ret, buf, 1024 );
#endif
            printf( " failed\n  !  x509parse_key returned %d - %s\n\n", ret, buf );
            rsa_free( &rsa );
            goto exit;
        }

        printf( " ok\n" );

        /*
         * 1.2 Print the key
         */
        printf( "  . Key information    ...\n" );
        mpi_write_file( "N:  ", &rsa.N, 16, NULL );
        mpi_write_file( "E:  ", &rsa.E, 16, NULL );
        mpi_write_file( "D:  ", &rsa.D, 16, NULL );
        mpi_write_file( "P:  ", &rsa.P, 16, NULL );
        mpi_write_file( "Q:  ", &rsa.Q, 16, NULL );
        mpi_write_file( "DP: ", &rsa.DP, 16, NULL );
        mpi_write_file( "DQ:  ", &rsa.DQ, 16, NULL );
        mpi_write_file( "QP:  ", &rsa.QP, 16, NULL );

    }
    else if( opt.mode == MODE_PUBLIC )
    {
        /*
         * 1.1. Load the key
         */
        printf( "\n  . Loading the public key ..." );
        fflush( stdout );

        ret = x509parse_public_keyfile( &rsa, opt.filename );

        if( ret != 0 )
        {
#ifdef POLARSSL_ERROR_C
            error_strerror( ret, buf, 1024 );
#endif
            printf( " failed\n  !  x509parse_public_key returned %d - %s\n\n", ret, buf );
            rsa_free( &rsa );
            goto exit;
        }

        printf( " ok\n" );

        /*
         * 1.2 Print the key
         */
        printf( "  . Key information    ...\n" );
        mpi_write_file( "N: ", &rsa.N, 16, NULL );
        mpi_write_file( "E:  ", &rsa.E, 16, NULL );
    }
    else
        goto usage;

    if( opt.output_mode == OUTPUT_MODE_PUBLIC )
    {
        write_public_key( &rsa, opt.output_file );
    }
    if( opt.output_mode == OUTPUT_MODE_PRIVATE )
    {
        write_private_key( &rsa, opt.output_file );
    }

exit:

    rsa_free( &rsa );

#if defined(_WIN32)
    printf( "  + Press Enter to exit this program.\n" );
    fflush( stdout ); getchar();
#endif

    return( ret );
}
#endif /* POLARSSL_BIGNUM_C && POLARSSL_RSA_C &&
          POLARSSL_X509_PARSE_C && POLARSSL_FS_IO */
