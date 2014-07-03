/*
 *  X.509 base functions for creating certificates / CSRs
 *
 *  Copyright (C) 2006-2013, Brainspark B.V.
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

#if defined(POLARSSL_X509_CREATE_C)

#include "polarssl/x509.h"
#include "polarssl/asn1write.h"
#include "polarssl/oid.h"

#if defined(_MSC_VER) && !defined strncasecmp && !defined(EFIX64) && \
    !defined(EFI32)
#define strncasecmp _strnicmp
#endif

int x509_string_to_names( asn1_named_data **head, const char *name )
{
    int ret = 0;
    const char *s = name, *c = s;
    const char *end = s + strlen( s );
    const char *oid = NULL;
    int in_tag = 1;
    asn1_named_data *cur;

    /* Clear existing chain if present */
    asn1_free_named_data_list( head );

    while( c <= end )
    {
        if( in_tag && *c == '=' )
        {
            if( c - s == 2 && strncasecmp( s, "CN", 2 ) == 0 )
                oid = OID_AT_CN;
            else if( c - s == 1 && strncasecmp( s, "C", 1 ) == 0 )
                oid = OID_AT_COUNTRY;
            else if( c - s == 1 && strncasecmp( s, "O", 1 ) == 0 )
                oid = OID_AT_ORGANIZATION;
            else if( c - s == 1 && strncasecmp( s, "L", 1 ) == 0 )
                oid = OID_AT_LOCALITY;
            else if( c - s == 1 && strncasecmp( s, "R", 1 ) == 0 )
                oid = OID_PKCS9_EMAIL;
            else if( c - s == 2 && strncasecmp( s, "OU", 2 ) == 0 )
                oid = OID_AT_ORG_UNIT;
            else if( c - s == 2 && strncasecmp( s, "ST", 2 ) == 0 )
                oid = OID_AT_STATE;
            else if( c - s == 12 && strncasecmp( s, "serialNumber", 12 ) == 0 )
                oid = OID_AT_SERIAL_NUMBER;
            else if( c - s == 13 && strncasecmp( s, "postalAddress", 13 ) == 0 )
                oid = OID_AT_POSTAL_ADDRESS;
            else if( c - s == 10 && strncasecmp( s, "postalCode", 10 ) == 0 )
                oid = OID_AT_POSTAL_CODE;
            else
            {
                ret = POLARSSL_ERR_X509_UNKNOWN_OID;
                goto exit;
            }

            s = c + 1;
            in_tag = 0;
        }

        if( !in_tag && ( *c == ',' || c == end ) )
        {
            if( ( cur = asn1_store_named_data( head, oid, strlen( oid ),
                                               (unsigned char *) s,
                                               c - s ) ) == NULL )
            {
                return( POLARSSL_ERR_X509_MALLOC_FAILED );
            }

            while( c < end && *(c + 1) == ' ' )
                c++;

            s = c + 1;
            in_tag = 1;
        }
        c++;
    }

exit:

    return( ret );
}

/* The first byte of the value in the asn1_named_data structure is reserved
 * to store the critical boolean for us
 */
int x509_set_extension( asn1_named_data **head, const char *oid, size_t oid_len,
                        int critical, const unsigned char *val, size_t val_len )
{
    asn1_named_data *cur;

    if( ( cur = asn1_store_named_data( head, oid, oid_len,
                                       NULL, val_len + 1 ) ) == NULL )
    {
        return( POLARSSL_ERR_X509_MALLOC_FAILED );
    }

    cur->val.p[0] = critical;
    memcpy( cur->val.p + 1, val, val_len );

    return( 0 );
}

/*
 *  RelativeDistinguishedName ::=
 *    SET OF AttributeTypeAndValue
 *
 *  AttributeTypeAndValue ::= SEQUENCE {
 *    type     AttributeType,
 *    value    AttributeValue }
 *
 *  AttributeType ::= OBJECT IDENTIFIER
 *
 *  AttributeValue ::= ANY DEFINED BY AttributeType
 */
static int x509_write_name( unsigned char **p, unsigned char *start,
                            const char *oid, size_t oid_len,
                            const unsigned char *name, size_t name_len )
{
    int ret;
    size_t len = 0;

    // Write PrintableString for all except OID_PKCS9_EMAIL
    //
    if( OID_SIZE( OID_PKCS9_EMAIL ) == oid_len &&
        memcmp( oid, OID_PKCS9_EMAIL, oid_len ) == 0 )
    {
        ASN1_CHK_ADD( len, asn1_write_ia5_string( p, start,
                                                  (const char *) name,
                                                  name_len ) );
    }
    else
    {
        ASN1_CHK_ADD( len, asn1_write_printable_string( p, start,
                                                        (const char *) name,
                                                        name_len ) );
    }

    // Write OID
    //
    ASN1_CHK_ADD( len, asn1_write_oid( p, start, oid, oid_len ) );

    ASN1_CHK_ADD( len, asn1_write_len( p, start, len ) );
    ASN1_CHK_ADD( len, asn1_write_tag( p, start, ASN1_CONSTRUCTED | ASN1_SEQUENCE ) );

    ASN1_CHK_ADD( len, asn1_write_len( p, start, len ) );
    ASN1_CHK_ADD( len, asn1_write_tag( p, start, ASN1_CONSTRUCTED | ASN1_SET ) );

    return( (int) len );
}

int x509_write_names( unsigned char **p, unsigned char *start,
                      asn1_named_data *first )
{
    int ret;
    size_t len = 0;
    asn1_named_data *cur = first;

    while( cur != NULL )
    {
        ASN1_CHK_ADD( len, x509_write_name( p, start, (char *) cur->oid.p,
                                            cur->oid.len,
                                            cur->val.p, cur->val.len ) );
        cur = cur->next;
    }

    ASN1_CHK_ADD( len, asn1_write_len( p, start, len ) );
    ASN1_CHK_ADD( len, asn1_write_tag( p, start, ASN1_CONSTRUCTED | ASN1_SEQUENCE ) );

    return( (int) len );
}

int x509_write_sig( unsigned char **p, unsigned char *start,
                    const char *oid, size_t oid_len,
                    unsigned char *sig, size_t size )
{
    int ret;
    size_t len = 0;

    if( *p - start < (int) size + 1 )
        return( POLARSSL_ERR_ASN1_BUF_TOO_SMALL );

    len = size;
    (*p) -= len;
    memcpy( *p, sig, len );

    *--(*p) = 0;
    len += 1;

    ASN1_CHK_ADD( len, asn1_write_len( p, start, len ) );
    ASN1_CHK_ADD( len, asn1_write_tag( p, start, ASN1_BIT_STRING ) );

    // Write OID
    //
    ASN1_CHK_ADD( len, asn1_write_algorithm_identifier( p, start, oid,
                                                        oid_len, 0 ) );

    return( (int) len );
}

static int x509_write_extension( unsigned char **p, unsigned char *start,
                                 asn1_named_data *ext )
{
    int ret;
    size_t len = 0;

    ASN1_CHK_ADD( len, asn1_write_raw_buffer( p, start, ext->val.p + 1,
                                              ext->val.len - 1 ) );
    ASN1_CHK_ADD( len, asn1_write_len( p, start, ext->val.len - 1 ) );
    ASN1_CHK_ADD( len, asn1_write_tag( p, start, ASN1_OCTET_STRING ) );

    if( ext->val.p[0] != 0 )
    {
        ASN1_CHK_ADD( len, asn1_write_bool( p, start, 1 ) );
    }

    ASN1_CHK_ADD( len, asn1_write_raw_buffer( p, start, ext->oid.p,
                                              ext->oid.len ) );
    ASN1_CHK_ADD( len, asn1_write_len( p, start, ext->oid.len ) );
    ASN1_CHK_ADD( len, asn1_write_tag( p, start, ASN1_OID ) );

    ASN1_CHK_ADD( len, asn1_write_len( p, start, len ) );
    ASN1_CHK_ADD( len, asn1_write_tag( p, start, ASN1_CONSTRUCTED | ASN1_SEQUENCE ) );

    return( (int) len );
}

/*
 * Extension  ::=  SEQUENCE  {
 *     extnID      OBJECT IDENTIFIER,
 *     critical    BOOLEAN DEFAULT FALSE,
 *     extnValue   OCTET STRING
 *                 -- contains the DER encoding of an ASN.1 value
 *                 -- corresponding to the extension type identified
 *                 -- by extnID
 *     }
 */
int x509_write_extensions( unsigned char **p, unsigned char *start,
                           asn1_named_data *first )
{
    int ret;
    size_t len = 0;
    asn1_named_data *cur_ext = first;

    while( cur_ext != NULL )
    {
        ASN1_CHK_ADD( len, x509_write_extension( p, start, cur_ext ) );
        cur_ext = cur_ext->next;
    }

    return( (int) len );
}

#endif /* POLARSSL_X509_CREATE_C */
