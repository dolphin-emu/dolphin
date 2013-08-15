/*
 *  sha1sum demonstration program
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

#include "polarssl/sha1.h"

#if !defined(POLARSSL_SHA1_C) || !defined(POLARSSL_FS_IO)
int main( int argc, char *argv[] )
{
    ((void) argc);
    ((void) argv);

    printf("POLARSSL_SHA1_C and/or POLARSSL_FS_IO not defined.\n");
    return( 0 );
}
#else
static int sha1_wrapper( char *filename, unsigned char *sum )
{
    int ret = sha1_file( filename, sum );

    if( ret == 1 )
        fprintf( stderr, "failed to open: %s\n", filename );

    if( ret == 2 )
        fprintf( stderr, "failed to read: %s\n", filename );

    return( ret );
}

static int sha1_print( char *filename )
{
    int i;
    unsigned char sum[20];

    if( sha1_wrapper( filename, sum ) != 0 )
        return( 1 );

    for( i = 0; i < 20; i++ )
        printf( "%02x", sum[i] );

    printf( "  %s\n", filename );
    return( 0 );
}

static int sha1_check( char *filename )
{
    int i;
    size_t n;
    FILE *f;
    int nb_err1, nb_err2;
    int nb_tot1, nb_tot2;
    unsigned char sum[20];
    char buf[41], line[1024];

    if( ( f = fopen( filename, "rb" ) ) == NULL )
    {
        printf( "failed to open: %s\n", filename );
        return( 1 );
    }

    nb_err1 = nb_err2 = 0;
    nb_tot1 = nb_tot2 = 0;

    memset( line, 0, sizeof( line ) );

    n = sizeof( line );

    while( fgets( line, (int) n - 1, f ) != NULL )
    {
        n = strlen( line );

        if( n < 44 )
            continue;

        if( line[40] != ' ' || line[41] != ' ' )
            continue;

        if( line[n - 1] == '\n' ) { n--; line[n] = '\0'; }
        if( line[n - 1] == '\r' ) { n--; line[n] = '\0'; }

        nb_tot1++;

        if( sha1_wrapper( line + 42, sum ) != 0 )
        {
            nb_err1++;
            continue;
        }

        nb_tot2++;

        for( i = 0; i < 20; i++ )
            sprintf( buf + i * 2, "%02x", sum[i] );

        if( memcmp( line, buf, 40 ) != 0 )
        {
            nb_err2++;
            fprintf( stderr, "wrong checksum: %s\n", line + 42 );
        }

        n = sizeof( line );
    }

    if( nb_err1 != 0 )
    {
        printf( "WARNING: %d (out of %d) input files could "
                "not be read\n", nb_err1, nb_tot1 );
    }

    if( nb_err2 != 0 )
    {
        printf( "WARNING: %d (out of %d) computed checksums did "
                "not match\n", nb_err2, nb_tot2 );
    }

    return( nb_err1 != 0 || nb_err2 != 0 );
}

int main( int argc, char *argv[] )
{
    int ret, i;

    if( argc == 1 )
    {
        printf( "print mode:  sha1sum <file> <file> ...\n" );
        printf( "check mode:  sha1sum -c <checksum file>\n" );

#if defined(_WIN32)
        printf( "\n  Press Enter to exit this program.\n" );
        fflush( stdout ); getchar();
#endif

        return( 1 );
    }

    if( argc == 3 && strcmp( "-c", argv[1] ) == 0 )
        return( sha1_check( argv[2] ) );

    ret = 0;
    for( i = 1; i < argc; i++ )
        ret |= sha1_print( argv[i] );

    return( ret );
}
#endif /* POLARSSL_SHA1_C && POLARSSL_FS_IO */
