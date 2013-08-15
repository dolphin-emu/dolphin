/*
 *  Classic "Hello, world" demonstration program
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

#include "polarssl/md5.h"

#if !defined(POLARSSL_MD5_C)
int main( int argc, char *argv[] )
{
    ((void) argc);
    ((void) argv);

    printf("POLARSSL_MD5_C not defined.\n");
    return( 0 );
}
#else
int main( int argc, char *argv[] )
{
    int i;
    unsigned char digest[16];
    char str[] = "Hello, world!";

    ((void) argc);
    ((void) argv);

    printf( "\n  MD5('%s') = ", str );

    md5( (unsigned char *) str, 13, digest );

    for( i = 0; i < 16; i++ )
        printf( "%02x", digest[i] );

    printf( "\n\n" );

#if defined(_WIN32)
    printf( "  Press Enter to exit this program.\n" );
    fflush( stdout ); getchar();
#endif

    return( 0 );
}
#endif /* POLARSSL_MD5_C */
