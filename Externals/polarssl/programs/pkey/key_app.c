/*
 *  Key reading application
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

#define MODE_NONE               0
#define MODE_PRIVATE            1
#define MODE_PUBLIC             2

#define DFL_MODE                MODE_NONE
#define DFL_FILENAME            "keyfile.key"
#define DFL_PASSWORD            ""
#define DFL_PASSWORD_FILE       ""
#define DFL_DEBUG_LEVEL         0

/*
 * global options
 */
struct options
{
    int mode;                   /* the mode to run the application in   */
    const char *filename;       /* filename of the key file             */
    const char *password;       /* password for the private key         */
    const char *password_file;  /* password_file for the private key    */
    int debug_level;            /* level of debugging                   */
} opt;

void my_debug( void *ctx, int level, const char *str )
{
    if( level < opt.debug_level )
    {
        fprintf( (FILE *) ctx, "%s", str );
        fflush(  (FILE *) ctx  );
    }
}

#define USAGE \
    "\n usage: key_app param=<>...\n"                   \
    "\n acceptable parameters:\n"                       \
    "    mode=private|public default: none\n"           \
    "    filename=%%s         default: keyfile.key\n"   \
    "    password=%%s         default: \"\"\n"          \
    "    password_file=%%s    default: \"\"\n"          \
    "    debug_level=%%d      default: 0 (disabled)\n"  \
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
    opt.password            = DFL_PASSWORD;
    opt.password_file       = DFL_PASSWORD_FILE;
    opt.debug_level         = DFL_DEBUG_LEVEL;

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
        else if( strcmp( p, "filename" ) == 0 )
            opt.filename = q;
        else if( strcmp( p, "password" ) == 0 )
            opt.password = q;
        else if( strcmp( p, "password_file" ) == 0 )
            opt.password_file = q;
        else if( strcmp( p, "debug_level" ) == 0 )
        {
            opt.debug_level = atoi( q );
            if( opt.debug_level < 0 || opt.debug_level > 65535 )
                goto usage;
        }
        else
            goto usage;
    }

    if( opt.mode == MODE_PRIVATE )
    {
        if( strlen( opt.password ) && strlen( opt.password_file ) )
        {
            printf( "Error: cannot have both password and password_file\n" );
            goto usage;
        }

        if( strlen( opt.password_file ) )
        {
            FILE *f;

            printf( "\n  . Loading the password file ..." );
            if( ( f = fopen( opt.password_file, "rb" ) ) == NULL )
            {
                printf( " failed\n  !  fopen returned NULL\n" );
                goto exit;
            }
            fgets( buf, 1024, f );
            fclose( f );

            i = strlen( buf );
            if( buf[i - 1] == '\n' ) buf[i - 1] = '\0';
            if( buf[i - 2] == '\r' ) buf[i - 2] = '\0';
            opt.password = buf;
        }

        /*
         * 1.1. Load the key
         */
        printf( "\n  . Loading the private key ..." );
        fflush( stdout );

        ret = x509parse_keyfile( &rsa, opt.filename, opt.password );

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
