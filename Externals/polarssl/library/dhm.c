/*
 *  Diffie-Hellman-Merkle key exchange
 *
 *  Copyright (C) 2006-2010, Brainspark B.V.
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
/*
 *  Reference:
 *
 *  http://www.cacr.math.uwaterloo.ca/hac/ (chapter 12)
 */

#include "polarssl/config.h"

#if defined(POLARSSL_DHM_C)

#include "polarssl/dhm.h"

/*
 * helper to validate the mpi size and import it
 */
static int dhm_read_bignum( mpi *X,
                            unsigned char **p,
                            const unsigned char *end )
{
    int ret, n;

    if( end - *p < 2 )
        return( POLARSSL_ERR_DHM_BAD_INPUT_DATA );

    n = ( (*p)[0] << 8 ) | (*p)[1];
    (*p) += 2;

    if( (int)( end - *p ) < n )
        return( POLARSSL_ERR_DHM_BAD_INPUT_DATA );

    if( ( ret = mpi_read_binary( X, *p, n ) ) != 0 )
        return( POLARSSL_ERR_DHM_READ_PARAMS_FAILED + ret );

    (*p) += n;

    return( 0 );
}

/*
 * Verify sanity of parameter with regards to P
 *
 * Parameter should be: 2 <= public_param <= P - 2
 *
 * For more information on the attack, see:
 *  http://www.cl.cam.ac.uk/~rja14/Papers/psandqs.pdf
 *  http://web.nvd.nist.gov/view/vuln/detail?vulnId=CVE-2005-2643
 */
static int dhm_check_range( const mpi *param, const mpi *P )
{
    mpi L, U;
    int ret = POLARSSL_ERR_DHM_BAD_INPUT_DATA;

    mpi_init( &L ); mpi_init( &U );
    mpi_lset( &L, 2 );
    mpi_sub_int( &U, P, 2 );

    if( mpi_cmp_mpi( param, &L ) >= 0 &&
        mpi_cmp_mpi( param, &U ) <= 0 )
    {
        ret = 0;
    }

    mpi_free( &L ); mpi_free( &U );

    return( ret );
}

/*
 * Parse the ServerKeyExchange parameters
 */
int dhm_read_params( dhm_context *ctx,
                     unsigned char **p,
                     const unsigned char *end )
{
    int ret;

    memset( ctx, 0, sizeof( dhm_context ) );

    if( ( ret = dhm_read_bignum( &ctx->P,  p, end ) ) != 0 ||
        ( ret = dhm_read_bignum( &ctx->G,  p, end ) ) != 0 ||
        ( ret = dhm_read_bignum( &ctx->GY, p, end ) ) != 0 )
        return( ret );

    if( ( ret = dhm_check_range( &ctx->GY, &ctx->P ) ) != 0 )
        return( ret );

    ctx->len = mpi_size( &ctx->P );

    if( end - *p < 2 )
        return( POLARSSL_ERR_DHM_BAD_INPUT_DATA );

    return( 0 );
}

/*
 * Setup and write the ServerKeyExchange parameters
 */
int dhm_make_params( dhm_context *ctx, int x_size,
                     unsigned char *output, size_t *olen,
                     int (*f_rng)(void *, unsigned char *, size_t),
                     void *p_rng )
{
    int ret, count = 0;
    size_t n1, n2, n3;
    unsigned char *p;

    if( mpi_cmp_int( &ctx->P, 0 ) == 0 )
        return( POLARSSL_ERR_DHM_BAD_INPUT_DATA );

    /*
     * Generate X as large as possible ( < P )
     */
    do
    {
        mpi_fill_random( &ctx->X, x_size, f_rng, p_rng );

        while( mpi_cmp_mpi( &ctx->X, &ctx->P ) >= 0 )
            mpi_shift_r( &ctx->X, 1 );

        if( count++ > 10 )
            return( POLARSSL_ERR_DHM_MAKE_PARAMS_FAILED );
    }
    while( dhm_check_range( &ctx->X, &ctx->P ) != 0 );

    /*
     * Calculate GX = G^X mod P
     */
    MPI_CHK( mpi_exp_mod( &ctx->GX, &ctx->G, &ctx->X,
                          &ctx->P , &ctx->RP ) );

    if( ( ret = dhm_check_range( &ctx->GX, &ctx->P ) ) != 0 )
        return( ret );

    /*
     * export P, G, GX
     */
#define DHM_MPI_EXPORT(X,n)                     \
    MPI_CHK( mpi_write_binary( X, p + 2, n ) ); \
    *p++ = (unsigned char)( n >> 8 );           \
    *p++ = (unsigned char)( n      ); p += n;

    n1 = mpi_size( &ctx->P  );
    n2 = mpi_size( &ctx->G  );
    n3 = mpi_size( &ctx->GX );

    p = output;
    DHM_MPI_EXPORT( &ctx->P , n1 );
    DHM_MPI_EXPORT( &ctx->G , n2 );
    DHM_MPI_EXPORT( &ctx->GX, n3 );

    *olen  = p - output;

    ctx->len = n1;

cleanup:

    if( ret != 0 )
        return( POLARSSL_ERR_DHM_MAKE_PARAMS_FAILED + ret );

    return( 0 );
}

/*
 * Import the peer's public value G^Y
 */
int dhm_read_public( dhm_context *ctx,
                     const unsigned char *input, size_t ilen )
{
    int ret;

    if( ctx == NULL || ilen < 1 || ilen > ctx->len )
        return( POLARSSL_ERR_DHM_BAD_INPUT_DATA );

    if( ( ret = mpi_read_binary( &ctx->GY, input, ilen ) ) != 0 )
        return( POLARSSL_ERR_DHM_READ_PUBLIC_FAILED + ret );

    return( 0 );
}

/*
 * Create own private value X and export G^X
 */
int dhm_make_public( dhm_context *ctx, int x_size,
                     unsigned char *output, size_t olen,
                     int (*f_rng)(void *, unsigned char *, size_t),
                     void *p_rng )
{
    int ret, count = 0;

    if( ctx == NULL || olen < 1 || olen > ctx->len )
        return( POLARSSL_ERR_DHM_BAD_INPUT_DATA );

    if( mpi_cmp_int( &ctx->P, 0 ) == 0 )
        return( POLARSSL_ERR_DHM_BAD_INPUT_DATA );

    /*
     * generate X and calculate GX = G^X mod P
     */
    do
    {
        mpi_fill_random( &ctx->X, x_size, f_rng, p_rng );

        while( mpi_cmp_mpi( &ctx->X, &ctx->P ) >= 0 )
            mpi_shift_r( &ctx->X, 1 );

        if( count++ > 10 )
            return( POLARSSL_ERR_DHM_MAKE_PUBLIC_FAILED );
    }
    while( dhm_check_range( &ctx->X, &ctx->P ) != 0 );

    MPI_CHK( mpi_exp_mod( &ctx->GX, &ctx->G, &ctx->X,
                          &ctx->P , &ctx->RP ) );

    if( ( ret = dhm_check_range( &ctx->GX, &ctx->P ) ) != 0 )
        return( ret );

    MPI_CHK( mpi_write_binary( &ctx->GX, output, olen ) );

cleanup:

    if( ret != 0 )
        return( POLARSSL_ERR_DHM_MAKE_PUBLIC_FAILED + ret );

    return( 0 );
}

/*
 * Derive and export the shared secret (G^Y)^X mod P
 */
int dhm_calc_secret( dhm_context *ctx,
                     unsigned char *output, size_t *olen )
{
    int ret;

    if( ctx == NULL || *olen < ctx->len )
        return( POLARSSL_ERR_DHM_BAD_INPUT_DATA );

    MPI_CHK( mpi_exp_mod( &ctx->K, &ctx->GY, &ctx->X,
                          &ctx->P, &ctx->RP ) );

    if( ( ret = dhm_check_range( &ctx->GY, &ctx->P ) ) != 0 )
        return( ret );

    *olen = mpi_size( &ctx->K );

    MPI_CHK( mpi_write_binary( &ctx->K, output, *olen ) );

cleanup:

    if( ret != 0 )
        return( POLARSSL_ERR_DHM_CALC_SECRET_FAILED + ret );

    return( 0 );
}

/*
 * Free the components of a DHM key
 */
void dhm_free( dhm_context *ctx )
{
    mpi_free( &ctx->RP ); mpi_free( &ctx->K ); mpi_free( &ctx->GY );
    mpi_free( &ctx->GX ); mpi_free( &ctx->X ); mpi_free( &ctx->G );
    mpi_free( &ctx->P );
}

#if defined(POLARSSL_SELF_TEST)

/*
 * Checkup routine
 */
int dhm_self_test( int verbose )
{
    return( verbose++ );
}

#endif

#endif
