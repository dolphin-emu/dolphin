/*
 * ASN.1 buffer writing functionality
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

#include "polarssl/config.h"

#if defined(POLARSSL_ASN1_WRITE_C)

#include "polarssl/asn1write.h"

int asn1_write_len( unsigned char **p, unsigned char *start, size_t len )
{
    if( len < 0x80 )
    {
        if( *p - start < 1 )
            return( POLARSSL_ERR_ASN1_BUF_TOO_SMALL );

        *--(*p) = len;
        return( 1 );
    }

    if( len <= 0xFF )
    {
        if( *p - start < 2 )
            return( POLARSSL_ERR_ASN1_BUF_TOO_SMALL );

        *--(*p) = len;
        *--(*p) = 0x81;
        return( 2 );
    }

    if( *p - start < 3 )
        return( POLARSSL_ERR_ASN1_BUF_TOO_SMALL );

    // We assume we never have lengths larger than 65535 bytes
    //
    *--(*p) = len % 256;
    *--(*p) = ( len / 256 ) % 256;
    *--(*p) = 0x82;

    return( 3 );
}

int asn1_write_tag( unsigned char **p, unsigned char *start, unsigned char tag )
{
    if( *p - start < 1 )
        return( POLARSSL_ERR_ASN1_BUF_TOO_SMALL );

    *--(*p) = tag;

    return( 1 );
}

int asn1_write_mpi( unsigned char **p, unsigned char *start, mpi *X )
{
    int ret;
    size_t len = 0;

    // Write the MPI
    //
    len = mpi_size( X );

    if( *p - start < (int) len )
        return( POLARSSL_ERR_ASN1_BUF_TOO_SMALL );

    (*p) -= len;
    mpi_write_binary( X, *p, len );

    // DER format assumes 2s complement for numbers, so the leftmost bit
    // should be 0 for positive numbers and 1 for negative numbers.
    //
    if ( X->s ==1 && **p & 0x80 )
    {
        if( *p - start < 1 )
            return( POLARSSL_ERR_ASN1_BUF_TOO_SMALL );

        *--(*p) = 0x00;
        len += 1;
    }

    ASN1_CHK_ADD( len, asn1_write_len( p, start, len ) );
    ASN1_CHK_ADD( len, asn1_write_tag( p, start, ASN1_INTEGER ) );

    return( len );
}
    
int asn1_write_null( unsigned char **p, unsigned char *start )
{
    int ret;
    size_t len = 0;

    // Write NULL
    //
    ASN1_CHK_ADD( len, asn1_write_len( p, start, 0) );
    ASN1_CHK_ADD( len, asn1_write_tag( p, start, ASN1_NULL ) );

    return( len );
}

int asn1_write_oid( unsigned char **p, unsigned char *start, char *oid )
{
    int ret;
    size_t len = 0;

    // Write OID
    //
    len = strlen( oid );

    if( *p - start < (int) len )
        return( POLARSSL_ERR_ASN1_BUF_TOO_SMALL );

    (*p) -= len;
    memcpy( *p, oid, len );

    ASN1_CHK_ADD( len , asn1_write_len( p, start, len ) );
    ASN1_CHK_ADD( len , asn1_write_tag( p, start, ASN1_OID ) );

    return( len );
}

int asn1_write_algorithm_identifier( unsigned char **p, unsigned char *start,
                                     char *algorithm_oid )
{
    int ret;
    size_t null_len = 0;
    size_t oid_len = 0;
    size_t len = 0;

    // Write NULL
    //
    ASN1_CHK_ADD( null_len, asn1_write_null( p, start ) );

    // Write OID
    //
    ASN1_CHK_ADD( oid_len, asn1_write_oid( p, start, algorithm_oid ) );

    len = oid_len + null_len;
    ASN1_CHK_ADD( len, asn1_write_len( p, start, oid_len + null_len ) );
    ASN1_CHK_ADD( len, asn1_write_tag( p, start,
                                       ASN1_CONSTRUCTED | ASN1_SEQUENCE ) );

    return( len );
}

int asn1_write_int( unsigned char **p, unsigned char *start, int val )
{
    int ret;
    size_t len = 0;

    // TODO negative values and values larger than 128
    // DER format assumes 2s complement for numbers, so the leftmost bit
    // should be 0 for positive numbers and 1 for negative numbers.
    //
    if( *p - start < 1 )
        return( POLARSSL_ERR_ASN1_BUF_TOO_SMALL );

    len += 1;
    *--(*p) = val;

    if ( val > 0 && **p & 0x80 )
    {
        if( *p - start < 1 )
            return( POLARSSL_ERR_ASN1_BUF_TOO_SMALL );

        *--(*p) = 0x00;
        len += 1;
    }

    ASN1_CHK_ADD( len, asn1_write_len( p, start, len ) );
    ASN1_CHK_ADD( len, asn1_write_tag( p, start, ASN1_INTEGER ) );

    return( len );
}

int asn1_write_printable_string( unsigned char **p, unsigned char *start,
                                 char *text )
{
    int ret;
    size_t len = 0;

    // Write string
    //
    len = strlen( text );

    if( *p - start < (int) len )
        return( POLARSSL_ERR_ASN1_BUF_TOO_SMALL );

    (*p) -= len;
    memcpy( *p, text, len );

    ASN1_CHK_ADD( len, asn1_write_len( p, start, len ) );
    ASN1_CHK_ADD( len, asn1_write_tag( p, start, ASN1_PRINTABLE_STRING ) );

    return( len );
}
    
int asn1_write_ia5_string( unsigned char **p, unsigned char *start,
                          char *text )
{
    int ret;
    size_t len = 0;

    // Write string
    //
    len = strlen( text );

    if( *p - start < (int) len )
        return( POLARSSL_ERR_ASN1_BUF_TOO_SMALL );

    (*p) -= len;
    memcpy( *p, text, len );

    ASN1_CHK_ADD( len, asn1_write_len( p, start, len ) );
    ASN1_CHK_ADD( len, asn1_write_tag( p, start, ASN1_IA5_STRING ) );

    return( len );
}
    

#endif
