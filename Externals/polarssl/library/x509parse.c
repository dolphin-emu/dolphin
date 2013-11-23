/*
 *  X.509 certificate and private key decoding
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
/*
 *  The ITU-T X.509 standard defines a certificate format for PKI.
 *
 *  http://www.ietf.org/rfc/rfc3279.txt
 *  http://www.ietf.org/rfc/rfc3280.txt
 *
 *  ftp://ftp.rsasecurity.com/pub/pkcs/ascii/pkcs-1v2.asc
 *
 *  http://www.itu.int/ITU-T/studygroups/com17/languages/X.680-0207.pdf
 *  http://www.itu.int/ITU-T/studygroups/com17/languages/X.690-0207.pdf
 */

#include "polarssl/config.h"

#if defined(POLARSSL_X509_PARSE_C)

#include "polarssl/x509.h"
#include "polarssl/asn1.h"
#include "polarssl/pem.h"
#include "polarssl/des.h"
#if defined(POLARSSL_MD2_C)
#include "polarssl/md2.h"
#endif
#if defined(POLARSSL_MD4_C)
#include "polarssl/md4.h"
#endif
#if defined(POLARSSL_MD5_C)
#include "polarssl/md5.h"
#endif
#if defined(POLARSSL_SHA1_C)
#include "polarssl/sha1.h"
#endif
#if defined(POLARSSL_SHA2_C)
#include "polarssl/sha2.h"
#endif
#if defined(POLARSSL_SHA4_C)
#include "polarssl/sha4.h"
#endif
#include "polarssl/dhm.h"
#if defined(POLARSSL_PKCS5_C)
#include "polarssl/pkcs5.h"
#endif
#if defined(POLARSSL_PKCS12_C)
#include "polarssl/pkcs12.h"
#endif

#include <string.h>
#include <stdlib.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#endif

#if defined(POLARSSL_FS_IO)
#include <stdio.h>
#if !defined(_WIN32)
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#endif
#endif

/* Compare a given OID string with an OID x509_buf * */
#define OID_CMP(oid_str, oid_buf) \
        ( ( OID_SIZE(oid_str) == (oid_buf)->len ) && \
                memcmp( (oid_str), (oid_buf)->p, (oid_buf)->len) == 0)

/*
 *  Version  ::=  INTEGER  {  v1(0), v2(1), v3(2)  }
 */
static int x509_get_version( unsigned char **p,
                             const unsigned char *end,
                             int *ver )
{
    int ret;
    size_t len;

    if( ( ret = asn1_get_tag( p, end, &len,
            ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | 0 ) ) != 0 )
    {
        if( ret == POLARSSL_ERR_ASN1_UNEXPECTED_TAG )
        {
            *ver = 0;
            return( 0 );
        }

        return( ret );
    }

    end = *p + len;

    if( ( ret = asn1_get_int( p, end, ver ) ) != 0 )
        return( POLARSSL_ERR_X509_CERT_INVALID_VERSION + ret );

    if( *p != end )
        return( POLARSSL_ERR_X509_CERT_INVALID_VERSION +
                POLARSSL_ERR_ASN1_LENGTH_MISMATCH );

    return( 0 );
}

/*
 *  Version  ::=  INTEGER  {  v1(0), v2(1)  }
 */
static int x509_crl_get_version( unsigned char **p,
                             const unsigned char *end,
                             int *ver )
{
    int ret;

    if( ( ret = asn1_get_int( p, end, ver ) ) != 0 )
    {
        if( ret == POLARSSL_ERR_ASN1_UNEXPECTED_TAG )
        {
            *ver = 0;
            return( 0 );
        }

        return( POLARSSL_ERR_X509_CERT_INVALID_VERSION + ret );
    }

    return( 0 );
}

/*
 *  CertificateSerialNumber  ::=  INTEGER
 */
static int x509_get_serial( unsigned char **p,
                            const unsigned char *end,
                            x509_buf *serial )
{
    int ret;

    if( ( end - *p ) < 1 )
        return( POLARSSL_ERR_X509_CERT_INVALID_SERIAL +
                POLARSSL_ERR_ASN1_OUT_OF_DATA );

    if( **p != ( ASN1_CONTEXT_SPECIFIC | ASN1_PRIMITIVE | 2 ) &&
        **p !=   ASN1_INTEGER )
        return( POLARSSL_ERR_X509_CERT_INVALID_SERIAL +
                POLARSSL_ERR_ASN1_UNEXPECTED_TAG );

    serial->tag = *(*p)++;

    if( ( ret = asn1_get_len( p, end, &serial->len ) ) != 0 )
        return( POLARSSL_ERR_X509_CERT_INVALID_SERIAL + ret );

    serial->p = *p;
    *p += serial->len;

    return( 0 );
}

/*
 *  AlgorithmIdentifier  ::=  SEQUENCE  {
 *       algorithm               OBJECT IDENTIFIER,
 *       parameters              ANY DEFINED BY algorithm OPTIONAL  }
 */
static int x509_get_alg( unsigned char **p,
                         const unsigned char *end,
                         x509_buf *alg )
{
    int ret;
    size_t len;

    if( ( ret = asn1_get_tag( p, end, &len,
            ASN1_CONSTRUCTED | ASN1_SEQUENCE ) ) != 0 )
        return( POLARSSL_ERR_X509_CERT_INVALID_ALG + ret );

    end = *p + len;
    alg->tag = **p;

    if( ( ret = asn1_get_tag( p, end, &alg->len, ASN1_OID ) ) != 0 )
        return( POLARSSL_ERR_X509_CERT_INVALID_ALG + ret );

    alg->p = *p;
    *p += alg->len;

    if( *p == end )
        return( 0 );

    /*
     * assume the algorithm parameters must be NULL
     */
    if( ( ret = asn1_get_tag( p, end, &len, ASN1_NULL ) ) != 0 )
        return( POLARSSL_ERR_X509_CERT_INVALID_ALG + ret );

    if( *p != end )
        return( POLARSSL_ERR_X509_CERT_INVALID_ALG +
                POLARSSL_ERR_ASN1_LENGTH_MISMATCH );

    return( 0 );
}

/*
 *  AttributeTypeAndValue ::= SEQUENCE {
 *    type     AttributeType,
 *    value    AttributeValue }
 *
 *  AttributeType ::= OBJECT IDENTIFIER
 *
 *  AttributeValue ::= ANY DEFINED BY AttributeType
 */
static int x509_get_attr_type_value( unsigned char **p,
                                     const unsigned char *end,
                                     x509_name *cur )
{
    int ret;
    size_t len;
    x509_buf *oid;
    x509_buf *val;

    if( ( ret = asn1_get_tag( p, end, &len,
            ASN1_CONSTRUCTED | ASN1_SEQUENCE ) ) != 0 )
        return( POLARSSL_ERR_X509_CERT_INVALID_NAME + ret );

    oid = &cur->oid;
    oid->tag = **p;

    if( ( ret = asn1_get_tag( p, end, &oid->len, ASN1_OID ) ) != 0 )
        return( POLARSSL_ERR_X509_CERT_INVALID_NAME + ret );

    oid->p = *p;
    *p += oid->len;

    if( ( end - *p ) < 1 )
        return( POLARSSL_ERR_X509_CERT_INVALID_NAME +
                POLARSSL_ERR_ASN1_OUT_OF_DATA );

    if( **p != ASN1_BMP_STRING && **p != ASN1_UTF8_STRING      &&
        **p != ASN1_T61_STRING && **p != ASN1_PRINTABLE_STRING &&
        **p != ASN1_IA5_STRING && **p != ASN1_UNIVERSAL_STRING )
        return( POLARSSL_ERR_X509_CERT_INVALID_NAME +
                POLARSSL_ERR_ASN1_UNEXPECTED_TAG );

    val = &cur->val;
    val->tag = *(*p)++;

    if( ( ret = asn1_get_len( p, end, &val->len ) ) != 0 )
        return( POLARSSL_ERR_X509_CERT_INVALID_NAME + ret );

    val->p = *p;
    *p += val->len;

    cur->next = NULL;

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
static int x509_get_name( unsigned char **p,
                          const unsigned char *end,
                          x509_name *cur )
{
    int ret;
    size_t len;
    const unsigned char *end2;
    x509_name *use; 
    
    if( ( ret = asn1_get_tag( p, end, &len,
            ASN1_CONSTRUCTED | ASN1_SET ) ) != 0 )
        return( POLARSSL_ERR_X509_CERT_INVALID_NAME + ret );

    end2 = end;
    end  = *p + len;
    use = cur;

    do
    {
        if( ( ret = x509_get_attr_type_value( p, end, use ) ) != 0 )
            return( ret );
        
        if( *p != end )
        {
            use->next = (x509_name *) malloc(
                    sizeof( x509_name ) );

            if( use->next == NULL )
                return( POLARSSL_ERR_X509_MALLOC_FAILED );
            
            memset( use->next, 0, sizeof( x509_name ) );

            use = use->next;
        }
    }
    while( *p != end );

    /*
     * recurse until end of SEQUENCE is reached
     */
    if( *p == end2 )
        return( 0 );

    cur->next = (x509_name *) malloc(
         sizeof( x509_name ) );

    if( cur->next == NULL )
        return( POLARSSL_ERR_X509_MALLOC_FAILED );

    memset( cur->next, 0, sizeof( x509_name ) );

    return( x509_get_name( p, end2, cur->next ) );
}

/*
 *  Time ::= CHOICE {
 *       utcTime        UTCTime,
 *       generalTime    GeneralizedTime }
 */
static int x509_get_time( unsigned char **p,
                          const unsigned char *end,
                          x509_time *time )
{
    int ret;
    size_t len;
    char date[64];
    unsigned char tag;

    if( ( end - *p ) < 1 )
        return( POLARSSL_ERR_X509_CERT_INVALID_DATE +
                POLARSSL_ERR_ASN1_OUT_OF_DATA );

    tag = **p;

    if ( tag == ASN1_UTC_TIME )
    {
        (*p)++;
        ret = asn1_get_len( p, end, &len );
        
        if( ret != 0 )
            return( POLARSSL_ERR_X509_CERT_INVALID_DATE + ret );

        memset( date,  0, sizeof( date ) );
        memcpy( date, *p, ( len < sizeof( date ) - 1 ) ?
                len : sizeof( date ) - 1 );

        if( sscanf( date, "%2d%2d%2d%2d%2d%2d",
                    &time->year, &time->mon, &time->day,
                    &time->hour, &time->min, &time->sec ) < 5 )
            return( POLARSSL_ERR_X509_CERT_INVALID_DATE );

        time->year +=  100 * ( time->year < 50 );
        time->year += 1900;

        *p += len;

        return( 0 );
    }
    else if ( tag == ASN1_GENERALIZED_TIME )
    {
        (*p)++;
        ret = asn1_get_len( p, end, &len );
        
        if( ret != 0 )
            return( POLARSSL_ERR_X509_CERT_INVALID_DATE + ret );

        memset( date,  0, sizeof( date ) );
        memcpy( date, *p, ( len < sizeof( date ) - 1 ) ?
                len : sizeof( date ) - 1 );

        if( sscanf( date, "%4d%2d%2d%2d%2d%2d",
                    &time->year, &time->mon, &time->day,
                    &time->hour, &time->min, &time->sec ) < 5 )
            return( POLARSSL_ERR_X509_CERT_INVALID_DATE );

        *p += len;

        return( 0 );
    }
    else
        return( POLARSSL_ERR_X509_CERT_INVALID_DATE + POLARSSL_ERR_ASN1_UNEXPECTED_TAG );
}


/*
 *  Validity ::= SEQUENCE {
 *       notBefore      Time,
 *       notAfter       Time }
 */
static int x509_get_dates( unsigned char **p,
                           const unsigned char *end,
                           x509_time *from,
                           x509_time *to )
{
    int ret;
    size_t len;

    if( ( ret = asn1_get_tag( p, end, &len,
            ASN1_CONSTRUCTED | ASN1_SEQUENCE ) ) != 0 )
        return( POLARSSL_ERR_X509_CERT_INVALID_DATE + ret );

    end = *p + len;

    if( ( ret = x509_get_time( p, end, from ) ) != 0 )
        return( ret );

    if( ( ret = x509_get_time( p, end, to ) ) != 0 )
        return( ret );

    if( *p != end )
        return( POLARSSL_ERR_X509_CERT_INVALID_DATE +
                POLARSSL_ERR_ASN1_LENGTH_MISMATCH );

    return( 0 );
}

/*
 *  SubjectPublicKeyInfo  ::=  SEQUENCE  {
 *       algorithm            AlgorithmIdentifier,
 *       subjectPublicKey     BIT STRING }
 */
static int x509_get_pubkey( unsigned char **p,
                            const unsigned char *end,
                            x509_buf *pk_alg_oid,
                            mpi *N, mpi *E )
{
    int ret;
    size_t len;
    unsigned char *end2;

    if( ( ret = x509_get_alg( p, end, pk_alg_oid ) ) != 0 )
        return( ret );

    /*
     * only RSA public keys handled at this time
     */
    if( pk_alg_oid->len != 9 ||
        memcmp( pk_alg_oid->p, OID_PKCS1_RSA, 9 ) != 0 )
    {
        return( POLARSSL_ERR_X509_UNKNOWN_PK_ALG );
    }

    if( ( ret = asn1_get_tag( p, end, &len, ASN1_BIT_STRING ) ) != 0 )
        return( POLARSSL_ERR_X509_CERT_INVALID_PUBKEY + ret );

    if( ( end - *p ) < 1 )
        return( POLARSSL_ERR_X509_CERT_INVALID_PUBKEY +
                POLARSSL_ERR_ASN1_OUT_OF_DATA );

    end2 = *p + len;

    if( *(*p)++ != 0 )
        return( POLARSSL_ERR_X509_CERT_INVALID_PUBKEY );

    /*
     *  RSAPublicKey ::= SEQUENCE {
     *      modulus           INTEGER,  -- n
     *      publicExponent    INTEGER   -- e
     *  }
     */
    if( ( ret = asn1_get_tag( p, end2, &len,
            ASN1_CONSTRUCTED | ASN1_SEQUENCE ) ) != 0 )
        return( POLARSSL_ERR_X509_CERT_INVALID_PUBKEY + ret );

    if( *p + len != end2 )
        return( POLARSSL_ERR_X509_CERT_INVALID_PUBKEY +
                POLARSSL_ERR_ASN1_LENGTH_MISMATCH );

    if( ( ret = asn1_get_mpi( p, end2, N ) ) != 0 ||
        ( ret = asn1_get_mpi( p, end2, E ) ) != 0 )
        return( POLARSSL_ERR_X509_CERT_INVALID_PUBKEY + ret );

    if( *p != end )
        return( POLARSSL_ERR_X509_CERT_INVALID_PUBKEY +
                POLARSSL_ERR_ASN1_LENGTH_MISMATCH );

    return( 0 );
}

static int x509_get_sig( unsigned char **p,
                         const unsigned char *end,
                         x509_buf *sig )
{
    int ret;
    size_t len;

    if( ( end - *p ) < 1 )
        return( POLARSSL_ERR_X509_CERT_INVALID_SIGNATURE +
                POLARSSL_ERR_ASN1_OUT_OF_DATA );

    sig->tag = **p;

    if( ( ret = asn1_get_tag( p, end, &len, ASN1_BIT_STRING ) ) != 0 )
        return( POLARSSL_ERR_X509_CERT_INVALID_SIGNATURE + ret );


    if( --len < 1 || *(*p)++ != 0 )
        return( POLARSSL_ERR_X509_CERT_INVALID_SIGNATURE );

    sig->len = len;
    sig->p = *p;

    *p += len;

    return( 0 );
}

/*
 * X.509 v2/v3 unique identifier (not parsed)
 */
static int x509_get_uid( unsigned char **p,
                         const unsigned char *end,
                         x509_buf *uid, int n )
{
    int ret;

    if( *p == end )
        return( 0 );

    uid->tag = **p;

    if( ( ret = asn1_get_tag( p, end, &uid->len,
            ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | n ) ) != 0 )
    {
        if( ret == POLARSSL_ERR_ASN1_UNEXPECTED_TAG )
            return( 0 );

        return( ret );
    }

    uid->p = *p;
    *p += uid->len;

    return( 0 );
}

/*
 * X.509 Extensions (No parsing of extensions, pointer should
 * be either manually updated or extensions should be parsed!
 */
static int x509_get_ext( unsigned char **p,
                         const unsigned char *end,
                         x509_buf *ext, int tag )
{
    int ret;
    size_t len;

    if( *p == end )
        return( 0 );

    ext->tag = **p;

    if( ( ret = asn1_get_tag( p, end, &ext->len,
            ASN1_CONTEXT_SPECIFIC | ASN1_CONSTRUCTED | tag ) ) != 0 )
        return( ret );

    ext->p = *p;
    end = *p + ext->len;

    /*
     * Extensions  ::=  SEQUENCE SIZE (1..MAX) OF Extension
     *
     * Extension  ::=  SEQUENCE  {
     *      extnID      OBJECT IDENTIFIER,
     *      critical    BOOLEAN DEFAULT FALSE,
     *      extnValue   OCTET STRING  }
     */
    if( ( ret = asn1_get_tag( p, end, &len,
            ASN1_CONSTRUCTED | ASN1_SEQUENCE ) ) != 0 )
        return( POLARSSL_ERR_X509_CERT_INVALID_EXTENSIONS + ret );

    if( end != *p + len )
        return( POLARSSL_ERR_X509_CERT_INVALID_EXTENSIONS +
                POLARSSL_ERR_ASN1_LENGTH_MISMATCH );

    return( 0 );
}

/*
 * X.509 CRL v2 extensions (no extensions parsed yet.)
 */
static int x509_get_crl_ext( unsigned char **p,
                             const unsigned char *end,
                             x509_buf *ext )
{
    int ret;
    size_t len = 0;

    /* Get explicit tag */
    if( ( ret = x509_get_ext( p, end, ext, 0) ) != 0 )
    {
        if( ret == POLARSSL_ERR_ASN1_UNEXPECTED_TAG )
            return( 0 );

        return( ret );
    }

    while( *p < end )
    {
        if( ( ret = asn1_get_tag( p, end, &len,
                ASN1_CONSTRUCTED | ASN1_SEQUENCE ) ) != 0 )
            return( POLARSSL_ERR_X509_CERT_INVALID_EXTENSIONS + ret );

        *p += len;
    }

    if( *p != end )
        return( POLARSSL_ERR_X509_CERT_INVALID_EXTENSIONS +
                POLARSSL_ERR_ASN1_LENGTH_MISMATCH );

    return( 0 );
}

/*
 * X.509 CRL v2 entry extensions (no extensions parsed yet.)
 */
static int x509_get_crl_entry_ext( unsigned char **p,
                             const unsigned char *end,
                             x509_buf *ext )
{
    int ret;
    size_t len = 0;

    /* OPTIONAL */
    if (end <= *p)
        return( 0 );

    ext->tag = **p;
    ext->p = *p;

    /*
     * Get CRL-entry extension sequence header
     * crlEntryExtensions      Extensions OPTIONAL  -- if present, MUST be v2
     */
    if( ( ret = asn1_get_tag( p, end, &ext->len,
            ASN1_CONSTRUCTED | ASN1_SEQUENCE ) ) != 0 )
    {
        if( ret == POLARSSL_ERR_ASN1_UNEXPECTED_TAG )
        {
            ext->p = NULL;
            return( 0 );
        }
        return( POLARSSL_ERR_X509_CERT_INVALID_EXTENSIONS + ret );
    }

	end = *p + ext->len;

    if( end != *p + ext->len )
        return( POLARSSL_ERR_X509_CERT_INVALID_EXTENSIONS +
                POLARSSL_ERR_ASN1_LENGTH_MISMATCH );

    while( *p < end )
    {
        if( ( ret = asn1_get_tag( p, end, &len,
                ASN1_CONSTRUCTED | ASN1_SEQUENCE ) ) != 0 )
            return( POLARSSL_ERR_X509_CERT_INVALID_EXTENSIONS + ret );

        *p += len;
    }

    if( *p != end )
        return( POLARSSL_ERR_X509_CERT_INVALID_EXTENSIONS +
                POLARSSL_ERR_ASN1_LENGTH_MISMATCH );

    return( 0 );
}

static int x509_get_basic_constraints( unsigned char **p,
                                       const unsigned char *end,
                                       int *ca_istrue,
                                       int *max_pathlen )
{
    int ret;
    size_t len;

    /*
     * BasicConstraints ::= SEQUENCE {
     *      cA                      BOOLEAN DEFAULT FALSE,
     *      pathLenConstraint       INTEGER (0..MAX) OPTIONAL }
     */
    *ca_istrue = 0; /* DEFAULT FALSE */
    *max_pathlen = 0; /* endless */

    if( ( ret = asn1_get_tag( p, end, &len,
            ASN1_CONSTRUCTED | ASN1_SEQUENCE ) ) != 0 )
        return( POLARSSL_ERR_X509_CERT_INVALID_EXTENSIONS + ret );

    if( *p == end )
        return 0;

    if( ( ret = asn1_get_bool( p, end, ca_istrue ) ) != 0 )
    {
        if( ret == POLARSSL_ERR_ASN1_UNEXPECTED_TAG )
            ret = asn1_get_int( p, end, ca_istrue );

        if( ret != 0 )
            return( POLARSSL_ERR_X509_CERT_INVALID_EXTENSIONS + ret );

        if( *ca_istrue != 0 )
            *ca_istrue = 1;
    }

    if( *p == end )
        return 0;

    if( ( ret = asn1_get_int( p, end, max_pathlen ) ) != 0 )
        return( POLARSSL_ERR_X509_CERT_INVALID_EXTENSIONS + ret );

    if( *p != end )
        return( POLARSSL_ERR_X509_CERT_INVALID_EXTENSIONS +
                POLARSSL_ERR_ASN1_LENGTH_MISMATCH );

    (*max_pathlen)++;

    return 0;
}

static int x509_get_ns_cert_type( unsigned char **p,
                                       const unsigned char *end,
                                       unsigned char *ns_cert_type)
{
    int ret;
    x509_bitstring bs = { 0, 0, NULL };

    if( ( ret = asn1_get_bitstring( p, end, &bs ) ) != 0 )
        return( POLARSSL_ERR_X509_CERT_INVALID_EXTENSIONS + ret );

    if( bs.len != 1 )
        return( POLARSSL_ERR_X509_CERT_INVALID_EXTENSIONS +
                POLARSSL_ERR_ASN1_INVALID_LENGTH );

    /* Get actual bitstring */
    *ns_cert_type = *bs.p;
    return 0;
}

static int x509_get_key_usage( unsigned char **p,
                               const unsigned char *end,
                               unsigned char *key_usage)
{
    int ret;
    x509_bitstring bs = { 0, 0, NULL };

    if( ( ret = asn1_get_bitstring( p, end, &bs ) ) != 0 )
        return( POLARSSL_ERR_X509_CERT_INVALID_EXTENSIONS + ret );

    if( bs.len < 1 )
        return( POLARSSL_ERR_X509_CERT_INVALID_EXTENSIONS +
                POLARSSL_ERR_ASN1_INVALID_LENGTH );

    /* Get actual bitstring */
    *key_usage = *bs.p;
    return 0;
}

/*
 * ExtKeyUsageSyntax ::= SEQUENCE SIZE (1..MAX) OF KeyPurposeId
 *
 * KeyPurposeId ::= OBJECT IDENTIFIER
 */
static int x509_get_ext_key_usage( unsigned char **p,
                               const unsigned char *end,
                               x509_sequence *ext_key_usage)
{
    int ret;

    if( ( ret = asn1_get_sequence_of( p, end, ext_key_usage, ASN1_OID ) ) != 0 )
        return( POLARSSL_ERR_X509_CERT_INVALID_EXTENSIONS + ret );

    /* Sequence length must be >= 1 */
    if( ext_key_usage->buf.p == NULL )
        return( POLARSSL_ERR_X509_CERT_INVALID_EXTENSIONS +
                POLARSSL_ERR_ASN1_INVALID_LENGTH );

    return 0;
}

/*
 * SubjectAltName ::= GeneralNames
 *
 * GeneralNames ::= SEQUENCE SIZE (1..MAX) OF GeneralName
 *
 * GeneralName ::= CHOICE {
 *      otherName                       [0]     OtherName,
 *      rfc822Name                      [1]     IA5String,
 *      dNSName                         [2]     IA5String,
 *      x400Address                     [3]     ORAddress,
 *      directoryName                   [4]     Name,
 *      ediPartyName                    [5]     EDIPartyName,
 *      uniformResourceIdentifier       [6]     IA5String,
 *      iPAddress                       [7]     OCTET STRING,
 *      registeredID                    [8]     OBJECT IDENTIFIER }
 *
 * OtherName ::= SEQUENCE {
 *      type-id    OBJECT IDENTIFIER,
 *      value      [0] EXPLICIT ANY DEFINED BY type-id }
 *
 * EDIPartyName ::= SEQUENCE {
 *      nameAssigner            [0]     DirectoryString OPTIONAL,
 *      partyName               [1]     DirectoryString }
 *
 * NOTE: PolarSSL only parses and uses dNSName at this point.
 */
static int x509_get_subject_alt_name( unsigned char **p,
                                      const unsigned char *end,
                                      x509_sequence *subject_alt_name )
{
    int ret;
    size_t len, tag_len;
    asn1_buf *buf;
    unsigned char tag;
    asn1_sequence *cur = subject_alt_name;

    /* Get main sequence tag */
    if( ( ret = asn1_get_tag( p, end, &len,
            ASN1_CONSTRUCTED | ASN1_SEQUENCE ) ) != 0 )
        return( POLARSSL_ERR_X509_CERT_INVALID_EXTENSIONS + ret );

    if( *p + len != end )
        return( POLARSSL_ERR_X509_CERT_INVALID_EXTENSIONS +
                POLARSSL_ERR_ASN1_LENGTH_MISMATCH );

    while( *p < end )
    {
        if( ( end - *p ) < 1 )
            return( POLARSSL_ERR_X509_CERT_INVALID_EXTENSIONS +
                    POLARSSL_ERR_ASN1_OUT_OF_DATA );

        tag = **p;
        (*p)++;
        if( ( ret = asn1_get_len( p, end, &tag_len ) ) != 0 )
            return( POLARSSL_ERR_X509_CERT_INVALID_EXTENSIONS + ret );

        if( ( tag & ASN1_CONTEXT_SPECIFIC ) != ASN1_CONTEXT_SPECIFIC )
            return( POLARSSL_ERR_X509_CERT_INVALID_EXTENSIONS +
                    POLARSSL_ERR_ASN1_UNEXPECTED_TAG );

        if( tag != ( ASN1_CONTEXT_SPECIFIC | 2 ) )
        {
            *p += tag_len;
            continue;
        }

        buf = &(cur->buf);
        buf->tag = tag;
        buf->p = *p;
        buf->len = tag_len;
        *p += buf->len;

        /* Allocate and assign next pointer */
        if (*p < end)
        {
            cur->next = (asn1_sequence *) malloc(
                 sizeof( asn1_sequence ) );

            if( cur->next == NULL )
                return( POLARSSL_ERR_X509_CERT_INVALID_EXTENSIONS +
                        POLARSSL_ERR_ASN1_MALLOC_FAILED );

            memset( cur->next, 0, sizeof( asn1_sequence ) );
            cur = cur->next;
        }
    }

    /* Set final sequence entry's next pointer to NULL */
    cur->next = NULL;

    if( *p != end )
        return( POLARSSL_ERR_X509_CERT_INVALID_EXTENSIONS +
                POLARSSL_ERR_ASN1_LENGTH_MISMATCH );

    return( 0 );
}

/*
 * X.509 v3 extensions
 *
 * TODO: Perform all of the basic constraints tests required by the RFC
 * TODO: Set values for undetected extensions to a sane default?
 *
 */
static int x509_get_crt_ext( unsigned char **p,
                             const unsigned char *end,
                             x509_cert *crt )
{
    int ret;
    size_t len;
    unsigned char *end_ext_data, *end_ext_octet;

    if( ( ret = x509_get_ext( p, end, &crt->v3_ext, 3 ) ) != 0 )
    {
        if( ret == POLARSSL_ERR_ASN1_UNEXPECTED_TAG )
            return( 0 );

        return( ret );
    }

    while( *p < end )
    {
        /*
         * Extension  ::=  SEQUENCE  {
         *      extnID      OBJECT IDENTIFIER,
         *      critical    BOOLEAN DEFAULT FALSE,
         *      extnValue   OCTET STRING  }
         */
        x509_buf extn_oid = {0, 0, NULL};
        int is_critical = 0; /* DEFAULT FALSE */

        if( ( ret = asn1_get_tag( p, end, &len,
                ASN1_CONSTRUCTED | ASN1_SEQUENCE ) ) != 0 )
            return( POLARSSL_ERR_X509_CERT_INVALID_EXTENSIONS + ret );

        end_ext_data = *p + len;

        /* Get extension ID */
        extn_oid.tag = **p;

        if( ( ret = asn1_get_tag( p, end, &extn_oid.len, ASN1_OID ) ) != 0 )
            return( POLARSSL_ERR_X509_CERT_INVALID_EXTENSIONS + ret );

        extn_oid.p = *p;
        *p += extn_oid.len;

        if( ( end - *p ) < 1 )
            return( POLARSSL_ERR_X509_CERT_INVALID_EXTENSIONS +
                    POLARSSL_ERR_ASN1_OUT_OF_DATA );

        /* Get optional critical */
        if( ( ret = asn1_get_bool( p, end_ext_data, &is_critical ) ) != 0 &&
            ( ret != POLARSSL_ERR_ASN1_UNEXPECTED_TAG ) )
            return( POLARSSL_ERR_X509_CERT_INVALID_EXTENSIONS + ret );

        /* Data should be octet string type */
        if( ( ret = asn1_get_tag( p, end_ext_data, &len,
                ASN1_OCTET_STRING ) ) != 0 )
            return( POLARSSL_ERR_X509_CERT_INVALID_EXTENSIONS + ret );

        end_ext_octet = *p + len;

        if( end_ext_octet != end_ext_data )
            return( POLARSSL_ERR_X509_CERT_INVALID_EXTENSIONS +
                    POLARSSL_ERR_ASN1_LENGTH_MISMATCH );

        /*
         * Detect supported extensions
         */
        if( ( OID_SIZE( OID_BASIC_CONSTRAINTS ) == extn_oid.len ) &&
                memcmp( extn_oid.p, OID_BASIC_CONSTRAINTS, extn_oid.len ) == 0 )
        {
            /* Parse basic constraints */
            if( ( ret = x509_get_basic_constraints( p, end_ext_octet,
                    &crt->ca_istrue, &crt->max_pathlen ) ) != 0 )
                return ( ret );
            crt->ext_types |= EXT_BASIC_CONSTRAINTS;
        }
        else if( ( OID_SIZE( OID_NS_CERT_TYPE ) == extn_oid.len ) &&
                memcmp( extn_oid.p, OID_NS_CERT_TYPE, extn_oid.len ) == 0 )
        {
            /* Parse netscape certificate type */
            if( ( ret = x509_get_ns_cert_type( p, end_ext_octet,
                    &crt->ns_cert_type ) ) != 0 )
                return ( ret );
            crt->ext_types |= EXT_NS_CERT_TYPE;
        }
        else if( ( OID_SIZE( OID_KEY_USAGE ) == extn_oid.len ) &&
                memcmp( extn_oid.p, OID_KEY_USAGE, extn_oid.len ) == 0 )
        {
            /* Parse key usage */
            if( ( ret = x509_get_key_usage( p, end_ext_octet,
                    &crt->key_usage ) ) != 0 )
                return ( ret );
            crt->ext_types |= EXT_KEY_USAGE;
        }
        else if( ( OID_SIZE( OID_EXTENDED_KEY_USAGE ) == extn_oid.len ) &&
                memcmp( extn_oid.p, OID_EXTENDED_KEY_USAGE, extn_oid.len ) == 0 )
        {
            /* Parse extended key usage */
            if( ( ret = x509_get_ext_key_usage( p, end_ext_octet,
                    &crt->ext_key_usage ) ) != 0 )
                return ( ret );
            crt->ext_types |= EXT_EXTENDED_KEY_USAGE;
        }
        else if( ( OID_SIZE( OID_SUBJECT_ALT_NAME ) == extn_oid.len ) &&
                memcmp( extn_oid.p, OID_SUBJECT_ALT_NAME, extn_oid.len ) == 0 )
        {
            /* Parse extended key usage */
            if( ( ret = x509_get_subject_alt_name( p, end_ext_octet,
                    &crt->subject_alt_names ) ) != 0 )
                return ( ret );
            crt->ext_types |= EXT_SUBJECT_ALT_NAME;
        }
        else
        {
            /* No parser found, skip extension */
            *p = end_ext_octet;

#if !defined(POLARSSL_X509_ALLOW_UNSUPPORTED_CRITICAL_EXTENSION)
            if( is_critical )
            {
                /* Data is marked as critical: fail */
                return ( POLARSSL_ERR_X509_CERT_INVALID_EXTENSIONS +
                        POLARSSL_ERR_ASN1_UNEXPECTED_TAG );
            }
#endif
        }
    }

    if( *p != end )
        return( POLARSSL_ERR_X509_CERT_INVALID_EXTENSIONS +
                POLARSSL_ERR_ASN1_LENGTH_MISMATCH );

    return( 0 );
}

/*
 * X.509 CRL Entries
 */
static int x509_get_entries( unsigned char **p,
                             const unsigned char *end,
                             x509_crl_entry *entry )
{
    int ret;
    size_t entry_len;
    x509_crl_entry *cur_entry = entry;

    if( *p == end )
        return( 0 );

    if( ( ret = asn1_get_tag( p, end, &entry_len,
            ASN1_SEQUENCE | ASN1_CONSTRUCTED ) ) != 0 )
    {
        if( ret == POLARSSL_ERR_ASN1_UNEXPECTED_TAG )
            return( 0 );

        return( ret );
    }

    end = *p + entry_len;

    while( *p < end )
    {
        size_t len2;
        const unsigned char *end2;

        if( ( ret = asn1_get_tag( p, end, &len2,
                ASN1_SEQUENCE | ASN1_CONSTRUCTED ) ) != 0 )
        {
            return( ret );
        }

        cur_entry->raw.tag = **p;
        cur_entry->raw.p = *p;
        cur_entry->raw.len = len2;
        end2 = *p + len2;

        if( ( ret = x509_get_serial( p, end2, &cur_entry->serial ) ) != 0 )
            return( ret );

        if( ( ret = x509_get_time( p, end2, &cur_entry->revocation_date ) ) != 0 )
            return( ret );

        if( ( ret = x509_get_crl_entry_ext( p, end2, &cur_entry->entry_ext ) ) != 0 )
            return( ret );

        if ( *p < end )
        {
            cur_entry->next = malloc( sizeof( x509_crl_entry ) );

            if( cur_entry->next == NULL )
                return( POLARSSL_ERR_X509_MALLOC_FAILED );

            cur_entry = cur_entry->next;
            memset( cur_entry, 0, sizeof( x509_crl_entry ) );
        }
    }

    return( 0 );
}

static int x509_get_sig_alg( const x509_buf *sig_oid, int *sig_alg )
{
    if( sig_oid->len == 9 &&
        memcmp( sig_oid->p, OID_PKCS1, 8 ) == 0 )
    {
        if( sig_oid->p[8] >= 2 && sig_oid->p[8] <= 5 )
        {
            *sig_alg = sig_oid->p[8];
            return( 0 );
        }

        if ( sig_oid->p[8] >= 11 && sig_oid->p[8] <= 14 )
        {
            *sig_alg = sig_oid->p[8];
            return( 0 );
        }

        return( POLARSSL_ERR_X509_CERT_UNKNOWN_SIG_ALG );
    }
    if( sig_oid->len == 5 &&
        memcmp( sig_oid->p, OID_RSA_SHA_OBS, 5 ) == 0 )
    {
        *sig_alg = SIG_RSA_SHA1;
        return( 0 );
    }

    return( POLARSSL_ERR_X509_CERT_UNKNOWN_SIG_ALG );
}

/*
 * Parse and fill a single X.509 certificate in DER format
 */
int x509parse_crt_der_core( x509_cert *crt, const unsigned char *buf,
                            size_t buflen )
{
    int ret;
    size_t len;
    unsigned char *p, *end, *crt_end;

    /*
     * Check for valid input
     */
    if( crt == NULL || buf == NULL )
        return( POLARSSL_ERR_X509_INVALID_INPUT );

    p = (unsigned char *) malloc( len = buflen );

    if( p == NULL )
        return( POLARSSL_ERR_X509_MALLOC_FAILED );

    memcpy( p, buf, buflen );

    buflen = 0;

    crt->raw.p = p;
    crt->raw.len = len;
    end = p + len;

    /*
     * Certificate  ::=  SEQUENCE  {
     *      tbsCertificate       TBSCertificate,
     *      signatureAlgorithm   AlgorithmIdentifier,
     *      signatureValue       BIT STRING  }
     */
    if( ( ret = asn1_get_tag( &p, end, &len,
            ASN1_CONSTRUCTED | ASN1_SEQUENCE ) ) != 0 )
    {
        x509_free( crt );
        return( POLARSSL_ERR_X509_CERT_INVALID_FORMAT );
    }

    if( len > (size_t) ( end - p ) )
    {
        x509_free( crt );
        return( POLARSSL_ERR_X509_CERT_INVALID_FORMAT +
                POLARSSL_ERR_ASN1_LENGTH_MISMATCH );
    }
    crt_end = p + len;

    /*
     * TBSCertificate  ::=  SEQUENCE  {
     */
    crt->tbs.p = p;

    if( ( ret = asn1_get_tag( &p, end, &len,
            ASN1_CONSTRUCTED | ASN1_SEQUENCE ) ) != 0 )
    {
        x509_free( crt );
        return( POLARSSL_ERR_X509_CERT_INVALID_FORMAT + ret );
    }

    end = p + len;
    crt->tbs.len = end - crt->tbs.p;

    /*
     * Version  ::=  INTEGER  {  v1(0), v2(1), v3(2)  }
     *
     * CertificateSerialNumber  ::=  INTEGER
     *
     * signature            AlgorithmIdentifier
     */
    if( ( ret = x509_get_version( &p, end, &crt->version ) ) != 0 ||
        ( ret = x509_get_serial(  &p, end, &crt->serial  ) ) != 0 ||
        ( ret = x509_get_alg(  &p, end, &crt->sig_oid1   ) ) != 0 )
    {
        x509_free( crt );
        return( ret );
    }

    crt->version++;

    if( crt->version > 3 )
    {
        x509_free( crt );
        return( POLARSSL_ERR_X509_CERT_UNKNOWN_VERSION );
    }

    if( ( ret = x509_get_sig_alg( &crt->sig_oid1, &crt->sig_alg ) ) != 0 )
    {
        x509_free( crt );
        return( ret );
    }

    /*
     * issuer               Name
     */
    crt->issuer_raw.p = p;

    if( ( ret = asn1_get_tag( &p, end, &len,
            ASN1_CONSTRUCTED | ASN1_SEQUENCE ) ) != 0 )
    {
        x509_free( crt );
        return( POLARSSL_ERR_X509_CERT_INVALID_FORMAT + ret );
    }

    if( ( ret = x509_get_name( &p, p + len, &crt->issuer ) ) != 0 )
    {
        x509_free( crt );
        return( ret );
    }

    crt->issuer_raw.len = p - crt->issuer_raw.p;

    /*
     * Validity ::= SEQUENCE {
     *      notBefore      Time,
     *      notAfter       Time }
     *
     */
    if( ( ret = x509_get_dates( &p, end, &crt->valid_from,
                                         &crt->valid_to ) ) != 0 )
    {
        x509_free( crt );
        return( ret );
    }

    /*
     * subject              Name
     */
    crt->subject_raw.p = p;

    if( ( ret = asn1_get_tag( &p, end, &len,
            ASN1_CONSTRUCTED | ASN1_SEQUENCE ) ) != 0 )
    {
        x509_free( crt );
        return( POLARSSL_ERR_X509_CERT_INVALID_FORMAT + ret );
    }

    if( len && ( ret = x509_get_name( &p, p + len, &crt->subject ) ) != 0 )
    {
        x509_free( crt );
        return( ret );
    }

    crt->subject_raw.len = p - crt->subject_raw.p;

    /*
     * SubjectPublicKeyInfo  ::=  SEQUENCE
     *      algorithm            AlgorithmIdentifier,
     *      subjectPublicKey     BIT STRING  }
     */
    if( ( ret = asn1_get_tag( &p, end, &len,
            ASN1_CONSTRUCTED | ASN1_SEQUENCE ) ) != 0 )
    {
        x509_free( crt );
        return( POLARSSL_ERR_X509_CERT_INVALID_FORMAT + ret );
    }

    if( ( ret = x509_get_pubkey( &p, p + len, &crt->pk_oid,
                                 &crt->rsa.N, &crt->rsa.E ) ) != 0 )
    {
        x509_free( crt );
        return( ret );
    }

    if( ( ret = rsa_check_pubkey( &crt->rsa ) ) != 0 )
    {
        x509_free( crt );
        return( ret );
    }

    crt->rsa.len = mpi_size( &crt->rsa.N );

    /*
     *  issuerUniqueID  [1]  IMPLICIT UniqueIdentifier OPTIONAL,
     *                       -- If present, version shall be v2 or v3
     *  subjectUniqueID [2]  IMPLICIT UniqueIdentifier OPTIONAL,
     *                       -- If present, version shall be v2 or v3
     *  extensions      [3]  EXPLICIT Extensions OPTIONAL
     *                       -- If present, version shall be v3
     */
    if( crt->version == 2 || crt->version == 3 )
    {
        ret = x509_get_uid( &p, end, &crt->issuer_id,  1 );
        if( ret != 0 )
        {
            x509_free( crt );
            return( ret );
        }
    }

    if( crt->version == 2 || crt->version == 3 )
    {
        ret = x509_get_uid( &p, end, &crt->subject_id,  2 );
        if( ret != 0 )
        {
            x509_free( crt );
            return( ret );
        }
    }

    if( crt->version == 3 )
    {
        ret = x509_get_crt_ext( &p, end, crt);
        if( ret != 0 )
        {
            x509_free( crt );
            return( ret );
        }
    }

    if( p != end )
    {
        x509_free( crt );
        return( POLARSSL_ERR_X509_CERT_INVALID_FORMAT +
                POLARSSL_ERR_ASN1_LENGTH_MISMATCH );
    }

    end = crt_end;

    /*
     *  signatureAlgorithm   AlgorithmIdentifier,
     *  signatureValue       BIT STRING
     */
    if( ( ret = x509_get_alg( &p, end, &crt->sig_oid2 ) ) != 0 )
    {
        x509_free( crt );
        return( ret );
    }

    if( crt->sig_oid1.len != crt->sig_oid2.len ||
        memcmp( crt->sig_oid1.p, crt->sig_oid2.p, crt->sig_oid1.len ) != 0 )
    {
        x509_free( crt );
        return( POLARSSL_ERR_X509_CERT_SIG_MISMATCH );
    }

    if( ( ret = x509_get_sig( &p, end, &crt->sig ) ) != 0 )
    {
        x509_free( crt );
        return( ret );
    }

    if( p != end )
    {
        x509_free( crt );
        return( POLARSSL_ERR_X509_CERT_INVALID_FORMAT +
                POLARSSL_ERR_ASN1_LENGTH_MISMATCH );
    }

    return( 0 );
}

/*
 * Parse one X.509 certificate in DER format from a buffer and add them to a
 * chained list
 */
int x509parse_crt_der( x509_cert *chain, const unsigned char *buf, size_t buflen )
{
    int ret;
    x509_cert *crt = chain, *prev = NULL;

    /*
     * Check for valid input
     */
    if( crt == NULL || buf == NULL )
        return( POLARSSL_ERR_X509_INVALID_INPUT );

    while( crt->version != 0 && crt->next != NULL )
    {
        prev = crt;
        crt = crt->next;
    }

    /*
     * Add new certificate on the end of the chain if needed.
     */
    if ( crt->version != 0 && crt->next == NULL)
    {
        crt->next = (x509_cert *) malloc( sizeof( x509_cert ) );

        if( crt->next == NULL )
            return( POLARSSL_ERR_X509_MALLOC_FAILED );

        prev = crt;
        crt = crt->next;
        memset( crt, 0, sizeof( x509_cert ) );
    }

    if( ( ret = x509parse_crt_der_core( crt, buf, buflen ) ) != 0 )
    {
        if( prev )
            prev->next = NULL;

        if( crt != chain )
            free( crt );

        return( ret );
    }

    return( 0 );
}

/*
 * Parse one or more PEM certificates from a buffer and add them to the chained list
 */
int x509parse_crt( x509_cert *chain, const unsigned char *buf, size_t buflen )
{
    int ret, success = 0, first_error = 0, total_failed = 0;
    int buf_format = X509_FORMAT_DER;

    /*
     * Check for valid input
     */
    if( chain == NULL || buf == NULL )
        return( POLARSSL_ERR_X509_INVALID_INPUT );

    /*
     * Determine buffer content. Buffer contains either one DER certificate or
     * one or more PEM certificates.
     */
#if defined(POLARSSL_PEM_C)
    if( strstr( (const char *) buf, "-----BEGIN CERTIFICATE-----" ) != NULL )
        buf_format = X509_FORMAT_PEM;
#endif

    if( buf_format == X509_FORMAT_DER )
        return x509parse_crt_der( chain, buf, buflen );

#if defined(POLARSSL_PEM_C)
    if( buf_format == X509_FORMAT_PEM )
    {
        pem_context pem;

        while( buflen > 0 )
        {
            size_t use_len;
            pem_init( &pem );

            ret = pem_read_buffer( &pem,
                           "-----BEGIN CERTIFICATE-----",
                           "-----END CERTIFICATE-----",
                           buf, NULL, 0, &use_len );

            if( ret == 0 )
            {
                /*
                 * Was PEM encoded
                 */
                buflen -= use_len;
                buf += use_len;
            }
            else if( ret == POLARSSL_ERR_PEM_BAD_INPUT_DATA )
            {
                return( ret );
            }
            else if( ret != POLARSSL_ERR_PEM_NO_HEADER_FOOTER_PRESENT )
            {
                pem_free( &pem );

                /*
                 * PEM header and footer were found
                 */
                buflen -= use_len;
                buf += use_len;

                if( first_error == 0 )
                    first_error = ret;

                continue;
            }
            else
                break;

            ret = x509parse_crt_der( chain, pem.buf, pem.buflen );

            pem_free( &pem );

            if( ret != 0 )
            {
                /*
                 * Quit parsing on a memory error
                 */
                if( ret == POLARSSL_ERR_X509_MALLOC_FAILED )
                    return( ret );

                if( first_error == 0 )
                    first_error = ret;

                total_failed++;
                continue;
            }

            success = 1;
        }
    }
#endif

    if( success )
        return( total_failed );
    else if( first_error )
        return( first_error );
    else
        return( POLARSSL_ERR_X509_CERT_UNKNOWN_FORMAT );
}

/*
 * Parse one or more CRLs and add them to the chained list
 */
int x509parse_crl( x509_crl *chain, const unsigned char *buf, size_t buflen )
{
    int ret;
    size_t len;
    unsigned char *p, *end;
    x509_crl *crl;
#if defined(POLARSSL_PEM_C)
    size_t use_len;
    pem_context pem;
#endif

    crl = chain;

    /*
     * Check for valid input
     */
    if( crl == NULL || buf == NULL )
        return( POLARSSL_ERR_X509_INVALID_INPUT );

    while( crl->version != 0 && crl->next != NULL )
        crl = crl->next;

    /*
     * Add new CRL on the end of the chain if needed.
     */
    if ( crl->version != 0 && crl->next == NULL)
    {
        crl->next = (x509_crl *) malloc( sizeof( x509_crl ) );

        if( crl->next == NULL )
        {
            x509_crl_free( crl );
            return( POLARSSL_ERR_X509_MALLOC_FAILED );
        }

        crl = crl->next;
        memset( crl, 0, sizeof( x509_crl ) );
    }

#if defined(POLARSSL_PEM_C)
    pem_init( &pem );
    ret = pem_read_buffer( &pem,
                           "-----BEGIN X509 CRL-----",
                           "-----END X509 CRL-----",
                           buf, NULL, 0, &use_len );

    if( ret == 0 )
    {
        /*
         * Was PEM encoded
         */
        buflen -= use_len;
        buf += use_len;

        /*
         * Steal PEM buffer
         */
        p = pem.buf;
        pem.buf = NULL;
        len = pem.buflen;
        pem_free( &pem );
    }
    else if( ret != POLARSSL_ERR_PEM_NO_HEADER_FOOTER_PRESENT )
    {
        pem_free( &pem );
        return( ret );
    }
    else
    {
        /*
         * nope, copy the raw DER data
         */
        p = (unsigned char *) malloc( len = buflen );

        if( p == NULL )
            return( POLARSSL_ERR_X509_MALLOC_FAILED );

        memcpy( p, buf, buflen );

        buflen = 0;
    }
#else
    p = (unsigned char *) malloc( len = buflen );

    if( p == NULL )
        return( POLARSSL_ERR_X509_MALLOC_FAILED );

    memcpy( p, buf, buflen );

    buflen = 0;
#endif

    crl->raw.p = p;
    crl->raw.len = len;
    end = p + len;

    /*
     * CertificateList  ::=  SEQUENCE  {
     *      tbsCertList          TBSCertList,
     *      signatureAlgorithm   AlgorithmIdentifier,
     *      signatureValue       BIT STRING  }
     */
    if( ( ret = asn1_get_tag( &p, end, &len,
            ASN1_CONSTRUCTED | ASN1_SEQUENCE ) ) != 0 )
    {
        x509_crl_free( crl );
        return( POLARSSL_ERR_X509_CERT_INVALID_FORMAT );
    }

    if( len != (size_t) ( end - p ) )
    {
        x509_crl_free( crl );
        return( POLARSSL_ERR_X509_CERT_INVALID_FORMAT +
                POLARSSL_ERR_ASN1_LENGTH_MISMATCH );
    }

    /*
     * TBSCertList  ::=  SEQUENCE  {
     */
    crl->tbs.p = p;

    if( ( ret = asn1_get_tag( &p, end, &len,
            ASN1_CONSTRUCTED | ASN1_SEQUENCE ) ) != 0 )
    {
        x509_crl_free( crl );
        return( POLARSSL_ERR_X509_CERT_INVALID_FORMAT + ret );
    }

    end = p + len;
    crl->tbs.len = end - crl->tbs.p;

    /*
     * Version  ::=  INTEGER  OPTIONAL {  v1(0), v2(1)  }
     *               -- if present, MUST be v2
     *
     * signature            AlgorithmIdentifier
     */
    if( ( ret = x509_crl_get_version( &p, end, &crl->version ) ) != 0 ||
        ( ret = x509_get_alg(  &p, end, &crl->sig_oid1   ) ) != 0 )
    {
        x509_crl_free( crl );
        return( ret );
    }

    crl->version++;

    if( crl->version > 2 )
    {
        x509_crl_free( crl );
        return( POLARSSL_ERR_X509_CERT_UNKNOWN_VERSION );
    }

    if( ( ret = x509_get_sig_alg( &crl->sig_oid1, &crl->sig_alg ) ) != 0 )
    {
        x509_crl_free( crl );
        return( POLARSSL_ERR_X509_CERT_UNKNOWN_SIG_ALG );
    }

    /*
     * issuer               Name
     */
    crl->issuer_raw.p = p;

    if( ( ret = asn1_get_tag( &p, end, &len,
            ASN1_CONSTRUCTED | ASN1_SEQUENCE ) ) != 0 )
    {
        x509_crl_free( crl );
        return( POLARSSL_ERR_X509_CERT_INVALID_FORMAT + ret );
    }

    if( ( ret = x509_get_name( &p, p + len, &crl->issuer ) ) != 0 )
    {
        x509_crl_free( crl );
        return( ret );
    }

    crl->issuer_raw.len = p - crl->issuer_raw.p;

    /*
     * thisUpdate          Time
     * nextUpdate          Time OPTIONAL
     */
    if( ( ret = x509_get_time( &p, end, &crl->this_update ) ) != 0 )
    {
        x509_crl_free( crl );
        return( ret );
    }

    if( ( ret = x509_get_time( &p, end, &crl->next_update ) ) != 0 )
    {
        if ( ret != ( POLARSSL_ERR_X509_CERT_INVALID_DATE +
                        POLARSSL_ERR_ASN1_UNEXPECTED_TAG ) &&
             ret != ( POLARSSL_ERR_X509_CERT_INVALID_DATE +
                        POLARSSL_ERR_ASN1_OUT_OF_DATA ) )
        {
            x509_crl_free( crl );
            return( ret );
        }
    }

    /*
     * revokedCertificates    SEQUENCE OF SEQUENCE   {
     *      userCertificate        CertificateSerialNumber,
     *      revocationDate         Time,
     *      crlEntryExtensions     Extensions OPTIONAL
     *                                   -- if present, MUST be v2
     *                        } OPTIONAL
     */
    if( ( ret = x509_get_entries( &p, end, &crl->entry ) ) != 0 )
    {
        x509_crl_free( crl );
        return( ret );
    }

    /*
     * crlExtensions          EXPLICIT Extensions OPTIONAL
     *                              -- if present, MUST be v2
     */
    if( crl->version == 2 )
    {
        ret = x509_get_crl_ext( &p, end, &crl->crl_ext );
                            
        if( ret != 0 )
        {
            x509_crl_free( crl );
            return( ret );
        }
    }

    if( p != end )
    {
        x509_crl_free( crl );
        return( POLARSSL_ERR_X509_CERT_INVALID_FORMAT +
                POLARSSL_ERR_ASN1_LENGTH_MISMATCH );
    }

    end = crl->raw.p + crl->raw.len;

    /*
     *  signatureAlgorithm   AlgorithmIdentifier,
     *  signatureValue       BIT STRING
     */
    if( ( ret = x509_get_alg( &p, end, &crl->sig_oid2 ) ) != 0 )
    {
        x509_crl_free( crl );
        return( ret );
    }

    if( crl->sig_oid1.len != crl->sig_oid2.len ||
        memcmp( crl->sig_oid1.p, crl->sig_oid2.p, crl->sig_oid1.len ) != 0 )
    {
        x509_crl_free( crl );
        return( POLARSSL_ERR_X509_CERT_SIG_MISMATCH );
    }

    if( ( ret = x509_get_sig( &p, end, &crl->sig ) ) != 0 )
    {
        x509_crl_free( crl );
        return( ret );
    }

    if( p != end )
    {
        x509_crl_free( crl );
        return( POLARSSL_ERR_X509_CERT_INVALID_FORMAT +
                POLARSSL_ERR_ASN1_LENGTH_MISMATCH );
    }

    if( buflen > 0 )
    {
        crl->next = (x509_crl *) malloc( sizeof( x509_crl ) );

        if( crl->next == NULL )
        {
            x509_crl_free( crl );
            return( POLARSSL_ERR_X509_MALLOC_FAILED );
        }

        crl = crl->next;
        memset( crl, 0, sizeof( x509_crl ) );

        return( x509parse_crl( crl, buf, buflen ) );
    }

    return( 0 );
}

#if defined(POLARSSL_FS_IO)
/*
 * Load all data from a file into a given buffer.
 */
int load_file( const char *path, unsigned char **buf, size_t *n )
{
    FILE *f;

    if( ( f = fopen( path, "rb" ) ) == NULL )
        return( POLARSSL_ERR_X509_FILE_IO_ERROR );

    fseek( f, 0, SEEK_END );
    *n = (size_t) ftell( f );
    fseek( f, 0, SEEK_SET );

    if( ( *buf = (unsigned char *) malloc( *n + 1 ) ) == NULL )
        return( POLARSSL_ERR_X509_MALLOC_FAILED );

    if( fread( *buf, 1, *n, f ) != *n )
    {
        fclose( f );
        free( *buf );
        return( POLARSSL_ERR_X509_FILE_IO_ERROR );
    }

    fclose( f );

    (*buf)[*n] = '\0';

    return( 0 );
}

/*
 * Load one or more certificates and add them to the chained list
 */
int x509parse_crtfile( x509_cert *chain, const char *path )
{
    int ret;
    size_t n;
    unsigned char *buf;

    if ( (ret = load_file( path, &buf, &n ) ) != 0 )
        return( ret );

    ret = x509parse_crt( chain, buf, n );

    memset( buf, 0, n + 1 );
    free( buf );

    return( ret );
}

int x509parse_crtpath( x509_cert *chain, const char *path )
{
    int ret = 0;
#if defined(_WIN32)
    int w_ret;
    WCHAR szDir[MAX_PATH];
    char filename[MAX_PATH];
	char *p;
    int len = strlen( path );

	WIN32_FIND_DATAW file_data;
    HANDLE hFind;

    if( len > MAX_PATH - 3 )
        return( POLARSSL_ERR_X509_INVALID_INPUT );

	memset( szDir, 0, sizeof(szDir) );
	memset( filename, 0, MAX_PATH );
	memcpy( filename, path, len );
	filename[len++] = '\\';
	p = filename + len;
    filename[len++] = '*';

	w_ret = MultiByteToWideChar( CP_ACP, 0, path, len, szDir, MAX_PATH - 3 );

    hFind = FindFirstFileW( szDir, &file_data );
    if (hFind == INVALID_HANDLE_VALUE) 
        return( POLARSSL_ERR_X509_FILE_IO_ERROR );

    len = MAX_PATH - len;
    do
    {
		memset( p, 0, len );

        if( file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
            continue;

		w_ret = WideCharToMultiByte( CP_ACP, 0, file_data.cFileName,
									 lstrlenW(file_data.cFileName),
									 p, len - 1,
									 NULL, NULL );

        w_ret = x509parse_crtfile( chain, filename );
        if( w_ret < 0 )
            ret++;
        else
            ret += w_ret;
    }
    while( FindNextFileW( hFind, &file_data ) != 0 );

    if (GetLastError() != ERROR_NO_MORE_FILES) 
        ret = POLARSSL_ERR_X509_FILE_IO_ERROR;

cleanup:
    FindClose( hFind );
#else
    int t_ret, i;
    struct stat sb;
    struct dirent entry, *result = NULL;
    char entry_name[255];
    DIR *dir = opendir( path );

    if( dir == NULL)
        return( POLARSSL_ERR_X509_FILE_IO_ERROR );

    while( ( t_ret = readdir_r( dir, &entry, &result ) ) == 0 )
    {
        if( result == NULL )
            break;

        snprintf( entry_name, sizeof(entry_name), "%s/%s", path, entry.d_name );

        i = stat( entry_name, &sb );

        if( i == -1 )
            return( POLARSSL_ERR_X509_FILE_IO_ERROR );

        if( !S_ISREG( sb.st_mode ) )
            continue;

        // Ignore parse errors
        //
        t_ret = x509parse_crtfile( chain, entry_name );
        if( t_ret < 0 )
            ret++;
        else
            ret += t_ret;
    }
    closedir( dir );
#endif

    return( ret );
}

/*
 * Load one or more CRLs and add them to the chained list
 */
int x509parse_crlfile( x509_crl *chain, const char *path )
{
    int ret;
    size_t n;
    unsigned char *buf;

    if ( (ret = load_file( path, &buf, &n ) ) != 0 )
        return( ret );

    ret = x509parse_crl( chain, buf, n );

    memset( buf, 0, n + 1 );
    free( buf );

    return( ret );
}

/*
 * Load and parse a private RSA key
 */
int x509parse_keyfile( rsa_context *rsa, const char *path, const char *pwd )
{
    int ret;
    size_t n;
    unsigned char *buf;

    if ( (ret = load_file( path, &buf, &n ) ) != 0 )
        return( ret );

    if( pwd == NULL )
        ret = x509parse_key( rsa, buf, n, NULL, 0 );
    else
        ret = x509parse_key( rsa, buf, n,
                (unsigned char *) pwd, strlen( pwd ) );

    memset( buf, 0, n + 1 );
    free( buf );

    return( ret );
}

/*
 * Load and parse a public RSA key
 */
int x509parse_public_keyfile( rsa_context *rsa, const char *path )
{
    int ret;
    size_t n;
    unsigned char *buf;

    if ( (ret = load_file( path, &buf, &n ) ) != 0 )
        return( ret );

    ret = x509parse_public_key( rsa, buf, n );

    memset( buf, 0, n + 1 );
    free( buf );

    return( ret );
}
#endif /* POLARSSL_FS_IO */

/*
 * Parse a PKCS#1 encoded private RSA key
 */
static int x509parse_key_pkcs1_der( rsa_context *rsa,
                                    const unsigned char *key,
                                    size_t keylen )
{
    int ret;
    size_t len;
    unsigned char *p, *end;

    p = (unsigned char *) key;
    end = p + keylen;

    /*
     * This function parses the RSAPrivateKey (PKCS#1)
     *
     *  RSAPrivateKey ::= SEQUENCE {
     *      version           Version,
     *      modulus           INTEGER,  -- n
     *      publicExponent    INTEGER,  -- e
     *      privateExponent   INTEGER,  -- d
     *      prime1            INTEGER,  -- p
     *      prime2            INTEGER,  -- q
     *      exponent1         INTEGER,  -- d mod (p-1)
     *      exponent2         INTEGER,  -- d mod (q-1)
     *      coefficient       INTEGER,  -- (inverse of q) mod p
     *      otherPrimeInfos   OtherPrimeInfos OPTIONAL
     *  }
     */
    if( ( ret = asn1_get_tag( &p, end, &len,
            ASN1_CONSTRUCTED | ASN1_SEQUENCE ) ) != 0 )
    {
        return( POLARSSL_ERR_X509_KEY_INVALID_FORMAT + ret );
    }

    end = p + len;

    if( ( ret = asn1_get_int( &p, end, &rsa->ver ) ) != 0 )
    {
        return( POLARSSL_ERR_X509_KEY_INVALID_FORMAT + ret );
    }

    if( rsa->ver != 0 )
    {
        return( POLARSSL_ERR_X509_KEY_INVALID_VERSION + ret );
    }

    if( ( ret = asn1_get_mpi( &p, end, &rsa->N  ) ) != 0 ||
        ( ret = asn1_get_mpi( &p, end, &rsa->E  ) ) != 0 ||
        ( ret = asn1_get_mpi( &p, end, &rsa->D  ) ) != 0 ||
        ( ret = asn1_get_mpi( &p, end, &rsa->P  ) ) != 0 ||
        ( ret = asn1_get_mpi( &p, end, &rsa->Q  ) ) != 0 ||
        ( ret = asn1_get_mpi( &p, end, &rsa->DP ) ) != 0 ||
        ( ret = asn1_get_mpi( &p, end, &rsa->DQ ) ) != 0 ||
        ( ret = asn1_get_mpi( &p, end, &rsa->QP ) ) != 0 )
    {
        rsa_free( rsa );
        return( POLARSSL_ERR_X509_KEY_INVALID_FORMAT + ret );
    }

    rsa->len = mpi_size( &rsa->N );

    if( p != end )
    {
        rsa_free( rsa );
        return( POLARSSL_ERR_X509_KEY_INVALID_FORMAT +
                POLARSSL_ERR_ASN1_LENGTH_MISMATCH );
    }

    if( ( ret = rsa_check_privkey( rsa ) ) != 0 )
    {
        rsa_free( rsa );
        return( ret );
    }

    return( 0 );
}

/*
 * Parse an unencrypted PKCS#8 encoded private RSA key
 */
static int x509parse_key_pkcs8_unencrypted_der(
                                    rsa_context *rsa,
                                    const unsigned char *key,
                                    size_t keylen )
{
    int ret;
    size_t len;
    unsigned char *p, *end;
    x509_buf pk_alg_oid;

    p = (unsigned char *) key;
    end = p + keylen;

    /*
     * This function parses the PrivatKeyInfo object (PKCS#8)
     *
     *  PrivateKeyInfo ::= SEQUENCE {
     *    version           Version,
     *    algorithm       AlgorithmIdentifier,
     *    PrivateKey      BIT STRING
     *  }
     *
     *  AlgorithmIdentifier ::= SEQUENCE {
     *    algorithm       OBJECT IDENTIFIER,
     *    parameters      ANY DEFINED BY algorithm OPTIONAL
     *  }
     *
     *  The PrivateKey BIT STRING is a PKCS#1 RSAPrivateKey
     */
    if( ( ret = asn1_get_tag( &p, end, &len,
            ASN1_CONSTRUCTED | ASN1_SEQUENCE ) ) != 0 )
    {
        return( POLARSSL_ERR_X509_KEY_INVALID_FORMAT + ret );
    }

    end = p + len;

    if( ( ret = asn1_get_int( &p, end, &rsa->ver ) ) != 0 )
    {
        return( POLARSSL_ERR_X509_KEY_INVALID_FORMAT + ret );
    }

    if( rsa->ver != 0 )
    {
        return( POLARSSL_ERR_X509_KEY_INVALID_VERSION + ret );
    }

    if( ( ret = x509_get_alg( &p, end, &pk_alg_oid ) ) != 0 )
    {
        return( POLARSSL_ERR_X509_KEY_INVALID_FORMAT + ret );
    }

    /*
     * only RSA keys handled at this time
     */
    if( pk_alg_oid.len != 9 ||
        memcmp( pk_alg_oid.p, OID_PKCS1_RSA, 9 ) != 0 )
    {
        return( POLARSSL_ERR_X509_UNKNOWN_PK_ALG );
    }

    /*
     * Get the OCTET STRING and parse the PKCS#1 format inside
     */
    if( ( ret = asn1_get_tag( &p, end, &len, ASN1_OCTET_STRING ) ) != 0 )
        return( POLARSSL_ERR_X509_KEY_INVALID_FORMAT + ret );

    if( ( end - p ) < 1 )
    {
        return( POLARSSL_ERR_X509_KEY_INVALID_FORMAT +
                POLARSSL_ERR_ASN1_OUT_OF_DATA );
    }

    end = p + len;

    if( ( ret = x509parse_key_pkcs1_der( rsa, p, end - p ) ) != 0 )
        return( ret );

    return( 0 );
}

/*
 * Parse an encrypted PKCS#8 encoded private RSA key
 */
static int x509parse_key_pkcs8_encrypted_der(
                                    rsa_context *rsa,
                                    const unsigned char *key,
                                    size_t keylen,
                                    const unsigned char *pwd,
                                    size_t pwdlen )
{
    int ret;
    size_t len;
    unsigned char *p, *end, *end2;
    x509_buf pbe_alg_oid, pbe_params;
    unsigned char buf[2048];

    memset(buf, 0, 2048);

    p = (unsigned char *) key;
    end = p + keylen;

    if( pwdlen == 0 )
        return( POLARSSL_ERR_X509_PASSWORD_REQUIRED );

    /*
     * This function parses the EncryptedPrivatKeyInfo object (PKCS#8)
     *
     *  EncryptedPrivateKeyInfo ::= SEQUENCE {
     *    encryptionAlgorithm  EncryptionAlgorithmIdentifier,
     *    encryptedData        EncryptedData
     *  }
     *
     *  EncryptionAlgorithmIdentifier ::= AlgorithmIdentifier
     *
     *  EncryptedData ::= OCTET STRING
     *
     *  The EncryptedData OCTET STRING is a PKCS#8 PrivateKeyInfo
     */
    if( ( ret = asn1_get_tag( &p, end, &len,
            ASN1_CONSTRUCTED | ASN1_SEQUENCE ) ) != 0 )
    {
        return( POLARSSL_ERR_X509_KEY_INVALID_FORMAT + ret );
    }

    end = p + len;

    if( ( ret = asn1_get_tag( &p, end, &len,
            ASN1_CONSTRUCTED | ASN1_SEQUENCE ) ) != 0 )
    {
        return( POLARSSL_ERR_X509_KEY_INVALID_FORMAT + ret );
    }

    end2 = p + len;

    if( ( ret = asn1_get_tag( &p, end, &pbe_alg_oid.len, ASN1_OID ) ) != 0 )
        return( POLARSSL_ERR_X509_KEY_INVALID_FORMAT + ret );

    pbe_alg_oid.p = p;
    p += pbe_alg_oid.len;

    /*
     * Store the algorithm parameters
     */
    pbe_params.p = p;
    pbe_params.len = end2 - p;
    p += pbe_params.len;

    if( ( ret = asn1_get_tag( &p, end, &len, ASN1_OCTET_STRING ) ) != 0 )
        return( POLARSSL_ERR_X509_KEY_INVALID_FORMAT + ret );

    // buf has been sized to 2048 bytes
    if( len > 2048 )
        return( POLARSSL_ERR_X509_INVALID_INPUT );

    /*
     * Decrypt EncryptedData with appropriate PDE
     */
#if defined(POLARSSL_PKCS12_C)
    if( OID_CMP( OID_PKCS12_PBE_SHA1_DES3_EDE_CBC, &pbe_alg_oid ) )
    {
        if( ( ret = pkcs12_pbe( &pbe_params, PKCS12_PBE_DECRYPT,
                                POLARSSL_CIPHER_DES_EDE3_CBC, POLARSSL_MD_SHA1,
                                pwd, pwdlen, p, len, buf ) ) != 0 )
        {
            if( ret == POLARSSL_ERR_PKCS12_PASSWORD_MISMATCH )
                return( POLARSSL_ERR_X509_PASSWORD_MISMATCH );

            return( ret );
        }
    }
    else if( OID_CMP( OID_PKCS12_PBE_SHA1_DES2_EDE_CBC, &pbe_alg_oid ) )
    {
        if( ( ret = pkcs12_pbe( &pbe_params, PKCS12_PBE_DECRYPT,
                                POLARSSL_CIPHER_DES_EDE_CBC, POLARSSL_MD_SHA1,
                                pwd, pwdlen, p, len, buf ) ) != 0 )
        {
            if( ret == POLARSSL_ERR_PKCS12_PASSWORD_MISMATCH )
                return( POLARSSL_ERR_X509_PASSWORD_MISMATCH );

            return( ret );
        }
    }
    else if( OID_CMP( OID_PKCS12_PBE_SHA1_RC4_128, &pbe_alg_oid ) )
    {
        if( ( ret = pkcs12_pbe_sha1_rc4_128( &pbe_params,
                                             PKCS12_PBE_DECRYPT,
                                             pwd, pwdlen,
                                             p, len, buf ) ) != 0 )
        {
            return( ret );
        }

        // Best guess for password mismatch when using RC4. If first tag is
        // not ASN1_CONSTRUCTED | ASN1_SEQUENCE
        //
        if( *buf != ( ASN1_CONSTRUCTED | ASN1_SEQUENCE ) )
            return( POLARSSL_ERR_X509_PASSWORD_MISMATCH );
    }
    else
#endif /* POLARSSL_PKCS12_C */
#if defined(POLARSSL_PKCS5_C)
    if( OID_CMP( OID_PKCS5_PBES2, &pbe_alg_oid ) )
    {
        if( ( ret = pkcs5_pbes2( &pbe_params, PKCS5_DECRYPT, pwd, pwdlen,
                                  p, len, buf ) ) != 0 )
        {
            if( ret == POLARSSL_ERR_PKCS5_PASSWORD_MISMATCH )
                return( POLARSSL_ERR_X509_PASSWORD_MISMATCH );

            return( ret );
        }
    }
    else
#endif /* POLARSSL_PKCS5_C */
        return( POLARSSL_ERR_X509_FEATURE_UNAVAILABLE );

    return x509parse_key_pkcs8_unencrypted_der( rsa, buf, len );
}

/*
 * Parse a private RSA key
 */
int x509parse_key( rsa_context *rsa, const unsigned char *key, size_t keylen,
                                     const unsigned char *pwd, size_t pwdlen )
{
    int ret;

#if defined(POLARSSL_PEM_C)
    size_t len;
    pem_context pem;

    pem_init( &pem );
    ret = pem_read_buffer( &pem,
                           "-----BEGIN RSA PRIVATE KEY-----",
                           "-----END RSA PRIVATE KEY-----",
                           key, pwd, pwdlen, &len );
    if( ret == 0 )
    {
        if( ( ret = x509parse_key_pkcs1_der( rsa, pem.buf, pem.buflen ) ) != 0 )
        {
            rsa_free( rsa );
        }

        pem_free( &pem );
        return( ret );
    }
    else if( ret == POLARSSL_ERR_PEM_PASSWORD_MISMATCH )
        return( POLARSSL_ERR_X509_PASSWORD_MISMATCH );
    else if( ret == POLARSSL_ERR_PEM_PASSWORD_REQUIRED )
        return( POLARSSL_ERR_X509_PASSWORD_REQUIRED );
    else if( ret != POLARSSL_ERR_PEM_NO_HEADER_FOOTER_PRESENT )
        return( ret );

    ret = pem_read_buffer( &pem,
                           "-----BEGIN PRIVATE KEY-----",
                           "-----END PRIVATE KEY-----",
                           key, NULL, 0, &len );
    if( ret == 0 )
    {
        if( ( ret = x509parse_key_pkcs8_unencrypted_der( rsa,
                                                pem.buf, pem.buflen ) ) != 0 )
        {
            rsa_free( rsa );
        }

        pem_free( &pem );
        return( ret );
    }
    else if( ret != POLARSSL_ERR_PEM_NO_HEADER_FOOTER_PRESENT )
        return( ret );

    ret = pem_read_buffer( &pem,
                           "-----BEGIN ENCRYPTED PRIVATE KEY-----",
                           "-----END ENCRYPTED PRIVATE KEY-----",
                           key, NULL, 0, &len );
    if( ret == 0 )
    {
        if( ( ret = x509parse_key_pkcs8_encrypted_der( rsa,
                                                pem.buf, pem.buflen,
                                                pwd, pwdlen ) ) != 0 )
        {
            rsa_free( rsa );
        }

        pem_free( &pem );
        return( ret );
    }
    else if( ret != POLARSSL_ERR_PEM_NO_HEADER_FOOTER_PRESENT )
        return( ret );
#else
    ((void) pwd);
    ((void) pwdlen);
#endif /* POLARSSL_PEM_C */

    // At this point we only know it's not a PEM formatted key. Could be any
    // of the known DER encoded private key formats
    //
    // We try the different DER format parsers to see if one passes without
    // error
    //
    if( ( ret = x509parse_key_pkcs8_encrypted_der( rsa, key, keylen,
                                                   pwd, pwdlen ) ) == 0 )
    {
        return( 0 );
    }

    rsa_free( rsa );

    if( ret == POLARSSL_ERR_X509_PASSWORD_MISMATCH )
    {
        return( ret );
    }

    if( ( ret = x509parse_key_pkcs8_unencrypted_der( rsa, key, keylen ) ) == 0 )
        return( 0 );

    rsa_free( rsa );

    if( ( ret = x509parse_key_pkcs1_der( rsa, key, keylen ) ) == 0 )
        return( 0 );

    rsa_free( rsa );

    return( POLARSSL_ERR_X509_KEY_INVALID_FORMAT );
}

/*
 * Parse a public RSA key
 */
int x509parse_public_key( rsa_context *rsa, const unsigned char *key, size_t keylen )
{
    int ret;
    size_t len;
    unsigned char *p, *end;
    x509_buf alg_oid;
#if defined(POLARSSL_PEM_C)
    pem_context pem;

    pem_init( &pem );
    ret = pem_read_buffer( &pem,
            "-----BEGIN PUBLIC KEY-----",
            "-----END PUBLIC KEY-----",
            key, NULL, 0, &len );

    if( ret == 0 )
    {
        /*
         * Was PEM encoded
         */
        keylen = pem.buflen;
    }
    else if( ret != POLARSSL_ERR_PEM_NO_HEADER_FOOTER_PRESENT )
    {
        pem_free( &pem );
        return( ret );
    }

    p = ( ret == 0 ) ? pem.buf : (unsigned char *) key;
#else
    p = (unsigned char *) key;
#endif
    end = p + keylen;

    /*
     *  PublicKeyInfo ::= SEQUENCE {
     *    algorithm       AlgorithmIdentifier,
     *    PublicKey       BIT STRING
     *  }
     *
     *  AlgorithmIdentifier ::= SEQUENCE {
     *    algorithm       OBJECT IDENTIFIER,
     *    parameters      ANY DEFINED BY algorithm OPTIONAL
     *  }
     *
     *  RSAPublicKey ::= SEQUENCE {
     *      modulus           INTEGER,  -- n
     *      publicExponent    INTEGER   -- e
     *  }
     */

    if( ( ret = asn1_get_tag( &p, end, &len,
                    ASN1_CONSTRUCTED | ASN1_SEQUENCE ) ) != 0 )
    {
#if defined(POLARSSL_PEM_C)
        pem_free( &pem );
#endif
        rsa_free( rsa );
        return( POLARSSL_ERR_X509_CERT_INVALID_FORMAT + ret );
    }

    if( ( ret = x509_get_pubkey( &p, end, &alg_oid, &rsa->N, &rsa->E ) ) != 0 )
    {
#if defined(POLARSSL_PEM_C)
        pem_free( &pem );
#endif
        rsa_free( rsa );
        return( POLARSSL_ERR_X509_KEY_INVALID_FORMAT + ret );
    }

    if( ( ret = rsa_check_pubkey( rsa ) ) != 0 )
    {
#if defined(POLARSSL_PEM_C)
        pem_free( &pem );
#endif
        rsa_free( rsa );
        return( ret );
    }

    rsa->len = mpi_size( &rsa->N );

#if defined(POLARSSL_PEM_C)
    pem_free( &pem );
#endif

    return( 0 );
}

#if defined(POLARSSL_DHM_C)
/*
 * Parse DHM parameters
 */
int x509parse_dhm( dhm_context *dhm, const unsigned char *dhmin, size_t dhminlen )
{
    int ret;
    size_t len;
    unsigned char *p, *end;
#if defined(POLARSSL_PEM_C)
    pem_context pem;

    pem_init( &pem );

    ret = pem_read_buffer( &pem, 
                           "-----BEGIN DH PARAMETERS-----",
                           "-----END DH PARAMETERS-----",
                           dhmin, NULL, 0, &dhminlen );

    if( ret == 0 )
    {
        /*
         * Was PEM encoded
         */
        dhminlen = pem.buflen;
    }
    else if( ret != POLARSSL_ERR_PEM_NO_HEADER_FOOTER_PRESENT )
    {
        pem_free( &pem );
        return( ret );
    }

    p = ( ret == 0 ) ? pem.buf : (unsigned char *) dhmin;
#else
    p = (unsigned char *) dhmin;
#endif
    end = p + dhminlen;

    memset( dhm, 0, sizeof( dhm_context ) );

    /*
     *  DHParams ::= SEQUENCE {
     *      prime            INTEGER,  -- P
     *      generator        INTEGER,  -- g
     *  }
     */
    if( ( ret = asn1_get_tag( &p, end, &len,
            ASN1_CONSTRUCTED | ASN1_SEQUENCE ) ) != 0 )
    {
#if defined(POLARSSL_PEM_C)
        pem_free( &pem );
#endif
        return( POLARSSL_ERR_X509_KEY_INVALID_FORMAT + ret );
    }

    end = p + len;

    if( ( ret = asn1_get_mpi( &p, end, &dhm->P  ) ) != 0 ||
        ( ret = asn1_get_mpi( &p, end, &dhm->G ) ) != 0 )
    {
#if defined(POLARSSL_PEM_C)
        pem_free( &pem );
#endif
        dhm_free( dhm );
        return( POLARSSL_ERR_X509_KEY_INVALID_FORMAT + ret );
    }

    if( p != end )
    {
#if defined(POLARSSL_PEM_C)
        pem_free( &pem );
#endif
        dhm_free( dhm );
        return( POLARSSL_ERR_X509_KEY_INVALID_FORMAT +
                POLARSSL_ERR_ASN1_LENGTH_MISMATCH );
    }

#if defined(POLARSSL_PEM_C)
    pem_free( &pem );
#endif

    return( 0 );
}

#if defined(POLARSSL_FS_IO)
/*
 * Load and parse a private RSA key
 */
int x509parse_dhmfile( dhm_context *dhm, const char *path )
{
    int ret;
    size_t n;
    unsigned char *buf;

    if ( ( ret = load_file( path, &buf, &n ) ) != 0 )
        return( ret );

    ret = x509parse_dhm( dhm, buf, n );

    memset( buf, 0, n + 1 );
    free( buf );

    return( ret );
}
#endif /* POLARSSL_FS_IO */
#endif /* POLARSSL_DHM_C */

#if defined _MSC_VER && !defined snprintf
#include <stdarg.h>

#if !defined vsnprintf
#define vsnprintf _vsnprintf
#endif // vsnprintf

/*
 * Windows _snprintf and _vsnprintf are not compatible to linux versions.
 * Result value is not size of buffer needed, but -1 if no fit is possible.
 *
 * This fuction tries to 'fix' this by at least suggesting enlarging the
 * size by 20.
 */
int compat_snprintf(char *str, size_t size, const char *format, ...)
{
    va_list ap;
    int res = -1;

    va_start( ap, format );

    res = vsnprintf( str, size, format, ap );

    va_end( ap );

    // No quick fix possible
    if ( res < 0 )
        return( (int) size + 20 );
    
    return res;
}

#define snprintf compat_snprintf
#endif

#define POLARSSL_ERR_DEBUG_BUF_TOO_SMALL    -2

#define SAFE_SNPRINTF()                         \
{                                               \
    if( ret == -1 )                             \
        return( -1 );                           \
                                                \
    if ( (unsigned int) ret > n ) {             \
        p[n - 1] = '\0';                        \
        return POLARSSL_ERR_DEBUG_BUF_TOO_SMALL;\
    }                                           \
                                                \
    n -= (unsigned int) ret;                    \
    p += (unsigned int) ret;                    \
}

/*
 * Store the name in printable form into buf; no more
 * than size characters will be written
 */
int x509parse_dn_gets( char *buf, size_t size, const x509_name *dn )
{
    int ret;
    size_t i, n;
    unsigned char c;
    const x509_name *name;
    char s[128], *p;

    memset( s, 0, sizeof( s ) );

    name = dn;
    p = buf;
    n = size;

    while( name != NULL )
    {
        if( !name->oid.p )
        {
            name = name->next;
            continue;
        }

        if( name != dn )
        {
            ret = snprintf( p, n, ", " );
            SAFE_SNPRINTF();
        }

        if( name->oid.len == 3 &&
            memcmp( name->oid.p, OID_X520, 2 ) == 0 )
        {
            switch( name->oid.p[2] )
            {
            case X520_COMMON_NAME:
                ret = snprintf( p, n, "CN=" ); break;

            case X520_COUNTRY:
                ret = snprintf( p, n, "C="  ); break;

            case X520_LOCALITY:
                ret = snprintf( p, n, "L="  ); break;

            case X520_STATE:
                ret = snprintf( p, n, "ST=" ); break;

            case X520_ORGANIZATION:
                ret = snprintf( p, n, "O="  ); break;

            case X520_ORG_UNIT:
                ret = snprintf( p, n, "OU=" ); break;

            default:
                ret = snprintf( p, n, "0x%02X=",
                               name->oid.p[2] );
                break;
            }
        SAFE_SNPRINTF();
        }
        else if( name->oid.len == 9 &&
                 memcmp( name->oid.p, OID_PKCS9, 8 ) == 0 )
        {
            switch( name->oid.p[8] )
            {
            case PKCS9_EMAIL:
                ret = snprintf( p, n, "emailAddress=" ); break;

            default:
                ret = snprintf( p, n, "0x%02X=",
                               name->oid.p[8] );
                break;
            }
        SAFE_SNPRINTF();
        }
        else
        {
            ret = snprintf( p, n, "\?\?=" );
            SAFE_SNPRINTF();
        }

        for( i = 0; i < name->val.len; i++ )
        {
            if( i >= sizeof( s ) - 1 )
                break;

            c = name->val.p[i];
            if( c < 32 || c == 127 || ( c > 128 && c < 160 ) )
                 s[i] = '?';
            else s[i] = c;
        }
        s[i] = '\0';
        ret = snprintf( p, n, "%s", s );
    SAFE_SNPRINTF();
        name = name->next;
    }

    return( (int) ( size - n ) );
}

/*
 * Store the serial in printable form into buf; no more
 * than size characters will be written
 */
int x509parse_serial_gets( char *buf, size_t size, const x509_buf *serial )
{
    int ret;
    size_t i, n, nr;
    char *p;

    p = buf;
    n = size;

    nr = ( serial->len <= 32 )
        ? serial->len  : 28;

    for( i = 0; i < nr; i++ )
    {
        if( i == 0 && nr > 1 && serial->p[i] == 0x0 )
            continue;

        ret = snprintf( p, n, "%02X%s",
                serial->p[i], ( i < nr - 1 ) ? ":" : "" );
        SAFE_SNPRINTF();
    }

    if( nr != serial->len )
    {
        ret = snprintf( p, n, "...." );
        SAFE_SNPRINTF();
    }

    return( (int) ( size - n ) );
}

/*
 * Return an informational string about the certificate.
 */
int x509parse_cert_info( char *buf, size_t size, const char *prefix,
                         const x509_cert *crt )
{
    int ret;
    size_t n;
    char *p;

    p = buf;
    n = size;

    ret = snprintf( p, n, "%scert. version : %d\n",
                               prefix, crt->version );
    SAFE_SNPRINTF();
    ret = snprintf( p, n, "%sserial number : ",
                               prefix );
    SAFE_SNPRINTF();

    ret = x509parse_serial_gets( p, n, &crt->serial);
    SAFE_SNPRINTF();

    ret = snprintf( p, n, "\n%sissuer name   : ", prefix );
    SAFE_SNPRINTF();
    ret = x509parse_dn_gets( p, n, &crt->issuer  );
    SAFE_SNPRINTF();

    ret = snprintf( p, n, "\n%ssubject name  : ", prefix );
    SAFE_SNPRINTF();
    ret = x509parse_dn_gets( p, n, &crt->subject );
    SAFE_SNPRINTF();

    ret = snprintf( p, n, "\n%sissued  on    : " \
                   "%04d-%02d-%02d %02d:%02d:%02d", prefix,
                   crt->valid_from.year, crt->valid_from.mon,
                   crt->valid_from.day,  crt->valid_from.hour,
                   crt->valid_from.min,  crt->valid_from.sec );
    SAFE_SNPRINTF();

    ret = snprintf( p, n, "\n%sexpires on    : " \
                   "%04d-%02d-%02d %02d:%02d:%02d", prefix,
                   crt->valid_to.year, crt->valid_to.mon,
                   crt->valid_to.day,  crt->valid_to.hour,
                   crt->valid_to.min,  crt->valid_to.sec );
    SAFE_SNPRINTF();

    ret = snprintf( p, n, "\n%ssigned using  : RSA+", prefix );
    SAFE_SNPRINTF();

    switch( crt->sig_alg )
    {
        case SIG_RSA_MD2    : ret = snprintf( p, n, "MD2"    ); break;
        case SIG_RSA_MD4    : ret = snprintf( p, n, "MD4"    ); break;
        case SIG_RSA_MD5    : ret = snprintf( p, n, "MD5"    ); break;
        case SIG_RSA_SHA1   : ret = snprintf( p, n, "SHA1"   ); break;
        case SIG_RSA_SHA224 : ret = snprintf( p, n, "SHA224" ); break;
        case SIG_RSA_SHA256 : ret = snprintf( p, n, "SHA256" ); break;
        case SIG_RSA_SHA384 : ret = snprintf( p, n, "SHA384" ); break;
        case SIG_RSA_SHA512 : ret = snprintf( p, n, "SHA512" ); break;
        default: ret = snprintf( p, n, "???"  ); break;
    }
    SAFE_SNPRINTF();

    ret = snprintf( p, n, "\n%sRSA key size  : %d bits\n", prefix,
                   (int) crt->rsa.N.n * (int) sizeof( t_uint ) * 8 );
    SAFE_SNPRINTF();

    return( (int) ( size - n ) );
}

/*
 * Return an informational string describing the given OID
 */
const char *x509_oid_get_description( x509_buf *oid )
{
    if ( oid == NULL )
        return ( NULL );
    
    else if( OID_CMP( OID_SERVER_AUTH, oid ) )
        return( STRING_SERVER_AUTH );
    
    else if( OID_CMP( OID_CLIENT_AUTH, oid ) )
        return( STRING_CLIENT_AUTH );
    
    else if( OID_CMP( OID_CODE_SIGNING, oid ) )
        return( STRING_CODE_SIGNING );
    
    else if( OID_CMP( OID_EMAIL_PROTECTION, oid ) )
        return( STRING_EMAIL_PROTECTION );
    
    else if( OID_CMP( OID_TIME_STAMPING, oid ) )
        return( STRING_TIME_STAMPING );

    else if( OID_CMP( OID_OCSP_SIGNING, oid ) )
        return( STRING_OCSP_SIGNING );

    return( NULL );
}

/* Return the x.y.z.... style numeric string for the given OID */
int x509_oid_get_numeric_string( char *buf, size_t size, x509_buf *oid )
{
    int ret;
    size_t i, n;
    unsigned int value;
    char *p;

    p = buf;
    n = size;

    /* First byte contains first two dots */
    if( oid->len > 0 )
    {
        ret = snprintf( p, n, "%d.%d", oid->p[0]/40, oid->p[0]%40 );
        SAFE_SNPRINTF();
    }

    /* TODO: value can overflow in value. */
    value = 0;
    for( i = 1; i < oid->len; i++ )
    {
        value <<= 7;
        value += oid->p[i] & 0x7F;

        if( !( oid->p[i] & 0x80 ) )
        {
            /* Last byte */
            ret = snprintf( p, n, ".%d", value );
            SAFE_SNPRINTF();
            value = 0;
        }
    }

    return( (int) ( size - n ) );
}

/*
 * Return an informational string about the CRL.
 */
int x509parse_crl_info( char *buf, size_t size, const char *prefix,
                        const x509_crl *crl )
{
    int ret;
    size_t n;
    char *p;
    const x509_crl_entry *entry;

    p = buf;
    n = size;

    ret = snprintf( p, n, "%sCRL version   : %d",
                               prefix, crl->version );
    SAFE_SNPRINTF();

    ret = snprintf( p, n, "\n%sissuer name   : ", prefix );
    SAFE_SNPRINTF();
    ret = x509parse_dn_gets( p, n, &crl->issuer );
    SAFE_SNPRINTF();

    ret = snprintf( p, n, "\n%sthis update   : " \
                   "%04d-%02d-%02d %02d:%02d:%02d", prefix,
                   crl->this_update.year, crl->this_update.mon,
                   crl->this_update.day,  crl->this_update.hour,
                   crl->this_update.min,  crl->this_update.sec );
    SAFE_SNPRINTF();

    ret = snprintf( p, n, "\n%snext update   : " \
                   "%04d-%02d-%02d %02d:%02d:%02d", prefix,
                   crl->next_update.year, crl->next_update.mon,
                   crl->next_update.day,  crl->next_update.hour,
                   crl->next_update.min,  crl->next_update.sec );
    SAFE_SNPRINTF();

    entry = &crl->entry;

    ret = snprintf( p, n, "\n%sRevoked certificates:",
                               prefix );
    SAFE_SNPRINTF();

    while( entry != NULL && entry->raw.len != 0 )
    {
        ret = snprintf( p, n, "\n%sserial number: ",
                               prefix );
        SAFE_SNPRINTF();

        ret = x509parse_serial_gets( p, n, &entry->serial);
        SAFE_SNPRINTF();

        ret = snprintf( p, n, " revocation date: " \
                   "%04d-%02d-%02d %02d:%02d:%02d",
                   entry->revocation_date.year, entry->revocation_date.mon,
                   entry->revocation_date.day,  entry->revocation_date.hour,
                   entry->revocation_date.min,  entry->revocation_date.sec );
        SAFE_SNPRINTF();

        entry = entry->next;
    }

    ret = snprintf( p, n, "\n%ssigned using  : RSA+", prefix );
    SAFE_SNPRINTF();

    switch( crl->sig_alg )
    {
        case SIG_RSA_MD2    : ret = snprintf( p, n, "MD2"    ); break;
        case SIG_RSA_MD4    : ret = snprintf( p, n, "MD4"    ); break;
        case SIG_RSA_MD5    : ret = snprintf( p, n, "MD5"    ); break;
        case SIG_RSA_SHA1   : ret = snprintf( p, n, "SHA1"   ); break;
        case SIG_RSA_SHA224 : ret = snprintf( p, n, "SHA224" ); break;
        case SIG_RSA_SHA256 : ret = snprintf( p, n, "SHA256" ); break;
        case SIG_RSA_SHA384 : ret = snprintf( p, n, "SHA384" ); break;
        case SIG_RSA_SHA512 : ret = snprintf( p, n, "SHA512" ); break;
        default: ret = snprintf( p, n, "???"  ); break;
    }
    SAFE_SNPRINTF();

    ret = snprintf( p, n, "\n" );
    SAFE_SNPRINTF();

    return( (int) ( size - n ) );
}

/*
 * Return 0 if the x509_time is still valid, or 1 otherwise.
 */
int x509parse_time_expired( const x509_time *to )
{
    int year, mon, day;
    int hour, min, sec;

#if defined(_WIN32)
    SYSTEMTIME st;

    GetLocalTime(&st);

    year = st.wYear;
    mon = st.wMonth;
    day = st.wDay;
    hour = st.wHour;
    min = st.wMinute;
    sec = st.wSecond;
#else
    struct tm *lt;
    time_t tt;

    tt = time( NULL );
    lt = localtime( &tt );

    year = lt->tm_year + 1900;
    mon = lt->tm_mon + 1;
    day = lt->tm_mday;
    hour = lt->tm_hour;
    min = lt->tm_min;
    sec = lt->tm_sec;
#endif

    if( year  > to->year )
        return( 1 );

    if( year == to->year &&
        mon   > to->mon )
        return( 1 );

    if( year == to->year &&
        mon  == to->mon  &&
        day   > to->day )
        return( 1 );

    if( year == to->year &&
        mon  == to->mon  &&
        day  == to->day  &&
        hour  > to->hour )
        return( 1 );

    if( year == to->year &&
        mon  == to->mon  &&
        day  == to->day  &&
        hour == to->hour &&
        min   > to->min  )
        return( 1 );

    if( year == to->year &&
        mon  == to->mon  &&
        day  == to->day  &&
        hour == to->hour &&
        min  == to->min  &&
        sec   > to->sec  )
        return( 1 );

    return( 0 );
}

/*
 * Return 1 if the certificate is revoked, or 0 otherwise.
 */
int x509parse_revoked( const x509_cert *crt, const x509_crl *crl )
{
    const x509_crl_entry *cur = &crl->entry;

    while( cur != NULL && cur->serial.len != 0 )
    {
        if( crt->serial.len == cur->serial.len &&
            memcmp( crt->serial.p, cur->serial.p, crt->serial.len ) == 0 )
        {
            if( x509parse_time_expired( &cur->revocation_date ) )
                return( 1 );
        }

        cur = cur->next;
    }

    return( 0 );
}

/*
 * Wrapper for x509 hashes.
 */
static void x509_hash( const unsigned char *in, size_t len, int alg,
                       unsigned char *out )
{
    switch( alg )
    {
#if defined(POLARSSL_MD2_C)
        case SIG_RSA_MD2    :  md2( in, len, out ); break;
#endif
#if defined(POLARSSL_MD4_C)
        case SIG_RSA_MD4    :  md4( in, len, out ); break;
#endif
#if defined(POLARSSL_MD5_C)
        case SIG_RSA_MD5    :  md5( in, len, out ); break;
#endif
#if defined(POLARSSL_SHA1_C)
        case SIG_RSA_SHA1   : sha1( in, len, out ); break;
#endif
#if defined(POLARSSL_SHA2_C)
        case SIG_RSA_SHA224 : sha2( in, len, out, 1 ); break;
        case SIG_RSA_SHA256 : sha2( in, len, out, 0 ); break;
#endif
#if defined(POLARSSL_SHA4_C)
        case SIG_RSA_SHA384 : sha4( in, len, out, 1 ); break;
        case SIG_RSA_SHA512 : sha4( in, len, out, 0 ); break;
#endif
        default:
            memset( out, '\xFF', 64 );
            break;
    }
}

/*
 * Check that the given certificate is valid accoring to the CRL.
 */
static int x509parse_verifycrl(x509_cert *crt, x509_cert *ca,
        x509_crl *crl_list)
{
    int flags = 0;
    int hash_id;
    unsigned char hash[64];

    if( ca == NULL )
        return( flags );

    /*
     * TODO: What happens if no CRL is present?
     * Suggestion: Revocation state should be unknown if no CRL is present.
     * For backwards compatibility this is not yet implemented.
     */

    while( crl_list != NULL )
    {
        if( crl_list->version == 0 ||
            crl_list->issuer_raw.len != ca->subject_raw.len ||
            memcmp( crl_list->issuer_raw.p, ca->subject_raw.p,
                    crl_list->issuer_raw.len ) != 0 )
        {
            crl_list = crl_list->next;
            continue;
        }

        /*
         * Check if CRL is correctly signed by the trusted CA
         */
        hash_id = crl_list->sig_alg;

        x509_hash( crl_list->tbs.p, crl_list->tbs.len, hash_id, hash );

        if( !rsa_pkcs1_verify( &ca->rsa, RSA_PUBLIC, hash_id,
                              0, hash, crl_list->sig.p ) == 0 )
        {
            /*
             * CRL is not trusted
             */
            flags |= BADCRL_NOT_TRUSTED;
            break;
        }

        /*
         * Check for validity of CRL (Do not drop out)
         */
        if( x509parse_time_expired( &crl_list->next_update ) )
            flags |= BADCRL_EXPIRED;

        /*
         * Check if certificate is revoked
         */
        if( x509parse_revoked(crt, crl_list) )
        {
            flags |= BADCERT_REVOKED;
            break;
        }

        crl_list = crl_list->next;
    }
    return flags;
}

int x509_wildcard_verify( const char *cn, x509_buf *name )
{
    size_t i;
    size_t cn_idx = 0;

    if( name->len < 3 || name->p[0] != '*' || name->p[1] != '.' )
        return( 0 );

    for( i = 0; i < strlen( cn ); ++i )
    {
        if( cn[i] == '.' )
        {
            cn_idx = i;
            break;
        }
    }

    if( cn_idx == 0 )
        return( 0 );

    if( strlen( cn ) - cn_idx == name->len - 1 &&
        memcmp( name->p + 1, cn + cn_idx, name->len - 1 ) == 0 )
    {
        return( 1 );
    }

    return( 0 );
}

static int x509parse_verify_top(
                x509_cert *child, x509_cert *trust_ca,
                x509_crl *ca_crl, int path_cnt, int *flags,
                int (*f_vrfy)(void *, x509_cert *, int, int *),
                void *p_vrfy )
{
    int hash_id, ret;
    int ca_flags = 0, check_path_cnt = path_cnt + 1;
    unsigned char hash[64];

    if( x509parse_time_expired( &child->valid_to ) )
        *flags |= BADCERT_EXPIRED;

    /*
     * Child is the top of the chain. Check against the trust_ca list.
     */
    *flags |= BADCERT_NOT_TRUSTED;

    while( trust_ca != NULL )
    {
        if( trust_ca->version == 0 ||
            child->issuer_raw.len != trust_ca->subject_raw.len ||
            memcmp( child->issuer_raw.p, trust_ca->subject_raw.p,
                    child->issuer_raw.len ) != 0 )
        {
            trust_ca = trust_ca->next;
            continue;
        }

        /*
         * Reduce path_len to check against if top of the chain is
         * the same as the trusted CA
         */
        if( child->subject_raw.len == trust_ca->subject_raw.len &&
            memcmp( child->subject_raw.p, trust_ca->subject_raw.p,
                            child->issuer_raw.len ) == 0 ) 
        {
            check_path_cnt--;
        }

        if( trust_ca->max_pathlen > 0 &&
            trust_ca->max_pathlen < check_path_cnt )
        {
            trust_ca = trust_ca->next;
            continue;
        }

        hash_id = child->sig_alg;

        x509_hash( child->tbs.p, child->tbs.len, hash_id, hash );

        if( rsa_pkcs1_verify( &trust_ca->rsa, RSA_PUBLIC, hash_id,
                    0, hash, child->sig.p ) != 0 )
        {
            trust_ca = trust_ca->next;
            continue;
        }

        /*
         * Top of chain is signed by a trusted CA
         */
        *flags &= ~BADCERT_NOT_TRUSTED;
        break;
    }

    /*
     * If top of chain is not the same as the trusted CA send a verify request
     * to the callback for any issues with validity and CRL presence for the
     * trusted CA certificate.
     */
    if( trust_ca != NULL &&
        ( child->subject_raw.len != trust_ca->subject_raw.len ||
          memcmp( child->subject_raw.p, trust_ca->subject_raw.p,
                            child->issuer_raw.len ) != 0 ) )
    {
        /* Check trusted CA's CRL for then chain's top crt */
        *flags |= x509parse_verifycrl( child, trust_ca, ca_crl );

        if( x509parse_time_expired( &trust_ca->valid_to ) )
            ca_flags |= BADCERT_EXPIRED;

        if( NULL != f_vrfy )
        {
            if( ( ret = f_vrfy( p_vrfy, trust_ca, path_cnt + 1, &ca_flags ) ) != 0 )
                return( ret );
        }
    }

    /* Call callback on top cert */
    if( NULL != f_vrfy )
    {
        if( ( ret = f_vrfy(p_vrfy, child, path_cnt, flags ) ) != 0 )
            return( ret );
    }

    *flags |= ca_flags;

    return( 0 );
}

static int x509parse_verify_child(
                x509_cert *child, x509_cert *parent, x509_cert *trust_ca,
                x509_crl *ca_crl, int path_cnt, int *flags,
                int (*f_vrfy)(void *, x509_cert *, int, int *),
                void *p_vrfy )
{
    int hash_id, ret;
    int parent_flags = 0;
    unsigned char hash[64];
    x509_cert *grandparent;

    if( x509parse_time_expired( &child->valid_to ) )
        *flags |= BADCERT_EXPIRED;

    hash_id = child->sig_alg;

    x509_hash( child->tbs.p, child->tbs.len, hash_id, hash );

    if( rsa_pkcs1_verify( &parent->rsa, RSA_PUBLIC, hash_id, 0, hash,
                           child->sig.p ) != 0 )
        *flags |= BADCERT_NOT_TRUSTED;
        
    /* Check trusted CA's CRL for the given crt */
    *flags |= x509parse_verifycrl(child, parent, ca_crl);

    grandparent = parent->next;

    while( grandparent != NULL )
    {
        if( grandparent->version == 0 ||
            grandparent->ca_istrue == 0 ||
            parent->issuer_raw.len != grandparent->subject_raw.len ||
            memcmp( parent->issuer_raw.p, grandparent->subject_raw.p,
                    parent->issuer_raw.len ) != 0 )
        {
            grandparent = grandparent->next;
            continue;
        }
        break;
    }

    if( grandparent != NULL )
    {
        /*
         * Part of the chain
         */
        ret = x509parse_verify_child( parent, grandparent, trust_ca, ca_crl, path_cnt + 1, &parent_flags, f_vrfy, p_vrfy );
        if( ret != 0 )
            return( ret );
    } 
    else
    {
        ret = x509parse_verify_top( parent, trust_ca, ca_crl, path_cnt + 1, &parent_flags, f_vrfy, p_vrfy );
        if( ret != 0 )
            return( ret );
    }

    /* child is verified to be a child of the parent, call verify callback */
    if( NULL != f_vrfy )
        if( ( ret = f_vrfy( p_vrfy, child, path_cnt, flags ) ) != 0 )
            return( ret );

    *flags |= parent_flags;

    return( 0 );
}

/*
 * Verify the certificate validity
 */
int x509parse_verify( x509_cert *crt,
                      x509_cert *trust_ca,
                      x509_crl *ca_crl,
                      const char *cn, int *flags,
                      int (*f_vrfy)(void *, x509_cert *, int, int *),
                      void *p_vrfy )
{
    size_t cn_len;
    int ret;
    int pathlen = 0;
    x509_cert *parent;
    x509_name *name;
    x509_sequence *cur = NULL;

    *flags = 0;

    if( cn != NULL )
    {
        name = &crt->subject;
        cn_len = strlen( cn );

        if( crt->ext_types & EXT_SUBJECT_ALT_NAME )
        {
            cur = &crt->subject_alt_names;

            while( cur != NULL )
            {
                if( cur->buf.len == cn_len &&
                    memcmp( cn, cur->buf.p, cn_len ) == 0 )
                    break;

                if( cur->buf.len > 2 &&
                    memcmp( cur->buf.p, "*.", 2 ) == 0 &&
                            x509_wildcard_verify( cn, &cur->buf ) )
                    break;

                cur = cur->next;
            }

            if( cur == NULL )
                *flags |= BADCERT_CN_MISMATCH;
        }
        else
        {
            while( name != NULL )
            {
                if( name->oid.len == 3 &&
                    memcmp( name->oid.p, OID_CN,  3 ) == 0 )
                {
                    if( name->val.len == cn_len &&
                        memcmp( name->val.p, cn, cn_len ) == 0 )
                        break;

                    if( name->val.len > 2 &&
                        memcmp( name->val.p, "*.", 2 ) == 0 &&
                                x509_wildcard_verify( cn, &name->val ) )
                        break;
                }

                name = name->next;
            }

            if( name == NULL )
                *flags |= BADCERT_CN_MISMATCH;
        }
    }

    /*
     * Iterate upwards in the given cert chain, to find our crt parent.
     * Ignore any upper cert with CA != TRUE.
     */
    parent = crt->next;

    while( parent != NULL && parent->version != 0 )
    {
        if( parent->ca_istrue == 0 ||
            crt->issuer_raw.len != parent->subject_raw.len ||
            memcmp( crt->issuer_raw.p, parent->subject_raw.p,
                    crt->issuer_raw.len ) != 0 )
        {
            parent = parent->next;
            continue;
        }
        break;
    }

    if( parent != NULL )
    {
        /*
         * Part of the chain
         */
        ret = x509parse_verify_child( crt, parent, trust_ca, ca_crl, pathlen, flags, f_vrfy, p_vrfy );
        if( ret != 0 )
            return( ret );
    } 
    else
    {
        ret = x509parse_verify_top( crt, trust_ca, ca_crl, pathlen, flags, f_vrfy, p_vrfy );
        if( ret != 0 )
            return( ret );
    }

    if( *flags != 0 )
        return( POLARSSL_ERR_X509_CERT_VERIFY_FAILED );

    return( 0 );
}

/*
 * Unallocate all certificate data
 */
void x509_free( x509_cert *crt )
{
    x509_cert *cert_cur = crt;
    x509_cert *cert_prv;
    x509_name *name_cur;
    x509_name *name_prv;
    x509_sequence *seq_cur;
    x509_sequence *seq_prv;

    if( crt == NULL )
        return;

    do
    {
        rsa_free( &cert_cur->rsa );

        name_cur = cert_cur->issuer.next;
        while( name_cur != NULL )
        {
            name_prv = name_cur;
            name_cur = name_cur->next;
            memset( name_prv, 0, sizeof( x509_name ) );
            free( name_prv );
        }

        name_cur = cert_cur->subject.next;
        while( name_cur != NULL )
        {
            name_prv = name_cur;
            name_cur = name_cur->next;
            memset( name_prv, 0, sizeof( x509_name ) );
            free( name_prv );
        }

        seq_cur = cert_cur->ext_key_usage.next;
        while( seq_cur != NULL )
        {
            seq_prv = seq_cur;
            seq_cur = seq_cur->next;
            memset( seq_prv, 0, sizeof( x509_sequence ) );
            free( seq_prv );
        }

        seq_cur = cert_cur->subject_alt_names.next;
        while( seq_cur != NULL )
        {
            seq_prv = seq_cur;
            seq_cur = seq_cur->next;
            memset( seq_prv, 0, sizeof( x509_sequence ) );
            free( seq_prv );
        }

        if( cert_cur->raw.p != NULL )
        {
            memset( cert_cur->raw.p, 0, cert_cur->raw.len );
            free( cert_cur->raw.p );
        }

        cert_cur = cert_cur->next;
    }
    while( cert_cur != NULL );

    cert_cur = crt;
    do
    {
        cert_prv = cert_cur;
        cert_cur = cert_cur->next;

        memset( cert_prv, 0, sizeof( x509_cert ) );
        if( cert_prv != crt )
            free( cert_prv );
    }
    while( cert_cur != NULL );
}

/*
 * Unallocate all CRL data
 */
void x509_crl_free( x509_crl *crl )
{
    x509_crl *crl_cur = crl;
    x509_crl *crl_prv;
    x509_name *name_cur;
    x509_name *name_prv;
    x509_crl_entry *entry_cur;
    x509_crl_entry *entry_prv;

    if( crl == NULL )
        return;

    do
    {
        name_cur = crl_cur->issuer.next;
        while( name_cur != NULL )
        {
            name_prv = name_cur;
            name_cur = name_cur->next;
            memset( name_prv, 0, sizeof( x509_name ) );
            free( name_prv );
        }

        entry_cur = crl_cur->entry.next;
        while( entry_cur != NULL )
        {
            entry_prv = entry_cur;
            entry_cur = entry_cur->next;
            memset( entry_prv, 0, sizeof( x509_crl_entry ) );
            free( entry_prv );
        }

        if( crl_cur->raw.p != NULL )
        {
            memset( crl_cur->raw.p, 0, crl_cur->raw.len );
            free( crl_cur->raw.p );
        }

        crl_cur = crl_cur->next;
    }
    while( crl_cur != NULL );

    crl_cur = crl;
    do
    {
        crl_prv = crl_cur;
        crl_cur = crl_cur->next;

        memset( crl_prv, 0, sizeof( x509_crl ) );
        if( crl_prv != crl )
            free( crl_prv );
    }
    while( crl_cur != NULL );
}

#if defined(POLARSSL_SELF_TEST)

#include "polarssl/certs.h"

/*
 * Checkup routine
 */
int x509_self_test( int verbose )
{
#if defined(POLARSSL_CERTS_C) && defined(POLARSSL_MD5_C)
    int ret;
    int flags;
    size_t i, j;
    x509_cert cacert;
    x509_cert clicert;
    rsa_context rsa;
#if defined(POLARSSL_DHM_C)
    dhm_context dhm;
#endif

    if( verbose != 0 )
        printf( "  X.509 certificate load: " );

    memset( &clicert, 0, sizeof( x509_cert ) );

    ret = x509parse_crt( &clicert, (const unsigned char *) test_cli_crt,
                         strlen( test_cli_crt ) );
    if( ret != 0 )
    {
        if( verbose != 0 )
            printf( "failed\n" );

        return( ret );
    }

    memset( &cacert, 0, sizeof( x509_cert ) );

    ret = x509parse_crt( &cacert, (const unsigned char *) test_ca_crt,
                         strlen( test_ca_crt ) );
    if( ret != 0 )
    {
        if( verbose != 0 )
            printf( "failed\n" );

        return( ret );
    }

    if( verbose != 0 )
        printf( "passed\n  X.509 private key load: " );

    i = strlen( test_ca_key );
    j = strlen( test_ca_pwd );

    rsa_init( &rsa, RSA_PKCS_V15, 0 );

    if( ( ret = x509parse_key( &rsa,
                    (const unsigned char *) test_ca_key, i,
                    (const unsigned char *) test_ca_pwd, j ) ) != 0 )
    {
        if( verbose != 0 )
            printf( "failed\n" );

        return( ret );
    }

    if( verbose != 0 )
        printf( "passed\n  X.509 signature verify: ");

    ret = x509parse_verify( &clicert, &cacert, NULL, "PolarSSL Client 2", &flags, NULL, NULL );
    if( ret != 0 )
    {
        printf("%02x", flags);
        if( verbose != 0 )
            printf( "failed\n" );

        return( ret );
    }

#if defined(POLARSSL_DHM_C)
    if( verbose != 0 )
        printf( "passed\n  X.509 DHM parameter load: " );

    i = strlen( test_dhm_params );
    j = strlen( test_ca_pwd );

    if( ( ret = x509parse_dhm( &dhm, (const unsigned char *) test_dhm_params, i ) ) != 0 )
    {
        if( verbose != 0 )
            printf( "failed\n" );

        return( ret );
    }

    if( verbose != 0 )
        printf( "passed\n\n" );
#endif

    x509_free( &cacert  );
    x509_free( &clicert );
    rsa_free( &rsa );
#if defined(POLARSSL_DHM_C)
    dhm_free( &dhm );
#endif

    return( 0 );
#else
    ((void) verbose);
    return( POLARSSL_ERR_X509_FEATURE_UNAVAILABLE );
#endif
}

#endif

#endif
