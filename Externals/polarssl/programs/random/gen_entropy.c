/**
 *  \brief Use and generate multiple entropies calls into a file
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

#include "polarssl/config.h"

#include "polarssl/entropy.h"

#include <stdio.h>

#if !defined(POLARSSL_ENTROPY_C)
int main( int argc, char *argv[] )
{
    ((void) argc);
    ((void) argv);

    printf("POLARSSL_ENTROPY_C not defined.\n");
    return( 0 );
}
#else
int main( int argc, char *argv[] )
{
    FILE *f;
    int i, k, ret;
    entropy_context entropy;
    unsigned char buf[ENTROPY_BLOCK_SIZE];

    if( argc < 2 )
    {
        fprintf( stderr, "usage: %s <output filename>\n", argv[0] );
        return( 1 );
    }

    if( ( f = fopen( argv[1], "wb+" ) ) == NULL )
    {
        printf( "failed to open '%s' for writing.\n", argv[0] );
        return( 1 );
    }

    entropy_init( &entropy );

    for( i = 0, k = 768; i < k; i++ )
    {
        ret = entropy_func( &entropy, buf, sizeof( buf ) );
        if( ret != 0 )
        {
            printf("failed!\n");
            goto cleanup;
        }

        fwrite( buf, 1, sizeof( buf ), f );

        printf( "Generating 32Mb of data in file '%s'... %04.1f" \
                "%% done\r", argv[1], (100 * (float) (i + 1)) / k );
        fflush( stdout );
    }

    ret = 0;

cleanup:

    fclose( f );

    return( ret );
}
#endif /* POLARSSL_ENTROPY_C */
