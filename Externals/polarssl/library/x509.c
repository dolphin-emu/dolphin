/*
 *  X.509 certificate and private key decoding
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

#if defined(POLARSSL_X509_USE_C)

#include "polarssl/x509.h"
#include "polarssl/asn1.h"
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
#if defined(_WIN32) && !defined(EFIX64) && !defined(EFI32)
#include <windows.h>
#else
#include <time.h>
#endif

#if defined(EFIX64) || defined(EFI32)
#include <stdio.h>
#endif

#if defined(POLARSSL_FS_IO)
#include <stdio.h>
#if !defined(_WIN32)
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#endif
#endif

/*
 *  CertificateSerialNumber  ::=  INTEGER
 */
int x509_get_serial( unsigned char **p, const unsigned char *end,
                     x509_buf *serial )
{
    int ret;

    if( ( end - *p ) < 1 )
        return( POLARSSL_ERR_X509_INVALID_SERIAL +
                POLARSSL_ERR_ASN1_OUT_OF_DATA );

    if( **p != ( ASN1_CONTEXT_SPECIFIC | ASN1_PRIMITIVE | 2 ) &&
        **p !=   ASN1_INTEGER )
        return( POLARSSL_ERR_X509_INVALID_SERIAL +
                POLARSSL_ERR_ASN1_UNEXPECTED_TAG );

    serial->tag = *(*p)++;

    if( ( ret = asn1_get_len( p, end, &serial->len ) ) != 0 )
        return( POLARSSL_ERR_X509_INVALID_SERIAL + ret );

    serial->p = *p;
    *p += serial->len;

    return( 0 );
}

/* Get an algorithm identifier without parameters (eg for signatures)
 *
 *  AlgorithmIdentifier  ::=  SEQUENCE  {
 *       algorithm               OBJECT IDENTIFIER,
 *       parameters              ANY DEFINED BY algorithm OPTIONAL  }
 */
int x509_get_alg_null( unsigned char **p, const unsigned char *end,
                       x509_buf *alg )
{
    int ret;

    if( ( ret = asn1_get_alg_null( p, end, alg ) ) != 0 )
        return( POLARSSL_ERR_X509_INVALID_ALG + ret );

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
        return( POLARSSL_ERR_X509_INVALID_NAME + ret );

    if( ( end - *p ) < 1 )
        return( POLARSSL_ERR_X509_INVALID_NAME +
                POLARSSL_ERR_ASN1_OUT_OF_DATA );

    oid = &cur->oid;
    oid->tag = **p;

    if( ( ret = asn1_get_tag( p, end, &oid->len, ASN1_OID ) ) != 0 )
        return( POLARSSL_ERR_X509_INVALID_NAME + ret );

    oid->p = *p;
    *p += oid->len;

    if( ( end - *p ) < 1 )
        return( POLARSSL_ERR_X509_INVALID_NAME +
                POLARSSL_ERR_ASN1_OUT_OF_DATA );

    if( **p != ASN1_BMP_STRING && **p != ASN1_UTF8_STRING      &&
        **p != ASN1_T61_STRING && **p != ASN1_PRINTABLE_STRING &&
        **p != ASN1_IA5_STRING && **p != ASN1_UNIVERSAL_STRING )
        return( POLARSSL_ERR_X509_INVALID_NAME +
                POLARSSL_ERR_ASN1_UNEXPECTED_TAG );

    val = &cur->val;
    val->tag = *(*p)++;

    if( ( ret = asn1_get_len( p, end, &val->len ) ) != 0 )
        return( POLARSSL_ERR_X509_INVALID_NAME + ret );

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
int x509_get_name( unsigned char **p, const unsigned char *end,
                   x509_name *cur )
{
    int ret;
    size_t len;
    const unsigned char *end2;
    x509_name *use;

    if( ( ret = asn1_get_tag( p, end, &len,
            ASN1_CONSTRUCTED | ASN1_SET ) ) != 0 )
        return( POLARSSL_ERR_X509_INVALID_NAME + ret );

    end2 = end;
    end  = *p + len;
    use = cur;

    do
    {
        if( ( ret = x509_get_attr_type_value( p, end, use ) ) != 0 )
            return( ret );

        if( *p != end )
        {
            use->next = (x509_name *) polarssl_malloc(
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

    cur->next = (x509_name *) polarssl_malloc(
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
int x509_get_time( unsigned char **p, const unsigned char *end,
                   x509_time *time )
{
    int ret;
    size_t len;
    char date[64];
    unsigned char tag;

    if( ( end - *p ) < 1 )
        return( POLARSSL_ERR_X509_INVALID_DATE +
                POLARSSL_ERR_ASN1_OUT_OF_DATA );

    tag = **p;

    if ( tag == ASN1_UTC_TIME )
    {
        (*p)++;
        ret = asn1_get_len( p, end, &len );

        if( ret != 0 )
            return( POLARSSL_ERR_X509_INVALID_DATE + ret );

        memset( date,  0, sizeof( date ) );
        memcpy( date, *p, ( len < sizeof( date ) - 1 ) ?
                len : sizeof( date ) - 1 );

        if( sscanf( date, "%2d%2d%2d%2d%2d%2d",
                    &time->year, &time->mon, &time->day,
                    &time->hour, &time->min, &time->sec ) < 5 )
            return( POLARSSL_ERR_X509_INVALID_DATE );

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
            return( POLARSSL_ERR_X509_INVALID_DATE + ret );

        memset( date,  0, sizeof( date ) );
        memcpy( date, *p, ( len < sizeof( date ) - 1 ) ?
                len : sizeof( date ) - 1 );

        if( sscanf( date, "%4d%2d%2d%2d%2d%2d",
                    &time->year, &time->mon, &time->day,
                    &time->hour, &time->min, &time->sec ) < 5 )
            return( POLARSSL_ERR_X509_INVALID_DATE );

        *p += len;

        return( 0 );
    }
    else
        return( POLARSSL_ERR_X509_INVALID_DATE +
                POLARSSL_ERR_ASN1_UNEXPECTED_TAG );
}

int x509_get_sig( unsigned char **p, const unsigned char *end, x509_buf *sig )
{
    int ret;
    size_t len;

    if( ( end - *p ) < 1 )
        return( POLARSSL_ERR_X509_INVALID_SIGNATURE +
                POLARSSL_ERR_ASN1_OUT_OF_DATA );

    sig->tag = **p;

    if( ( ret = asn1_get_bitstring_null( p, end, &len ) ) != 0 )
        return( POLARSSL_ERR_X509_INVALID_SIGNATURE + ret );

    sig->len = len;
    sig->p = *p;

    *p += len;

    return( 0 );
}

int x509_get_sig_alg( const x509_buf *sig_oid, md_type_t *md_alg,
                      pk_type_t *pk_alg )
{
    int ret = oid_get_sig_alg( sig_oid, md_alg, pk_alg );

    if( ret != 0 )
        return( POLARSSL_ERR_X509_UNKNOWN_SIG_ALG + ret );

    return( 0 );
}

/*
 * X.509 Extensions (No parsing of extensions, pointer should
 * be either manually updated or extensions should be parsed!
 */
int x509_get_ext( unsigned char **p, const unsigned char *end,
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
        return( POLARSSL_ERR_X509_INVALID_EXTENSIONS + ret );

    if( end != *p + len )
        return( POLARSSL_ERR_X509_INVALID_EXTENSIONS +
                POLARSSL_ERR_ASN1_LENGTH_MISMATCH );

    return( 0 );
}

#if defined(POLARSSL_FS_IO)
/*
 * Load all data from a file into a given buffer.
 */
int x509_load_file( const char *path, unsigned char **buf, size_t *n )
{
    FILE *f;
    long size;

    if( ( f = fopen( path, "rb" ) ) == NULL )
        return( POLARSSL_ERR_X509_FILE_IO_ERROR );

    fseek( f, 0, SEEK_END );
    if( ( size = ftell( f ) ) == -1 )
    {
        fclose( f );
        return( POLARSSL_ERR_X509_FILE_IO_ERROR );
    }
    fseek( f, 0, SEEK_SET );

    *n = (size_t) size;

    if( *n + 1 == 0 ||
        ( *buf = (unsigned char *) polarssl_malloc( *n + 1 ) ) == NULL )
    {
        fclose( f );
        return( POLARSSL_ERR_X509_MALLOC_FAILED );
    }

    if( fread( *buf, 1, *n, f ) != *n )
    {
        fclose( f );
        polarssl_free( *buf );
        return( POLARSSL_ERR_X509_FILE_IO_ERROR );
    }

    fclose( f );

    (*buf)[*n] = '\0';

    return( 0 );
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

/*
 * Store the name in printable form into buf; no more
 * than size characters will be written
 */
int x509_dn_gets( char *buf, size_t size, const x509_name *dn )
{
    int ret;
    size_t i, n;
    unsigned char c;
    const x509_name *name;
    const char *short_name = NULL;
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

        ret = oid_get_attr_short_name( &name->oid, &short_name );

        if( ret == 0 )
            ret = snprintf( p, n, "%s=", short_name );
        else
            ret = snprintf( p, n, "\?\?=" );
        SAFE_SNPRINTF();

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
int x509_serial_gets( char *buf, size_t size, const x509_buf *serial )
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
 * Helper for writing "RSA key size", "EC key size", etc
 */
int x509_key_size_helper( char *buf, size_t size, const char *name )
{
    char *p = buf;
    size_t n = size;
    int ret;

    if( strlen( name ) + sizeof( " key size" ) > size )
        return POLARSSL_ERR_DEBUG_BUF_TOO_SMALL;

    ret = snprintf( p, n, "%s key size", name );
    SAFE_SNPRINTF();

    return( 0 );
}

/*
 * Return an informational string describing the given OID
 */
const char *x509_oid_get_description( x509_buf *oid )
{
    const char *desc = NULL;
    int ret;

    ret = oid_get_extended_key_usage( oid, &desc );

    if( ret != 0 )
        return( NULL );

    return( desc );
}

/* Return the x.y.z.... style numeric string for the given OID */
int x509_oid_get_numeric_string( char *buf, size_t size, x509_buf *oid )
{
    return oid_get_numeric_string( buf, size, oid );
}

/*
 * Return 0 if the x509_time is still valid, or 1 otherwise.
 */
#if defined(POLARSSL_HAVE_TIME)
int x509_time_expired( const x509_time *to )
{
    int year, mon, day;
    int hour, min, sec;

#if defined(_WIN32) && !defined(EFIX64) && !defined(EFI32)
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
#else  /* POLARSSL_HAVE_TIME */
int x509_time_expired( const x509_time *to )
{
    ((void) to);
    return( 0 );
}
#endif /* POLARSSL_HAVE_TIME */

#if defined(POLARSSL_SELF_TEST)

#include "polarssl/x509_crt.h"
#include "polarssl/certs.h"

/*
 * Checkup routine
 */
int x509_self_test( int verbose )
{
#if defined(POLARSSL_CERTS_C) && defined(POLARSSL_MD5_C)
    int ret;
    int flags;
    x509_crt cacert;
    x509_crt clicert;

    if( verbose != 0 )
        printf( "  X.509 certificate load: " );

    x509_crt_init( &clicert );

    ret = x509_crt_parse( &clicert, (const unsigned char *) test_cli_crt,
                          strlen( test_cli_crt ) );
    if( ret != 0 )
    {
        if( verbose != 0 )
            printf( "failed\n" );

        return( ret );
    }

    x509_crt_init( &cacert );

    ret = x509_crt_parse( &cacert, (const unsigned char *) test_ca_crt,
                          strlen( test_ca_crt ) );
    if( ret != 0 )
    {
        if( verbose != 0 )
            printf( "failed\n" );

        return( ret );
    }

    if( verbose != 0 )
        printf( "passed\n  X.509 signature verify: ");

    ret = x509_crt_verify( &clicert, &cacert, NULL, NULL, &flags, NULL, NULL );
    if( ret != 0 )
    {
        if( verbose != 0 )
            printf( "failed\n" );

        printf("ret = %d, &flags = %04x\n", ret, flags);

        return( ret );
    }

    if( verbose != 0 )
        printf( "passed\n\n");

    x509_crt_free( &cacert  );
    x509_crt_free( &clicert );

    return( 0 );
#else
    ((void) verbose);
    return( POLARSSL_ERR_X509_FEATURE_UNAVAILABLE );
#endif
}

#endif

#endif /* POLARSSL_X509_USE_C */
