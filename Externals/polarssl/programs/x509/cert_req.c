/*
 *  Certificate request generation
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
#include <stdlib.h>
#include <stdio.h>

#include "polarssl/config.h"

#include "polarssl/error.h"
#include "polarssl/rsa.h"
#include "polarssl/x509.h"
#include "polarssl/base64.h"
#include "polarssl/x509write.h"

#define DFL_FILENAME            "keyfile.key"
#define DFL_DEBUG_LEVEL         0
#define DFL_OUTPUT_FILENAME     "cert.req"
#define DFL_SUBJECT_NAME        "CN=Cert,O=PolarSSL,C=NL"

/*
 * global options
 */
struct options
{
    char *filename;             /* filename of the key file             */
    int debug_level;            /* level of debugging                   */
    char *output_file;          /* where to store the constructed key file  */
    char *subject_name;         /* subject name for certificate request */
} opt;

void my_debug( void *ctx, int level, const char *str )
{
    if( level < opt.debug_level )
    {
        fprintf( (FILE *) ctx, "%s", str );
        fflush(  (FILE *) ctx  );
    }
}

void write_certificate_request( rsa_context *rsa, x509_req_name *req_name,
                                char *output_file )
{
    FILE *f;
    unsigned char output_buf[4096];
    unsigned char base_buf[4096];
    unsigned char *c;
    int ret;
    size_t len = 0, olen = 4096;

    memset(output_buf, 0, 4096);
    ret = x509_write_cert_req( output_buf, 4096, rsa, req_name, SIG_RSA_SHA1 );

    if( ret < 0 )
        return;

    len = ret;
    c = output_buf + 4095 - len;

    base64_encode( base_buf, &olen, c, len );

    c = base_buf;

    f = fopen( output_file, "w" );
    fprintf(f, "-----BEGIN CERTIFICATE REQUEST-----\n");
    while (olen)
    {
        int use_len = olen;
        if (use_len > 64) use_len = 64;
        fwrite( c, 1, use_len, f );
        olen -= use_len;
        c += use_len;
        fprintf(f, "\n");
    }
    fprintf(f, "-----END CERTIFICATE REQUEST-----\n");
    fclose(f);
}

#define USAGE \
    "\n usage: key_app param=<>...\n"                   \
    "\n acceptable parameters:\n"                       \
    "    filename=%%s         default: keyfile.key\n"   \
    "    debug_level=%%d      default: 0 (disabled)\n"  \
    "    output_file=%%s      default: cert.req\n"      \
    "    subject_name=%%s     default: CN=Cert,O=PolarSSL,C=NL\n"   \
    "\n"

#if !defined(POLARSSL_BIGNUM_C) || !defined(POLARSSL_RSA_C) ||         \
    !defined(POLARSSL_X509_PARSE_C) || !defined(POLARSSL_FS_IO)
int main( int argc, char *argv[] )
{
    ((void) argc);
    ((void) argv);

    printf("POLARSSL_BIGNUM_C and/or POLARSSL_RSA_C and/or "
           "POLARSSL_X509_PARSE_C and/or POLARSSL_FS_IO not defined.\n");
    return( 0 );
}
#else
int main( int argc, char *argv[] )
{
    int ret = 0;
    rsa_context rsa;
    char buf[1024];
    int i, j, n;
    char *p, *q;
    char *s, *c, *end;
    int in_tag;
    char *oid = NULL;
    x509_req_name *req_name = NULL;
    x509_req_name *cur = req_name;

    /*
     * Set to sane values
     */
    memset( &rsa, 0, sizeof( rsa_context ) );
    memset( buf, 0, 1024 );

    if( argc == 0 )
    {
    usage:
        printf( USAGE );
        goto exit;
    }

    opt.filename            = DFL_FILENAME;
    opt.debug_level         = DFL_DEBUG_LEVEL;
    opt.output_file         = DFL_OUTPUT_FILENAME;
    opt.subject_name        = DFL_SUBJECT_NAME;

    for( i = 1; i < argc; i++ )
    {

        p = argv[i];
        if( ( q = strchr( p, '=' ) ) == NULL )
            goto usage;
        *q++ = '\0';

        n = strlen( p );
        for( j = 0; j < n; j++ )
        {
            if( argv[i][j] >= 'A' && argv[i][j] <= 'Z' )
                argv[i][j] |= 0x20;
        }

        if( strcmp( p, "filename" ) == 0 )
            opt.filename = q;
        else if( strcmp( p, "output_file" ) == 0 )
            opt.output_file = q;
        else if( strcmp( p, "debug_level" ) == 0 )
        {
            opt.debug_level = atoi( q );
            if( opt.debug_level < 0 || opt.debug_level > 65535 )
                goto usage;
        }
        else if( strcmp( p, "subject_name" ) == 0 )
        {
            opt.subject_name = q;
        }
        else
            goto usage;
    }

    /*
     * 1.0. Check the subject name for validity
     */
    s = opt.subject_name;
    end = s + strlen( s );

    c = s;

    in_tag = 1;
    while( c <= end )
    {
        if( in_tag && *c == '=' )
        {
            if( memcmp( s, "CN", 2 ) == 0 && c - s == 2 )
                oid = OID_CN;
            else if( memcmp( s, "C", 1 ) == 0 && c - s == 1 )
                oid = OID_COUNTRY;
            else if( memcmp( s, "O", 1 ) == 0 && c - s == 1 )
                oid = OID_ORGANIZATION;
            else if( memcmp( s, "L", 1 ) == 0 && c - s == 1 )
                oid = OID_LOCALITY;
            else if( memcmp( s, "R", 1 ) == 0 && c - s == 1 )
                oid = OID_PKCS9_EMAIL;
            else if( memcmp( s, "OU", 2 ) == 0 && c - s == 2 )
                oid = OID_ORG_UNIT;
            else if( memcmp( s, "ST", 2 ) == 0 && c - s == 2 )
                oid = OID_STATE;
            else
            {
                printf("Failed to parse subject name.\n");
                goto exit;
            }

            s = c + 1;
            in_tag = 0;
        }
        
        if( !in_tag && ( *c == ',' || c == end ) )
        {
            if( c - s > 127 )
            {
                printf("Name too large for buffer.\n");
                goto exit;
            }

            if( cur == NULL )
            {
                req_name = malloc( sizeof(x509_req_name) );
                cur = req_name;
            }
            else
            {
                cur->next = malloc( sizeof(x509_req_name) );
                cur = cur->next;
            }

            if( cur == NULL )
            {
                printf( "Failed to allocate memory.\n" );
                goto exit;
            }

            memset( cur, 0, sizeof(x509_req_name) );

            strncpy( cur->oid, oid, strlen( oid ) );
            strncpy( cur->name, s, c - s );

            s = c + 1;
            in_tag = 1;
        }
        c++;
    }

    /*
     * 1.1. Load the key
     */
    printf( "\n  . Loading the private key ..." );
    fflush( stdout );

    ret = x509parse_keyfile( &rsa, opt.filename, NULL );

    if( ret != 0 )
    {
#ifdef POLARSSL_ERROR_C
        error_strerror( ret, buf, 1024 );
#endif
        printf( " failed\n  !  x509parse_key returned %d - %s\n\n", ret, buf );
        rsa_free( &rsa );
        goto exit;
    }

    printf( " ok\n" );

    write_certificate_request( &rsa, req_name, opt.output_file );

exit:

    rsa_free( &rsa );

#if defined(_WIN32)
    printf( "  + Press Enter to exit this program.\n" );
    fflush( stdout ); getchar();
#endif

    return( ret );
}
#endif /* POLARSSL_BIGNUM_C && POLARSSL_RSA_C &&
          POLARSSL_X509_PARSE_C && POLARSSL_FS_IO */
