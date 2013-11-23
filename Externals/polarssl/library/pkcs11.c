/**
 * \file pkcs11.c
 *
 * \brief Wrapper for PKCS#11 library libpkcs11-helper
 *
 * \author Adriaan de Jong <dejong@fox-it.com>
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

#include "polarssl/pkcs11.h"

#if defined(POLARSSL_PKCS11_C)

#include <stdlib.h>

int pkcs11_x509_cert_init( x509_cert *cert, pkcs11h_certificate_t pkcs11_cert )
{
    int ret = 1;
    unsigned char *cert_blob = NULL;
    size_t cert_blob_size = 0;

    if( cert == NULL )
    {
        ret = 2;
        goto cleanup;
    }

    if( pkcs11h_certificate_getCertificateBlob( pkcs11_cert, NULL, &cert_blob_size ) != CKR_OK )
    {
        ret = 3;
        goto cleanup;
    }

    cert_blob = malloc( cert_blob_size );
    if( NULL == cert_blob )
    {
        ret = 4;
        goto cleanup;
    }

    if( pkcs11h_certificate_getCertificateBlob( pkcs11_cert, cert_blob, &cert_blob_size ) != CKR_OK )
    {
        ret = 5;
        goto cleanup;
    }

    if( 0 != x509parse_crt(cert, cert_blob, cert_blob_size ) )
    {
        ret = 6;
        goto cleanup;
    }

    ret = 0;

cleanup:
    if( NULL != cert_blob )
        free( cert_blob );

    return ret;
}


int pkcs11_priv_key_init( pkcs11_context *priv_key,
        pkcs11h_certificate_t pkcs11_cert )
{
    int ret = 1;
    x509_cert cert;

    memset( &cert, 0, sizeof( cert ) );

    if( priv_key == NULL )
        goto cleanup;

    if( 0 != pkcs11_x509_cert_init( &cert, pkcs11_cert ) )
        goto cleanup;

    priv_key->len = cert.rsa.len;
    priv_key->pkcs11h_cert = pkcs11_cert;

    ret = 0;

cleanup:
    x509_free( &cert );

    return ret;
}

void pkcs11_priv_key_free( pkcs11_context *priv_key )
{
    if( NULL != priv_key )
        pkcs11h_certificate_freeCertificate( priv_key->pkcs11h_cert );
}

int pkcs11_decrypt( pkcs11_context *ctx,
                       int mode, size_t *olen,
                       const unsigned char *input,
                       unsigned char *output,
                       size_t output_max_len )
{
    size_t input_len, output_len;

    if( NULL == ctx )
        return( POLARSSL_ERR_RSA_BAD_INPUT_DATA );

    if( RSA_PUBLIC == mode )
        return( POLARSSL_ERR_RSA_BAD_INPUT_DATA );

    output_len = input_len = ctx->len;

    if( input_len < 16 || input_len > output_max_len )
        return( POLARSSL_ERR_RSA_BAD_INPUT_DATA );

    /* Determine size of output buffer */
    if( pkcs11h_certificate_decryptAny( ctx->pkcs11h_cert, CKM_RSA_PKCS, input,
            input_len, NULL, &output_len ) != CKR_OK )
    {
        return( POLARSSL_ERR_RSA_BAD_INPUT_DATA );
    }

    if( output_len > output_max_len )
        return( POLARSSL_ERR_RSA_OUTPUT_TOO_LARGE );

    if( pkcs11h_certificate_decryptAny( ctx->pkcs11h_cert, CKM_RSA_PKCS, input,
            input_len, output, &output_len ) != CKR_OK )
    {
        return( POLARSSL_ERR_RSA_BAD_INPUT_DATA );
    }
    *olen = output_len;
    return( 0 );
}

int pkcs11_sign( pkcs11_context *ctx,
                    int mode,
                    int hash_id,
                    unsigned int hashlen,
                    const unsigned char *hash,
                    unsigned char *sig )
{
    size_t olen, asn_len;
    unsigned char *p = sig;

    if( NULL == ctx )
        return POLARSSL_ERR_RSA_BAD_INPUT_DATA;

    if( RSA_PUBLIC == mode )
        return POLARSSL_ERR_RSA_BAD_INPUT_DATA;

    olen = ctx->len;

    switch( hash_id )
    {
        case SIG_RSA_RAW:
            asn_len = 0;
            memcpy( p, hash, hashlen );
            break;

        case SIG_RSA_MD2:
            asn_len = OID_SIZE(ASN1_HASH_MDX);
            memcpy( p, ASN1_HASH_MDX, asn_len );
            memcpy( p + asn_len, hash, hashlen );
            p[13] = 2; break;

        case SIG_RSA_MD4:
            asn_len = OID_SIZE(ASN1_HASH_MDX);
            memcpy( p, ASN1_HASH_MDX, asn_len );
            memcpy( p + asn_len, hash, hashlen );
            p[13] = 4; break;

        case SIG_RSA_MD5:
            asn_len = OID_SIZE(ASN1_HASH_MDX);
            memcpy( p, ASN1_HASH_MDX, asn_len );
            memcpy( p + asn_len, hash, hashlen );
            p[13] = 5; break;

        case SIG_RSA_SHA1:
            asn_len = OID_SIZE(ASN1_HASH_SHA1);
            memcpy( p, ASN1_HASH_SHA1, asn_len );
            memcpy( p + 15, hash, hashlen );
            break;

        case SIG_RSA_SHA224:
            asn_len = OID_SIZE(ASN1_HASH_SHA2X);
            memcpy( p, ASN1_HASH_SHA2X, asn_len );
            memcpy( p + asn_len, hash, hashlen );
            p[1] += hashlen; p[14] = 4; p[18] += hashlen; break;

        case SIG_RSA_SHA256:
            asn_len = OID_SIZE(ASN1_HASH_SHA2X);
            memcpy( p, ASN1_HASH_SHA2X, asn_len );
            memcpy( p + asn_len, hash, hashlen );
            p[1] += hashlen; p[14] = 1; p[18] += hashlen; break;

        case SIG_RSA_SHA384:
            asn_len = OID_SIZE(ASN1_HASH_SHA2X);
            memcpy( p, ASN1_HASH_SHA2X, asn_len );
            memcpy( p + asn_len, hash, hashlen );
            p[1] += hashlen; p[14] = 2; p[18] += hashlen; break;

        case SIG_RSA_SHA512:
            asn_len = OID_SIZE(ASN1_HASH_SHA2X);
            memcpy( p, ASN1_HASH_SHA2X, asn_len );
            memcpy( p + asn_len, hash, hashlen );
            p[1] += hashlen; p[14] = 3; p[18] += hashlen; break;

        default:
            return( POLARSSL_ERR_RSA_BAD_INPUT_DATA );
    }

    if( pkcs11h_certificate_signAny( ctx->pkcs11h_cert, CKM_RSA_PKCS, sig,
            asn_len + hashlen, sig, &olen ) != CKR_OK )
    {
        return( POLARSSL_ERR_RSA_BAD_INPUT_DATA );
    }

    return( 0 );
}

#endif /* defined(POLARSSL_PKCS11_C) */
