/*
 *  Simple MPI demonstration program
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
#include "polarssl/bignum.h"

#if !defined(POLARSSL_BIGNUM_C) || !defined(POLARSSL_FS_IO)
int main( int argc, char *argv[] )
{
    ((void) argc);
    ((void) argv);

    printf("POLARSSL_BIGNUM_C and/or POLARSSL_FS_IO not defined.\n");
    return( 0 );
}
#else
int main( int argc, char *argv[] )
{
    mpi E, P, Q, N, H, D, X, Y, Z;

    ((void) argc);
    ((void) argv);

    mpi_init( &E ); mpi_init( &P ); mpi_init( &Q ); mpi_init( &N );
    mpi_init( &H ); mpi_init( &D ); mpi_init( &X ); mpi_init( &Y );
    mpi_init( &Z );

    mpi_read_string( &P, 10, "2789" );
    mpi_read_string( &Q, 10, "3203" );
    mpi_read_string( &E, 10,  "257" );
    mpi_mul_mpi( &N, &P, &Q );

    printf( "\n  Public key:\n\n" );
    mpi_write_file( "  N = ", &N, 10, NULL );
    mpi_write_file( "  E = ", &E, 10, NULL );

    printf( "\n  Private key:\n\n" );
    mpi_write_file( "  P = ", &P, 10, NULL );
    mpi_write_file( "  Q = ", &Q, 10, NULL );

#if defined(POLARSSL_GENPRIME)
    mpi_sub_int( &P, &P, 1 );
    mpi_sub_int( &Q, &Q, 1 );
    mpi_mul_mpi( &H, &P, &Q );
    mpi_inv_mod( &D, &E, &H );

    mpi_write_file( "  D = E^-1 mod (P-1)*(Q-1) = ",
                    &D, 10, NULL );
#else
    printf("\nTest skipped (POLARSSL_GENPRIME not defined).\n\n");
#endif
    mpi_read_string( &X, 10, "55555" );
    mpi_exp_mod( &Y, &X, &E, &N, NULL );
    mpi_exp_mod( &Z, &Y, &D, &N, NULL );

    printf( "\n  RSA operation:\n\n" );
    mpi_write_file( "  X (plaintext)  = ", &X, 10, NULL );
    mpi_write_file( "  Y (ciphertext) = X^E mod N = ", &Y, 10, NULL );
    mpi_write_file( "  Z (decrypted)  = Y^D mod N = ", &Z, 10, NULL );
    printf( "\n" );

    mpi_free( &E ); mpi_free( &P ); mpi_free( &Q ); mpi_free( &N );
    mpi_free( &H ); mpi_free( &D ); mpi_free( &X ); mpi_free( &Y );
    mpi_free( &Z );

#if defined(_WIN32)
    printf( "  Press Enter to exit this program.\n" );
    fflush( stdout ); getchar();
#endif

    return( 0 );
}
#endif /* POLARSSL_BIGNUM_C && POLARSSL_FS_IO */
