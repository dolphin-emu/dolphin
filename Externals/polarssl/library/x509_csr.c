/*
 *  X.509 Certificate Signing Request (CSR) parsing
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

#if defined(POLARSSL_X509_CSR_PARSE_C)

#include "polarssl/x509_csr.h"
#include "polarssl/oid.h"
#if defined(POLARSSL_PEM_PARSE_C)
#include "polarssl/pem.h"
#endif

#if defined(POLARSSL_MEMORY_C)
#include "polarssl/memory.h"
#else
#define polarssl_malloc     malloc
#define polarssl_free       free
#endif

#include <string.h>
#include <stdlib.h>

#if defined(POLARSSL_FS_IO) || defined(EFIX64) || defined(EFI32)
#include <stdio.h>
#endif

/*
 *  Version  ::=  INTEGER  {  v1(0)  }
 */
static int x509_csr_get_version( unsigned char **p,
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

        return( POLARSSL_ERR_X509_INVALID_VERSION + ret );
    }

    return( 0 );
}

/*
 * Parse a CSR
 */
int x509_csr_parse( x509_csr *csr, const unsigned char *buf, size_t buflen )
{
    int ret;
    size_t len;
    unsigned char *p, *end;
#if defined(POLARSSL_PEM_PARSE_C)
    size_t use_len;
    pem_context pem;
#endif

    /*
     * Check for valid input
     */
    if( csr == NULL || buf == NULL )
        return( POLARSSL_ERR_X509_BAD_INPUT_DATA );

    x509_csr_init( csr );

#if defined(POLARSSL_PEM_PARSE_C)
    pem_init( &pem );
    ret = pem_read_buffer( &pem,
                           "-----BEGIN CERTIFICATE REQUEST-----",
                           "-----END CERTIFICATE REQUEST-----",
                           buf, NULL, 0, &use_len );

    if( ret == 0 )
    {
        /*
         * Was PEM encoded, steal PEM buffer
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
#endif
    {
        /*
         * nope, copy the raw DER data
         */
        p = (unsigned char *) polarssl_malloc( len = buflen );

        if( p == NULL )
            return( POLARSSL_ERR_X509_MALLOC_FAILED );

        memcpy( p, buf, buflen );
    }

    csr->raw.p = p;
    csr->raw.len = len;
    end = p + len;

    /*
     *  CertificationRequest ::= SEQUENCE {
     *       certificationRequestInfo CertificationRequestInfo,
     *       signatureAlgorithm AlgorithmIdentifier,
     *       signature          BIT STRING
     *  }
     */
    if( ( ret = asn1_get_tag( &p, end, &len,
            ASN1_CONSTRUCTED | ASN1_SEQUENCE ) ) != 0 )
    {
        x509_csr_free( csr );
        return( POLARSSL_ERR_X509_INVALID_FORMAT );
    }

    if( len != (size_t) ( end - p ) )
    {
        x509_csr_free( csr );
        return( POLARSSL_ERR_X509_INVALID_FORMAT +
                POLARSSL_ERR_ASN1_LENGTH_MISMATCH );
    }

    /*
     *  CertificationRequestInfo ::= SEQUENCE {
     */
    csr->cri.p = p;

    if( ( ret = asn1_get_tag( &p, end, &len,
            ASN1_CONSTRUCTED | ASN1_SEQUENCE ) ) != 0 )
    {
        x509_csr_free( csr );
        return( POLARSSL_ERR_X509_INVALID_FORMAT + ret );
    }

    end = p + len;
    csr->cri.len = end - csr->cri.p;

    /*
     *  Version  ::=  INTEGER {  v1(0) }
     */
    if( ( ret = x509_csr_get_version( &p, end, &csr->version ) ) != 0 )
    {
        x509_csr_free( csr );
        return( ret );
    }

    csr->version++;

    if( csr->version != 1 )
    {
        x509_csr_free( csr );
        return( POLARSSL_ERR_X509_UNKNOWN_VERSION );
    }

    /*
     *  subject               Name
     */
    csr->subject_raw.p = p;

    if( ( ret = asn1_get_tag( &p, end, &len,
            ASN1_CONSTRUCTED | ASN1_SEQUENCE ) ) != 0 )
    {
        x509_csr_free( csr );
        return( POLARSSL_ERR_X509_INVALID_FORMAT + ret );
    }

    if( ( ret = x509_get_name( &p, p + len, &csr->subject ) ) != 0 )
    {
        x509_csr_free( csr );
        return( ret );
    }

    csr->subject_raw.len = p - csr->subject_raw.p;

    /*
     *  subjectPKInfo SubjectPublicKeyInfo
     */
    if( ( ret = pk_parse_subpubkey( &p, end, &csr->pk ) ) != 0 )
    {
        x509_csr_free( csr );
        return( ret );
    }

    /*
     *  attributes    [0] Attributes
     */
    if( ( ret = asn1_get_tag( &p, end, &len,
            ASN1_CONSTRUCTED | ASN1_CONTEXT_SPECIFIC ) ) != 0 )
    {
        x509_csr_free( csr );
        return( POLARSSL_ERR_X509_INVALID_FORMAT + ret );
    }
    // TODO Parse Attributes / extension requests

    p += len;

    end = csr->raw.p + csr->raw.len;

    /*
     *  signatureAlgorithm   AlgorithmIdentifier,
     *  signature            BIT STRING
     */
    if( ( ret = x509_get_alg_null( &p, end, &csr->sig_oid ) ) != 0 )
    {
        x509_csr_free( csr );
        return( ret );
    }

    if( ( ret = x509_get_sig_alg( &csr->sig_oid, &csr->sig_md,
                                  &csr->sig_pk ) ) != 0 )
    {
        x509_csr_free( csr );
        return( POLARSSL_ERR_X509_UNKNOWN_SIG_ALG );
    }

    if( ( ret = x509_get_sig( &p, end, &csr->sig ) ) != 0 )
    {
        x509_csr_free( csr );
        return( ret );
    }

    if( p != end )
    {
        x509_csr_free( csr );
        return( POLARSSL_ERR_X509_INVALID_FORMAT +
                POLARSSL_ERR_ASN1_LENGTH_MISMATCH );
    }

    return( 0 );
}

#if defined(POLARSSL_FS_IO)
/*
 * Load a CSR into the structure
 */
int x509_csr_parse_file( x509_csr *csr, const char *path )
{
    int ret;
    size_t n;
    unsigned char *buf;

    if ( ( ret = x509_load_file( path, &buf, &n ) ) != 0 )
        return( ret );

    ret = x509_csr_parse( csr, buf, n );

    memset( buf, 0, n + 1 );
    polarssl_free( buf );

    return( ret );
}
#endif /* POLARSSL_FS_IO */

#if defined(_MSC_VER) && !defined snprintf && !defined(EFIX64) && \
    !defined(EFI32)
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
static int compat_snprintf(char *str, size_t size, const char *format, ...)
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

#define BEFORE_COLON    14
#define BC              "14"
/*
 * Return an informational string about the CSR.
 */
int x509_csr_info( char *buf, size_t size, const char *prefix,
                   const x509_csr *csr )
{
    int ret;
    size_t n;
    char *p;
    const char *desc;
    char key_size_str[BEFORE_COLON];

    p = buf;
    n = size;

    ret = snprintf( p, n, "%sCSR version   : %d",
                               prefix, csr->version );
    SAFE_SNPRINTF();

    ret = snprintf( p, n, "\n%ssubject name  : ", prefix );
    SAFE_SNPRINTF();
    ret = x509_dn_gets( p, n, &csr->subject );
    SAFE_SNPRINTF();

    ret = snprintf( p, n, "\n%ssigned using  : ", prefix );
    SAFE_SNPRINTF();

    ret = oid_get_sig_alg_desc( &csr->sig_oid, &desc );
    if( ret != 0 )
        ret = snprintf( p, n, "???"  );
    else
        ret = snprintf( p, n, "%s", desc );
    SAFE_SNPRINTF();

    if( ( ret = x509_key_size_helper( key_size_str, BEFORE_COLON,
                                      pk_get_name( &csr->pk ) ) ) != 0 )
    {
        return( ret );
    }

    ret = snprintf( p, n, "\n%s%-" BC "s: %d bits\n", prefix, key_size_str,
                          (int) pk_get_size( &csr->pk ) );
    SAFE_SNPRINTF();

    return( (int) ( size - n ) );
}

/*
 * Initialize a CSR
 */
void x509_csr_init( x509_csr *csr )
{
    memset( csr, 0, sizeof(x509_csr) );
}

/*
 * Unallocate all CSR data
 */
void x509_csr_free( x509_csr *csr )
{
    x509_name *name_cur;
    x509_name *name_prv;

    if( csr == NULL )
        return;

    pk_free( &csr->pk );

    name_cur = csr->subject.next;
    while( name_cur != NULL )
    {
        name_prv = name_cur;
        name_cur = name_cur->next;
        memset( name_prv, 0, sizeof( x509_name ) );
        polarssl_free( name_prv );
    }

    if( csr->raw.p != NULL )
    {
        memset( csr->raw.p, 0, csr->raw.len );
        polarssl_free( csr->raw.p );
    }

    memset( csr, 0, sizeof( x509_csr ) );
}

#endif /* POLARSSL_X509_CSR_PARSE_C */
