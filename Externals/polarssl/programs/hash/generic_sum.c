/*
 *  generic message digest layer demonstration program
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

#if !defined(POLARSSL_MD_C)
int main( int argc, char *argv[] )
{
    ((void) argc);
    ((void) argv);

    printf("POLARSSL_MD_C not defined.\n");
    return( 0 );
}
#else
static int generic_wrapper( const md_info_t *md_info, char *filename, unsigned char *sum )
{
    int ret = md_file( md_info, filename, sum );

    if( ret == 1 )
        fprintf( stderr, "failed to open: %s\n", filename );

    if( ret == 2 )
        fprintf( stderr, "failed to read: %s\n", filename );

    return( ret );
}

static int generic_print( const md_info_t *md_info, char *filename )
{
    int i;
    unsigned char sum[POLARSSL_MD_MAX_SIZE];

    if( generic_wrapper( md_info, filename, sum ) != 0 )
        return( 1 );

    for( i = 0; i < md_info->size; i++ )
        printf( "%02x", sum[i] );

    printf( "  %s\n", filename );
    return( 0 );
}

static int generic_check( const md_info_t *md_info, char *filename )
{
    int i;
    size_t n;
    FILE *f;
    int nb_err1, nb_err2;
    int nb_tot1, nb_tot2;
    unsigned char sum[POLARSSL_MD_MAX_SIZE];
    char buf[POLARSSL_MD_MAX_SIZE * 2 + 1], line[1024];

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

        if( n < (size_t) 2 * md_info->size + 4 )
        {
            printf("No '%s' hash found on line.\n", md_info->name);
            continue;
        }

        if( line[2 * md_info->size] != ' ' || line[2 * md_info->size + 1] != ' ' )
        {
            printf("No '%s' hash found on line.\n", md_info->name);
            continue;
        }

        if( line[n - 1] == '\n' ) { n--; line[n] = '\0'; }
        if( line[n - 1] == '\r' ) { n--; line[n] = '\0'; }

        nb_tot1++;

        if( generic_wrapper( md_info, line + 2 + 2 * md_info->size, sum ) != 0 )
        {
            nb_err1++;
            continue;
        }

        nb_tot2++;

        for( i = 0; i < md_info->size; i++ )
            sprintf( buf + i * 2, "%02x", sum[i] );

        if( memcmp( line, buf, 2 * md_info->size ) != 0 )
        {
            nb_err2++;
            fprintf( stderr, "wrong checksum: %s\n", line + 66 );
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
    const md_info_t *md_info;
    md_context_t md_ctx;

    memset( &md_ctx, 0, sizeof( md_context_t ));

    if( argc == 1 )
    {
        const int *list;

        printf( "print mode:  generic_sum <md> <file> <file> ...\n" );
        printf( "check mode:  generic_sum <md> -c <checksum file>\n" );

        printf( "\nAvailable message digests:\n" );
        list = md_list();
        while( *list )
        {
            md_info = md_info_from_type( *list );
            printf( "  %s\n", md_info->name );
            list++;
        }

#if defined(_WIN32)
        printf( "\n  Press Enter to exit this program.\n" );
        fflush( stdout ); getchar();
#endif

        return( 1 );
    }

    /*
     * Read the MD from the command line
     */
    md_info = md_info_from_string( argv[1] );
    if( md_info == NULL )
    {
        fprintf( stderr, "Message Digest '%s' not found\n", argv[1] );
        return( 1 );
    }
    if( md_init_ctx( &md_ctx, md_info) )
    {
        fprintf( stderr, "Failed to initialize context.\n" );
        return( 1 );
    }

    ret = 0;
    if( argc == 4 && strcmp( "-c", argv[2] ) == 0 )
    {
        ret |= generic_check( md_info, argv[3] );
        goto exit;
    }

    for( i = 2; i < argc; i++ )
        ret |= generic_print( md_info, argv[i] );

exit:
    md_free_ctx( &md_ctx );

    return( ret );
}
#endif /* POLARSSL_MD_C */
