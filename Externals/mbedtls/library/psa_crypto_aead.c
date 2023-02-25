/*
 *  PSA AEAD entry points
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include "common.h"

#if defined(MBEDTLS_PSA_CRYPTO_C)

#include "psa_crypto_aead.h"
#include "psa_crypto_core.h"

#include "mbedtls/ccm.h"
#include "mbedtls/chachapoly.h"
#include "mbedtls/cipher.h"
#include "mbedtls/gcm.h"

typedef struct
{
    psa_algorithm_t core_alg;
    uint8_t tag_length;
    union
    {
        unsigned dummy; /* Make the union non-empty even with no supported algorithms. */
#if defined(MBEDTLS_PSA_BUILTIN_ALG_CCM)
        mbedtls_ccm_context ccm;
#endif /* MBEDTLS_PSA_BUILTIN_ALG_CCM */
#if defined(MBEDTLS_PSA_BUILTIN_ALG_GCM)
        mbedtls_gcm_context gcm;
#endif /* MBEDTLS_PSA_BUILTIN_ALG_GCM */
#if defined(MBEDTLS_PSA_BUILTIN_ALG_CHACHA20_POLY1305)
        mbedtls_chachapoly_context chachapoly;
#endif /* MBEDTLS_PSA_BUILTIN_ALG_CHACHA20_POLY1305 */
    } ctx;
} aead_operation_t;

#define AEAD_OPERATION_INIT {0, 0, {0}}

static void psa_aead_abort_internal( aead_operation_t *operation )
{
    switch( operation->core_alg )
    {
#if defined(MBEDTLS_PSA_BUILTIN_ALG_CCM)
        case PSA_ALG_CCM:
            mbedtls_ccm_free( &operation->ctx.ccm );
            break;
#endif /* MBEDTLS_PSA_BUILTIN_ALG_CCM */
#if defined(MBEDTLS_PSA_BUILTIN_ALG_GCM)
        case PSA_ALG_GCM:
            mbedtls_gcm_free( &operation->ctx.gcm );
            break;
#endif /* MBEDTLS_PSA_BUILTIN_ALG_GCM */
#if defined(MBEDTLS_PSA_BUILTIN_ALG_CHACHA20_POLY1305)
        case PSA_ALG_CHACHA20_POLY1305:
            mbedtls_chachapoly_free( &operation->ctx.chachapoly );
            break;
#endif /* MBEDTLS_PSA_BUILTIN_ALG_CHACHA20_POLY1305 */
    }
}

static psa_status_t psa_aead_setup(
    aead_operation_t *operation,
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer,
    psa_algorithm_t alg )
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    size_t key_bits;
    const mbedtls_cipher_info_t *cipher_info;
    mbedtls_cipher_id_t cipher_id;
    size_t full_tag_length = 0;

    key_bits = attributes->core.bits;

    cipher_info = mbedtls_cipher_info_from_psa( alg,
                                                attributes->core.type, key_bits,
                                                &cipher_id );
    if( cipher_info == NULL )
        return( PSA_ERROR_NOT_SUPPORTED );

    switch( PSA_ALG_AEAD_WITH_SHORTENED_TAG( alg, 0 ) )
    {
#if defined(MBEDTLS_PSA_BUILTIN_ALG_CCM)
        case PSA_ALG_AEAD_WITH_SHORTENED_TAG( PSA_ALG_CCM, 0 ):
            operation->core_alg = PSA_ALG_CCM;
            full_tag_length = 16;
            /* CCM allows the following tag lengths: 4, 6, 8, 10, 12, 14, 16.
             * The call to mbedtls_ccm_encrypt_and_tag or
             * mbedtls_ccm_auth_decrypt will validate the tag length. */
            if( PSA_BLOCK_CIPHER_BLOCK_LENGTH( attributes->core.type ) != 16 )
                return( PSA_ERROR_INVALID_ARGUMENT );

            mbedtls_ccm_init( &operation->ctx.ccm );
            status = mbedtls_to_psa_error(
                mbedtls_ccm_setkey( &operation->ctx.ccm, cipher_id,
                                    key_buffer, (unsigned int) key_bits ) );
            if( status != PSA_SUCCESS )
                return( status );
            break;
#endif /* MBEDTLS_PSA_BUILTIN_ALG_CCM */

#if defined(MBEDTLS_PSA_BUILTIN_ALG_GCM)
        case PSA_ALG_AEAD_WITH_SHORTENED_TAG( PSA_ALG_GCM, 0 ):
            operation->core_alg = PSA_ALG_GCM;
            full_tag_length = 16;
            /* GCM allows the following tag lengths: 4, 8, 12, 13, 14, 15, 16.
             * The call to mbedtls_gcm_crypt_and_tag or
             * mbedtls_gcm_auth_decrypt will validate the tag length. */
            if( PSA_BLOCK_CIPHER_BLOCK_LENGTH( attributes->core.type ) != 16 )
                return( PSA_ERROR_INVALID_ARGUMENT );

            mbedtls_gcm_init( &operation->ctx.gcm );
            status = mbedtls_to_psa_error(
                mbedtls_gcm_setkey( &operation->ctx.gcm, cipher_id,
                                    key_buffer, (unsigned int) key_bits ) );
            if( status != PSA_SUCCESS )
                return( status );
            break;
#endif /* MBEDTLS_PSA_BUILTIN_ALG_GCM */

#if defined(MBEDTLS_PSA_BUILTIN_ALG_CHACHA20_POLY1305)
        case PSA_ALG_AEAD_WITH_SHORTENED_TAG( PSA_ALG_CHACHA20_POLY1305, 0 ):
            operation->core_alg = PSA_ALG_CHACHA20_POLY1305;
            full_tag_length = 16;
            /* We only support the default tag length. */
            if( alg != PSA_ALG_CHACHA20_POLY1305 )
                return( PSA_ERROR_NOT_SUPPORTED );

            mbedtls_chachapoly_init( &operation->ctx.chachapoly );
            status = mbedtls_to_psa_error(
                mbedtls_chachapoly_setkey( &operation->ctx.chachapoly,
                                           key_buffer ) );
            if( status != PSA_SUCCESS )
                return( status );
            break;
#endif /* MBEDTLS_PSA_BUILTIN_ALG_CHACHA20_POLY1305 */

        default:
            (void) status;
            (void) key_buffer;
            return( PSA_ERROR_NOT_SUPPORTED );
    }

    if( PSA_AEAD_TAG_LENGTH( attributes->core.type,
                             key_bits, alg )
        > full_tag_length )
        return( PSA_ERROR_INVALID_ARGUMENT );

    operation->tag_length = PSA_AEAD_TAG_LENGTH( attributes->core.type,
                                                 key_bits,
                                                 alg );

    return( PSA_SUCCESS );
}

psa_status_t mbedtls_psa_aead_encrypt(
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer, size_t key_buffer_size,
    psa_algorithm_t alg,
    const uint8_t *nonce, size_t nonce_length,
    const uint8_t *additional_data, size_t additional_data_length,
    const uint8_t *plaintext, size_t plaintext_length,
    uint8_t *ciphertext, size_t ciphertext_size, size_t *ciphertext_length )
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    aead_operation_t operation = AEAD_OPERATION_INIT;
    uint8_t *tag;
    (void) key_buffer_size;

    status = psa_aead_setup( &operation, attributes, key_buffer, alg );
    if( status != PSA_SUCCESS )
        goto exit;

    /* For all currently supported modes, the tag is at the end of the
     * ciphertext. */
    if( ciphertext_size < ( plaintext_length + operation.tag_length ) )
    {
        status = PSA_ERROR_BUFFER_TOO_SMALL;
        goto exit;
    }
    tag = ciphertext + plaintext_length;

#if defined(MBEDTLS_PSA_BUILTIN_ALG_CCM)
    if( operation.core_alg == PSA_ALG_CCM )
    {
        status = mbedtls_to_psa_error(
            mbedtls_ccm_encrypt_and_tag( &operation.ctx.ccm,
                                         plaintext_length,
                                         nonce, nonce_length,
                                         additional_data,
                                         additional_data_length,
                                         plaintext, ciphertext,
                                         tag, operation.tag_length ) );
    }
    else
#endif /* MBEDTLS_PSA_BUILTIN_ALG_CCM */
#if defined(MBEDTLS_PSA_BUILTIN_ALG_GCM)
    if( operation.core_alg == PSA_ALG_GCM )
    {
        status = mbedtls_to_psa_error(
            mbedtls_gcm_crypt_and_tag( &operation.ctx.gcm,
                                       MBEDTLS_GCM_ENCRYPT,
                                       plaintext_length,
                                       nonce, nonce_length,
                                       additional_data, additional_data_length,
                                       plaintext, ciphertext,
                                       operation.tag_length, tag ) );
    }
    else
#endif /* MBEDTLS_PSA_BUILTIN_ALG_GCM */
#if defined(MBEDTLS_PSA_BUILTIN_ALG_CHACHA20_POLY1305)
    if( operation.core_alg == PSA_ALG_CHACHA20_POLY1305 )
    {
        if( nonce_length != 12 )
        {
            if( nonce_length == 8 )
                status = PSA_ERROR_NOT_SUPPORTED;
            else
                status = PSA_ERROR_INVALID_ARGUMENT;
            goto exit;
        }

        if( operation.tag_length != 16 )
        {
            status = PSA_ERROR_NOT_SUPPORTED;
            goto exit;
        }
        status = mbedtls_to_psa_error(
            mbedtls_chachapoly_encrypt_and_tag( &operation.ctx.chachapoly,
                                                plaintext_length,
                                                nonce,
                                                additional_data,
                                                additional_data_length,
                                                plaintext,
                                                ciphertext,
                                                tag ) );
    }
    else
#endif /* MBEDTLS_PSA_BUILTIN_ALG_CHACHA20_POLY1305 */
    {
        (void) tag;
        (void) nonce;
        (void) nonce_length;
        (void) additional_data;
        (void) additional_data_length;
        (void) plaintext;
        return( PSA_ERROR_NOT_SUPPORTED );
    }

    if( status == PSA_SUCCESS )
        *ciphertext_length = plaintext_length + operation.tag_length;

exit:
    psa_aead_abort_internal( &operation );

    return( status );
}

/* Locate the tag in a ciphertext buffer containing the encrypted data
 * followed by the tag. Return the length of the part preceding the tag in
 * *plaintext_length. This is the size of the plaintext in modes where
 * the encrypted data has the same size as the plaintext, such as
 * CCM and GCM. */
static psa_status_t psa_aead_unpadded_locate_tag( size_t tag_length,
                                                  const uint8_t *ciphertext,
                                                  size_t ciphertext_length,
                                                  size_t plaintext_size,
                                                  const uint8_t **p_tag )
{
    size_t payload_length;
    if( tag_length > ciphertext_length )
        return( PSA_ERROR_INVALID_ARGUMENT );
    payload_length = ciphertext_length - tag_length;
    if( payload_length > plaintext_size )
        return( PSA_ERROR_BUFFER_TOO_SMALL );
    *p_tag = ciphertext + payload_length;
    return( PSA_SUCCESS );
}

psa_status_t mbedtls_psa_aead_decrypt(
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer, size_t key_buffer_size,
    psa_algorithm_t alg,
    const uint8_t *nonce, size_t nonce_length,
    const uint8_t *additional_data, size_t additional_data_length,
    const uint8_t *ciphertext, size_t ciphertext_length,
    uint8_t *plaintext, size_t plaintext_size, size_t *plaintext_length )
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    aead_operation_t operation = AEAD_OPERATION_INIT;
    const uint8_t *tag = NULL;
    (void) key_buffer_size;

    status = psa_aead_setup( &operation, attributes, key_buffer, alg );
    if( status != PSA_SUCCESS )
        goto exit;

    status = psa_aead_unpadded_locate_tag( operation.tag_length,
                                           ciphertext, ciphertext_length,
                                           plaintext_size, &tag );
    if( status != PSA_SUCCESS )
        goto exit;

#if defined(MBEDTLS_PSA_BUILTIN_ALG_CCM)
    if( operation.core_alg == PSA_ALG_CCM )
    {
        status = mbedtls_to_psa_error(
            mbedtls_ccm_auth_decrypt( &operation.ctx.ccm,
                                      ciphertext_length - operation.tag_length,
                                      nonce, nonce_length,
                                      additional_data,
                                      additional_data_length,
                                      ciphertext, plaintext,
                                      tag, operation.tag_length ) );
    }
    else
#endif /* MBEDTLS_PSA_BUILTIN_ALG_CCM */
#if defined(MBEDTLS_PSA_BUILTIN_ALG_GCM)
    if( operation.core_alg == PSA_ALG_GCM )
    {
        status = mbedtls_to_psa_error(
            mbedtls_gcm_auth_decrypt( &operation.ctx.gcm,
                                      ciphertext_length - operation.tag_length,
                                      nonce, nonce_length,
                                      additional_data,
                                      additional_data_length,
                                      tag, operation.tag_length,
                                      ciphertext, plaintext ) );
    }
    else
#endif /* MBEDTLS_PSA_BUILTIN_ALG_GCM */
#if defined(MBEDTLS_PSA_BUILTIN_ALG_CHACHA20_POLY1305)
    if( operation.core_alg == PSA_ALG_CHACHA20_POLY1305 )
    {
        if( nonce_length != 12 )
        {
            if( nonce_length == 8 )
                status = PSA_ERROR_NOT_SUPPORTED;
            else
                status = PSA_ERROR_INVALID_ARGUMENT;
            goto exit;
        }

        if( operation.tag_length != 16 )
        {
            status = PSA_ERROR_NOT_SUPPORTED;
            goto exit;
        }
        status = mbedtls_to_psa_error(
            mbedtls_chachapoly_auth_decrypt( &operation.ctx.chachapoly,
                                             ciphertext_length - operation.tag_length,
                                             nonce,
                                             additional_data,
                                             additional_data_length,
                                             tag,
                                             ciphertext,
                                             plaintext ) );
    }
    else
#endif /* MBEDTLS_PSA_BUILTIN_ALG_CHACHA20_POLY1305 */
    {
        (void) nonce;
        (void) nonce_length;
        (void) additional_data;
        (void) additional_data_length;
        (void) plaintext;
        return( PSA_ERROR_NOT_SUPPORTED );
    }

    if( status == PSA_SUCCESS )
        *plaintext_length = ciphertext_length - operation.tag_length;

exit:
    psa_aead_abort_internal( &operation );

    if( status == PSA_SUCCESS )
        *plaintext_length = ciphertext_length - operation.tag_length;
    return( status );
}

#endif /* MBEDTLS_PSA_CRYPTO_C */

