/**
 * \file pkcs#5.h
 *
 * \brief PKCS#5 functions
 *
 * \author Mathias Olsson <mathias@kompetensum.com>
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
#ifndef POLARSSL_PKCS5_H
#define POLARSSL_PKCS5_H

#include <string.h>

#include "asn1.h"
#include "md.h"

#ifdef _MSC_VER
#include <basetsd.h>
typedef UINT32 uint32_t;
#else
#include <inttypes.h>
#endif

#define POLARSSL_ERR_PKCS5_BAD_INPUT_DATA                  -0x3f80  /**< Bad input parameters to function. */
#define POLARSSL_ERR_PKCS5_INVALID_FORMAT                  -0x3f00  /**< Unexpected ASN.1 data. */
#define POLARSSL_ERR_PKCS5_FEATURE_UNAVAILABLE             -0x3e80  /**< Requested encryption or digest alg not available. */
#define POLARSSL_ERR_PKCS5_PASSWORD_MISMATCH               -0x3e00  /**< Given private key password does not allow for correct decryption. */

#define PKCS5_DECRYPT      0
#define PKCS5_ENCRYPT      1

/*
 * PKCS#5 OIDs
 */
#define OID_PKCS5               "\x2a\x86\x48\x86\xf7\x0d\x01\x05"
#define OID_PKCS5_PBES2         OID_PKCS5 "\x0d"
#define OID_PKCS5_PBKDF2        OID_PKCS5 "\x0c"

/*
 * Encryption Algorithm OIDs
 */
#define OID_DES_CBC             "\x2b\x0e\x03\x02\x07"
#define OID_DES_EDE3_CBC        "\x2a\x86\x48\x86\xf7\x0d\x03\x07"

/*
 * Digest Algorithm OIDs
 */
#define OID_HMAC_SHA1           "\x2a\x86\x48\x86\xf7\x0d\x02\x07"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief          PKCS#5 PBES2 function
 *
 * \param pbe_params the ASN.1 algorithm parameters
 * \param mode       either PKCS5_DECRYPT or PKCS5_ENCRYPT
 * \param pwd        password to use when generating key
 * \param plen       length of password
 * \param data       data to process
 * \param datalen    length of data
 * \param output     output buffer
 *
 * \returns        0 on success, or a PolarSSL error code if verification fails.
 */
int pkcs5_pbes2( asn1_buf *pbe_params, int mode,
                 const unsigned char *pwd,  size_t pwdlen,
                 const unsigned char *data, size_t datalen,
                 unsigned char *output );

/**
 * \brief          PKCS#5 PBKDF2 using HMAC
 *
 * \param ctx      Generic HMAC context
 * \param password Password to use when generating key
 * \param plen     Length of password
 * \param salt     Salt to use when generating key
 * \param slen     Length of salt
 * \param iteration_count       Iteration count
 * \param key_length            Length of generated key
 * \param output   Generated key. Must be at least as big as key_length
 *
 * \returns        0 on success, or a PolarSSL error code if verification fails.
 */
int pkcs5_pbkdf2_hmac( md_context_t *ctx, const unsigned char *password,
                       size_t plen, const unsigned char *salt, size_t slen,
                       unsigned int iteration_count,
                       uint32_t key_length, unsigned char *output );

/**
 * \brief          Checkup routine
 *
 * \return         0 if successful, or 1 if the test failed
 */
int pkcs5_self_test( int verbose );

#ifdef __cplusplus
}
#endif

#endif /* pkcs5.h */
