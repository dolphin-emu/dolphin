/**
 * \file gcm.h
 *
 * \brief Galois/Counter mode for AES
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
#ifndef POLARSSL_GCM_H
#define POLARSSL_GCM_H

#include "aes.h"

#ifdef _MSC_VER
#include <basetsd.h>
typedef UINT64 uint64_t;
#else
#include <stdint.h>
#endif

#define GCM_ENCRYPT     1
#define GCM_DECRYPT     0

#define POLARSSL_ERR_GCM_AUTH_FAILED                       -0x0012  /**< Authenticated decryption failed. */
#define POLARSSL_ERR_GCM_BAD_INPUT                         -0x0014  /**< Bad input parameters to function. */

/**
 * \brief          GCM context structure
 */
typedef struct {
    aes_context aes_ctx;        /*!< AES context used */
    uint64_t HL[16];            /*!< Precalculated HTable */
    uint64_t HH[16];            /*!< Precalculated HTable */
}
gcm_context;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief           GCM initialization (encryption)
 *
 * \param ctx       GCM context to be initialized
 * \param key       encryption key
 * \param keysize   must be 128, 192 or 256
 *
 * \return          0 if successful, or POLARSSL_ERR_AES_INVALID_KEY_LENGTH
 */
int gcm_init( gcm_context *ctx, const unsigned char *key, unsigned int keysize );

/**
 * \brief           GCM buffer encryption/decryption using AES
 *
 * \note On encryption, the output buffer can be the same as the input buffer.
 *       On decryption, the output buffer cannot be the same as input buffer.
 *       If buffers overlap, the output buffer must trail at least 8 bytes
 *       behind the input buffer.
 *
 * \param ctx       GCM context
 * \param mode      GCM_ENCRYPT or GCM_DECRYPT
 * \param length    length of the input data
 * \param iv        initialization vector
 * \param iv_len    length of IV
 * \param add       additional data
 * \param add_len   length of additional data
 * \param input     buffer holding the input data
 * \param output    buffer for holding the output data
 * \param tag_len   length of the tag to generate
 * \param tag       buffer for holding the tag
 *
 * \return         0 if successful
 */
int gcm_crypt_and_tag( gcm_context *ctx,
                       int mode,
                       size_t length,
                       const unsigned char *iv,
                       size_t iv_len,
                       const unsigned char *add,
                       size_t add_len,
                       const unsigned char *input,
                       unsigned char *output,
                       size_t tag_len,
                       unsigned char *tag );

/**
 * \brief           GCM buffer authenticated decryption using AES
 *
 * \note On decryption, the output buffer cannot be the same as input buffer.
 *       If buffers overlap, the output buffer must trail at least 8 bytes
 *       behind the input buffer.
 *
 * \param ctx       GCM context
 * \param length    length of the input data
 * \param iv        initialization vector
 * \param iv_len    length of IV
 * \param add       additional data
 * \param add_len   length of additional data
 * \param tag       buffer holding the tag
 * \param tag_len   length of the tag 
 * \param input     buffer holding the input data
 * \param output    buffer for holding the output data
 *
 * \return         0 if successful and authenticated,
 *                 POLARSSL_ERR_GCM_AUTH_FAILED if tag does not match
 */
int gcm_auth_decrypt( gcm_context *ctx,
                      size_t length,
                      const unsigned char *iv,
                      size_t iv_len,
                      const unsigned char *add,
                      size_t add_len,
                      const unsigned char *tag, 
                      size_t tag_len,
                      const unsigned char *input,
                      unsigned char *output );

/**
 * \brief          Checkup routine
 *
 * \return         0 if successful, or 1 if the test failed
 */
int gcm_self_test( int verbose );

#ifdef __cplusplus
}
#endif

#endif /* gcm.h */
