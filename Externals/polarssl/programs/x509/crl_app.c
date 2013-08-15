/*
 *  CRL reading application
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

#include "polarssl/x509.h"

#define DFL_FILENAME            "crl.pem"
#define DFL_DEBUG_LEVEL         0

/*
 * global options
 */
struct options
{
    const char *filename;       /* filename of the certificate file     */
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
    "\n usage: crl_app param=<>...\n"                   \
    "\n acceptable parameters:\n"                       \
    "    filename=%%s         default: crl.pem\n"      \
    "    debug_level=%%d      default: 0 (disabled)\n"  \
    "\n"

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
    int ret = 0;
    unsigned char buf[100000];
    x509_crl crl;
    int i, j, n;
    char *p, *q;

    /*
     * Set to sane values
     */
    memset( &crl, 0, sizeof( x509_crl ) );

    if( argc == 0 )
    {
    usage:
        printf( USAGE );
        goto exit;
    }

    opt.filename            = DFL_FILENAME;
    opt.debug_level         = DFL_DEBUG_LEVEL;

    for( i = 1; i < argc; i++ )
    {
        n = strlen( argv[i] );

        for( j = 0; j < n; j++ )
        {
            if( argv[i][j] >= 'A' && argv[i][j] <= 'Z' )
                argv[i][j] |= 0x20;
        }

        p = argv[i];
        if( ( q = strchr( p, '=' ) ) == NULL )
            goto usage;
        *q++ = '\0';

        if( strcmp( p, "filename" ) == 0 )
            opt.filename = q;
        else if( strcmp( p, "debug_level" ) == 0 )
        {
            opt.debug_level = atoi( q );
            if( opt.debug_level < 0 || opt.debug_level > 65535 )
                goto usage;
        }
        else
            goto usage;
    }

    /*
     * 1.1. Load the CRL
     */
    printf( "\n  . Loading the CRL ..." );
    fflush( stdout );

    ret = x509parse_crlfile( &crl, opt.filename );

    if( ret != 0 )
    {
        printf( " failed\n  !  x509parse_crl returned %d\n\n", ret );
        x509_crl_free( &crl );
        goto exit;
    }

    printf( " ok\n" );

    /*
     * 1.2 Print the CRL
     */
    printf( "  . CRL information    ...\n" );
    ret = x509parse_crl_info( (char *) buf, sizeof( buf ) - 1, "      ", &crl );
    if( ret == -1 )
    {
        printf( " failed\n  !  x509parse_crl_info returned %d\n\n", ret );
        x509_crl_free( &crl );
        goto exit;
    }

    printf( "%s\n", buf );

exit:
    x509_crl_free( &crl );

#if defined(_WIN32)
    printf( "  + Press Enter to exit this program.\n" );
    fflush( stdout ); getchar();
#endif

    return( ret );
}
#endif /* POLARSSL_BIGNUM_C && POLARSSL_RSA_C && POLARSSL_X509_PARSE_C &&
          POLARSSL_FS_IO */
