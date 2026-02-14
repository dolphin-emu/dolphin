/*
 *  PSA crypto layer on top of Mbed TLS crypto
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#include "common.h"

#if defined(MBEDTLS_PSA_CRYPTO_C)

#if defined(MBEDTLS_PSA_CRYPTO_CONFIG)
#include "check_crypto_config.h"
#endif

#include "psa/crypto.h"

#include "psa_crypto_cipher.h"
#include "psa_crypto_core.h"
#include "psa_crypto_invasive.h"
#include "psa_crypto_driver_wrappers.h"
#include "psa_crypto_ecp.h"
#include "psa_crypto_hash.h"
#include "psa_crypto_mac.h"
#include "psa_crypto_rsa.h"
#include "psa_crypto_ecp.h"
#if defined(MBEDTLS_PSA_CRYPTO_SE_C)
#include "psa_crypto_se.h"
#endif
#include "psa_crypto_slot_management.h"
/* Include internal declarations that are useful for implementing persistently
 * stored keys. */
#include "psa_crypto_storage.h"

#include "psa_crypto_random_impl.h"

#include <stdlib.h>
#include <string.h>
#include "mbedtls/platform.h"

#include "mbedtls/aes.h"
#include "mbedtls/arc4.h"
#include "mbedtls/asn1.h"
#include "mbedtls/asn1write.h"
#include "mbedtls/bignum.h"
#include "mbedtls/blowfish.h"
#include "mbedtls/camellia.h"
#include "mbedtls/chacha20.h"
#include "mbedtls/chachapoly.h"
#include "mbedtls/cipher.h"
#include "mbedtls/ccm.h"
#include "mbedtls/cmac.h"
#include "mbedtls/des.h"
#include "mbedtls/ecdh.h"
#include "mbedtls/ecp.h"
#include "mbedtls/entropy.h"
#include "mbedtls/error.h"
#include "mbedtls/gcm.h"
#include "mbedtls/md2.h"
#include "mbedtls/md4.h"
#include "mbedtls/md5.h"
#include "mbedtls/md.h"
#include "mbedtls/md_internal.h"
#include "mbedtls/pk.h"
#include "mbedtls/pk_internal.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/error.h"
#include "mbedtls/ripemd160.h"
#include "mbedtls/rsa.h"
#include "mbedtls/sha1.h"
#include "mbedtls/sha256.h"
#include "mbedtls/sha512.h"
#include "mbedtls/xtea.h"

#define ARRAY_LENGTH(array) (sizeof(array) / sizeof(*(array)))

/****************************************************************/
/* Global data, support functions and library management */
/****************************************************************/

static int key_type_is_raw_bytes(psa_key_type_t type)
{
    return PSA_KEY_TYPE_IS_UNSTRUCTURED(type);
}

/* Values for psa_global_data_t::rng_state */
#define RNG_NOT_INITIALIZED 0
#define RNG_INITIALIZED 1
#define RNG_SEEDED 2

typedef struct {
    unsigned initialized : 1;
    unsigned rng_state : 2;
    mbedtls_psa_random_context_t rng;
} psa_global_data_t;

static psa_global_data_t global_data;

#if !defined(MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG)
mbedtls_psa_drbg_context_t *const mbedtls_psa_random_state =
    &global_data.rng.drbg;
#endif

#define GUARD_MODULE_INITIALIZED        \
    if (global_data.initialized == 0)  \
    return PSA_ERROR_BAD_STATE;

#if !defined(MBEDTLS_PSA_ASSUME_EXCLUSIVE_BUFFERS)

/* Declare a local copy of an input buffer and a variable that will be used
 * to store a pointer to the start of the buffer.
 *
 * Note: This macro must be called before any operations which may jump to
 * the exit label, so that the local input copy object is safe to be freed.
 *
 * Assumptions:
 * - input is the name of a pointer to the buffer to be copied
 * - The name LOCAL_INPUT_COPY_OF_input is unused in the current scope
 * - input_copy_name is a name that is unused in the current scope
 */
#define LOCAL_INPUT_DECLARE(input, input_copy_name) \
    psa_crypto_local_input_t LOCAL_INPUT_COPY_OF_##input = PSA_CRYPTO_LOCAL_INPUT_INIT; \
    const uint8_t *input_copy_name = NULL;

/* Allocate a copy of the buffer input and set the pointer input_copy to
 * point to the start of the copy.
 *
 * Assumptions:
 * - psa_status_t status exists
 * - An exit label is declared
 * - input is the name of a pointer to the buffer to be copied
 * - LOCAL_INPUT_DECLARE(input, input_copy) has previously been called
 */
#define LOCAL_INPUT_ALLOC(input, length, input_copy) \
    status = psa_crypto_local_input_alloc(input, length, \
                                          &LOCAL_INPUT_COPY_OF_##input); \
    if (status != PSA_SUCCESS) { \
        goto exit; \
    } \
    input_copy = LOCAL_INPUT_COPY_OF_##input.buffer;

/* Free the local input copy allocated previously by LOCAL_INPUT_ALLOC()
 *
 * Assumptions:
 * - input_copy is the name of the input copy pointer set by LOCAL_INPUT_ALLOC()
 * - input is the name of the original buffer that was copied
 */
#define LOCAL_INPUT_FREE(input, input_copy) \
    input_copy = NULL; \
    psa_crypto_local_input_free(&LOCAL_INPUT_COPY_OF_##input);

/* Declare a local copy of an output buffer and a variable that will be used
 * to store a pointer to the start of the buffer.
 *
 * Note: This macro must be called before any operations which may jump to
 * the exit label, so that the local output copy object is safe to be freed.
 *
 * Assumptions:
 * - output is the name of a pointer to the buffer to be copied
 * - The name LOCAL_OUTPUT_COPY_OF_output is unused in the current scope
 * - output_copy_name is a name that is unused in the current scope
 */
#define LOCAL_OUTPUT_DECLARE(output, output_copy_name) \
    psa_crypto_local_output_t LOCAL_OUTPUT_COPY_OF_##output = PSA_CRYPTO_LOCAL_OUTPUT_INIT; \
    uint8_t *output_copy_name = NULL;

/* Allocate a copy of the buffer output and set the pointer output_copy to
 * point to the start of the copy.
 *
 * Assumptions:
 * - psa_status_t status exists
 * - An exit label is declared
 * - output is the name of a pointer to the buffer to be copied
 * - LOCAL_OUTPUT_DECLARE(output, output_copy) has previously been called
 */
#define LOCAL_OUTPUT_ALLOC(output, length, output_copy) \
    status = psa_crypto_local_output_alloc(output, length, \
                                           &LOCAL_OUTPUT_COPY_OF_##output); \
    if (status != PSA_SUCCESS) { \
        goto exit; \
    } \
    output_copy = LOCAL_OUTPUT_COPY_OF_##output.buffer;

/* Free the local output copy allocated previously by LOCAL_OUTPUT_ALLOC()
 * after first copying back its contents to the original buffer.
 *
 * Assumptions:
 * - psa_status_t status exists
 * - output_copy is the name of the output copy pointer set by LOCAL_OUTPUT_ALLOC()
 * - output is the name of the original buffer that was copied
 */
#define LOCAL_OUTPUT_FREE(output, output_copy) \
    output_copy = NULL; \
    do { \
        psa_status_t local_output_status; \
        local_output_status = psa_crypto_local_output_free(&LOCAL_OUTPUT_COPY_OF_##output); \
        if (local_output_status != PSA_SUCCESS) { \
            /* Since this error case is an internal error, it's more serious than \
             * any existing error code and so it's fine to overwrite the existing \
             * status. */ \
            status = local_output_status; \
        } \
    } while (0)
#else /* !MBEDTLS_PSA_ASSUME_EXCLUSIVE_BUFFERS */
#define LOCAL_INPUT_DECLARE(input, input_copy_name) \
    const uint8_t *input_copy_name = NULL;
#define LOCAL_INPUT_ALLOC(input, length, input_copy) \
    input_copy = input;
#define LOCAL_INPUT_FREE(input, input_copy) \
    input_copy = NULL;
#define LOCAL_OUTPUT_DECLARE(output, output_copy_name) \
    uint8_t *output_copy_name = NULL;
#define LOCAL_OUTPUT_ALLOC(output, length, output_copy) \
    output_copy = output;
#define LOCAL_OUTPUT_FREE(output, output_copy) \
    output_copy = NULL;
#endif /* !MBEDTLS_PSA_ASSUME_EXCLUSIVE_BUFFERS */

psa_status_t mbedtls_to_psa_error(int ret)
{
    /* Mbed TLS error codes can combine a high-level error code and a
     * low-level error code. The low-level error usually reflects the
     * root cause better, so dispatch on that preferably. */
    int low_level_ret = -(-ret & 0x007f);
    switch (low_level_ret != 0 ? low_level_ret : ret) {
        case 0:
            return PSA_SUCCESS;

        case MBEDTLS_ERR_AES_INVALID_KEY_LENGTH:
        case MBEDTLS_ERR_AES_INVALID_INPUT_LENGTH:
        case MBEDTLS_ERR_AES_FEATURE_UNAVAILABLE:
            return PSA_ERROR_NOT_SUPPORTED;
        case MBEDTLS_ERR_AES_HW_ACCEL_FAILED:
            return PSA_ERROR_HARDWARE_FAILURE;

        case MBEDTLS_ERR_ARC4_HW_ACCEL_FAILED:
            return PSA_ERROR_HARDWARE_FAILURE;

        case MBEDTLS_ERR_ASN1_OUT_OF_DATA:
        case MBEDTLS_ERR_ASN1_UNEXPECTED_TAG:
        case MBEDTLS_ERR_ASN1_INVALID_LENGTH:
        case MBEDTLS_ERR_ASN1_LENGTH_MISMATCH:
        case MBEDTLS_ERR_ASN1_INVALID_DATA:
            return PSA_ERROR_INVALID_ARGUMENT;
        case MBEDTLS_ERR_ASN1_ALLOC_FAILED:
            return PSA_ERROR_INSUFFICIENT_MEMORY;
        case MBEDTLS_ERR_ASN1_BUF_TOO_SMALL:
            return PSA_ERROR_BUFFER_TOO_SMALL;

#if defined(MBEDTLS_ERR_BLOWFISH_BAD_INPUT_DATA)
        case MBEDTLS_ERR_BLOWFISH_BAD_INPUT_DATA:
#elif defined(MBEDTLS_ERR_BLOWFISH_INVALID_KEY_LENGTH)
        case MBEDTLS_ERR_BLOWFISH_INVALID_KEY_LENGTH:
#endif
        case MBEDTLS_ERR_BLOWFISH_INVALID_INPUT_LENGTH:
            return PSA_ERROR_NOT_SUPPORTED;
        case MBEDTLS_ERR_BLOWFISH_HW_ACCEL_FAILED:
            return PSA_ERROR_HARDWARE_FAILURE;

#if defined(MBEDTLS_ERR_CAMELLIA_BAD_INPUT_DATA)
        case MBEDTLS_ERR_CAMELLIA_BAD_INPUT_DATA:
#elif defined(MBEDTLS_ERR_CAMELLIA_INVALID_KEY_LENGTH)
        case MBEDTLS_ERR_CAMELLIA_INVALID_KEY_LENGTH:
#endif
        case MBEDTLS_ERR_CAMELLIA_INVALID_INPUT_LENGTH:
            return PSA_ERROR_NOT_SUPPORTED;
        case MBEDTLS_ERR_CAMELLIA_HW_ACCEL_FAILED:
            return PSA_ERROR_HARDWARE_FAILURE;

        case MBEDTLS_ERR_CCM_BAD_INPUT:
            return PSA_ERROR_INVALID_ARGUMENT;
        case MBEDTLS_ERR_CCM_AUTH_FAILED:
            return PSA_ERROR_INVALID_SIGNATURE;
        case MBEDTLS_ERR_CCM_HW_ACCEL_FAILED:
            return PSA_ERROR_HARDWARE_FAILURE;

        case MBEDTLS_ERR_CHACHA20_BAD_INPUT_DATA:
            return PSA_ERROR_INVALID_ARGUMENT;

        case MBEDTLS_ERR_CHACHAPOLY_BAD_STATE:
            return PSA_ERROR_BAD_STATE;
        case MBEDTLS_ERR_CHACHAPOLY_AUTH_FAILED:
            return PSA_ERROR_INVALID_SIGNATURE;

        case MBEDTLS_ERR_CIPHER_FEATURE_UNAVAILABLE:
            return PSA_ERROR_NOT_SUPPORTED;
        case MBEDTLS_ERR_CIPHER_BAD_INPUT_DATA:
            return PSA_ERROR_INVALID_ARGUMENT;
        case MBEDTLS_ERR_CIPHER_ALLOC_FAILED:
            return PSA_ERROR_INSUFFICIENT_MEMORY;
        case MBEDTLS_ERR_CIPHER_INVALID_PADDING:
            return PSA_ERROR_INVALID_PADDING;
        case MBEDTLS_ERR_CIPHER_FULL_BLOCK_EXPECTED:
            return PSA_ERROR_INVALID_ARGUMENT;
        case MBEDTLS_ERR_CIPHER_AUTH_FAILED:
            return PSA_ERROR_INVALID_SIGNATURE;
        case MBEDTLS_ERR_CIPHER_INVALID_CONTEXT:
            return PSA_ERROR_CORRUPTION_DETECTED;
        case MBEDTLS_ERR_CIPHER_HW_ACCEL_FAILED:
            return PSA_ERROR_HARDWARE_FAILURE;

        case MBEDTLS_ERR_CMAC_HW_ACCEL_FAILED:
            return PSA_ERROR_HARDWARE_FAILURE;

#if !(defined(MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG) ||      \
            defined(MBEDTLS_PSA_HMAC_DRBG_MD_TYPE))
        /* Only check CTR_DRBG error codes if underlying mbedtls_xxx
         * functions are passed a CTR_DRBG instance. */
        case MBEDTLS_ERR_CTR_DRBG_ENTROPY_SOURCE_FAILED:
            return PSA_ERROR_INSUFFICIENT_ENTROPY;
        case MBEDTLS_ERR_CTR_DRBG_REQUEST_TOO_BIG:
        case MBEDTLS_ERR_CTR_DRBG_INPUT_TOO_BIG:
            return PSA_ERROR_NOT_SUPPORTED;
        case MBEDTLS_ERR_CTR_DRBG_FILE_IO_ERROR:
            return PSA_ERROR_INSUFFICIENT_ENTROPY;
#endif

        case MBEDTLS_ERR_DES_INVALID_INPUT_LENGTH:
            return PSA_ERROR_NOT_SUPPORTED;
        case MBEDTLS_ERR_DES_HW_ACCEL_FAILED:
            return PSA_ERROR_HARDWARE_FAILURE;

        case MBEDTLS_ERR_ENTROPY_NO_SOURCES_DEFINED:
        case MBEDTLS_ERR_ENTROPY_NO_STRONG_SOURCE:
        case MBEDTLS_ERR_ENTROPY_SOURCE_FAILED:
            return PSA_ERROR_INSUFFICIENT_ENTROPY;

        case MBEDTLS_ERR_GCM_AUTH_FAILED:
            return PSA_ERROR_INVALID_SIGNATURE;
        case MBEDTLS_ERR_GCM_BAD_INPUT:
            return PSA_ERROR_INVALID_ARGUMENT;
        case MBEDTLS_ERR_GCM_HW_ACCEL_FAILED:
            return PSA_ERROR_HARDWARE_FAILURE;

#if !defined(MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG) &&        \
            defined(MBEDTLS_PSA_HMAC_DRBG_MD_TYPE)
        /* Only check HMAC_DRBG error codes if underlying mbedtls_xxx
         * functions are passed a HMAC_DRBG instance. */
        case MBEDTLS_ERR_HMAC_DRBG_ENTROPY_SOURCE_FAILED:
            return PSA_ERROR_INSUFFICIENT_ENTROPY;
        case MBEDTLS_ERR_HMAC_DRBG_REQUEST_TOO_BIG:
        case MBEDTLS_ERR_HMAC_DRBG_INPUT_TOO_BIG:
            return PSA_ERROR_NOT_SUPPORTED;
        case MBEDTLS_ERR_HMAC_DRBG_FILE_IO_ERROR:
            return PSA_ERROR_INSUFFICIENT_ENTROPY;
#endif

        case MBEDTLS_ERR_MD2_HW_ACCEL_FAILED:
        case MBEDTLS_ERR_MD4_HW_ACCEL_FAILED:
        case MBEDTLS_ERR_MD5_HW_ACCEL_FAILED:
            return PSA_ERROR_HARDWARE_FAILURE;

        case MBEDTLS_ERR_MD_FEATURE_UNAVAILABLE:
            return PSA_ERROR_NOT_SUPPORTED;
        case MBEDTLS_ERR_MD_BAD_INPUT_DATA:
            return PSA_ERROR_INVALID_ARGUMENT;
        case MBEDTLS_ERR_MD_ALLOC_FAILED:
            return PSA_ERROR_INSUFFICIENT_MEMORY;
        case MBEDTLS_ERR_MD_FILE_IO_ERROR:
            return PSA_ERROR_STORAGE_FAILURE;
        case MBEDTLS_ERR_MD_HW_ACCEL_FAILED:
            return PSA_ERROR_HARDWARE_FAILURE;

        case MBEDTLS_ERR_MPI_FILE_IO_ERROR:
            return PSA_ERROR_STORAGE_FAILURE;
        case MBEDTLS_ERR_MPI_BAD_INPUT_DATA:
            return PSA_ERROR_INVALID_ARGUMENT;
        case MBEDTLS_ERR_MPI_INVALID_CHARACTER:
            return PSA_ERROR_INVALID_ARGUMENT;
        case MBEDTLS_ERR_MPI_BUFFER_TOO_SMALL:
            return PSA_ERROR_BUFFER_TOO_SMALL;
        case MBEDTLS_ERR_MPI_NEGATIVE_VALUE:
            return PSA_ERROR_INVALID_ARGUMENT;
        case MBEDTLS_ERR_MPI_DIVISION_BY_ZERO:
            return PSA_ERROR_INVALID_ARGUMENT;
        case MBEDTLS_ERR_MPI_NOT_ACCEPTABLE:
            return PSA_ERROR_INVALID_ARGUMENT;
        case MBEDTLS_ERR_MPI_ALLOC_FAILED:
            return PSA_ERROR_INSUFFICIENT_MEMORY;

        case MBEDTLS_ERR_PK_ALLOC_FAILED:
            return PSA_ERROR_INSUFFICIENT_MEMORY;
        case MBEDTLS_ERR_PK_TYPE_MISMATCH:
        case MBEDTLS_ERR_PK_BAD_INPUT_DATA:
            return PSA_ERROR_INVALID_ARGUMENT;
        case MBEDTLS_ERR_PK_FILE_IO_ERROR:
            return PSA_ERROR_STORAGE_FAILURE;
        case MBEDTLS_ERR_PK_KEY_INVALID_VERSION:
        case MBEDTLS_ERR_PK_KEY_INVALID_FORMAT:
            return PSA_ERROR_INVALID_ARGUMENT;
        case MBEDTLS_ERR_PK_UNKNOWN_PK_ALG:
            return PSA_ERROR_NOT_SUPPORTED;
        case MBEDTLS_ERR_PK_PASSWORD_REQUIRED:
        case MBEDTLS_ERR_PK_PASSWORD_MISMATCH:
            return PSA_ERROR_NOT_PERMITTED;
        case MBEDTLS_ERR_PK_INVALID_PUBKEY:
            return PSA_ERROR_INVALID_ARGUMENT;
        case MBEDTLS_ERR_PK_INVALID_ALG:
        case MBEDTLS_ERR_PK_UNKNOWN_NAMED_CURVE:
        case MBEDTLS_ERR_PK_FEATURE_UNAVAILABLE:
            return PSA_ERROR_NOT_SUPPORTED;
        case MBEDTLS_ERR_PK_SIG_LEN_MISMATCH:
            return PSA_ERROR_INVALID_SIGNATURE;
        case MBEDTLS_ERR_PK_HW_ACCEL_FAILED:
            return PSA_ERROR_HARDWARE_FAILURE;

        case MBEDTLS_ERR_PLATFORM_HW_ACCEL_FAILED:
            return PSA_ERROR_HARDWARE_FAILURE;
        case MBEDTLS_ERR_PLATFORM_FEATURE_UNSUPPORTED:
            return PSA_ERROR_NOT_SUPPORTED;

        case MBEDTLS_ERR_RIPEMD160_HW_ACCEL_FAILED:
            return PSA_ERROR_HARDWARE_FAILURE;

        case MBEDTLS_ERR_RSA_BAD_INPUT_DATA:
            return PSA_ERROR_INVALID_ARGUMENT;
        case MBEDTLS_ERR_RSA_INVALID_PADDING:
            return PSA_ERROR_INVALID_PADDING;
        case MBEDTLS_ERR_RSA_KEY_GEN_FAILED:
            return PSA_ERROR_HARDWARE_FAILURE;
        case MBEDTLS_ERR_RSA_KEY_CHECK_FAILED:
            return PSA_ERROR_INVALID_ARGUMENT;
        case MBEDTLS_ERR_RSA_PUBLIC_FAILED:
        case MBEDTLS_ERR_RSA_PRIVATE_FAILED:
            return PSA_ERROR_CORRUPTION_DETECTED;
        case MBEDTLS_ERR_RSA_VERIFY_FAILED:
            return PSA_ERROR_INVALID_SIGNATURE;
        case MBEDTLS_ERR_RSA_OUTPUT_TOO_LARGE:
            return PSA_ERROR_BUFFER_TOO_SMALL;
        case MBEDTLS_ERR_RSA_RNG_FAILED:
            return PSA_ERROR_INSUFFICIENT_ENTROPY;
        case MBEDTLS_ERR_RSA_UNSUPPORTED_OPERATION:
            return PSA_ERROR_NOT_SUPPORTED;
        case MBEDTLS_ERR_RSA_HW_ACCEL_FAILED:
            return PSA_ERROR_HARDWARE_FAILURE;

        case MBEDTLS_ERR_SHA1_HW_ACCEL_FAILED:
        case MBEDTLS_ERR_SHA256_HW_ACCEL_FAILED:
        case MBEDTLS_ERR_SHA512_HW_ACCEL_FAILED:
            return PSA_ERROR_HARDWARE_FAILURE;

        case MBEDTLS_ERR_XTEA_INVALID_INPUT_LENGTH:
            return PSA_ERROR_INVALID_ARGUMENT;
        case MBEDTLS_ERR_XTEA_HW_ACCEL_FAILED:
            return PSA_ERROR_HARDWARE_FAILURE;

        case MBEDTLS_ERR_ECP_BAD_INPUT_DATA:
        case MBEDTLS_ERR_ECP_INVALID_KEY:
            return PSA_ERROR_INVALID_ARGUMENT;
        case MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL:
            return PSA_ERROR_BUFFER_TOO_SMALL;
        case MBEDTLS_ERR_ECP_FEATURE_UNAVAILABLE:
            return PSA_ERROR_NOT_SUPPORTED;
        case MBEDTLS_ERR_ECP_SIG_LEN_MISMATCH:
        case MBEDTLS_ERR_ECP_VERIFY_FAILED:
            return PSA_ERROR_INVALID_SIGNATURE;
        case MBEDTLS_ERR_ECP_ALLOC_FAILED:
            return PSA_ERROR_INSUFFICIENT_MEMORY;
        case MBEDTLS_ERR_ECP_RANDOM_FAILED:
            return PSA_ERROR_INSUFFICIENT_ENTROPY;
        case MBEDTLS_ERR_ECP_HW_ACCEL_FAILED:
            return PSA_ERROR_HARDWARE_FAILURE;

        case MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED:
            return PSA_ERROR_CORRUPTION_DETECTED;

        default:
            return PSA_ERROR_GENERIC_ERROR;
    }
}




/****************************************************************/
/* Key management */
/****************************************************************/

#if defined(MBEDTLS_PSA_BUILTIN_KEY_TYPE_ECC_KEY_PAIR) || \
    defined(MBEDTLS_PSA_BUILTIN_KEY_TYPE_ECC_PUBLIC_KEY) || \
    defined(MBEDTLS_PSA_BUILTIN_ALG_ECDSA) || \
    defined(MBEDTLS_PSA_BUILTIN_ALG_DETERMINISTIC_ECDSA) || \
    defined(MBEDTLS_PSA_BUILTIN_ALG_ECDH)
mbedtls_ecp_group_id mbedtls_ecc_group_of_psa(psa_ecc_family_t curve,
                                              size_t bits,
                                              int bits_is_sloppy)
{
    switch (curve) {
        case PSA_ECC_FAMILY_SECP_R1:
            switch (bits) {
#if defined(PSA_WANT_ECC_SECP_R1_192)
                case 192:
                    return MBEDTLS_ECP_DP_SECP192R1;
#endif
#if defined(PSA_WANT_ECC_SECP_R1_224)
                case 224:
                    return MBEDTLS_ECP_DP_SECP224R1;
#endif
#if defined(PSA_WANT_ECC_SECP_R1_256)
                case 256:
                    return MBEDTLS_ECP_DP_SECP256R1;
#endif
#if defined(PSA_WANT_ECC_SECP_R1_384)
                case 384:
                    return MBEDTLS_ECP_DP_SECP384R1;
#endif
#if defined(PSA_WANT_ECC_SECP_R1_521)
                case 521:
                    return MBEDTLS_ECP_DP_SECP521R1;
                case 528:
                    if (bits_is_sloppy) {
                        return MBEDTLS_ECP_DP_SECP521R1;
                    }
                    break;
#endif
            }
            break;

        case PSA_ECC_FAMILY_BRAINPOOL_P_R1:
            switch (bits) {
#if defined(PSA_WANT_ECC_BRAINPOOL_P_R1_256)
                case 256:
                    return MBEDTLS_ECP_DP_BP256R1;
#endif
#if defined(PSA_WANT_ECC_BRAINPOOL_P_R1_384)
                case 384:
                    return MBEDTLS_ECP_DP_BP384R1;
#endif
#if defined(PSA_WANT_ECC_BRAINPOOL_P_R1_512)
                case 512:
                    return MBEDTLS_ECP_DP_BP512R1;
#endif
            }
            break;

        case PSA_ECC_FAMILY_MONTGOMERY:
            switch (bits) {
#if defined(PSA_WANT_ECC_MONTGOMERY_255)
                case 255:
                    return MBEDTLS_ECP_DP_CURVE25519;
                case 256:
                    if (bits_is_sloppy) {
                        return MBEDTLS_ECP_DP_CURVE25519;
                    }
                    break;
#endif
#if defined(PSA_WANT_ECC_MONTGOMERY_448)
                case 448:
                    return MBEDTLS_ECP_DP_CURVE448;
#endif
            }
            break;

        case PSA_ECC_FAMILY_SECP_K1:
            switch (bits) {
#if defined(PSA_WANT_ECC_SECP_K1_192)
                case 192:
                    return MBEDTLS_ECP_DP_SECP192K1;
#endif
#if defined(PSA_WANT_ECC_SECP_K1_224)
                case 224:
                    return MBEDTLS_ECP_DP_SECP224K1;
#endif
#if defined(PSA_WANT_ECC_SECP_K1_256)
                case 256:
                    return MBEDTLS_ECP_DP_SECP256K1;
#endif
            }
            break;
    }

    (void) bits_is_sloppy;
    return MBEDTLS_ECP_DP_NONE;
}
#endif /* defined(MBEDTLS_PSA_BUILTIN_KEY_TYPE_ECC_KEY_PAIR) ||
          defined(MBEDTLS_PSA_BUILTIN_KEY_TYPE_ECC_PUBLIC_KEY) ||
          defined(MBEDTLS_PSA_BUILTIN_ALG_ECDSA) ||
          defined(MBEDTLS_PSA_BUILTIN_ALG_DETERMINISTIC_ECDSA) ||
          defined(MBEDTLS_PSA_BUILTIN_ALG_ECDH) */

static psa_status_t validate_unstructured_key_bit_size(psa_key_type_t type,
                                                       size_t bits)
{
    /* Check that the bit size is acceptable for the key type */
    switch (type) {
        case PSA_KEY_TYPE_RAW_DATA:
        case PSA_KEY_TYPE_HMAC:
        case PSA_KEY_TYPE_DERIVE:
            break;
#if defined(PSA_WANT_KEY_TYPE_AES)
        case PSA_KEY_TYPE_AES:
            if (bits != 128 && bits != 192 && bits != 256) {
                return PSA_ERROR_INVALID_ARGUMENT;
            }
            break;
#endif
#if defined(PSA_WANT_KEY_TYPE_ARIA)
        case PSA_KEY_TYPE_ARIA:
            if (bits != 128 && bits != 192 && bits != 256) {
                return PSA_ERROR_INVALID_ARGUMENT;
            }
            break;
#endif
#if defined(PSA_WANT_KEY_TYPE_CAMELLIA)
        case PSA_KEY_TYPE_CAMELLIA:
            if (bits != 128 && bits != 192 && bits != 256) {
                return PSA_ERROR_INVALID_ARGUMENT;
            }
            break;
#endif
#if defined(PSA_WANT_KEY_TYPE_DES)
        case PSA_KEY_TYPE_DES:
            if (bits != 64 && bits != 128 && bits != 192) {
                return PSA_ERROR_INVALID_ARGUMENT;
            }
            break;
#endif
#if defined(PSA_WANT_KEY_TYPE_ARC4)
        case PSA_KEY_TYPE_ARC4:
            if (bits < 8 || bits > 2048) {
                return PSA_ERROR_INVALID_ARGUMENT;
            }
            break;
#endif
#if defined(PSA_WANT_KEY_TYPE_CHACHA20)
        case PSA_KEY_TYPE_CHACHA20:
            if (bits != 256) {
                return PSA_ERROR_INVALID_ARGUMENT;
            }
            break;
#endif
        default:
            return PSA_ERROR_NOT_SUPPORTED;
    }
    if (bits % 8 != 0) {
        return PSA_ERROR_INVALID_ARGUMENT;
    }

    return PSA_SUCCESS;
}

/** Check whether a given key type is valid for use with a given MAC algorithm
 *
 * Upon successful return of this function, the behavior of #PSA_MAC_LENGTH
 * when called with the validated \p algorithm and \p key_type is well-defined.
 *
 * \param[in] algorithm     The specific MAC algorithm (can be wildcard).
 * \param[in] key_type      The key type of the key to be used with the
 *                          \p algorithm.
 *
 * \retval #PSA_SUCCESS
 *         The \p key_type is valid for use with the \p algorithm
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         The \p key_type is not valid for use with the \p algorithm
 */
MBEDTLS_STATIC_TESTABLE psa_status_t psa_mac_key_can_do(
    psa_algorithm_t algorithm,
    psa_key_type_t key_type)
{
    if (PSA_ALG_IS_HMAC(algorithm)) {
        if (key_type == PSA_KEY_TYPE_HMAC) {
            return PSA_SUCCESS;
        }
    }

    if (PSA_ALG_IS_BLOCK_CIPHER_MAC(algorithm)) {
        /* Check that we're calling PSA_BLOCK_CIPHER_BLOCK_LENGTH with a cipher
         * key. */
        if ((key_type & PSA_KEY_TYPE_CATEGORY_MASK) ==
            PSA_KEY_TYPE_CATEGORY_SYMMETRIC) {
            /* PSA_BLOCK_CIPHER_BLOCK_LENGTH returns 1 for stream ciphers and
             * the block length (larger than 1) for block ciphers. */
            if (PSA_BLOCK_CIPHER_BLOCK_LENGTH(key_type) > 1) {
                return PSA_SUCCESS;
            }
        }
    }

    return PSA_ERROR_INVALID_ARGUMENT;
}

psa_status_t psa_allocate_buffer_to_slot(psa_key_slot_t *slot,
                                         size_t buffer_length)
{
    if (slot->key.data != NULL) {
        return PSA_ERROR_ALREADY_EXISTS;
    }

    slot->key.data = mbedtls_calloc(1, buffer_length);
    if (slot->key.data == NULL) {
        return PSA_ERROR_INSUFFICIENT_MEMORY;
    }

    slot->key.bytes = buffer_length;
    return PSA_SUCCESS;
}

psa_status_t psa_copy_key_material_into_slot(psa_key_slot_t *slot,
                                             const uint8_t *data,
                                             size_t data_length)
{
    psa_status_t status = psa_allocate_buffer_to_slot(slot,
                                                      data_length);
    if (status != PSA_SUCCESS) {
        return status;
    }

    memcpy(slot->key.data, data, data_length);
    return PSA_SUCCESS;
}

psa_status_t psa_import_key_into_slot(
    const psa_key_attributes_t *attributes,
    const uint8_t *data, size_t data_length,
    uint8_t *key_buffer, size_t key_buffer_size,
    size_t *key_buffer_length, size_t *bits)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    psa_key_type_t type = attributes->core.type;

    /* zero-length keys are never supported. */
    if (data_length == 0) {
        return PSA_ERROR_NOT_SUPPORTED;
    }

    if (key_type_is_raw_bytes(type)) {
        *bits = PSA_BYTES_TO_BITS(data_length);

        /* Ensure that the bytes-to-bits conversion hasn't overflown. */
        if (data_length > SIZE_MAX / 8) {
            return PSA_ERROR_NOT_SUPPORTED;
        }

        /* Enforce a size limit, and in particular ensure that the bit
         * size fits in its representation type. */
        if ((*bits) > PSA_MAX_KEY_BITS) {
            return PSA_ERROR_NOT_SUPPORTED;
        }

        status = validate_unstructured_key_bit_size(type, *bits);
        if (status != PSA_SUCCESS) {
            return status;
        }

        /* Copy the key material. */
        memcpy(key_buffer, data, data_length);
        *key_buffer_length = data_length;
        (void) key_buffer_size;

        return PSA_SUCCESS;
    } else if (PSA_KEY_TYPE_IS_ASYMMETRIC(type)) {
#if defined(MBEDTLS_PSA_BUILTIN_KEY_TYPE_ECC_KEY_PAIR) || \
        defined(MBEDTLS_PSA_BUILTIN_KEY_TYPE_ECC_PUBLIC_KEY)
        if (PSA_KEY_TYPE_IS_ECC(type)) {
            return mbedtls_psa_ecp_import_key(attributes,
                                              data, data_length,
                                              key_buffer, key_buffer_size,
                                              key_buffer_length,
                                              bits);
        }
#endif /* defined(MBEDTLS_PSA_BUILTIN_KEY_TYPE_ECC_KEY_PAIR) ||
        * defined(MBEDTLS_PSA_BUILTIN_KEY_TYPE_ECC_PUBLIC_KEY) */
#if defined(MBEDTLS_PSA_BUILTIN_KEY_TYPE_RSA_KEY_PAIR) || \
        defined(MBEDTLS_PSA_BUILTIN_KEY_TYPE_RSA_PUBLIC_KEY)
        if (PSA_KEY_TYPE_IS_RSA(type)) {
            return mbedtls_psa_rsa_import_key(attributes,
                                              data, data_length,
                                              key_buffer, key_buffer_size,
                                              key_buffer_length,
                                              bits);
        }
#endif /* defined(MBEDTLS_PSA_BUILTIN_KEY_TYPE_RSA_KEY_PAIR) ||
        * defined(MBEDTLS_PSA_BUILTIN_KEY_TYPE_RSA_PUBLIC_KEY) */
    }

    return PSA_ERROR_NOT_SUPPORTED;
}

/** Calculate the intersection of two algorithm usage policies.
 *
 * Return 0 (which allows no operation) on incompatibility.
 */
static psa_algorithm_t psa_key_policy_algorithm_intersection(
    psa_key_type_t key_type,
    psa_algorithm_t alg1,
    psa_algorithm_t alg2)
{
    /* Common case: both sides actually specify the same policy. */
    if (alg1 == alg2) {
        return alg1;
    }
    /* If the policies are from the same hash-and-sign family, check
     * if one is a wildcard. If so the other has the specific algorithm. */
    if (PSA_ALG_IS_SIGN_HASH(alg1) &&
        PSA_ALG_IS_SIGN_HASH(alg2) &&
        (alg1 & ~PSA_ALG_HASH_MASK) == (alg2 & ~PSA_ALG_HASH_MASK)) {
        if (PSA_ALG_SIGN_GET_HASH(alg1) == PSA_ALG_ANY_HASH) {
            return alg2;
        }
        if (PSA_ALG_SIGN_GET_HASH(alg2) == PSA_ALG_ANY_HASH) {
            return alg1;
        }
    }
    /* If the policies are from the same AEAD family, check whether
     * one of them is a minimum-tag-length wildcard. Calculate the most
     * restrictive tag length. */
    if (PSA_ALG_IS_AEAD(alg1) && PSA_ALG_IS_AEAD(alg2) &&
        (PSA_ALG_AEAD_WITH_SHORTENED_TAG(alg1, 0) ==
         PSA_ALG_AEAD_WITH_SHORTENED_TAG(alg2, 0))) {
        size_t alg1_len = PSA_ALG_AEAD_GET_TAG_LENGTH(alg1);
        size_t alg2_len = PSA_ALG_AEAD_GET_TAG_LENGTH(alg2);
        size_t restricted_len = alg1_len > alg2_len ? alg1_len : alg2_len;

        /* If both are wildcards, return most restrictive wildcard */
        if (((alg1 & PSA_ALG_AEAD_AT_LEAST_THIS_LENGTH_FLAG) != 0) &&
            ((alg2 & PSA_ALG_AEAD_AT_LEAST_THIS_LENGTH_FLAG) != 0)) {
            return PSA_ALG_AEAD_WITH_AT_LEAST_THIS_LENGTH_TAG(
                alg1, restricted_len);
        }
        /* If only one is a wildcard, return specific algorithm if compatible. */
        if (((alg1 & PSA_ALG_AEAD_AT_LEAST_THIS_LENGTH_FLAG) != 0) &&
            (alg1_len <= alg2_len)) {
            return alg2;
        }
        if (((alg2 & PSA_ALG_AEAD_AT_LEAST_THIS_LENGTH_FLAG) != 0) &&
            (alg2_len <= alg1_len)) {
            return alg1;
        }
    }
    /* If the policies are from the same MAC family, check whether one
     * of them is a minimum-MAC-length policy. Calculate the most
     * restrictive tag length. */
    if (PSA_ALG_IS_MAC(alg1) && PSA_ALG_IS_MAC(alg2) &&
        (PSA_ALG_FULL_LENGTH_MAC(alg1) ==
         PSA_ALG_FULL_LENGTH_MAC(alg2))) {
        /* Validate the combination of key type and algorithm. Since the base
         * algorithm of alg1 and alg2 are the same, we only need this once. */
        if (PSA_SUCCESS != psa_mac_key_can_do(alg1, key_type)) {
            return 0;
        }

        /* Get the (exact or at-least) output lengths for both sides of the
         * requested intersection. None of the currently supported algorithms
         * have an output length dependent on the actual key size, so setting it
         * to a bogus value of 0 is currently OK.
         *
         * Note that for at-least-this-length wildcard algorithms, the output
         * length is set to the shortest allowed length, which allows us to
         * calculate the most restrictive tag length for the intersection. */
        size_t alg1_len = PSA_MAC_LENGTH(key_type, 0, alg1);
        size_t alg2_len = PSA_MAC_LENGTH(key_type, 0, alg2);
        size_t restricted_len = alg1_len > alg2_len ? alg1_len : alg2_len;

        /* If both are wildcards, return most restrictive wildcard */
        if (((alg1 & PSA_ALG_MAC_AT_LEAST_THIS_LENGTH_FLAG) != 0) &&
            ((alg2 & PSA_ALG_MAC_AT_LEAST_THIS_LENGTH_FLAG) != 0)) {
            return PSA_ALG_AT_LEAST_THIS_LENGTH_MAC(alg1, restricted_len);
        }

        /* If only one is an at-least-this-length policy, the intersection would
         * be the other (fixed-length) policy as long as said fixed length is
         * equal to or larger than the shortest allowed length. */
        if ((alg1 & PSA_ALG_MAC_AT_LEAST_THIS_LENGTH_FLAG) != 0) {
            return (alg1_len <= alg2_len) ? alg2 : 0;
        }
        if ((alg2 & PSA_ALG_MAC_AT_LEAST_THIS_LENGTH_FLAG) != 0) {
            return (alg2_len <= alg1_len) ? alg1 : 0;
        }

        /* If none of them are wildcards, check whether they define the same tag
         * length. This is still possible here when one is default-length and
         * the other specific-length. Ensure to always return the
         * specific-length version for the intersection. */
        if (alg1_len == alg2_len) {
            return PSA_ALG_TRUNCATED_MAC(alg1, alg1_len);
        }
    }
    /* If the policies are incompatible, allow nothing. */
    return 0;
}

static int psa_key_algorithm_permits(psa_key_type_t key_type,
                                     psa_algorithm_t policy_alg,
                                     psa_algorithm_t requested_alg)
{
    /* Common case: the policy only allows requested_alg. */
    if (requested_alg == policy_alg) {
        return 1;
    }
    /* If policy_alg is a hash-and-sign with a wildcard for the hash,
     * and requested_alg is the same hash-and-sign family with any hash,
     * then requested_alg is compliant with policy_alg. */
    if (PSA_ALG_IS_SIGN_HASH(requested_alg) &&
        PSA_ALG_SIGN_GET_HASH(policy_alg) == PSA_ALG_ANY_HASH) {
        return (policy_alg & ~PSA_ALG_HASH_MASK) ==
               (requested_alg & ~PSA_ALG_HASH_MASK);
    }
    /* If policy_alg is a wildcard AEAD algorithm of the same base as
     * the requested algorithm, check the requested tag length to be
     * equal-length or longer than the wildcard-specified length. */
    if (PSA_ALG_IS_AEAD(policy_alg) &&
        PSA_ALG_IS_AEAD(requested_alg) &&
        (PSA_ALG_AEAD_WITH_SHORTENED_TAG(policy_alg, 0) ==
         PSA_ALG_AEAD_WITH_SHORTENED_TAG(requested_alg, 0)) &&
        ((policy_alg & PSA_ALG_AEAD_AT_LEAST_THIS_LENGTH_FLAG) != 0)) {
        return PSA_ALG_AEAD_GET_TAG_LENGTH(policy_alg) <=
               PSA_ALG_AEAD_GET_TAG_LENGTH(requested_alg);
    }
    /* If policy_alg is a MAC algorithm of the same base as the requested
     * algorithm, check whether their MAC lengths are compatible. */
    if (PSA_ALG_IS_MAC(policy_alg) &&
        PSA_ALG_IS_MAC(requested_alg) &&
        (PSA_ALG_FULL_LENGTH_MAC(policy_alg) ==
         PSA_ALG_FULL_LENGTH_MAC(requested_alg))) {
        /* Validate the combination of key type and algorithm. Since the policy
         * and requested algorithms are the same, we only need this once. */
        if (PSA_SUCCESS != psa_mac_key_can_do(policy_alg, key_type)) {
            return 0;
        }

        /* Get both the requested output length for the algorithm which is to be
         * verified, and the default output length for the base algorithm.
         * Note that none of the currently supported algorithms have an output
         * length dependent on actual key size, so setting it to a bogus value
         * of 0 is currently OK. */
        size_t requested_output_length = PSA_MAC_LENGTH(
            key_type, 0, requested_alg);
        size_t default_output_length = PSA_MAC_LENGTH(
            key_type, 0,
            PSA_ALG_FULL_LENGTH_MAC(requested_alg));

        /* If the policy is default-length, only allow an algorithm with
         * a declared exact-length matching the default. */
        if (PSA_MAC_TRUNCATED_LENGTH(policy_alg) == 0) {
            return requested_output_length == default_output_length;
        }

        /* If the requested algorithm is default-length, allow it if the policy
         * length exactly matches the default length. */
        if (PSA_MAC_TRUNCATED_LENGTH(requested_alg) == 0 &&
            PSA_MAC_TRUNCATED_LENGTH(policy_alg) == default_output_length) {
            return 1;
        }

        /* If policy_alg is an at-least-this-length wildcard MAC algorithm,
         * check for the requested MAC length to be equal to or longer than the
         * minimum allowed length. */
        if ((policy_alg & PSA_ALG_MAC_AT_LEAST_THIS_LENGTH_FLAG) != 0) {
            return PSA_MAC_TRUNCATED_LENGTH(policy_alg) <=
                   requested_output_length;
        }
    }
    /* If policy_alg is a generic key agreement operation, then using it for
     * a key derivation with that key agreement should also be allowed. This
     * behaviour is expected to be defined in a future specification version. */
    if (PSA_ALG_IS_RAW_KEY_AGREEMENT(policy_alg) &&
        PSA_ALG_IS_KEY_AGREEMENT(requested_alg)) {
        return PSA_ALG_KEY_AGREEMENT_GET_BASE(requested_alg) ==
               policy_alg;
    }
    /* If it isn't explicitly permitted, it's forbidden. */
    return 0;
}

/** Test whether a policy permits an algorithm.
 *
 * The caller must test usage flags separately.
 *
 * \note This function requires providing the key type for which the policy is
 *       being validated, since some algorithm policy definitions (e.g. MAC)
 *       have different properties depending on what kind of cipher it is
 *       combined with.
 *
 * \retval PSA_SUCCESS                  When \p alg is a specific algorithm
 *                                      allowed by the \p policy.
 * \retval PSA_ERROR_INVALID_ARGUMENT   When \p alg is not a specific algorithm
 * \retval PSA_ERROR_NOT_PERMITTED      When \p alg is a specific algorithm, but
 *                                      the \p policy does not allow it.
 */
static psa_status_t psa_key_policy_permits(const psa_key_policy_t *policy,
                                           psa_key_type_t key_type,
                                           psa_algorithm_t alg)
{
    /* '0' is not a valid algorithm */
    if (alg == 0) {
        return PSA_ERROR_INVALID_ARGUMENT;
    }

    /* A requested algorithm cannot be a wildcard. */
    if (PSA_ALG_IS_WILDCARD(alg)) {
        return PSA_ERROR_INVALID_ARGUMENT;
    }

    if (psa_key_algorithm_permits(key_type, policy->alg, alg) ||
        psa_key_algorithm_permits(key_type, policy->alg2, alg)) {
        return PSA_SUCCESS;
    } else {
        return PSA_ERROR_NOT_PERMITTED;
    }
}

/** Restrict a key policy based on a constraint.
 *
 * \note This function requires providing the key type for which the policy is
 *       being restricted, since some algorithm policy definitions (e.g. MAC)
 *       have different properties depending on what kind of cipher it is
 *       combined with.
 *
 * \param[in] key_type      The key type for which to restrict the policy
 * \param[in,out] policy    The policy to restrict.
 * \param[in] constraint    The policy constraint to apply.
 *
 * \retval #PSA_SUCCESS
 *         \c *policy contains the intersection of the original value of
 *         \c *policy and \c *constraint.
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         \c key_type, \c *policy and \c *constraint are incompatible.
 *         \c *policy is unchanged.
 */
static psa_status_t psa_restrict_key_policy(
    psa_key_type_t key_type,
    psa_key_policy_t *policy,
    const psa_key_policy_t *constraint)
{
    psa_algorithm_t intersection_alg =
        psa_key_policy_algorithm_intersection(key_type, policy->alg,
                                              constraint->alg);
    psa_algorithm_t intersection_alg2 =
        psa_key_policy_algorithm_intersection(key_type, policy->alg2,
                                              constraint->alg2);
    if (intersection_alg == 0 && policy->alg != 0 && constraint->alg != 0) {
        return PSA_ERROR_INVALID_ARGUMENT;
    }
    if (intersection_alg2 == 0 && policy->alg2 != 0 && constraint->alg2 != 0) {
        return PSA_ERROR_INVALID_ARGUMENT;
    }
    policy->usage &= constraint->usage;
    policy->alg = intersection_alg;
    policy->alg2 = intersection_alg2;
    return PSA_SUCCESS;
}

/** Get the description of a key given its identifier and policy constraints
 *  and lock it.
 *
 * The key must have allow all the usage flags set in \p usage. If \p alg is
 * nonzero, the key must allow operations with this algorithm. If \p alg is
 * zero, the algorithm is not checked.
 *
 * In case of a persistent key, the function loads the description of the key
 * into a key slot if not already done.
 *
 * On success, the returned key slot is locked. It is the responsibility of
 * the caller to unlock the key slot when it does not access it anymore.
 */
static psa_status_t psa_get_and_lock_key_slot_with_policy(
    mbedtls_svc_key_id_t key,
    psa_key_slot_t **p_slot,
    psa_key_usage_t usage,
    psa_algorithm_t alg)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    psa_key_slot_t *slot;

    status = psa_get_and_lock_key_slot(key, p_slot);
    if (status != PSA_SUCCESS) {
        return status;
    }
    slot = *p_slot;

    /* Enforce that usage policy for the key slot contains all the flags
     * required by the usage parameter. There is one exception: public
     * keys can always be exported, so we treat public key objects as
     * if they had the export flag. */
    if (PSA_KEY_TYPE_IS_PUBLIC_KEY(slot->attr.type)) {
        usage &= ~PSA_KEY_USAGE_EXPORT;
    }

    if ((slot->attr.policy.usage & usage) != usage) {
        status = PSA_ERROR_NOT_PERMITTED;
        goto error;
    }

    /* Enforce that the usage policy permits the requested algorithm. */
    if (alg != 0) {
        status = psa_key_policy_permits(&slot->attr.policy,
                                        slot->attr.type,
                                        alg);
        if (status != PSA_SUCCESS) {
            goto error;
        }
    }

    return PSA_SUCCESS;

error:
    *p_slot = NULL;
    psa_unlock_key_slot(slot);

    return status;
}

/** Get a key slot containing a transparent key and lock it.
 *
 * A transparent key is a key for which the key material is directly
 * available, as opposed to a key in a secure element and/or to be used
 * by a secure element.
 *
 * This is a temporary function that may be used instead of
 * psa_get_and_lock_key_slot_with_policy() when there is no opaque key support
 * for a cryptographic operation.
 *
 * On success, the returned key slot is locked. It is the responsibility of the
 * caller to unlock the key slot when it does not access it anymore.
 */
static psa_status_t psa_get_and_lock_transparent_key_slot_with_policy(
    mbedtls_svc_key_id_t key,
    psa_key_slot_t **p_slot,
    psa_key_usage_t usage,
    psa_algorithm_t alg)
{
    psa_status_t status = psa_get_and_lock_key_slot_with_policy(key, p_slot,
                                                                usage, alg);
    if (status != PSA_SUCCESS) {
        return status;
    }

    if (psa_key_lifetime_is_external((*p_slot)->attr.lifetime)) {
        psa_unlock_key_slot(*p_slot);
        *p_slot = NULL;
        return PSA_ERROR_NOT_SUPPORTED;
    }

    return PSA_SUCCESS;
}

psa_status_t psa_remove_key_data_from_memory(psa_key_slot_t *slot)
{
    /* Data pointer will always be either a valid pointer or NULL in an
     * initialized slot, so we can just free it. */
    if (slot->key.data != NULL) {
        mbedtls_platform_zeroize(slot->key.data, slot->key.bytes);
    }

    mbedtls_free(slot->key.data);
    slot->key.data = NULL;
    slot->key.bytes = 0;

    return PSA_SUCCESS;
}

/** Completely wipe a slot in memory, including its policy.
 * Persistent storage is not affected. */
psa_status_t psa_wipe_key_slot(psa_key_slot_t *slot)
{
    psa_status_t status = psa_remove_key_data_from_memory(slot);

    /*
     * As the return error code may not be handled in case of multiple errors,
     * do our best to report an unexpected lock counter: if available
     * call MBEDTLS_PARAM_FAILED that may terminate execution (if called as
     * part of the execution of a test suite this will stop the test suite
     * execution).
     */
    if (slot->lock_count != 1) {
#ifdef MBEDTLS_CHECK_PARAMS
        MBEDTLS_PARAM_FAILED(slot->lock_count == 1);
#endif
        status = PSA_ERROR_CORRUPTION_DETECTED;
    }

    /* Multipart operations may still be using the key. This is safe
     * because all multipart operation objects are independent from
     * the key slot: if they need to access the key after the setup
     * phase, they have a copy of the key. Note that this means that
     * key material can linger until all operations are completed. */
    /* At this point, key material and other type-specific content has
     * been wiped. Clear remaining metadata. We can call memset and not
     * zeroize because the metadata is not particularly sensitive. */
    memset(slot, 0, sizeof(*slot));
    return status;
}

psa_status_t psa_destroy_key(mbedtls_svc_key_id_t key)
{
    psa_key_slot_t *slot;
    psa_status_t status; /* status of the last operation */
    psa_status_t overall_status = PSA_SUCCESS;
#if defined(MBEDTLS_PSA_CRYPTO_SE_C)
    psa_se_drv_table_entry_t *driver;
#endif /* MBEDTLS_PSA_CRYPTO_SE_C */

    if (mbedtls_svc_key_id_is_null(key)) {
        return PSA_SUCCESS;
    }

    /*
     * Get the description of the key in a key slot. In case of a persistent
     * key, this will load the key description from persistent memory if not
     * done yet. We cannot avoid this loading as without it we don't know if
     * the key is operated by an SE or not and this information is needed by
     * the current implementation.
     */
    status = psa_get_and_lock_key_slot(key, &slot);
    if (status != PSA_SUCCESS) {
        return status;
    }

    /*
     * If the key slot containing the key description is under access by the
     * library (apart from the present access), the key cannot be destroyed
     * yet. For the time being, just return in error. Eventually (to be
     * implemented), the key should be destroyed when all accesses have
     * stopped.
     */
    if (slot->lock_count > 1) {
        psa_unlock_key_slot(slot);
        return PSA_ERROR_GENERIC_ERROR;
    }

    if (PSA_KEY_LIFETIME_IS_READ_ONLY(slot->attr.lifetime)) {
        /* Refuse the destruction of a read-only key (which may or may not work
         * if we attempt it, depending on whether the key is merely read-only
         * by policy or actually physically read-only).
         * Just do the best we can, which is to wipe the copy in memory
         * (done in this function's cleanup code). */
        overall_status = PSA_ERROR_NOT_PERMITTED;
        goto exit;
    }

#if defined(MBEDTLS_PSA_CRYPTO_SE_C)
    driver = psa_get_se_driver_entry(slot->attr.lifetime);
    if (driver != NULL) {
        /* For a key in a secure element, we need to do three things:
         * remove the key file in internal storage, destroy the
         * key inside the secure element, and update the driver's
         * persistent data. Start a transaction that will encompass these
         * three actions. */
        psa_crypto_prepare_transaction(PSA_CRYPTO_TRANSACTION_DESTROY_KEY);
        psa_crypto_transaction.key.lifetime = slot->attr.lifetime;
        psa_crypto_transaction.key.slot = psa_key_slot_get_slot_number(slot);
        psa_crypto_transaction.key.id = slot->attr.id;
        status = psa_crypto_save_transaction();
        if (status != PSA_SUCCESS) {
            (void) psa_crypto_stop_transaction();
            /* We should still try to destroy the key in the secure
             * element and the key metadata in storage. This is especially
             * important if the error is that the storage is full.
             * But how to do it exactly without risking an inconsistent
             * state after a reset?
             * https://github.com/ARMmbed/mbed-crypto/issues/215
             */
            overall_status = status;
            goto exit;
        }

        status = psa_destroy_se_key(driver,
                                    psa_key_slot_get_slot_number(slot));
        if (overall_status == PSA_SUCCESS) {
            overall_status = status;
        }
    }
#endif /* MBEDTLS_PSA_CRYPTO_SE_C */

#if defined(MBEDTLS_PSA_CRYPTO_STORAGE_C)
    if (!PSA_KEY_LIFETIME_IS_VOLATILE(slot->attr.lifetime)) {
        status = psa_destroy_persistent_key(slot->attr.id);
        if (overall_status == PSA_SUCCESS) {
            overall_status = status;
        }

        /* TODO: other slots may have a copy of the same key. We should
         * invalidate them.
         * https://github.com/ARMmbed/mbed-crypto/issues/214
         */
    }
#endif /* defined(MBEDTLS_PSA_CRYPTO_STORAGE_C) */

#if defined(MBEDTLS_PSA_CRYPTO_SE_C)
    if (driver != NULL) {
        status = psa_save_se_persistent_data(driver);
        if (overall_status == PSA_SUCCESS) {
            overall_status = status;
        }
        status = psa_crypto_stop_transaction();
        if (overall_status == PSA_SUCCESS) {
            overall_status = status;
        }
    }
#endif /* MBEDTLS_PSA_CRYPTO_SE_C */

exit:
    status = psa_wipe_key_slot(slot);
    /* Prioritize CORRUPTION_DETECTED from wiping over a storage error */
    if (status != PSA_SUCCESS) {
        overall_status = status;
    }
    return overall_status;
}

#if defined(MBEDTLS_PSA_BUILTIN_KEY_TYPE_RSA_KEY_PAIR) || \
    defined(MBEDTLS_PSA_BUILTIN_KEY_TYPE_RSA_PUBLIC_KEY)
static psa_status_t psa_get_rsa_public_exponent(
    const mbedtls_rsa_context *rsa,
    psa_key_attributes_t *attributes)
{
    mbedtls_mpi mpi;
    int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;
    uint8_t *buffer = NULL;
    size_t buflen;
    mbedtls_mpi_init(&mpi);

    ret = mbedtls_rsa_export(rsa, NULL, NULL, NULL, NULL, &mpi);
    if (ret != 0) {
        goto exit;
    }
    if (mbedtls_mpi_cmp_int(&mpi, 65537) == 0) {
        /* It's the default value, which is reported as an empty string,
         * so there's nothing to do. */
        goto exit;
    }

    buflen = mbedtls_mpi_size(&mpi);
    buffer = mbedtls_calloc(1, buflen);
    if (buffer == NULL) {
        ret = MBEDTLS_ERR_MPI_ALLOC_FAILED;
        goto exit;
    }
    ret = mbedtls_mpi_write_binary(&mpi, buffer, buflen);
    if (ret != 0) {
        goto exit;
    }
    attributes->domain_parameters = buffer;
    attributes->domain_parameters_size = buflen;

exit:
    mbedtls_mpi_free(&mpi);
    if (ret != 0) {
        mbedtls_free(buffer);
    }
    return mbedtls_to_psa_error(ret);
}
#endif /* defined(MBEDTLS_PSA_BUILTIN_KEY_TYPE_RSA_KEY_PAIR) ||
        * defined(MBEDTLS_PSA_BUILTIN_KEY_TYPE_RSA_PUBLIC_KEY) */

/** Retrieve all the publicly-accessible attributes of a key.
 */
psa_status_t psa_get_key_attributes(mbedtls_svc_key_id_t key,
                                    psa_key_attributes_t *attributes)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    psa_status_t unlock_status = PSA_ERROR_CORRUPTION_DETECTED;
    psa_key_slot_t *slot;

    psa_reset_key_attributes(attributes);

    status = psa_get_and_lock_key_slot_with_policy(key, &slot, 0, 0);
    if (status != PSA_SUCCESS) {
        return status;
    }

    attributes->core = slot->attr;
    attributes->core.flags &= (MBEDTLS_PSA_KA_MASK_EXTERNAL_ONLY |
                               MBEDTLS_PSA_KA_MASK_DUAL_USE);

#if defined(MBEDTLS_PSA_CRYPTO_SE_C)
    if (psa_get_se_driver_entry(slot->attr.lifetime) != NULL) {
        psa_set_key_slot_number(attributes,
                                psa_key_slot_get_slot_number(slot));
    }
#endif /* MBEDTLS_PSA_CRYPTO_SE_C */

    switch (slot->attr.type) {
#if defined(MBEDTLS_PSA_BUILTIN_KEY_TYPE_RSA_KEY_PAIR) || \
    defined(MBEDTLS_PSA_BUILTIN_KEY_TYPE_RSA_PUBLIC_KEY)
        case PSA_KEY_TYPE_RSA_KEY_PAIR:
        case PSA_KEY_TYPE_RSA_PUBLIC_KEY:
            /* TODO: reporting the public exponent for opaque keys
             * is not yet implemented.
             * https://github.com/ARMmbed/mbed-crypto/issues/216
             */
            if (!psa_key_lifetime_is_external(slot->attr.lifetime)) {
                mbedtls_rsa_context *rsa = NULL;

                status = mbedtls_psa_rsa_load_representation(
                    slot->attr.type,
                    slot->key.data,
                    slot->key.bytes,
                    &rsa);
                if (status != PSA_SUCCESS) {
                    break;
                }

                status = psa_get_rsa_public_exponent(rsa,
                                                     attributes);
                mbedtls_rsa_free(rsa);
                mbedtls_free(rsa);
            }
            break;
#endif /* defined(MBEDTLS_PSA_BUILTIN_KEY_TYPE_RSA_KEY_PAIR) ||
        * defined(MBEDTLS_PSA_BUILTIN_KEY_TYPE_RSA_PUBLIC_KEY) */
        default:
            /* Nothing else to do. */
            break;
    }

    if (status != PSA_SUCCESS) {
        psa_reset_key_attributes(attributes);
    }

    unlock_status = psa_unlock_key_slot(slot);

    return (status == PSA_SUCCESS) ? unlock_status : status;
}

#if defined(MBEDTLS_PSA_CRYPTO_SE_C)
psa_status_t psa_get_key_slot_number(
    const psa_key_attributes_t *attributes,
    psa_key_slot_number_t *slot_number)
{
    if (attributes->core.flags & MBEDTLS_PSA_KA_FLAG_HAS_SLOT_NUMBER) {
        *slot_number = attributes->slot_number;
        return PSA_SUCCESS;
    } else {
        return PSA_ERROR_INVALID_ARGUMENT;
    }
}
#endif /* MBEDTLS_PSA_CRYPTO_SE_C */

static psa_status_t psa_export_key_buffer_internal(const uint8_t *key_buffer,
                                                   size_t key_buffer_size,
                                                   uint8_t *data,
                                                   size_t data_size,
                                                   size_t *data_length)
{
    if (key_buffer_size > data_size) {
        return PSA_ERROR_BUFFER_TOO_SMALL;
    }
    memcpy(data, key_buffer, key_buffer_size);
    memset(data + key_buffer_size, 0,
           data_size - key_buffer_size);
    *data_length = key_buffer_size;
    return PSA_SUCCESS;
}

psa_status_t psa_export_key_internal(
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer, size_t key_buffer_size,
    uint8_t *data, size_t data_size, size_t *data_length)
{
    psa_key_type_t type = attributes->core.type;

    if (key_type_is_raw_bytes(type) ||
        PSA_KEY_TYPE_IS_RSA(type)   ||
        PSA_KEY_TYPE_IS_ECC(type)) {
        return psa_export_key_buffer_internal(
            key_buffer, key_buffer_size,
            data, data_size, data_length);
    } else {
        /* This shouldn't happen in the reference implementation, but
           it is valid for a special-purpose implementation to omit
           support for exporting certain key types. */
        return PSA_ERROR_NOT_SUPPORTED;
    }
}

psa_status_t psa_export_key(mbedtls_svc_key_id_t key,
                            uint8_t *data_external,
                            size_t data_size,
                            size_t *data_length)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    psa_status_t unlock_status = PSA_ERROR_CORRUPTION_DETECTED;
    psa_key_slot_t *slot;
    LOCAL_OUTPUT_DECLARE(data_external, data);
    /* Reject a zero-length output buffer now, since this can never be a
     * valid key representation. This way we know that data must be a valid
     * pointer and we can do things like memset(data, ..., data_size). */
    if (data_size == 0) {
        return PSA_ERROR_BUFFER_TOO_SMALL;
    }

    /* Set the key to empty now, so that even when there are errors, we always
     * set data_length to a value between 0 and data_size. On error, setting
     * the key to empty is a good choice because an empty key representation is
     * unlikely to be accepted anywhere. */
    *data_length = 0;

    /* Export requires the EXPORT flag. There is an exception for public keys,
     * which don't require any flag, but
     * psa_get_and_lock_key_slot_with_policy() takes care of this.
     */
    status = psa_get_and_lock_key_slot_with_policy(key, &slot,
                                                   PSA_KEY_USAGE_EXPORT, 0);
    if (status != PSA_SUCCESS) {
        return status;
    }

    LOCAL_OUTPUT_ALLOC(data_external, data_size, data);

    psa_key_attributes_t attributes = {
        .core = slot->attr
    };
    status = psa_driver_wrapper_export_key(&attributes,
                                           slot->key.data, slot->key.bytes,
                                           data, data_size, data_length);

#if !defined(MBEDTLS_PSA_ASSUME_EXCLUSIVE_BUFFERS)
exit:
#endif
    unlock_status = psa_unlock_key_slot(slot);

    LOCAL_OUTPUT_FREE(data_external, data);
    return (status == PSA_SUCCESS) ? unlock_status : status;
}

psa_status_t psa_export_public_key_internal(
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer,
    size_t key_buffer_size,
    uint8_t *data,
    size_t data_size,
    size_t *data_length)
{
    psa_key_type_t type = attributes->core.type;

    if (PSA_KEY_TYPE_IS_RSA(type) || PSA_KEY_TYPE_IS_ECC(type)) {
        if (PSA_KEY_TYPE_IS_PUBLIC_KEY(type)) {
            /* Exporting public -> public */
            return psa_export_key_buffer_internal(
                key_buffer, key_buffer_size,
                data, data_size, data_length);
        }

        if (PSA_KEY_TYPE_IS_RSA(type)) {
#if defined(MBEDTLS_PSA_BUILTIN_KEY_TYPE_RSA_KEY_PAIR) || \
            defined(MBEDTLS_PSA_BUILTIN_KEY_TYPE_RSA_PUBLIC_KEY)
            return mbedtls_psa_rsa_export_public_key(attributes,
                                                     key_buffer,
                                                     key_buffer_size,
                                                     data,
                                                     data_size,
                                                     data_length);
#else
            /* We don't know how to convert a private RSA key to public. */
            return PSA_ERROR_NOT_SUPPORTED;
#endif /* defined(MBEDTLS_PSA_BUILTIN_KEY_TYPE_RSA_KEY_PAIR) ||
        * defined(MBEDTLS_PSA_BUILTIN_KEY_TYPE_RSA_PUBLIC_KEY) */
        } else {
#if defined(MBEDTLS_PSA_BUILTIN_KEY_TYPE_ECC_KEY_PAIR) || \
            defined(MBEDTLS_PSA_BUILTIN_KEY_TYPE_ECC_PUBLIC_KEY)
            return mbedtls_psa_ecp_export_public_key(attributes,
                                                     key_buffer,
                                                     key_buffer_size,
                                                     data,
                                                     data_size,
                                                     data_length);
#else
            /* We don't know how to convert a private ECC key to public */
            return PSA_ERROR_NOT_SUPPORTED;
#endif /* defined(MBEDTLS_PSA_BUILTIN_KEY_TYPE_ECC_KEY_PAIR) ||
        * defined(MBEDTLS_PSA_BUILTIN_KEY_TYPE_ECC_PUBLIC_KEY) */
        }
    } else {
        /* This shouldn't happen in the reference implementation, but
           it is valid for a special-purpose implementation to omit
           support for exporting certain key types. */
        return PSA_ERROR_NOT_SUPPORTED;
    }
}

psa_status_t psa_export_public_key(mbedtls_svc_key_id_t key,
                                   uint8_t *data_external,
                                   size_t data_size,
                                   size_t *data_length)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    psa_status_t unlock_status = PSA_ERROR_CORRUPTION_DETECTED;
    psa_key_attributes_t attributes;
    psa_key_slot_t *slot;
    LOCAL_OUTPUT_DECLARE(data_external, data);

    /* Reject a zero-length output buffer now, since this can never be a
     * valid key representation. This way we know that data must be a valid
     * pointer and we can do things like memset(data, ..., data_size). */
    if (data_size == 0) {
        return PSA_ERROR_BUFFER_TOO_SMALL;
    }

    /* Set the key to empty now, so that even when there are errors, we always
     * set data_length to a value between 0 and data_size. On error, setting
     * the key to empty is a good choice because an empty key representation is
     * unlikely to be accepted anywhere. */
    *data_length = 0;

    /* Exporting a public key doesn't require a usage flag. */
    status = psa_get_and_lock_key_slot_with_policy(key, &slot, 0, 0);
    if (status != PSA_SUCCESS) {
        return status;
    }

    LOCAL_OUTPUT_ALLOC(data_external, data_size, data);

    if (!PSA_KEY_TYPE_IS_ASYMMETRIC(slot->attr.type)) {
        status = PSA_ERROR_INVALID_ARGUMENT;
        goto exit;
    }

    attributes = (psa_key_attributes_t) {
        .core = slot->attr
    };
    status = psa_driver_wrapper_export_public_key(
        &attributes, slot->key.data, slot->key.bytes,
        data, data_size, data_length);

exit:
    unlock_status = psa_unlock_key_slot(slot);

    LOCAL_OUTPUT_FREE(data_external, data);
    return (status == PSA_SUCCESS) ? unlock_status : status;
}

MBEDTLS_STATIC_ASSERT((MBEDTLS_PSA_KA_MASK_EXTERNAL_ONLY & MBEDTLS_PSA_KA_MASK_DUAL_USE) == 0,
                      "One or more key attribute flag is listed as both external-only and dual-use");
MBEDTLS_STATIC_ASSERT((PSA_KA_MASK_INTERNAL_ONLY & MBEDTLS_PSA_KA_MASK_DUAL_USE) == 0,
                      "One or more key attribute flag is listed as both internal-only and dual-use");
MBEDTLS_STATIC_ASSERT((PSA_KA_MASK_INTERNAL_ONLY & MBEDTLS_PSA_KA_MASK_EXTERNAL_ONLY) == 0,
                      "One or more key attribute flag is listed as both internal-only and external-only");

/** Validate that a key policy is internally well-formed.
 *
 * This function only rejects invalid policies. It does not validate the
 * consistency of the policy with respect to other attributes of the key
 * such as the key type.
 */
static psa_status_t psa_validate_key_policy(const psa_key_policy_t *policy)
{
    if ((policy->usage & ~(PSA_KEY_USAGE_EXPORT |
                           PSA_KEY_USAGE_COPY |
                           PSA_KEY_USAGE_ENCRYPT |
                           PSA_KEY_USAGE_DECRYPT |
                           PSA_KEY_USAGE_SIGN_MESSAGE |
                           PSA_KEY_USAGE_VERIFY_MESSAGE |
                           PSA_KEY_USAGE_SIGN_HASH |
                           PSA_KEY_USAGE_VERIFY_HASH |
                           PSA_KEY_USAGE_DERIVE)) != 0) {
        return PSA_ERROR_INVALID_ARGUMENT;
    }

    return PSA_SUCCESS;
}

/** Validate the internal consistency of key attributes.
 *
 * This function only rejects invalid attribute values. If does not
 * validate the consistency of the attributes with any key data that may
 * be involved in the creation of the key.
 *
 * Call this function early in the key creation process.
 *
 * \param[in] attributes    Key attributes for the new key.
 * \param[out] p_drv        On any return, the driver for the key, if any.
 *                          NULL for a transparent key.
 *
 */
static psa_status_t psa_validate_key_attributes(
    const psa_key_attributes_t *attributes,
    psa_se_drv_table_entry_t **p_drv)
{
    psa_status_t status = PSA_ERROR_INVALID_ARGUMENT;
    psa_key_lifetime_t lifetime = psa_get_key_lifetime(attributes);
    mbedtls_svc_key_id_t key = psa_get_key_id(attributes);

    status = psa_validate_key_location(lifetime, p_drv);
    if (status != PSA_SUCCESS) {
        return status;
    }

    status = psa_validate_key_persistence(lifetime);
    if (status != PSA_SUCCESS) {
        return status;
    }

    if (PSA_KEY_LIFETIME_IS_VOLATILE(lifetime)) {
        if (MBEDTLS_SVC_KEY_ID_GET_KEY_ID(key) != 0) {
            return PSA_ERROR_INVALID_ARGUMENT;
        }
    } else {
        if (!psa_is_valid_key_id(psa_get_key_id(attributes), 0)) {
            return PSA_ERROR_INVALID_ARGUMENT;
        }
    }

    status = psa_validate_key_policy(&attributes->core.policy);
    if (status != PSA_SUCCESS) {
        return status;
    }

    /* Refuse to create overly large keys.
     * Note that this doesn't trigger on import if the attributes don't
     * explicitly specify a size (so psa_get_key_bits returns 0), so
     * psa_import_key() needs its own checks. */
    if (psa_get_key_bits(attributes) > PSA_MAX_KEY_BITS) {
        return PSA_ERROR_NOT_SUPPORTED;
    }

    /* Reject invalid flags. These should not be reachable through the API. */
    if (attributes->core.flags & ~(MBEDTLS_PSA_KA_MASK_EXTERNAL_ONLY |
                                   MBEDTLS_PSA_KA_MASK_DUAL_USE)) {
        return PSA_ERROR_INVALID_ARGUMENT;
    }

    return PSA_SUCCESS;
}

/** Prepare a key slot to receive key material.
 *
 * This function allocates a key slot and sets its metadata.
 *
 * If this function fails, call psa_fail_key_creation().
 *
 * This function is intended to be used as follows:
 * -# Call psa_start_key_creation() to allocate a key slot, prepare
 *    it with the specified attributes, and in case of a volatile key assign it
 *    a volatile key identifier.
 * -# Populate the slot with the key material.
 * -# Call psa_finish_key_creation() to finalize the creation of the slot.
 * In case of failure at any step, stop the sequence and call
 * psa_fail_key_creation().
 *
 * On success, the key slot is locked. It is the responsibility of the caller
 * to unlock the key slot when it does not access it anymore.
 *
 * \param method            An identification of the calling function.
 * \param[in] attributes    Key attributes for the new key.
 * \param[out] p_slot       On success, a pointer to the prepared slot.
 * \param[out] p_drv        On any return, the driver for the key, if any.
 *                          NULL for a transparent key.
 *
 * \retval #PSA_SUCCESS
 *         The key slot is ready to receive key material.
 * \return If this function fails, the key slot is an invalid state.
 *         You must call psa_fail_key_creation() to wipe and free the slot.
 */
static psa_status_t psa_start_key_creation(
    psa_key_creation_method_t method,
    const psa_key_attributes_t *attributes,
    psa_key_slot_t **p_slot,
    psa_se_drv_table_entry_t **p_drv)
{
    psa_status_t status;
    psa_key_id_t volatile_key_id;
    psa_key_slot_t *slot;

    (void) method;
    *p_drv = NULL;

    status = psa_validate_key_attributes(attributes, p_drv);
    if (status != PSA_SUCCESS) {
        return status;
    }

    status = psa_get_empty_key_slot(&volatile_key_id, p_slot);
    if (status != PSA_SUCCESS) {
        return status;
    }
    slot = *p_slot;

    /* We're storing the declared bit-size of the key. It's up to each
     * creation mechanism to verify that this information is correct.
     * It's automatically correct for mechanisms that use the bit-size as
     * an input (generate, device) but not for those where the bit-size
     * is optional (import, copy). In case of a volatile key, assign it the
     * volatile key identifier associated to the slot returned to contain its
     * definition. */

    slot->attr = attributes->core;
    if (PSA_KEY_LIFETIME_IS_VOLATILE(slot->attr.lifetime)) {
#if !defined(MBEDTLS_PSA_CRYPTO_KEY_ID_ENCODES_OWNER)
        slot->attr.id = volatile_key_id;
#else
        slot->attr.id.key_id = volatile_key_id;
#endif
    }

    /* Erase external-only flags from the internal copy. To access
     * external-only flags, query `attributes`. Thanks to the check
     * in psa_validate_key_attributes(), this leaves the dual-use
     * flags and any internal flag that psa_get_empty_key_slot()
     * may have set. */
    slot->attr.flags &= ~MBEDTLS_PSA_KA_MASK_EXTERNAL_ONLY;

#if defined(MBEDTLS_PSA_CRYPTO_SE_C)
    /* For a key in a secure element, we need to do three things
     * when creating or registering a persistent key:
     * create the key file in internal storage, create the
     * key inside the secure element, and update the driver's
     * persistent data. This is done by starting a transaction that will
     * encompass these three actions.
     * For registering a volatile key, we just need to find an appropriate
     * slot number inside the SE. Since the key is designated volatile, creating
     * a transaction is not required. */
    /* The first thing to do is to find a slot number for the new key.
     * We save the slot number in persistent storage as part of the
     * transaction data. It will be needed to recover if the power
     * fails during the key creation process, to clean up on the secure
     * element side after restarting. Obtaining a slot number from the
     * secure element driver updates its persistent state, but we do not yet
     * save the driver's persistent state, so that if the power fails,
     * we can roll back to a state where the key doesn't exist. */
    if (*p_drv != NULL) {
        psa_key_slot_number_t slot_number;
        status = psa_find_se_slot_for_key(attributes, method, *p_drv,
                                          &slot_number);
        if (status != PSA_SUCCESS) {
            return status;
        }

        if (!PSA_KEY_LIFETIME_IS_VOLATILE(attributes->core.lifetime)) {
            psa_crypto_prepare_transaction(PSA_CRYPTO_TRANSACTION_CREATE_KEY);
            psa_crypto_transaction.key.lifetime = slot->attr.lifetime;
            psa_crypto_transaction.key.slot = slot_number;
            psa_crypto_transaction.key.id = slot->attr.id;
            status = psa_crypto_save_transaction();
            if (status != PSA_SUCCESS) {
                (void) psa_crypto_stop_transaction();
                return status;
            }
        }

        status = psa_copy_key_material_into_slot(
            slot, (uint8_t *) (&slot_number), sizeof(slot_number));
        if (status != PSA_SUCCESS) {
            return status;
        }
    }

    if (*p_drv == NULL && method == PSA_KEY_CREATION_REGISTER) {
        /* Key registration only makes sense with a secure element. */
        return PSA_ERROR_INVALID_ARGUMENT;
    }
#endif /* MBEDTLS_PSA_CRYPTO_SE_C */

    return PSA_SUCCESS;
}

/** Finalize the creation of a key once its key material has been set.
 *
 * This entails writing the key to persistent storage.
 *
 * If this function fails, call psa_fail_key_creation().
 * See the documentation of psa_start_key_creation() for the intended use
 * of this function.
 *
 * If the finalization succeeds, the function unlocks the key slot (it was
 * locked by psa_start_key_creation()) and the key slot cannot be accessed
 * anymore as part of the key creation process.
 *
 * \param[in,out] slot  Pointer to the slot with key material.
 * \param[in] driver    The secure element driver for the key,
 *                      or NULL for a transparent key.
 * \param[out] key      On success, identifier of the key. Note that the
 *                      key identifier is also stored in the key slot.
 *
 * \retval #PSA_SUCCESS
 *         The key was successfully created.
 * \retval #PSA_ERROR_INSUFFICIENT_MEMORY \emptydescription
 * \retval #PSA_ERROR_INSUFFICIENT_STORAGE \emptydescription
 * \retval #PSA_ERROR_ALREADY_EXISTS \emptydescription
 * \retval #PSA_ERROR_DATA_INVALID \emptydescription
 * \retval #PSA_ERROR_DATA_CORRUPT \emptydescription
 * \retval #PSA_ERROR_STORAGE_FAILURE \emptydescription
 *
 * \return If this function fails, the key slot is an invalid state.
 *         You must call psa_fail_key_creation() to wipe and free the slot.
 */
static psa_status_t psa_finish_key_creation(
    psa_key_slot_t *slot,
    psa_se_drv_table_entry_t *driver,
    mbedtls_svc_key_id_t *key)
{
    psa_status_t status = PSA_SUCCESS;
    (void) slot;
    (void) driver;

#if defined(MBEDTLS_PSA_CRYPTO_STORAGE_C)
    if (!PSA_KEY_LIFETIME_IS_VOLATILE(slot->attr.lifetime)) {
#if defined(MBEDTLS_PSA_CRYPTO_SE_C)
        if (driver != NULL) {
            psa_se_key_data_storage_t data;
            psa_key_slot_number_t slot_number =
                psa_key_slot_get_slot_number(slot);

            MBEDTLS_STATIC_ASSERT(sizeof(slot_number) ==
                                  sizeof(data.slot_number),
                                  "Slot number size does not match psa_se_key_data_storage_t");

            memcpy(&data.slot_number, &slot_number, sizeof(slot_number));
            status = psa_save_persistent_key(&slot->attr,
                                             (uint8_t *) &data,
                                             sizeof(data));
        } else
#endif /* MBEDTLS_PSA_CRYPTO_SE_C */
        {
            /* Key material is saved in export representation in the slot, so
             * just pass the slot buffer for storage. */
            status = psa_save_persistent_key(&slot->attr,
                                             slot->key.data,
                                             slot->key.bytes);
        }
    }
#endif /* defined(MBEDTLS_PSA_CRYPTO_STORAGE_C) */

#if defined(MBEDTLS_PSA_CRYPTO_SE_C)
    /* Finish the transaction for a key creation. This does not
     * happen when registering an existing key. Detect this case
     * by checking whether a transaction is in progress (actual
     * creation of a persistent key in a secure element requires a transaction,
     * but registration or volatile key creation doesn't use one). */
    if (driver != NULL &&
        psa_crypto_transaction.unknown.type == PSA_CRYPTO_TRANSACTION_CREATE_KEY) {
        status = psa_save_se_persistent_data(driver);
        if (status != PSA_SUCCESS) {
            psa_destroy_persistent_key(slot->attr.id);
            return status;
        }
        status = psa_crypto_stop_transaction();
    }
#endif /* MBEDTLS_PSA_CRYPTO_SE_C */

    if (status == PSA_SUCCESS) {
        *key = slot->attr.id;
        status = psa_unlock_key_slot(slot);
        if (status != PSA_SUCCESS) {
            *key = MBEDTLS_SVC_KEY_ID_INIT;
        }
    }

    return status;
}

/** Abort the creation of a key.
 *
 * You may call this function after calling psa_start_key_creation(),
 * or after psa_finish_key_creation() fails. In other circumstances, this
 * function may not clean up persistent storage.
 * See the documentation of psa_start_key_creation() for the intended use
 * of this function.
 *
 * \param[in,out] slot  Pointer to the slot with key material.
 * \param[in] driver    The secure element driver for the key,
 *                      or NULL for a transparent key.
 */
static void psa_fail_key_creation(psa_key_slot_t *slot,
                                  psa_se_drv_table_entry_t *driver)
{
    (void) driver;

    if (slot == NULL) {
        return;
    }

#if defined(MBEDTLS_PSA_CRYPTO_SE_C)
    /* TODO: If the key has already been created in the secure
     * element, and the failure happened later (when saving metadata
     * to internal storage), we need to destroy the key in the secure
     * element.
     * https://github.com/ARMmbed/mbed-crypto/issues/217
     */

    /* Abort the ongoing transaction if any (there may not be one if
     * the creation process failed before starting one, or if the
     * key creation is a registration of a key in a secure element).
     * Earlier functions must already have done what it takes to undo any
     * partial creation. All that's left is to update the transaction data
     * itself. */
    (void) psa_crypto_stop_transaction();
#endif /* MBEDTLS_PSA_CRYPTO_SE_C */

    psa_wipe_key_slot(slot);
}

/** Validate optional attributes during key creation.
 *
 * Some key attributes are optional during key creation. If they are
 * specified in the attributes structure, check that they are consistent
 * with the data in the slot.
 *
 * This function should be called near the end of key creation, after
 * the slot in memory is fully populated but before saving persistent data.
 */
static psa_status_t psa_validate_optional_attributes(
    const psa_key_slot_t *slot,
    const psa_key_attributes_t *attributes)
{
    if (attributes->core.type != 0) {
        if (attributes->core.type != slot->attr.type) {
            return PSA_ERROR_INVALID_ARGUMENT;
        }
    }

    if (attributes->domain_parameters_size != 0) {
#if defined(MBEDTLS_PSA_BUILTIN_KEY_TYPE_RSA_KEY_PAIR) || \
        defined(MBEDTLS_PSA_BUILTIN_KEY_TYPE_RSA_PUBLIC_KEY)
        if (PSA_KEY_TYPE_IS_RSA(slot->attr.type)) {
            mbedtls_rsa_context *rsa = NULL;
            mbedtls_mpi actual, required;
            int ret = MBEDTLS_ERR_ERROR_CORRUPTION_DETECTED;

            psa_status_t status = mbedtls_psa_rsa_load_representation(
                slot->attr.type,
                slot->key.data,
                slot->key.bytes,
                &rsa);
            if (status != PSA_SUCCESS) {
                return status;
            }

            mbedtls_mpi_init(&actual);
            mbedtls_mpi_init(&required);
            ret = mbedtls_rsa_export(rsa,
                                     NULL, NULL, NULL, NULL, &actual);
            mbedtls_rsa_free(rsa);
            mbedtls_free(rsa);
            if (ret != 0) {
                goto rsa_exit;
            }
            ret = mbedtls_mpi_read_binary(&required,
                                          attributes->domain_parameters,
                                          attributes->domain_parameters_size);
            if (ret != 0) {
                goto rsa_exit;
            }
            if (mbedtls_mpi_cmp_mpi(&actual, &required) != 0) {
                ret = MBEDTLS_ERR_RSA_BAD_INPUT_DATA;
            }
rsa_exit:
            mbedtls_mpi_free(&actual);
            mbedtls_mpi_free(&required);
            if (ret != 0) {
                return mbedtls_to_psa_error(ret);
            }
        } else
#endif /* defined(MBEDTLS_PSA_BUILTIN_KEY_TYPE_RSA_KEY_PAIR) ||
        * defined(MBEDTLS_PSA_BUILTIN_KEY_TYPE_RSA_PUBLIC_KEY) */
        {
            return PSA_ERROR_INVALID_ARGUMENT;
        }
    }

    if (attributes->core.bits != 0) {
        if (attributes->core.bits != slot->attr.bits) {
            return PSA_ERROR_INVALID_ARGUMENT;
        }
    }

    return PSA_SUCCESS;
}

psa_status_t psa_import_key(const psa_key_attributes_t *attributes,
                            const uint8_t *data_external,
                            size_t data_length,
                            mbedtls_svc_key_id_t *key)
{
    psa_status_t status;
    LOCAL_INPUT_DECLARE(data_external, data);
    psa_key_slot_t *slot = NULL;
    psa_se_drv_table_entry_t *driver = NULL;
    size_t bits;

    *key = MBEDTLS_SVC_KEY_ID_INIT;

    /* Reject zero-length symmetric keys (including raw data key objects).
     * This also rejects any key which might be encoded as an empty string,
     * which is never valid. */
    if (data_length == 0) {
        return PSA_ERROR_INVALID_ARGUMENT;
    }

    LOCAL_INPUT_ALLOC(data_external, data_length, data);

    status = psa_start_key_creation(PSA_KEY_CREATION_IMPORT, attributes,
                                    &slot, &driver);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    /* In the case of a transparent key or an opaque key stored in local
     * storage (thus not in the case of generating a key in a secure element
     * or cryptoprocessor with storage), we have to allocate a buffer to
     * hold the generated key material. */
    if (slot->key.data == NULL) {
        status = psa_allocate_buffer_to_slot(slot, data_length);
        if (status != PSA_SUCCESS) {
            goto exit;
        }
    }

    bits = slot->attr.bits;
    status = psa_driver_wrapper_import_key(attributes,
                                           data, data_length,
                                           slot->key.data,
                                           slot->key.bytes,
                                           &slot->key.bytes, &bits);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    if (slot->attr.bits == 0) {
        slot->attr.bits = (psa_key_bits_t) bits;
    } else if (bits != slot->attr.bits) {
        status = PSA_ERROR_INVALID_ARGUMENT;
        goto exit;
    }

    status = psa_validate_optional_attributes(slot, attributes);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    status = psa_finish_key_creation(slot, driver, key);
exit:
    LOCAL_INPUT_FREE(data_external, data);
    if (status != PSA_SUCCESS) {
        psa_fail_key_creation(slot, driver);
    }

    return status;
}

#if defined(MBEDTLS_PSA_CRYPTO_SE_C)
psa_status_t mbedtls_psa_register_se_key(
    const psa_key_attributes_t *attributes)
{
    psa_status_t status;
    psa_key_slot_t *slot = NULL;
    psa_se_drv_table_entry_t *driver = NULL;
    mbedtls_svc_key_id_t key = MBEDTLS_SVC_KEY_ID_INIT;

    /* Leaving attributes unspecified is not currently supported.
     * It could make sense to query the key type and size from the
     * secure element, but not all secure elements support this
     * and the driver HAL doesn't currently support it. */
    if (psa_get_key_type(attributes) == PSA_KEY_TYPE_NONE) {
        return PSA_ERROR_NOT_SUPPORTED;
    }
    if (psa_get_key_bits(attributes) == 0) {
        return PSA_ERROR_NOT_SUPPORTED;
    }

    /* Not usable with volatile keys, even with an appropriate location,
     * due to the API design.
     * https://github.com/Mbed-TLS/mbedtls/issues/9253
     */
    if (PSA_KEY_LIFETIME_IS_VOLATILE(psa_get_key_lifetime(attributes))) {
        return PSA_ERROR_INVALID_ARGUMENT;
    }

    status = psa_start_key_creation(PSA_KEY_CREATION_REGISTER, attributes,
                                    &slot, &driver);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    status = psa_finish_key_creation(slot, driver, &key);

exit:
    if (status != PSA_SUCCESS) {
        psa_fail_key_creation(slot, driver);
    }

    /* Registration doesn't keep the key in RAM. */
    psa_close_key(key);
    return status;
}
#endif /* MBEDTLS_PSA_CRYPTO_SE_C */

static psa_status_t psa_copy_key_material(const psa_key_slot_t *source,
                                          psa_key_slot_t *target)
{
    psa_status_t status = psa_copy_key_material_into_slot(target,
                                                          source->key.data,
                                                          source->key.bytes);
    if (status != PSA_SUCCESS) {
        return status;
    }

    target->attr.type = source->attr.type;
    target->attr.bits = source->attr.bits;

    return PSA_SUCCESS;
}

psa_status_t psa_copy_key(mbedtls_svc_key_id_t source_key,
                          const psa_key_attributes_t *specified_attributes,
                          mbedtls_svc_key_id_t *target_key)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    psa_status_t unlock_status = PSA_ERROR_CORRUPTION_DETECTED;
    psa_key_slot_t *source_slot = NULL;
    psa_key_slot_t *target_slot = NULL;
    psa_key_attributes_t actual_attributes = *specified_attributes;
    psa_se_drv_table_entry_t *driver = NULL;

    *target_key = MBEDTLS_SVC_KEY_ID_INIT;

    status = psa_get_and_lock_transparent_key_slot_with_policy(
        source_key, &source_slot, PSA_KEY_USAGE_COPY, 0);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    status = psa_validate_optional_attributes(source_slot,
                                              specified_attributes);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    status = psa_restrict_key_policy(source_slot->attr.type,
                                     &actual_attributes.core.policy,
                                     &source_slot->attr.policy);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    status = psa_start_key_creation(PSA_KEY_CREATION_COPY, &actual_attributes,
                                    &target_slot, &driver);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

#if defined(MBEDTLS_PSA_CRYPTO_SE_C)
    if (driver != NULL) {
        /* Copying to a secure element is not implemented yet. */
        status = PSA_ERROR_NOT_SUPPORTED;
        goto exit;
    }
#endif /* MBEDTLS_PSA_CRYPTO_SE_C */

    if (psa_key_lifetime_is_external(actual_attributes.core.lifetime)) {
        /*
         * Copying through an opaque driver is not implemented yet, consider
         * a lifetime with an external location as an invalid parameter for
         * now.
         */
        status = PSA_ERROR_INVALID_ARGUMENT;
        goto exit;
    }

    status = psa_copy_key_material(source_slot, target_slot);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    status = psa_finish_key_creation(target_slot, driver, target_key);
exit:
    if (status != PSA_SUCCESS) {
        psa_fail_key_creation(target_slot, driver);
    }

    unlock_status = psa_unlock_key_slot(source_slot);

    return (status == PSA_SUCCESS) ? unlock_status : status;
}



/****************************************************************/
/* Message digests */
/****************************************************************/

static int is_hash_supported(psa_algorithm_t alg)
{
    switch (alg) {
#if defined(PSA_WANT_ALG_MD2)
        case PSA_ALG_MD2:
            return 1;
#endif
#if defined(PSA_WANT_ALG_MD4)
        case PSA_ALG_MD4:
            return 1;
#endif
#if defined(PSA_WANT_ALG_MD5)
        case PSA_ALG_MD5:
            return 1;
#endif
#if defined(PSA_WANT_ALG_RIPEMD160)
        case PSA_ALG_RIPEMD160:
            return 1;
#endif
#if defined(PSA_WANT_ALG_SHA_1)
        case PSA_ALG_SHA_1:
            return 1;
#endif
#if defined(PSA_WANT_ALG_SHA_224)
        case PSA_ALG_SHA_224:
            return 1;
#endif
#if defined(PSA_WANT_ALG_SHA_256)
        case PSA_ALG_SHA_256:
            return 1;
#endif
#if defined(PSA_WANT_ALG_SHA_384)
        case PSA_ALG_SHA_384:
            return 1;
#endif
#if defined(PSA_WANT_ALG_SHA_512)
        case PSA_ALG_SHA_512:
            return 1;
#endif
        default:
            return 0;
    }
}

psa_status_t psa_hash_abort(psa_hash_operation_t *operation)
{
    /* Aborting a non-active operation is allowed */
    if (operation->id == 0) {
        return PSA_SUCCESS;
    }

    psa_status_t status = psa_driver_wrapper_hash_abort(operation);
    operation->id = 0;

    return status;
}

psa_status_t psa_hash_setup(psa_hash_operation_t *operation,
                            psa_algorithm_t alg)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;

    /* A context must be freshly initialized before it can be set up. */
    if (operation->id != 0) {
        status = PSA_ERROR_BAD_STATE;
        goto exit;
    }

    if (!PSA_ALG_IS_HASH(alg)) {
        status = PSA_ERROR_INVALID_ARGUMENT;
        goto exit;
    }

    /* Ensure all of the context is zeroized, since PSA_HASH_OPERATION_INIT only
     * directly zeroes the int-sized dummy member of the context union. */
    memset(&operation->ctx, 0, sizeof(operation->ctx));

    status = psa_driver_wrapper_hash_setup(operation, alg);

exit:
    if (status != PSA_SUCCESS) {
        psa_hash_abort(operation);
    }

    return status;
}

psa_status_t psa_hash_update(psa_hash_operation_t *operation,
                             const uint8_t *input_external,
                             size_t input_length)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    LOCAL_INPUT_DECLARE(input_external, input);

    if (operation->id == 0) {
        status = PSA_ERROR_BAD_STATE;
        goto exit;
    }

    /* Don't require hash implementations to behave correctly on a
     * zero-length input, which may have an invalid pointer. */
    if (input_length == 0) {
        return PSA_SUCCESS;
    }

    LOCAL_INPUT_ALLOC(input_external, input_length, input);
    status = psa_driver_wrapper_hash_update(operation, input, input_length);

exit:
    if (status != PSA_SUCCESS) {
        psa_hash_abort(operation);
    }

    LOCAL_INPUT_FREE(input_external, input);
    return status;
}

static psa_status_t psa_hash_finish_internal(psa_hash_operation_t *operation,
                                             uint8_t *hash,
                                             size_t hash_size,
                                             size_t *hash_length)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;

    *hash_length = 0;
    if (operation->id == 0) {
        return PSA_ERROR_BAD_STATE;
    }

    status = psa_driver_wrapper_hash_finish(
        operation, hash, hash_size, hash_length);
    psa_hash_abort(operation);

    return status;
}

psa_status_t psa_hash_finish(psa_hash_operation_t *operation,
                             uint8_t *hash_external,
                             size_t hash_size,
                             size_t *hash_length)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    LOCAL_OUTPUT_DECLARE(hash_external, hash);

    LOCAL_OUTPUT_ALLOC(hash_external, hash_size, hash);
    status = psa_hash_finish_internal(operation, hash, hash_size, hash_length);

#if !defined(MBEDTLS_PSA_ASSUME_EXCLUSIVE_BUFFERS)
exit:
#endif
    LOCAL_OUTPUT_FREE(hash_external, hash);
    return status;
}

psa_status_t psa_hash_verify(psa_hash_operation_t *operation,
                             const uint8_t *hash_external,
                             size_t hash_length)
{
    uint8_t actual_hash[PSA_HASH_MAX_SIZE];
    size_t actual_hash_length;
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    LOCAL_INPUT_DECLARE(hash_external, hash);

    status = psa_hash_finish_internal(
        operation,
        actual_hash, sizeof(actual_hash),
        &actual_hash_length);

    if (status != PSA_SUCCESS) {
        goto exit;
    }

    if (actual_hash_length != hash_length) {
        status = PSA_ERROR_INVALID_SIGNATURE;
        goto exit;
    }

    LOCAL_INPUT_ALLOC(hash_external, hash_length, hash);
    if (mbedtls_psa_safer_memcmp(hash, actual_hash, actual_hash_length) != 0) {
        status = PSA_ERROR_INVALID_SIGNATURE;
    }

exit:
    mbedtls_platform_zeroize(actual_hash, sizeof(actual_hash));
    if (status != PSA_SUCCESS) {
        psa_hash_abort(operation);
    }
    LOCAL_INPUT_FREE(hash_external, hash);
    return status;
}

psa_status_t psa_hash_compute(psa_algorithm_t alg,
                              const uint8_t *input_external, size_t input_length,
                              uint8_t *hash_external, size_t hash_size,
                              size_t *hash_length)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    LOCAL_INPUT_DECLARE(input_external, input);
    LOCAL_OUTPUT_DECLARE(hash_external, hash);

    *hash_length = 0;
    if (!PSA_ALG_IS_HASH(alg)) {
        return PSA_ERROR_INVALID_ARGUMENT;
    }

    LOCAL_INPUT_ALLOC(input_external, input_length, input);
    LOCAL_OUTPUT_ALLOC(hash_external, hash_size, hash);
    status = psa_driver_wrapper_hash_compute(alg, input, input_length,
                                             hash, hash_size, hash_length);

#if !defined(MBEDTLS_PSA_ASSUME_EXCLUSIVE_BUFFERS)
exit:
#endif
    LOCAL_INPUT_FREE(input_external, input);
    LOCAL_OUTPUT_FREE(hash_external, hash);
    return status;
}

psa_status_t psa_hash_compare(psa_algorithm_t alg,
                              const uint8_t *input_external, size_t input_length,
                              const uint8_t *hash_external, size_t hash_length)
{
    uint8_t actual_hash[PSA_HASH_MAX_SIZE];
    size_t actual_hash_length;
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;

    LOCAL_INPUT_DECLARE(input_external, input);
    LOCAL_INPUT_DECLARE(hash_external, hash);

    if (!PSA_ALG_IS_HASH(alg)) {
        status = PSA_ERROR_INVALID_ARGUMENT;
        return status;
    }

    LOCAL_INPUT_ALLOC(input_external, input_length, input);
    status = psa_driver_wrapper_hash_compute(
        alg, input, input_length,
        actual_hash, sizeof(actual_hash),
        &actual_hash_length);
    if (status != PSA_SUCCESS) {
        goto exit;
    }
    if (actual_hash_length != hash_length) {
        status = PSA_ERROR_INVALID_SIGNATURE;
        goto exit;
    }

    LOCAL_INPUT_ALLOC(hash_external, hash_length, hash);
    if (mbedtls_psa_safer_memcmp(hash, actual_hash, actual_hash_length) != 0) {
        status = PSA_ERROR_INVALID_SIGNATURE;
    }

exit:
    mbedtls_platform_zeroize(actual_hash, sizeof(actual_hash));

    LOCAL_INPUT_FREE(input_external, input);
    LOCAL_INPUT_FREE(hash_external, hash);

    return status;
}

psa_status_t psa_hash_clone(const psa_hash_operation_t *source_operation,
                            psa_hash_operation_t *target_operation)
{
    if (source_operation->id == 0 ||
        target_operation->id != 0) {
        return PSA_ERROR_BAD_STATE;
    }

    psa_status_t status = psa_driver_wrapper_hash_clone(source_operation,
                                                        target_operation);
    if (status != PSA_SUCCESS) {
        psa_hash_abort(target_operation);
    }

    return status;
}


/****************************************************************/
/* MAC */
/****************************************************************/

psa_status_t psa_mac_abort(psa_mac_operation_t *operation)
{
    /* Aborting a non-active operation is allowed */
    if (operation->id == 0) {
        return PSA_SUCCESS;
    }

    psa_status_t status = psa_driver_wrapper_mac_abort(operation);
    operation->mac_size = 0;
    operation->is_sign = 0;
    operation->id = 0;

    return status;
}

static psa_status_t psa_mac_finalize_alg_and_key_validation(
    psa_algorithm_t alg,
    const psa_key_attributes_t *attributes,
    uint8_t *mac_size)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    psa_key_type_t key_type = psa_get_key_type(attributes);
    size_t key_bits = psa_get_key_bits(attributes);

    if (!PSA_ALG_IS_MAC(alg)) {
        return PSA_ERROR_INVALID_ARGUMENT;
    }

    /* Validate the combination of key type and algorithm */
    status = psa_mac_key_can_do(alg, key_type);
    if (status != PSA_SUCCESS) {
        return status;
    }

    /* Get the output length for the algorithm and key combination */
    *mac_size = PSA_MAC_LENGTH(key_type, key_bits, alg);

    if (*mac_size < 4) {
        /* A very short MAC is too short for security since it can be
         * brute-forced. Ancient protocols with 32-bit MACs do exist,
         * so we make this our minimum, even though 32 bits is still
         * too small for security. */
        return PSA_ERROR_NOT_SUPPORTED;
    }

    if (*mac_size > PSA_MAC_LENGTH(key_type, key_bits,
                                   PSA_ALG_FULL_LENGTH_MAC(alg))) {
        /* It's impossible to "truncate" to a larger length than the full length
         * of the algorithm. */
        return PSA_ERROR_INVALID_ARGUMENT;
    }

    if (*mac_size > PSA_MAC_MAX_SIZE) {
        /* PSA_MAC_LENGTH returns the correct length even for a MAC algorithm
         * that is disabled in the compile-time configuration. The result can
         * therefore be larger than PSA_MAC_MAX_SIZE, which does take the
         * configuration into account. In this case, force a return of
         * PSA_ERROR_NOT_SUPPORTED here. Otherwise psa_mac_verify(), or
         * psa_mac_compute(mac_size=PSA_MAC_MAX_SIZE), would return
         * PSA_ERROR_BUFFER_TOO_SMALL for an unsupported algorithm whose MAC size
         * is larger than PSA_MAC_MAX_SIZE, which is misleading and which breaks
         * systematically generated tests. */
        return PSA_ERROR_NOT_SUPPORTED;
    }

    return PSA_SUCCESS;
}

static psa_status_t psa_mac_setup(psa_mac_operation_t *operation,
                                  mbedtls_svc_key_id_t key,
                                  psa_algorithm_t alg,
                                  int is_sign)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    psa_status_t unlock_status = PSA_ERROR_CORRUPTION_DETECTED;
    psa_key_attributes_t attributes;
    psa_key_slot_t *slot = NULL;

    /* A context must be freshly initialized before it can be set up. */
    if (operation->id != 0) {
        status = PSA_ERROR_BAD_STATE;
        goto exit;
    }

    status = psa_get_and_lock_key_slot_with_policy(
        key,
        &slot,
        is_sign ? PSA_KEY_USAGE_SIGN_MESSAGE : PSA_KEY_USAGE_VERIFY_MESSAGE,
        alg);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    attributes = (psa_key_attributes_t) {
        .core = slot->attr
    };

    status = psa_mac_finalize_alg_and_key_validation(alg, &attributes,
                                                     &operation->mac_size);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    operation->is_sign = is_sign;
    /* Dispatch the MAC setup call with validated input */
    if (is_sign) {
        status = psa_driver_wrapper_mac_sign_setup(operation,
                                                   &attributes,
                                                   slot->key.data,
                                                   slot->key.bytes,
                                                   alg);
    } else {
        status = psa_driver_wrapper_mac_verify_setup(operation,
                                                     &attributes,
                                                     slot->key.data,
                                                     slot->key.bytes,
                                                     alg);
    }

exit:
    if (status != PSA_SUCCESS) {
        psa_mac_abort(operation);
    }

    unlock_status = psa_unlock_key_slot(slot);

    return (status == PSA_SUCCESS) ? unlock_status : status;
}

psa_status_t psa_mac_sign_setup(psa_mac_operation_t *operation,
                                mbedtls_svc_key_id_t key,
                                psa_algorithm_t alg)
{
    return psa_mac_setup(operation, key, alg, 1);
}

psa_status_t psa_mac_verify_setup(psa_mac_operation_t *operation,
                                  mbedtls_svc_key_id_t key,
                                  psa_algorithm_t alg)
{
    return psa_mac_setup(operation, key, alg, 0);
}

psa_status_t psa_mac_update(psa_mac_operation_t *operation,
                            const uint8_t *input_external,
                            size_t input_length)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    LOCAL_INPUT_DECLARE(input_external, input);

    if (operation->id == 0) {
        status = PSA_ERROR_BAD_STATE;
        return status;
    }

    /* Don't require hash implementations to behave correctly on a
     * zero-length input, which may have an invalid pointer. */
    if (input_length == 0) {
        status = PSA_SUCCESS;
        return status;
    }

    LOCAL_INPUT_ALLOC(input_external, input_length, input);
    status = psa_driver_wrapper_mac_update(operation, input, input_length);

    if (status != PSA_SUCCESS) {
        psa_mac_abort(operation);
    }

#if !defined(MBEDTLS_PSA_ASSUME_EXCLUSIVE_BUFFERS)
exit:
#endif
    LOCAL_INPUT_FREE(input_external, input);

    return status;
}

psa_status_t psa_mac_sign_finish(psa_mac_operation_t *operation,
                                 uint8_t *mac_external,
                                 size_t mac_size,
                                 size_t *mac_length)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    psa_status_t abort_status = PSA_ERROR_CORRUPTION_DETECTED;
    LOCAL_OUTPUT_DECLARE(mac_external, mac);
    LOCAL_OUTPUT_ALLOC(mac_external, mac_size, mac);

    if (operation->id == 0) {
        status = PSA_ERROR_BAD_STATE;
        goto exit;
    }

    if (!operation->is_sign) {
        status = PSA_ERROR_BAD_STATE;
        goto exit;
    }

    /* Sanity check. This will guarantee that mac_size != 0 (and so mac != NULL)
     * once all the error checks are done. */
    if (operation->mac_size == 0) {
        status = PSA_ERROR_BAD_STATE;
        goto exit;
    }

    if (mac_size < operation->mac_size) {
        status = PSA_ERROR_BUFFER_TOO_SMALL;
        goto exit;
    }

    status = psa_driver_wrapper_mac_sign_finish(operation,
                                                mac, operation->mac_size,
                                                mac_length);

exit:
    /* In case of success, set the potential excess room in the output buffer
     * to an invalid value, to avoid potentially leaking a longer MAC.
     * In case of error, set the output length and content to a safe default,
     * such that in case the caller misses an error check, the output would be
     * an unachievable MAC.
     */
    if (status != PSA_SUCCESS) {
        *mac_length = mac_size;
        operation->mac_size = 0;
    }

    if ((mac != NULL) && (mac_size > operation->mac_size)) {
        memset(&mac[operation->mac_size], '!',
               mac_size - operation->mac_size);
    }

    abort_status = psa_mac_abort(operation);
    LOCAL_OUTPUT_FREE(mac_external, mac);

    return status == PSA_SUCCESS ? abort_status : status;
}

psa_status_t psa_mac_verify_finish(psa_mac_operation_t *operation,
                                   const uint8_t *mac_external,
                                   size_t mac_length)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    psa_status_t abort_status = PSA_ERROR_CORRUPTION_DETECTED;
    LOCAL_INPUT_DECLARE(mac_external, mac);

    if (operation->id == 0) {
        status = PSA_ERROR_BAD_STATE;
        goto exit;
    }

    if (operation->is_sign) {
        status = PSA_ERROR_BAD_STATE;
        goto exit;
    }

    if (operation->mac_size != mac_length) {
        status = PSA_ERROR_INVALID_SIGNATURE;
        goto exit;
    }

    LOCAL_INPUT_ALLOC(mac_external, mac_length, mac);
    status = psa_driver_wrapper_mac_verify_finish(operation,
                                                  mac, mac_length);

exit:
    abort_status = psa_mac_abort(operation);
    LOCAL_INPUT_FREE(mac_external, mac);

    return status == PSA_SUCCESS ? abort_status : status;
}

static psa_status_t psa_mac_compute_internal(mbedtls_svc_key_id_t key,
                                             psa_algorithm_t alg,
                                             const uint8_t *input,
                                             size_t input_length,
                                             uint8_t *mac,
                                             size_t mac_size,
                                             size_t *mac_length,
                                             int is_sign)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    psa_status_t unlock_status = PSA_ERROR_CORRUPTION_DETECTED;
    psa_key_attributes_t attributes;
    psa_key_slot_t *slot;
    uint8_t operation_mac_size = 0;

    status = psa_get_and_lock_key_slot_with_policy(
        key,
        &slot,
        is_sign ? PSA_KEY_USAGE_SIGN_MESSAGE : PSA_KEY_USAGE_VERIFY_MESSAGE,
        alg);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    attributes = (psa_key_attributes_t) {
        .core = slot->attr
    };

    status = psa_mac_finalize_alg_and_key_validation(alg, &attributes,
                                                     &operation_mac_size);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    if (mac_size < operation_mac_size) {
        status = PSA_ERROR_BUFFER_TOO_SMALL;
        goto exit;
    }

    status = psa_driver_wrapper_mac_compute(
        &attributes,
        slot->key.data, slot->key.bytes,
        alg,
        input, input_length,
        mac, operation_mac_size, mac_length);

exit:
    /* In case of success, set the potential excess room in the output buffer
     * to an invalid value, to avoid potentially leaking a longer MAC.
     * In case of error, set the output length and content to a safe default,
     * such that in case the caller misses an error check, the output would be
     * an unachievable MAC.
     */
    if (status != PSA_SUCCESS) {
        *mac_length = mac_size;
        operation_mac_size = 0;
    }
    if (mac_size > operation_mac_size) {
        memset(&mac[operation_mac_size], '!', mac_size - operation_mac_size);
    }

    unlock_status = psa_unlock_key_slot(slot);

    return (status == PSA_SUCCESS) ? unlock_status : status;
}

psa_status_t psa_mac_compute(mbedtls_svc_key_id_t key,
                             psa_algorithm_t alg,
                             const uint8_t *input_external,
                             size_t input_length,
                             uint8_t *mac_external,
                             size_t mac_size,
                             size_t *mac_length)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    LOCAL_INPUT_DECLARE(input_external, input);
    LOCAL_OUTPUT_DECLARE(mac_external, mac);

    LOCAL_INPUT_ALLOC(input_external, input_length, input);
    LOCAL_OUTPUT_ALLOC(mac_external, mac_size, mac);
    status = psa_mac_compute_internal(key, alg,
                                      input, input_length,
                                      mac, mac_size, mac_length, 1);

#if !defined(MBEDTLS_PSA_ASSUME_EXCLUSIVE_BUFFERS)
exit:
#endif
    LOCAL_INPUT_FREE(input_external, input);
    LOCAL_OUTPUT_FREE(mac_external, mac);

    return status;
}

psa_status_t psa_mac_verify(mbedtls_svc_key_id_t key,
                            psa_algorithm_t alg,
                            const uint8_t *input_external,
                            size_t input_length,
                            const uint8_t *mac_external,
                            size_t mac_length)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    uint8_t actual_mac[PSA_MAC_MAX_SIZE];
    size_t actual_mac_length;
    LOCAL_INPUT_DECLARE(input_external, input);
    LOCAL_INPUT_DECLARE(mac_external, mac);

    LOCAL_INPUT_ALLOC(input_external, input_length, input);
    status = psa_mac_compute_internal(key, alg,
                                      input, input_length,
                                      actual_mac, sizeof(actual_mac),
                                      &actual_mac_length, 0);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    if (mac_length != actual_mac_length) {
        status = PSA_ERROR_INVALID_SIGNATURE;
        goto exit;
    }

    LOCAL_INPUT_ALLOC(mac_external, mac_length, mac);
    if (mbedtls_psa_safer_memcmp(mac, actual_mac, actual_mac_length) != 0) {
        status = PSA_ERROR_INVALID_SIGNATURE;
        goto exit;
    }

exit:
    mbedtls_platform_zeroize(actual_mac, sizeof(actual_mac));
    LOCAL_INPUT_FREE(input_external, input);
    LOCAL_INPUT_FREE(mac_external, mac);

    return status;
}

/****************************************************************/
/* Asymmetric cryptography */
/****************************************************************/

static psa_status_t psa_sign_verify_check_alg(int input_is_message,
                                              psa_algorithm_t alg)
{
    if (input_is_message) {
        if (!PSA_ALG_IS_SIGN_MESSAGE(alg)) {
            return PSA_ERROR_INVALID_ARGUMENT;
        }
    }

    psa_algorithm_t hash_alg = 0;
    if (PSA_ALG_IS_SIGN_HASH(alg)) {
        hash_alg = PSA_ALG_SIGN_GET_HASH(alg);
    }

    /* Now hash_alg==0 if alg by itself doesn't need a hash.
     * This is good enough for sign-hash, but a guaranteed failure for
     * sign-message which needs to hash first for all algorithms
     * supported at the moment. */

    if (hash_alg == 0 && input_is_message) {
        return PSA_ERROR_INVALID_ARGUMENT;
    }
    if (hash_alg == PSA_ALG_ANY_HASH) {
        return PSA_ERROR_INVALID_ARGUMENT;
    }
    /* Give up immediately if the hash is not supported. This has
     * several advantages:
     * - For mechanisms that don't use the hash at all (e.g.
     *   ECDSA verification, randomized ECDSA signature), without
     *   this check, the operation would succeed even though it has
     *   been given an invalid argument. This would not be insecure
     *   since the hash was not necessary, but it would be weird.
     * - For mechanisms that do use the hash, we avoid an error
     *   deep inside the execution. In principle this doesn't matter,
     *   but there is a little more risk of a bug in error handling
     *   deep inside than in this preliminary check.
     * - When calling a driver, the driver might be capable of using
     *   a hash that the core doesn't support. This could potentially
     *   result in a buffer overflow if the hash is larger than the
     *   maximum hash size assumed by the core.
     * - Returning a consistent error makes it possible to test
     *   not-supported hashes in a consistent way.
     */
    if (hash_alg != 0 && !is_hash_supported(hash_alg)) {
        return PSA_ERROR_NOT_SUPPORTED;
    }

    return PSA_SUCCESS;
}

static psa_status_t psa_sign_internal(mbedtls_svc_key_id_t key,
                                      int input_is_message,
                                      psa_algorithm_t alg,
                                      const uint8_t *input,
                                      size_t input_length,
                                      uint8_t *signature,
                                      size_t signature_size,
                                      size_t *signature_length)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    psa_status_t unlock_status = PSA_ERROR_CORRUPTION_DETECTED;
    psa_key_attributes_t attributes;
    psa_key_slot_t *slot;

    *signature_length = 0;

    status = psa_sign_verify_check_alg(input_is_message, alg);
    if (status != PSA_SUCCESS) {
        return status;
    }

    /* Immediately reject a zero-length signature buffer. This guarantees
     * that signature must be a valid pointer. (On the other hand, the input
     * buffer can in principle be empty since it doesn't actually have
     * to be a hash.) */
    if (signature_size == 0) {
        return PSA_ERROR_BUFFER_TOO_SMALL;
    }

    status = psa_get_and_lock_key_slot_with_policy(
        key, &slot,
        input_is_message ? PSA_KEY_USAGE_SIGN_MESSAGE :
        PSA_KEY_USAGE_SIGN_HASH,
        alg);

    if (status != PSA_SUCCESS) {
        goto exit;
    }

    if (!PSA_KEY_TYPE_IS_KEY_PAIR(slot->attr.type)) {
        status = PSA_ERROR_INVALID_ARGUMENT;
        goto exit;
    }

    attributes = (psa_key_attributes_t)  {
        .core = slot->attr
    };

    if (input_is_message) {
        status = psa_driver_wrapper_sign_message(
            &attributes, slot->key.data, slot->key.bytes,
            alg, input, input_length,
            signature, signature_size, signature_length);
    } else {

        status = psa_driver_wrapper_sign_hash(
            &attributes, slot->key.data, slot->key.bytes,
            alg, input, input_length,
            signature, signature_size, signature_length);
    }


exit:
    /* Fill the unused part of the output buffer (the whole buffer on error,
     * the trailing part on success) with something that isn't a valid signature
     * (barring an attack on the signature and deliberately-crafted input),
     * in case the caller doesn't check the return status properly. */
    if (status == PSA_SUCCESS) {
        memset(signature + *signature_length, '!',
               signature_size - *signature_length);
    } else {
        memset(signature, '!', signature_size);
    }
    /* If signature_size is 0 then we have nothing to do. We must not call
     * memset because signature may be NULL in this case. */

    unlock_status = psa_unlock_key_slot(slot);

    return (status == PSA_SUCCESS) ? unlock_status : status;
}

static psa_status_t psa_verify_internal(mbedtls_svc_key_id_t key,
                                        int input_is_message,
                                        psa_algorithm_t alg,
                                        const uint8_t *input,
                                        size_t input_length,
                                        const uint8_t *signature,
                                        size_t signature_length)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    psa_status_t unlock_status = PSA_ERROR_CORRUPTION_DETECTED;
    psa_key_slot_t *slot;

    status = psa_sign_verify_check_alg(input_is_message, alg);
    if (status != PSA_SUCCESS) {
        return status;
    }

    status = psa_get_and_lock_key_slot_with_policy(
        key, &slot,
        input_is_message ? PSA_KEY_USAGE_VERIFY_MESSAGE :
        PSA_KEY_USAGE_VERIFY_HASH,
        alg);

    if (status != PSA_SUCCESS) {
        return status;
    }

    psa_key_attributes_t attributes = {
        .core = slot->attr
    };

    if (input_is_message) {
        status = psa_driver_wrapper_verify_message(
            &attributes, slot->key.data, slot->key.bytes,
            alg, input, input_length,
            signature, signature_length);
    } else {
        status = psa_driver_wrapper_verify_hash(
            &attributes, slot->key.data, slot->key.bytes,
            alg, input, input_length,
            signature, signature_length);
    }

    unlock_status = psa_unlock_key_slot(slot);

    return (status == PSA_SUCCESS) ? unlock_status : status;

}

psa_status_t psa_sign_message_builtin(
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer,
    size_t key_buffer_size,
    psa_algorithm_t alg,
    const uint8_t *input,
    size_t input_length,
    uint8_t *signature,
    size_t signature_size,
    size_t *signature_length)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;

    if (PSA_ALG_IS_SIGN_HASH(alg)) {
        size_t hash_length;
        uint8_t hash[PSA_HASH_MAX_SIZE];

        status = psa_driver_wrapper_hash_compute(
            PSA_ALG_SIGN_GET_HASH(alg),
            input, input_length,
            hash, sizeof(hash), &hash_length);

        if (status != PSA_SUCCESS) {
            return status;
        }

        return psa_driver_wrapper_sign_hash(
            attributes, key_buffer, key_buffer_size,
            alg, hash, hash_length,
            signature, signature_size, signature_length);
    }

    return PSA_ERROR_NOT_SUPPORTED;
}

psa_status_t psa_sign_message(mbedtls_svc_key_id_t key,
                              psa_algorithm_t alg,
                              const uint8_t *input_external,
                              size_t input_length,
                              uint8_t *signature_external,
                              size_t signature_size,
                              size_t *signature_length)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    LOCAL_INPUT_DECLARE(input_external, input);
    LOCAL_OUTPUT_DECLARE(signature_external, signature);

    LOCAL_INPUT_ALLOC(input_external, input_length, input);
    LOCAL_OUTPUT_ALLOC(signature_external, signature_size, signature);
    status = psa_sign_internal(key, 1, alg, input, input_length, signature,
                               signature_size, signature_length);

#if !defined(MBEDTLS_PSA_ASSUME_EXCLUSIVE_BUFFERS)
exit:
#endif
    LOCAL_INPUT_FREE(input_external, input);
    LOCAL_OUTPUT_FREE(signature_external, signature);
    return status;
}

psa_status_t psa_verify_message_builtin(
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer,
    size_t key_buffer_size,
    psa_algorithm_t alg,
    const uint8_t *input,
    size_t input_length,
    const uint8_t *signature,
    size_t signature_length)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;

    if (PSA_ALG_IS_SIGN_HASH(alg)) {
        size_t hash_length;
        uint8_t hash[PSA_HASH_MAX_SIZE];

        status = psa_driver_wrapper_hash_compute(
            PSA_ALG_SIGN_GET_HASH(alg),
            input, input_length,
            hash, sizeof(hash), &hash_length);

        if (status != PSA_SUCCESS) {
            return status;
        }

        return psa_driver_wrapper_verify_hash(
            attributes, key_buffer, key_buffer_size,
            alg, hash, hash_length,
            signature, signature_length);
    }

    return PSA_ERROR_NOT_SUPPORTED;
}

psa_status_t psa_verify_message(mbedtls_svc_key_id_t key,
                                psa_algorithm_t alg,
                                const uint8_t *input_external,
                                size_t input_length,
                                const uint8_t *signature_external,
                                size_t signature_length)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    LOCAL_INPUT_DECLARE(input_external, input);
    LOCAL_INPUT_DECLARE(signature_external, signature);

    LOCAL_INPUT_ALLOC(input_external, input_length, input);
    LOCAL_INPUT_ALLOC(signature_external, signature_length, signature);
    status = psa_verify_internal(key, 1, alg, input, input_length, signature,
                                 signature_length);

#if !defined(MBEDTLS_PSA_ASSUME_EXCLUSIVE_BUFFERS)
exit:
#endif
    LOCAL_INPUT_FREE(input_external, input);
    LOCAL_INPUT_FREE(signature_external, signature);

    return status;
}

psa_status_t psa_sign_hash_builtin(
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer, size_t key_buffer_size,
    psa_algorithm_t alg, const uint8_t *hash, size_t hash_length,
    uint8_t *signature, size_t signature_size, size_t *signature_length)
{
    if (attributes->core.type == PSA_KEY_TYPE_RSA_KEY_PAIR) {
        if (PSA_ALG_IS_RSA_PKCS1V15_SIGN(alg) ||
            PSA_ALG_IS_RSA_PSS(alg)) {
#if defined(MBEDTLS_PSA_BUILTIN_ALG_RSA_PKCS1V15_SIGN) || \
            defined(MBEDTLS_PSA_BUILTIN_ALG_RSA_PSS)
            return mbedtls_psa_rsa_sign_hash(
                attributes,
                key_buffer, key_buffer_size,
                alg, hash, hash_length,
                signature, signature_size, signature_length);
#endif /* defined(MBEDTLS_PSA_BUILTIN_ALG_RSA_PKCS1V15_SIGN) ||
        * defined(MBEDTLS_PSA_BUILTIN_ALG_RSA_PSS) */
        } else {
            return PSA_ERROR_INVALID_ARGUMENT;
        }
    } else if (PSA_KEY_TYPE_IS_ECC(attributes->core.type)) {
        if (PSA_ALG_IS_ECDSA(alg)) {
#if defined(MBEDTLS_PSA_BUILTIN_ALG_ECDSA) || \
            defined(MBEDTLS_PSA_BUILTIN_ALG_DETERMINISTIC_ECDSA)
            return mbedtls_psa_ecdsa_sign_hash(
                attributes,
                key_buffer, key_buffer_size,
                alg, hash, hash_length,
                signature, signature_size, signature_length);
#endif /* defined(MBEDTLS_PSA_BUILTIN_ALG_ECDSA) ||
        * defined(MBEDTLS_PSA_BUILTIN_ALG_DETERMINISTIC_ECDSA) */
        } else {
            return PSA_ERROR_INVALID_ARGUMENT;
        }
    }

    (void) key_buffer;
    (void) key_buffer_size;
    (void) hash;
    (void) hash_length;
    (void) signature;
    (void) signature_size;
    (void) signature_length;

    return PSA_ERROR_NOT_SUPPORTED;
}

psa_status_t psa_sign_hash(mbedtls_svc_key_id_t key,
                           psa_algorithm_t alg,
                           const uint8_t *hash_external,
                           size_t hash_length,
                           uint8_t *signature_external,
                           size_t signature_size,
                           size_t *signature_length)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    LOCAL_INPUT_DECLARE(hash_external, hash);
    LOCAL_OUTPUT_DECLARE(signature_external, signature);

    LOCAL_INPUT_ALLOC(hash_external, hash_length, hash);
    LOCAL_OUTPUT_ALLOC(signature_external, signature_size, signature);
    status = psa_sign_internal(key, 0, alg, hash, hash_length, signature,
                               signature_size, signature_length);

#if !defined(MBEDTLS_PSA_ASSUME_EXCLUSIVE_BUFFERS)
exit:
#endif
    LOCAL_INPUT_FREE(hash_external, hash);
    LOCAL_OUTPUT_FREE(signature_external, signature);

    return status;
}

psa_status_t psa_verify_hash_builtin(
    const psa_key_attributes_t *attributes,
    const uint8_t *key_buffer, size_t key_buffer_size,
    psa_algorithm_t alg, const uint8_t *hash, size_t hash_length,
    const uint8_t *signature, size_t signature_length)
{
    if (PSA_KEY_TYPE_IS_RSA(attributes->core.type)) {
        if (PSA_ALG_IS_RSA_PKCS1V15_SIGN(alg) ||
            PSA_ALG_IS_RSA_PSS(alg)) {
#if defined(MBEDTLS_PSA_BUILTIN_ALG_RSA_PKCS1V15_SIGN) || \
            defined(MBEDTLS_PSA_BUILTIN_ALG_RSA_PSS)
            return mbedtls_psa_rsa_verify_hash(
                attributes,
                key_buffer, key_buffer_size,
                alg, hash, hash_length,
                signature, signature_length);
#endif /* defined(MBEDTLS_PSA_BUILTIN_ALG_RSA_PKCS1V15_SIGN) ||
        * defined(MBEDTLS_PSA_BUILTIN_ALG_RSA_PSS) */
        } else {
            return PSA_ERROR_INVALID_ARGUMENT;
        }
    } else if (PSA_KEY_TYPE_IS_ECC(attributes->core.type)) {
        if (PSA_ALG_IS_ECDSA(alg)) {
#if defined(MBEDTLS_PSA_BUILTIN_ALG_ECDSA) || \
            defined(MBEDTLS_PSA_BUILTIN_ALG_DETERMINISTIC_ECDSA)
            return mbedtls_psa_ecdsa_verify_hash(
                attributes,
                key_buffer, key_buffer_size,
                alg, hash, hash_length,
                signature, signature_length);
#endif /* defined(MBEDTLS_PSA_BUILTIN_ALG_ECDSA) ||
        * defined(MBEDTLS_PSA_BUILTIN_ALG_DETERMINISTIC_ECDSA) */
        } else {
            return PSA_ERROR_INVALID_ARGUMENT;
        }
    }

    (void) key_buffer;
    (void) key_buffer_size;
    (void) hash;
    (void) hash_length;
    (void) signature;
    (void) signature_length;

    return PSA_ERROR_NOT_SUPPORTED;
}

psa_status_t psa_verify_hash(mbedtls_svc_key_id_t key,
                             psa_algorithm_t alg,
                             const uint8_t *hash_external,
                             size_t hash_length,
                             const uint8_t *signature_external,
                             size_t signature_length)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    LOCAL_INPUT_DECLARE(hash_external, hash);
    LOCAL_INPUT_DECLARE(signature_external, signature);

    LOCAL_INPUT_ALLOC(hash_external, hash_length, hash);
    LOCAL_INPUT_ALLOC(signature_external, signature_length, signature);
    status = psa_verify_internal(key, 0, alg, hash, hash_length, signature,
                                 signature_length);

#if !defined(MBEDTLS_PSA_ASSUME_EXCLUSIVE_BUFFERS)
exit:
#endif
    LOCAL_INPUT_FREE(hash_external, hash);
    LOCAL_INPUT_FREE(signature_external, signature);

    return status;
}

#if defined(MBEDTLS_PSA_BUILTIN_ALG_RSA_OAEP)
static void psa_rsa_oaep_set_padding_mode(psa_algorithm_t alg,
                                          mbedtls_rsa_context *rsa)
{
    psa_algorithm_t hash_alg = PSA_ALG_RSA_OAEP_GET_HASH(alg);
    const mbedtls_md_info_t *md_info = mbedtls_md_info_from_psa(hash_alg);
    mbedtls_md_type_t md_alg = mbedtls_md_get_type(md_info);
    mbedtls_rsa_set_padding(rsa, MBEDTLS_RSA_PKCS_V21, md_alg);
}
#endif /* defined(MBEDTLS_PSA_BUILTIN_ALG_RSA_OAEP) */

psa_status_t psa_asymmetric_encrypt(mbedtls_svc_key_id_t key,
                                    psa_algorithm_t alg,
                                    const uint8_t *input_external,
                                    size_t input_length,
                                    const uint8_t *salt_external,
                                    size_t salt_length,
                                    uint8_t *output_external,
                                    size_t output_size,
                                    size_t *output_length)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    psa_status_t unlock_status = PSA_ERROR_CORRUPTION_DETECTED;
    psa_key_slot_t *slot;
    LOCAL_INPUT_DECLARE(input_external, input);
    LOCAL_INPUT_DECLARE(salt_external, salt);
    LOCAL_OUTPUT_DECLARE(output_external, output);

    (void) input;
    (void) input_length;
    (void) salt;
    (void) output;
    (void) output_size;

    *output_length = 0;

    if (!PSA_ALG_IS_RSA_OAEP(alg) && salt_length != 0) {
        return PSA_ERROR_INVALID_ARGUMENT;
    }

    status = psa_get_and_lock_transparent_key_slot_with_policy(
        key, &slot, PSA_KEY_USAGE_ENCRYPT, alg);
    if (status != PSA_SUCCESS) {
        return status;
    }
    if (!(PSA_KEY_TYPE_IS_PUBLIC_KEY(slot->attr.type) ||
          PSA_KEY_TYPE_IS_KEY_PAIR(slot->attr.type))) {
        status = PSA_ERROR_INVALID_ARGUMENT;
        goto exit;
    }

    if (PSA_KEY_TYPE_IS_RSA(slot->attr.type)) {
#if defined(MBEDTLS_PSA_BUILTIN_ALG_RSA_PKCS1V15_CRYPT) || \
        defined(MBEDTLS_PSA_BUILTIN_ALG_RSA_OAEP)
        mbedtls_rsa_context *rsa = NULL;
        status = mbedtls_psa_rsa_load_representation(slot->attr.type,
                                                     slot->key.data,
                                                     slot->key.bytes,
                                                     &rsa);
        if (status != PSA_SUCCESS) {
            goto rsa_exit;
        }

        if (output_size < mbedtls_rsa_get_len(rsa)) {
            status = PSA_ERROR_BUFFER_TOO_SMALL;
            goto rsa_exit;
        }
#endif /* defined(MBEDTLS_PSA_BUILTIN_ALG_RSA_PKCS1V15_CRYPT) ||
        * defined(MBEDTLS_PSA_BUILTIN_ALG_RSA_OAEP) */
        LOCAL_INPUT_ALLOC(input_external, input_length, input);
        LOCAL_INPUT_ALLOC(salt_external, salt_length, salt);
        LOCAL_OUTPUT_ALLOC(output_external, output_size, output);
        if (alg == PSA_ALG_RSA_PKCS1V15_CRYPT) {
#if defined(MBEDTLS_PSA_BUILTIN_ALG_RSA_PKCS1V15_CRYPT)
            status = mbedtls_to_psa_error(
                mbedtls_rsa_pkcs1_encrypt(rsa,
                                          mbedtls_psa_get_random,
                                          MBEDTLS_PSA_RANDOM_STATE,
                                          MBEDTLS_RSA_PUBLIC,
                                          input_length,
                                          input,
                                          output));
#else
            status = PSA_ERROR_NOT_SUPPORTED;
#endif /* MBEDTLS_PSA_BUILTIN_ALG_RSA_PKCS1V15_CRYPT */
        } else
        if (PSA_ALG_IS_RSA_OAEP(alg)) {
#if defined(MBEDTLS_PSA_BUILTIN_ALG_RSA_OAEP)
            psa_rsa_oaep_set_padding_mode(alg, rsa);
            status = mbedtls_to_psa_error(
                mbedtls_rsa_rsaes_oaep_encrypt(rsa,
                                               mbedtls_psa_get_random,
                                               MBEDTLS_PSA_RANDOM_STATE,
                                               MBEDTLS_RSA_PUBLIC,
                                               salt, salt_length,
                                               input_length,
                                               input,
                                               output));
#else
            status = PSA_ERROR_NOT_SUPPORTED;
#endif /* MBEDTLS_PSA_BUILTIN_ALG_RSA_OAEP */
        } else {
            status = PSA_ERROR_INVALID_ARGUMENT;
        }
#if defined(MBEDTLS_PSA_BUILTIN_ALG_RSA_PKCS1V15_CRYPT) || \
        defined(MBEDTLS_PSA_BUILTIN_ALG_RSA_OAEP)
rsa_exit:
        if (status == PSA_SUCCESS) {
            *output_length = mbedtls_rsa_get_len(rsa);
        }

        mbedtls_rsa_free(rsa);
        mbedtls_free(rsa);
#endif /* defined(MBEDTLS_PSA_BUILTIN_ALG_RSA_PKCS1V15_CRYPT) ||
        * defined(MBEDTLS_PSA_BUILTIN_ALG_RSA_OAEP) */
    } else {
        status = PSA_ERROR_NOT_SUPPORTED;
    }

exit:
    unlock_status = psa_unlock_key_slot(slot);

    LOCAL_INPUT_FREE(input_external, input);
    LOCAL_INPUT_FREE(salt_external, salt);
    LOCAL_OUTPUT_FREE(output_external, output);

    return (status == PSA_SUCCESS) ? unlock_status : status;
}

psa_status_t psa_asymmetric_decrypt(mbedtls_svc_key_id_t key,
                                    psa_algorithm_t alg,
                                    const uint8_t *input_external,
                                    size_t input_length,
                                    const uint8_t *salt_external,
                                    size_t salt_length,
                                    uint8_t *output_external,
                                    size_t output_size,
                                    size_t *output_length)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    psa_status_t unlock_status = PSA_ERROR_CORRUPTION_DETECTED;
    psa_key_slot_t *slot;
    LOCAL_INPUT_DECLARE(input_external, input);
    LOCAL_INPUT_DECLARE(salt_external, salt);
    LOCAL_OUTPUT_DECLARE(output_external, output);

    (void) input;
    (void) input_length;
    (void) salt;
    (void) output;
    (void) output_size;

    *output_length = 0;

    if (!PSA_ALG_IS_RSA_OAEP(alg) && salt_length != 0) {
        return PSA_ERROR_INVALID_ARGUMENT;
    }

    status = psa_get_and_lock_transparent_key_slot_with_policy(
        key, &slot, PSA_KEY_USAGE_DECRYPT, alg);
    if (status != PSA_SUCCESS) {
        return status;
    }
    if (!PSA_KEY_TYPE_IS_KEY_PAIR(slot->attr.type)) {
        status = PSA_ERROR_INVALID_ARGUMENT;
        goto exit;
    }

    if (slot->attr.type == PSA_KEY_TYPE_RSA_KEY_PAIR) {
#if defined(MBEDTLS_PSA_BUILTIN_ALG_RSA_PKCS1V15_CRYPT) || \
        defined(MBEDTLS_PSA_BUILTIN_ALG_RSA_OAEP)
        mbedtls_rsa_context *rsa = NULL;
        status = mbedtls_psa_rsa_load_representation(slot->attr.type,
                                                     slot->key.data,
                                                     slot->key.bytes,
                                                     &rsa);
        if (status != PSA_SUCCESS) {
            goto exit;
        }

        if (input_length != mbedtls_rsa_get_len(rsa)) {
            status = PSA_ERROR_INVALID_ARGUMENT;
            goto rsa_exit;
        }
#endif /* defined(MBEDTLS_PSA_BUILTIN_ALG_RSA_PKCS1V15_CRYPT) ||
        * defined(MBEDTLS_PSA_BUILTIN_ALG_RSA_OAEP) */
        LOCAL_INPUT_ALLOC(input_external, input_length, input);
        LOCAL_INPUT_ALLOC(salt_external, salt_length, salt);
        LOCAL_OUTPUT_ALLOC(output_external, output_size, output);
        if (alg == PSA_ALG_RSA_PKCS1V15_CRYPT) {
#if defined(MBEDTLS_PSA_BUILTIN_ALG_RSA_PKCS1V15_CRYPT)
            status = mbedtls_to_psa_error(
                mbedtls_rsa_pkcs1_decrypt(rsa,
                                          mbedtls_psa_get_random,
                                          MBEDTLS_PSA_RANDOM_STATE,
                                          MBEDTLS_RSA_PRIVATE,
                                          output_length,
                                          input,
                                          output,
                                          output_size));
#else
            status = PSA_ERROR_NOT_SUPPORTED;
#endif /* MBEDTLS_PSA_BUILTIN_ALG_RSA_PKCS1V15_CRYPT */
        } else
        if (PSA_ALG_IS_RSA_OAEP(alg)) {
#if defined(MBEDTLS_PSA_BUILTIN_ALG_RSA_OAEP)
            psa_rsa_oaep_set_padding_mode(alg, rsa);
            status = mbedtls_to_psa_error(
                mbedtls_rsa_rsaes_oaep_decrypt(rsa,
                                               mbedtls_psa_get_random,
                                               MBEDTLS_PSA_RANDOM_STATE,
                                               MBEDTLS_RSA_PRIVATE,
                                               salt, salt_length,
                                               output_length,
                                               input,
                                               output,
                                               output_size));
#else
            status = PSA_ERROR_NOT_SUPPORTED;
#endif /* MBEDTLS_PSA_BUILTIN_ALG_RSA_OAEP */
        } else {
            status = PSA_ERROR_INVALID_ARGUMENT;
        }

#if defined(MBEDTLS_PSA_BUILTIN_ALG_RSA_PKCS1V15_CRYPT) || \
        defined(MBEDTLS_PSA_BUILTIN_ALG_RSA_OAEP)
rsa_exit:
        mbedtls_rsa_free(rsa);
        mbedtls_free(rsa);
#endif /* defined(MBEDTLS_PSA_BUILTIN_ALG_RSA_PKCS1V15_CRYPT) ||
        * defined(MBEDTLS_PSA_BUILTIN_ALG_RSA_OAEP) */
    } else {
        status = PSA_ERROR_NOT_SUPPORTED;
    }

exit:
    unlock_status = psa_unlock_key_slot(slot);

    LOCAL_INPUT_FREE(input_external, input);
    LOCAL_INPUT_FREE(salt_external, salt);
    LOCAL_OUTPUT_FREE(output_external, output);

    return (status == PSA_SUCCESS) ? unlock_status : status;
}

static psa_status_t psa_generate_random_internal(uint8_t *output,
                                                 size_t output_size)
{
    GUARD_MODULE_INITIALIZED;

    psa_status_t status;

#if defined(MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG)

    size_t output_length = 0;
    status = mbedtls_psa_external_get_random(&global_data.rng,
                                             output, output_size,
                                             &output_length);
    if (status != PSA_SUCCESS) {
        goto exit;
    }
    /* Breaking up a request into smaller chunks is currently not supported
     * for the external RNG interface. */
    if (output_length != output_size) {
        status = PSA_ERROR_INSUFFICIENT_ENTROPY;
        goto exit;
    }
    status = PSA_SUCCESS;

#else /* MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG */

    while (output_size > 0) {
        size_t request_size =
            (output_size > MBEDTLS_PSA_RANDOM_MAX_REQUEST ?
             MBEDTLS_PSA_RANDOM_MAX_REQUEST :
             output_size);
        int ret = mbedtls_psa_get_random(MBEDTLS_PSA_RANDOM_STATE,
                                         output, request_size);
        if (ret != 0) {
            status = mbedtls_to_psa_error(ret);
            goto exit;
        }
        output_size -= request_size;
        output += request_size;
    }
    status = PSA_SUCCESS;
#endif /* MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG */

exit:
    return status;
}


/****************************************************************/
/* Symmetric cryptography */
/****************************************************************/

static psa_status_t psa_cipher_setup(psa_cipher_operation_t *operation,
                                     mbedtls_svc_key_id_t key,
                                     psa_algorithm_t alg,
                                     mbedtls_operation_t cipher_operation)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    psa_status_t unlock_status = PSA_ERROR_CORRUPTION_DETECTED;
    psa_key_attributes_t attributes;
    psa_key_slot_t *slot = NULL;
    psa_key_usage_t usage = (cipher_operation == MBEDTLS_ENCRYPT ?
                             PSA_KEY_USAGE_ENCRYPT :
                             PSA_KEY_USAGE_DECRYPT);

    /* A context must be freshly initialized before it can be set up. */
    if (operation->id != 0) {
        status = PSA_ERROR_BAD_STATE;
        goto exit;
    }

    if (!PSA_ALG_IS_CIPHER(alg)) {
        status = PSA_ERROR_INVALID_ARGUMENT;
        goto exit;
    }

    status = psa_get_and_lock_key_slot_with_policy(key, &slot, usage, alg);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    /* Initialize the operation struct members, except for id. The id member
     * is used to indicate to psa_cipher_abort that there are resources to free,
     * so we only set it (in the driver wrapper) after resources have been
     * allocated/initialized. */
    operation->iv_set = 0;
    if (alg == PSA_ALG_ECB_NO_PADDING) {
        operation->iv_required = 0;
    } else if (slot->attr.type == PSA_KEY_TYPE_ARC4) {
        operation->iv_required = 0;
    } else {
        operation->iv_required = 1;
    }
    operation->default_iv_length = PSA_CIPHER_IV_LENGTH(slot->attr.type, alg);

    attributes = (psa_key_attributes_t) {
        .core = slot->attr
    };

    /* Try doing the operation through a driver before using software fallback. */
    if (cipher_operation == MBEDTLS_ENCRYPT) {
        status = psa_driver_wrapper_cipher_encrypt_setup(operation,
                                                         &attributes,
                                                         slot->key.data,
                                                         slot->key.bytes,
                                                         alg);
    } else {
        status = psa_driver_wrapper_cipher_decrypt_setup(operation,
                                                         &attributes,
                                                         slot->key.data,
                                                         slot->key.bytes,
                                                         alg);
    }

exit:
    if (status != PSA_SUCCESS) {
        psa_cipher_abort(operation);
    }

    unlock_status = psa_unlock_key_slot(slot);

    return (status == PSA_SUCCESS) ? unlock_status : status;
}

psa_status_t psa_cipher_encrypt_setup(psa_cipher_operation_t *operation,
                                      mbedtls_svc_key_id_t key,
                                      psa_algorithm_t alg)
{
    return psa_cipher_setup(operation, key, alg, MBEDTLS_ENCRYPT);
}

psa_status_t psa_cipher_decrypt_setup(psa_cipher_operation_t *operation,
                                      mbedtls_svc_key_id_t key,
                                      psa_algorithm_t alg)
{
    return psa_cipher_setup(operation, key, alg, MBEDTLS_DECRYPT);
}

psa_status_t psa_cipher_generate_iv(psa_cipher_operation_t *operation,
                                    uint8_t *iv_external,
                                    size_t iv_size,
                                    size_t *iv_length)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    size_t default_iv_length = 0;

    LOCAL_OUTPUT_DECLARE(iv_external, iv);

    if (operation->id == 0) {
        status = PSA_ERROR_BAD_STATE;
        goto exit;
    }

    if (operation->iv_set || !operation->iv_required) {
        status = PSA_ERROR_BAD_STATE;
        goto exit;
    }

    default_iv_length = operation->default_iv_length;
    if (iv_size < default_iv_length) {
        status = PSA_ERROR_BUFFER_TOO_SMALL;
        goto exit;
    }

    if (default_iv_length > PSA_CIPHER_IV_MAX_SIZE) {
        status = PSA_ERROR_GENERIC_ERROR;
        goto exit;
    }

    LOCAL_OUTPUT_ALLOC(iv_external, default_iv_length, iv);

    status = psa_generate_random_internal(iv, default_iv_length);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    status = psa_driver_wrapper_cipher_set_iv(operation,
                                              iv, default_iv_length);

exit:
    if (status == PSA_SUCCESS) {
        *iv_length = default_iv_length;
        operation->iv_set = 1;
    } else {
        *iv_length = 0;
        psa_cipher_abort(operation);
        if (iv != NULL) {
            mbedtls_platform_zeroize(iv, default_iv_length);
        }
    }

    LOCAL_OUTPUT_FREE(iv_external, iv);
    return status;
}

psa_status_t psa_cipher_set_iv(psa_cipher_operation_t *operation,
                               const uint8_t *iv_external,
                               size_t iv_length)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;

    LOCAL_INPUT_DECLARE(iv_external, iv);

    if (operation->id == 0) {
        status = PSA_ERROR_BAD_STATE;
        goto exit;
    }

    if (operation->iv_set || !operation->iv_required) {
        status = PSA_ERROR_BAD_STATE;
        goto exit;
    }

    if (iv_length > PSA_CIPHER_IV_MAX_SIZE) {
        status = PSA_ERROR_INVALID_ARGUMENT;
        goto exit;
    }

    LOCAL_INPUT_ALLOC(iv_external, iv_length, iv);

    status = psa_driver_wrapper_cipher_set_iv(operation,
                                              iv,
                                              iv_length);

exit:
    if (status == PSA_SUCCESS) {
        operation->iv_set = 1;
    } else {
        psa_cipher_abort(operation);
    }

    LOCAL_INPUT_FREE(iv_external, iv);

    return status;
}

psa_status_t psa_cipher_update(psa_cipher_operation_t *operation,
                               const uint8_t *input_external,
                               size_t input_length,
                               uint8_t *output_external,
                               size_t output_size,
                               size_t *output_length)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;

    LOCAL_INPUT_DECLARE(input_external, input);
    LOCAL_OUTPUT_DECLARE(output_external, output);

    if (operation->id == 0) {
        status = PSA_ERROR_BAD_STATE;
        goto exit;
    }

    if (operation->iv_required && !operation->iv_set) {
        status = PSA_ERROR_BAD_STATE;
        goto exit;
    }

    LOCAL_INPUT_ALLOC(input_external, input_length, input);
    LOCAL_OUTPUT_ALLOC(output_external, output_size, output);

    status = psa_driver_wrapper_cipher_update(operation,
                                              input,
                                              input_length,
                                              output,
                                              output_size,
                                              output_length);

exit:
    if (status != PSA_SUCCESS) {
        psa_cipher_abort(operation);
    }

    LOCAL_INPUT_FREE(input_external, input);
    LOCAL_OUTPUT_FREE(output_external, output);

    return status;
}

psa_status_t psa_cipher_finish(psa_cipher_operation_t *operation,
                               uint8_t *output_external,
                               size_t output_size,
                               size_t *output_length)
{
    psa_status_t status = PSA_ERROR_GENERIC_ERROR;

    LOCAL_OUTPUT_DECLARE(output_external, output);

    if (operation->id == 0) {
        status = PSA_ERROR_BAD_STATE;
        goto exit;
    }

    if (operation->iv_required && !operation->iv_set) {
        status = PSA_ERROR_BAD_STATE;
        goto exit;
    }

    LOCAL_OUTPUT_ALLOC(output_external, output_size, output);

    status = psa_driver_wrapper_cipher_finish(operation,
                                              output,
                                              output_size,
                                              output_length);

exit:
    if (status == PSA_SUCCESS) {
        status = psa_cipher_abort(operation);
    } else {
        *output_length = 0;
        (void) psa_cipher_abort(operation);
    }

    LOCAL_OUTPUT_FREE(output_external, output);

    return status;
}

psa_status_t psa_cipher_abort(psa_cipher_operation_t *operation)
{
    if (operation->id == 0) {
        /* The object has (apparently) been initialized but it is not (yet)
         * in use. It's ok to call abort on such an object, and there's
         * nothing to do. */
        return PSA_SUCCESS;
    }

    psa_driver_wrapper_cipher_abort(operation);

    operation->id = 0;
    operation->iv_set = 0;
    operation->iv_required = 0;

    return PSA_SUCCESS;
}

psa_status_t psa_cipher_encrypt(mbedtls_svc_key_id_t key,
                                psa_algorithm_t alg,
                                const uint8_t *input_external,
                                size_t input_length,
                                uint8_t *output_external,
                                size_t output_size,
                                size_t *output_length)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    psa_status_t unlock_status = PSA_ERROR_CORRUPTION_DETECTED;
    psa_key_attributes_t attributes;
    psa_key_slot_t *slot = NULL;
    uint8_t local_iv[PSA_CIPHER_IV_MAX_SIZE];
    size_t default_iv_length = 0;

    LOCAL_INPUT_DECLARE(input_external, input);
    LOCAL_OUTPUT_DECLARE(output_external, output);

    if (!PSA_ALG_IS_CIPHER(alg)) {
        status = PSA_ERROR_INVALID_ARGUMENT;
        goto exit;
    }

    status = psa_get_and_lock_key_slot_with_policy(key, &slot,
                                                   PSA_KEY_USAGE_ENCRYPT,
                                                   alg);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    attributes = (psa_key_attributes_t) {
        .core = slot->attr
    };

    default_iv_length = PSA_CIPHER_IV_LENGTH(slot->attr.type, alg);
    if (default_iv_length > PSA_CIPHER_IV_MAX_SIZE) {
        status = PSA_ERROR_GENERIC_ERROR;
        goto exit;
    }

    if (default_iv_length > 0) {
        if (output_size < default_iv_length) {
            status = PSA_ERROR_BUFFER_TOO_SMALL;
            goto exit;
        }

        status = psa_generate_random_internal(local_iv, default_iv_length);
        if (status != PSA_SUCCESS) {
            goto exit;
        }
    }

    LOCAL_INPUT_ALLOC(input_external, input_length, input);
    LOCAL_OUTPUT_ALLOC(output_external, output_size, output);

    status = psa_driver_wrapper_cipher_encrypt(
        &attributes, slot->key.data, slot->key.bytes,
        alg, local_iv, default_iv_length, input, input_length,
        mbedtls_buffer_offset(output, default_iv_length),
        output_size - default_iv_length, output_length);

exit:
    unlock_status = psa_unlock_key_slot(slot);
    if (status == PSA_SUCCESS) {
        status = unlock_status;
    }

    if (status == PSA_SUCCESS) {
        if (default_iv_length > 0) {
            memcpy(output, local_iv, default_iv_length);
        }
        *output_length += default_iv_length;
    } else {
        *output_length = 0;
    }

    LOCAL_INPUT_FREE(input_external, input);
    LOCAL_OUTPUT_FREE(output_external, output);

    return status;
}

psa_status_t psa_cipher_decrypt(mbedtls_svc_key_id_t key,
                                psa_algorithm_t alg,
                                const uint8_t *input_external,
                                size_t input_length,
                                uint8_t *output_external,
                                size_t output_size,
                                size_t *output_length)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    psa_status_t unlock_status = PSA_ERROR_CORRUPTION_DETECTED;
    psa_key_attributes_t attributes;
    psa_key_slot_t *slot = NULL;

    LOCAL_INPUT_DECLARE(input_external, input);
    LOCAL_OUTPUT_DECLARE(output_external, output);

    if (!PSA_ALG_IS_CIPHER(alg)) {
        status = PSA_ERROR_INVALID_ARGUMENT;
        goto exit;
    }

    status = psa_get_and_lock_key_slot_with_policy(key, &slot,
                                                   PSA_KEY_USAGE_DECRYPT,
                                                   alg);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    attributes = (psa_key_attributes_t) {
        .core = slot->attr
    };

    if (input_length < PSA_CIPHER_IV_LENGTH(slot->attr.type, alg)) {
        status = PSA_ERROR_INVALID_ARGUMENT;
        goto exit;
    }

    LOCAL_INPUT_ALLOC(input_external, input_length, input);
    LOCAL_OUTPUT_ALLOC(output_external, output_size, output);

    status = psa_driver_wrapper_cipher_decrypt(
        &attributes, slot->key.data, slot->key.bytes,
        alg, input, input_length,
        output, output_size, output_length);

exit:
    unlock_status = psa_unlock_key_slot(slot);
    if (status == PSA_SUCCESS) {
        status = unlock_status;
    }

    if (status != PSA_SUCCESS) {
        *output_length = 0;
    }

    LOCAL_INPUT_FREE(input_external, input);
    LOCAL_OUTPUT_FREE(output_external, output);

    return status;
}


/****************************************************************/
/* AEAD */
/****************************************************************/

psa_status_t psa_aead_encrypt(mbedtls_svc_key_id_t key,
                              psa_algorithm_t alg,
                              const uint8_t *nonce_external,
                              size_t nonce_length,
                              const uint8_t *additional_data_external,
                              size_t additional_data_length,
                              const uint8_t *plaintext_external,
                              size_t plaintext_length,
                              uint8_t *ciphertext_external,
                              size_t ciphertext_size,
                              size_t *ciphertext_length)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    psa_key_slot_t *slot;

    LOCAL_INPUT_DECLARE(nonce_external, nonce);
    LOCAL_INPUT_DECLARE(additional_data_external, additional_data);
    LOCAL_INPUT_DECLARE(plaintext_external, plaintext);
    LOCAL_OUTPUT_DECLARE(ciphertext_external, ciphertext);

    *ciphertext_length = 0;

    if (!PSA_ALG_IS_AEAD(alg) || PSA_ALG_IS_WILDCARD(alg)) {
        return PSA_ERROR_NOT_SUPPORTED;
    }

    status = psa_get_and_lock_key_slot_with_policy(
        key, &slot, PSA_KEY_USAGE_ENCRYPT, alg);
    if (status != PSA_SUCCESS) {
        return status;
    }

    psa_key_attributes_t attributes = {
        .core = slot->attr
    };

    LOCAL_INPUT_ALLOC(nonce_external, nonce_length, nonce);
    LOCAL_INPUT_ALLOC(additional_data_external, additional_data_length, additional_data);
    LOCAL_INPUT_ALLOC(plaintext_external, plaintext_length, plaintext);
    LOCAL_OUTPUT_ALLOC(ciphertext_external, ciphertext_size, ciphertext);

    status = psa_driver_wrapper_aead_encrypt(
        &attributes, slot->key.data, slot->key.bytes,
        alg,
        nonce, nonce_length,
        additional_data, additional_data_length,
        plaintext, plaintext_length,
        ciphertext, ciphertext_size, ciphertext_length);

    if (status != PSA_SUCCESS && ciphertext_size != 0) {
        memset(ciphertext, 0, ciphertext_size);
    }

/* Exit label is only used for buffer copying, prevent unused warnings. */
#if !defined(MBEDTLS_PSA_ASSUME_EXCLUSIVE_BUFFERS)
exit:
#endif
    LOCAL_INPUT_FREE(nonce_external, nonce);
    LOCAL_INPUT_FREE(additional_data_external, additional_data);
    LOCAL_INPUT_FREE(plaintext_external, plaintext);
    LOCAL_OUTPUT_FREE(ciphertext_external, ciphertext);

    psa_unlock_key_slot(slot);

    return status;
}

psa_status_t psa_aead_decrypt(mbedtls_svc_key_id_t key,
                              psa_algorithm_t alg,
                              const uint8_t *nonce_external,
                              size_t nonce_length,
                              const uint8_t *additional_data_external,
                              size_t additional_data_length,
                              const uint8_t *ciphertext_external,
                              size_t ciphertext_length,
                              uint8_t *plaintext_external,
                              size_t plaintext_size,
                              size_t *plaintext_length)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    psa_key_slot_t *slot;

    LOCAL_INPUT_DECLARE(nonce_external, nonce);
    LOCAL_INPUT_DECLARE(additional_data_external, additional_data);
    LOCAL_INPUT_DECLARE(ciphertext_external, ciphertext);
    LOCAL_OUTPUT_DECLARE(plaintext_external, plaintext);

    *plaintext_length = 0;

    if (!PSA_ALG_IS_AEAD(alg) || PSA_ALG_IS_WILDCARD(alg)) {
        return PSA_ERROR_NOT_SUPPORTED;
    }

    status = psa_get_and_lock_key_slot_with_policy(
        key, &slot, PSA_KEY_USAGE_DECRYPT, alg);
    if (status != PSA_SUCCESS) {
        return status;
    }

    psa_key_attributes_t attributes = {
        .core = slot->attr
    };

    LOCAL_INPUT_ALLOC(nonce_external, nonce_length, nonce);
    LOCAL_INPUT_ALLOC(additional_data_external, additional_data_length,
                      additional_data);
    LOCAL_INPUT_ALLOC(ciphertext_external, ciphertext_length, ciphertext);
    LOCAL_OUTPUT_ALLOC(plaintext_external, plaintext_size, plaintext);

    status = psa_driver_wrapper_aead_decrypt(
        &attributes, slot->key.data, slot->key.bytes,
        alg,
        nonce, nonce_length,
        additional_data, additional_data_length,
        ciphertext, ciphertext_length,
        plaintext, plaintext_size, plaintext_length);

    if (status != PSA_SUCCESS && plaintext_size != 0) {
        memset(plaintext, 0, plaintext_size);
    }

/* Exit label is only used for buffer copying, prevent unused warnings. */
#if !defined(MBEDTLS_PSA_ASSUME_EXCLUSIVE_BUFFERS)
exit:
#endif
    LOCAL_INPUT_FREE(nonce_external, nonce);
    LOCAL_INPUT_FREE(additional_data_external, additional_data);
    LOCAL_INPUT_FREE(ciphertext_external, ciphertext);
    LOCAL_OUTPUT_FREE(plaintext_external, plaintext);

    psa_unlock_key_slot(slot);

    return status;
}

/****************************************************************/
/* Generators */
/****************************************************************/

#if defined(MBEDTLS_PSA_BUILTIN_ALG_HKDF) || \
    defined(MBEDTLS_PSA_BUILTIN_ALG_TLS12_PRF) || \
    defined(MBEDTLS_PSA_BUILTIN_ALG_TLS12_PSK_TO_MS)
#define AT_LEAST_ONE_BUILTIN_KDF
#endif /* At least one builtin KDF */

#if defined(MBEDTLS_PSA_BUILTIN_ALG_HKDF) || \
    defined(MBEDTLS_PSA_BUILTIN_ALG_TLS12_PRF) || \
    defined(MBEDTLS_PSA_BUILTIN_ALG_TLS12_PSK_TO_MS)
static psa_status_t psa_key_derivation_start_hmac(
    psa_mac_operation_t *operation,
    psa_algorithm_t hash_alg,
    const uint8_t *hmac_key,
    size_t hmac_key_length)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&attributes, PSA_KEY_TYPE_HMAC);
    psa_set_key_bits(&attributes, PSA_BYTES_TO_BITS(hmac_key_length));
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_SIGN_HASH);

    operation->is_sign = 1;
    operation->mac_size = PSA_HASH_LENGTH(hash_alg);

    status = psa_driver_wrapper_mac_sign_setup(operation,
                                               &attributes,
                                               hmac_key, hmac_key_length,
                                               PSA_ALG_HMAC(hash_alg));

    psa_reset_key_attributes(&attributes);
    return status;
}
#endif /* KDF algorithms reliant on HMAC */

#define HKDF_STATE_INIT 0 /* no input yet */
#define HKDF_STATE_STARTED 1 /* got salt */
#define HKDF_STATE_KEYED 2 /* got key */
#define HKDF_STATE_OUTPUT 3 /* output started */

static psa_algorithm_t psa_key_derivation_get_kdf_alg(
    const psa_key_derivation_operation_t *operation)
{
    if (PSA_ALG_IS_KEY_AGREEMENT(operation->alg)) {
        return PSA_ALG_KEY_AGREEMENT_GET_KDF(operation->alg);
    } else {
        return operation->alg;
    }
}

psa_status_t psa_key_derivation_abort(psa_key_derivation_operation_t *operation)
{
    psa_status_t status = PSA_SUCCESS;
    psa_algorithm_t kdf_alg = psa_key_derivation_get_kdf_alg(operation);
    if (kdf_alg == 0) {
        /* The object has (apparently) been initialized but it is not
         * in use. It's ok to call abort on such an object, and there's
         * nothing to do. */
    } else
#if defined(MBEDTLS_PSA_BUILTIN_ALG_HKDF)
    if (PSA_ALG_IS_HKDF(kdf_alg)) {
        mbedtls_free(operation->ctx.hkdf.info);
        status = psa_mac_abort(&operation->ctx.hkdf.hmac);
    } else
#endif /* defined(MBEDTLS_PSA_BUILTIN_ALG_HKDF */
#if defined(MBEDTLS_PSA_BUILTIN_ALG_TLS12_PRF) || \
    defined(MBEDTLS_PSA_BUILTIN_ALG_TLS12_PSK_TO_MS)
    if (PSA_ALG_IS_TLS12_PRF(kdf_alg) ||
        /* TLS-1.2 PSK-to-MS KDF uses the same core as TLS-1.2 PRF */
        PSA_ALG_IS_TLS12_PSK_TO_MS(kdf_alg)) {
        if (operation->ctx.tls12_prf.secret != NULL) {
            mbedtls_platform_zeroize(operation->ctx.tls12_prf.secret,
                                     operation->ctx.tls12_prf.secret_length);
            mbedtls_free(operation->ctx.tls12_prf.secret);
        }

        if (operation->ctx.tls12_prf.seed != NULL) {
            mbedtls_platform_zeroize(operation->ctx.tls12_prf.seed,
                                     operation->ctx.tls12_prf.seed_length);
            mbedtls_free(operation->ctx.tls12_prf.seed);
        }

        if (operation->ctx.tls12_prf.label != NULL) {
            mbedtls_platform_zeroize(operation->ctx.tls12_prf.label,
                                     operation->ctx.tls12_prf.label_length);
            mbedtls_free(operation->ctx.tls12_prf.label);
        }

        status = PSA_SUCCESS;

        /* We leave the fields Ai and output_block to be erased safely by the
         * mbedtls_platform_zeroize() in the end of this function. */
    } else
#endif /* defined(MBEDTLS_PSA_BUILTIN_ALG_TLS12_PRF) ||
        * defined(MBEDTLS_PSA_BUILTIN_ALG_TLS12_PSK_TO_MS) */
    {
        status = PSA_ERROR_BAD_STATE;
    }
    mbedtls_platform_zeroize(operation, sizeof(*operation));
    return status;
}

psa_status_t psa_key_derivation_get_capacity(const psa_key_derivation_operation_t *operation,
                                             size_t *capacity)
{
    if (operation->alg == 0) {
        /* This is a blank key derivation operation. */
        return PSA_ERROR_BAD_STATE;
    }

    *capacity = operation->capacity;
    return PSA_SUCCESS;
}

psa_status_t psa_key_derivation_set_capacity(psa_key_derivation_operation_t *operation,
                                             size_t capacity)
{
    if (operation->alg == 0) {
        return PSA_ERROR_BAD_STATE;
    }
    if (capacity > operation->capacity) {
        return PSA_ERROR_INVALID_ARGUMENT;
    }
    operation->capacity = capacity;
    return PSA_SUCCESS;
}

#if defined(MBEDTLS_PSA_BUILTIN_ALG_HKDF)
/* Read some bytes from an HKDF-based operation. This performs a chunk
 * of the expand phase of the HKDF algorithm. */
static psa_status_t psa_key_derivation_hkdf_read(psa_hkdf_key_derivation_t *hkdf,
                                                 psa_algorithm_t hash_alg,
                                                 uint8_t *output,
                                                 size_t output_length)
{
    uint8_t hash_length = PSA_HASH_LENGTH(hash_alg);
    size_t hmac_output_length;
    psa_status_t status;

    if (hkdf->state < HKDF_STATE_KEYED || !hkdf->info_set) {
        return PSA_ERROR_BAD_STATE;
    }
    hkdf->state = HKDF_STATE_OUTPUT;

    while (output_length != 0) {
        /* Copy what remains of the current block */
        uint8_t n = hash_length - hkdf->offset_in_block;
        if (n > output_length) {
            n = (uint8_t) output_length;
        }
        memcpy(output, hkdf->output_block + hkdf->offset_in_block, n);
        output += n;
        output_length -= n;
        hkdf->offset_in_block += n;
        if (output_length == 0) {
            break;
        }
        /* We can't be wanting more output after block 0xff, otherwise
         * the capacity check in psa_key_derivation_output_bytes() would have
         * prevented this call. It could happen only if the operation
         * object was corrupted or if this function is called directly
         * inside the library. */
        if (hkdf->block_number == 0xff) {
            return PSA_ERROR_BAD_STATE;
        }

        /* We need a new block */
        ++hkdf->block_number;
        hkdf->offset_in_block = 0;

        status = psa_key_derivation_start_hmac(&hkdf->hmac,
                                               hash_alg,
                                               hkdf->prk,
                                               hash_length);
        if (status != PSA_SUCCESS) {
            return status;
        }

        if (hkdf->block_number != 1) {
            status = psa_mac_update(&hkdf->hmac,
                                    hkdf->output_block,
                                    hash_length);
            if (status != PSA_SUCCESS) {
                return status;
            }
        }
        status = psa_mac_update(&hkdf->hmac,
                                hkdf->info,
                                hkdf->info_length);
        if (status != PSA_SUCCESS) {
            return status;
        }
        status = psa_mac_update(&hkdf->hmac,
                                &hkdf->block_number, 1);
        if (status != PSA_SUCCESS) {
            return status;
        }
        status = psa_mac_sign_finish(&hkdf->hmac,
                                     hkdf->output_block,
                                     sizeof(hkdf->output_block),
                                     &hmac_output_length);
        if (status != PSA_SUCCESS) {
            return status;
        }
    }

    return PSA_SUCCESS;
}
#endif /* MBEDTLS_PSA_BUILTIN_ALG_HKDF */

#if defined(MBEDTLS_PSA_BUILTIN_ALG_TLS12_PRF) || \
    defined(MBEDTLS_PSA_BUILTIN_ALG_TLS12_PSK_TO_MS)
static psa_status_t psa_key_derivation_tls12_prf_generate_next_block(
    psa_tls12_prf_key_derivation_t *tls12_prf,
    psa_algorithm_t alg)
{
    psa_algorithm_t hash_alg = PSA_ALG_HKDF_GET_HASH(alg);
    uint8_t hash_length = PSA_HASH_LENGTH(hash_alg);
    psa_mac_operation_t hmac = PSA_MAC_OPERATION_INIT;
    size_t hmac_output_length;
    psa_status_t status, cleanup_status;

    /* We can't be wanting more output after block 0xff, otherwise
     * the capacity check in psa_key_derivation_output_bytes() would have
     * prevented this call. It could happen only if the operation
     * object was corrupted or if this function is called directly
     * inside the library. */
    if (tls12_prf->block_number == 0xff) {
        return PSA_ERROR_CORRUPTION_DETECTED;
    }

    /* We need a new block */
    ++tls12_prf->block_number;
    tls12_prf->left_in_block = hash_length;

    /* Recall the definition of the TLS-1.2-PRF from RFC 5246:
     *
     * PRF(secret, label, seed) = P_<hash>(secret, label + seed)
     *
     * P_hash(secret, seed) = HMAC_hash(secret, A(1) + seed) +
     *                        HMAC_hash(secret, A(2) + seed) +
     *                        HMAC_hash(secret, A(3) + seed) + ...
     *
     * A(0) = seed
     * A(i) = HMAC_hash(secret, A(i-1))
     *
     * The `psa_tls12_prf_key_derivation` structure saves the block
     * `HMAC_hash(secret, A(i) + seed)` from which the output
     * is currently extracted as `output_block` and where i is
     * `block_number`.
     */

    status = psa_key_derivation_start_hmac(&hmac,
                                           hash_alg,
                                           tls12_prf->secret,
                                           tls12_prf->secret_length);
    if (status != PSA_SUCCESS) {
        goto cleanup;
    }

    /* Calculate A(i) where i = tls12_prf->block_number. */
    if (tls12_prf->block_number == 1) {
        /* A(1) = HMAC_hash(secret, A(0)), where A(0) = seed. (The RFC overloads
         * the variable seed and in this instance means it in the context of the
         * P_hash function, where seed = label + seed.) */
        status = psa_mac_update(&hmac,
                                tls12_prf->label,
                                tls12_prf->label_length);
        if (status != PSA_SUCCESS) {
            goto cleanup;
        }
        status = psa_mac_update(&hmac,
                                tls12_prf->seed,
                                tls12_prf->seed_length);
        if (status != PSA_SUCCESS) {
            goto cleanup;
        }
    } else {
        /* A(i) = HMAC_hash(secret, A(i-1)) */
        status = psa_mac_update(&hmac, tls12_prf->Ai, hash_length);
        if (status != PSA_SUCCESS) {
            goto cleanup;
        }
    }

    status = psa_mac_sign_finish(&hmac,
                                 tls12_prf->Ai, hash_length,
                                 &hmac_output_length);
    if (hmac_output_length != hash_length) {
        status = PSA_ERROR_CORRUPTION_DETECTED;
    }
    if (status != PSA_SUCCESS) {
        goto cleanup;
    }

    /* Calculate HMAC_hash(secret, A(i) + label + seed). */
    status = psa_key_derivation_start_hmac(&hmac,
                                           hash_alg,
                                           tls12_prf->secret,
                                           tls12_prf->secret_length);
    if (status != PSA_SUCCESS) {
        goto cleanup;
    }
    status = psa_mac_update(&hmac, tls12_prf->Ai, hash_length);
    if (status != PSA_SUCCESS) {
        goto cleanup;
    }
    status = psa_mac_update(&hmac, tls12_prf->label, tls12_prf->label_length);
    if (status != PSA_SUCCESS) {
        goto cleanup;
    }
    status = psa_mac_update(&hmac, tls12_prf->seed, tls12_prf->seed_length);
    if (status != PSA_SUCCESS) {
        goto cleanup;
    }
    status = psa_mac_sign_finish(&hmac,
                                 tls12_prf->output_block, hash_length,
                                 &hmac_output_length);
    if (status != PSA_SUCCESS) {
        goto cleanup;
    }


cleanup:
    cleanup_status = psa_mac_abort(&hmac);
    if (status == PSA_SUCCESS && cleanup_status != PSA_SUCCESS) {
        status = cleanup_status;
    }

    return status;
}

static psa_status_t psa_key_derivation_tls12_prf_read(
    psa_tls12_prf_key_derivation_t *tls12_prf,
    psa_algorithm_t alg,
    uint8_t *output,
    size_t output_length)
{
    psa_algorithm_t hash_alg = PSA_ALG_TLS12_PRF_GET_HASH(alg);
    uint8_t hash_length = PSA_HASH_LENGTH(hash_alg);
    psa_status_t status;
    uint8_t offset, length;

    switch (tls12_prf->state) {
        case PSA_TLS12_PRF_STATE_LABEL_SET:
            tls12_prf->state = PSA_TLS12_PRF_STATE_OUTPUT;
            break;
        case PSA_TLS12_PRF_STATE_OUTPUT:
            break;
        default:
            return PSA_ERROR_BAD_STATE;
    }

    while (output_length != 0) {
        /* Check if we have fully processed the current block. */
        if (tls12_prf->left_in_block == 0) {
            status = psa_key_derivation_tls12_prf_generate_next_block(tls12_prf,
                                                                      alg);
            if (status != PSA_SUCCESS) {
                return status;
            }

            continue;
        }

        if (tls12_prf->left_in_block > output_length) {
            length = (uint8_t) output_length;
        } else {
            length = tls12_prf->left_in_block;
        }

        offset = hash_length - tls12_prf->left_in_block;
        memcpy(output, tls12_prf->output_block + offset, length);
        output += length;
        output_length -= length;
        tls12_prf->left_in_block -= length;
    }

    return PSA_SUCCESS;
}
#endif /* MBEDTLS_PSA_BUILTIN_ALG_TLS12_PRF ||
        * MBEDTLS_PSA_BUILTIN_ALG_TLS12_PSK_TO_MS */

psa_status_t psa_key_derivation_output_bytes(
    psa_key_derivation_operation_t *operation,
    uint8_t *output_external,
    size_t output_length)
{
    psa_status_t status;
    LOCAL_OUTPUT_DECLARE(output_external, output);

    psa_algorithm_t kdf_alg = psa_key_derivation_get_kdf_alg(operation);

    if (operation->alg == 0) {
        /* This is a blank operation. */
        return PSA_ERROR_BAD_STATE;
    }

    if (output_length == 0 && operation->capacity == 0) {
        /* Edge case: this is a finished operation, and 0 bytes
         * were requested. The right error in this case could
         * be either INSUFFICIENT_CAPACITY or BAD_STATE. Return
         * INSUFFICIENT_CAPACITY, which is right for a finished
         * operation, for consistency with the case when
         * output_length > 0. */
        return PSA_ERROR_INSUFFICIENT_DATA;
    }

    LOCAL_OUTPUT_ALLOC(output_external, output_length, output);
    if (output_length > operation->capacity) {
        operation->capacity = 0;
        /* Go through the error path to wipe all confidential data now
         * that the operation object is useless. */
        status = PSA_ERROR_INSUFFICIENT_DATA;
        goto exit;
    }
    operation->capacity -= output_length;

#if defined(MBEDTLS_PSA_BUILTIN_ALG_HKDF)
    if (PSA_ALG_IS_HKDF(kdf_alg)) {
        psa_algorithm_t hash_alg = PSA_ALG_HKDF_GET_HASH(kdf_alg);
        status = psa_key_derivation_hkdf_read(&operation->ctx.hkdf, hash_alg,
                                              output, output_length);
    } else
#endif /* MBEDTLS_PSA_BUILTIN_ALG_HKDF */
#if defined(MBEDTLS_PSA_BUILTIN_ALG_TLS12_PRF) || \
    defined(MBEDTLS_PSA_BUILTIN_ALG_TLS12_PSK_TO_MS)
    if (PSA_ALG_IS_TLS12_PRF(kdf_alg) ||
        PSA_ALG_IS_TLS12_PSK_TO_MS(kdf_alg)) {
        status = psa_key_derivation_tls12_prf_read(&operation->ctx.tls12_prf,
                                                   kdf_alg, output,
                                                   output_length);
    } else
#endif /* MBEDTLS_PSA_BUILTIN_ALG_TLS12_PRF ||
        * MBEDTLS_PSA_BUILTIN_ALG_TLS12_PSK_TO_MS */
    {
        (void) kdf_alg;
        status = PSA_ERROR_BAD_STATE;
        LOCAL_OUTPUT_FREE(output_external, output);

        return status;
    }

exit:
    if (status != PSA_SUCCESS) {
        /* Preserve the algorithm upon errors, but clear all sensitive state.
         * This allows us to differentiate between exhausted operations and
         * blank operations, so we can return PSA_ERROR_BAD_STATE on blank
         * operations. */
        psa_algorithm_t alg = operation->alg;
        psa_key_derivation_abort(operation);
        operation->alg = alg;
        if (output != NULL) {
            memset(output, '!', output_length);
        }
    }

    LOCAL_OUTPUT_FREE(output_external, output);
    return status;
}

#if defined(MBEDTLS_PSA_BUILTIN_KEY_TYPE_DES)
static void psa_des_set_key_parity(uint8_t *data, size_t data_size)
{
    if (data_size >= 8) {
        mbedtls_des_key_set_parity(data);
    }
    if (data_size >= 16) {
        mbedtls_des_key_set_parity(data + 8);
    }
    if (data_size >= 24) {
        mbedtls_des_key_set_parity(data + 16);
    }
}
#endif /* MBEDTLS_PSA_BUILTIN_KEY_TYPE_DES */

static psa_status_t psa_generate_derived_key_internal(
    psa_key_slot_t *slot,
    size_t bits,
    psa_key_derivation_operation_t *operation)
{
    uint8_t *data = NULL;
    size_t bytes = PSA_BITS_TO_BYTES(bits);
    psa_status_t status;
    psa_key_attributes_t attributes;

    if (!key_type_is_raw_bytes(slot->attr.type)) {
        return PSA_ERROR_INVALID_ARGUMENT;
    }
    if (bits % 8 != 0) {
        return PSA_ERROR_INVALID_ARGUMENT;
    }
    data = mbedtls_calloc(1, bytes);
    if (data == NULL) {
        return PSA_ERROR_INSUFFICIENT_MEMORY;
    }

    status = psa_key_derivation_output_bytes(operation, data, bytes);
    if (status != PSA_SUCCESS) {
        goto exit;
    }
#if defined(MBEDTLS_PSA_BUILTIN_KEY_TYPE_DES)
    if (slot->attr.type == PSA_KEY_TYPE_DES) {
        psa_des_set_key_parity(data, bytes);
    }
#endif /* MBEDTLS_PSA_BUILTIN_KEY_TYPE_DES */

    status = psa_allocate_buffer_to_slot(slot, bytes);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    slot->attr.bits = (psa_key_bits_t) bits;
    attributes = (psa_key_attributes_t) {
        .core = slot->attr
    };

    status = psa_driver_wrapper_import_key(&attributes,
                                           data, bytes,
                                           slot->key.data,
                                           slot->key.bytes,
                                           &slot->key.bytes, &bits);
    if (bits != slot->attr.bits) {
        status = PSA_ERROR_INVALID_ARGUMENT;
    }

exit:
    mbedtls_free(data);
    return status;
}

psa_status_t psa_key_derivation_output_key(const psa_key_attributes_t *attributes,
                                           psa_key_derivation_operation_t *operation,
                                           mbedtls_svc_key_id_t *key)
{
    psa_status_t status;
    psa_key_slot_t *slot = NULL;
    psa_se_drv_table_entry_t *driver = NULL;

    *key = MBEDTLS_SVC_KEY_ID_INIT;

    /* Reject any attempt to create a zero-length key so that we don't
     * risk tripping up later, e.g. on a malloc(0) that returns NULL. */
    if (psa_get_key_bits(attributes) == 0) {
        return PSA_ERROR_INVALID_ARGUMENT;
    }

    if (operation->alg == PSA_ALG_NONE) {
        return PSA_ERROR_BAD_STATE;
    }

    if (!operation->can_output_key) {
        return PSA_ERROR_NOT_PERMITTED;
    }

    status = psa_start_key_creation(PSA_KEY_CREATION_DERIVE, attributes,
                                    &slot, &driver);
#if defined(MBEDTLS_PSA_CRYPTO_SE_C)
    if (driver != NULL) {
        /* Deriving a key in a secure element is not implemented yet. */
        status = PSA_ERROR_NOT_SUPPORTED;
    }
#endif /* MBEDTLS_PSA_CRYPTO_SE_C */
    if (status == PSA_SUCCESS) {
        status = psa_generate_derived_key_internal(slot,
                                                   attributes->core.bits,
                                                   operation);
    }
    if (status == PSA_SUCCESS) {
        status = psa_finish_key_creation(slot, driver, key);
    }
    if (status != PSA_SUCCESS) {
        psa_fail_key_creation(slot, driver);
    }

    return status;
}



/****************************************************************/
/* Key derivation */
/****************************************************************/

#if defined(AT_LEAST_ONE_BUILTIN_KDF)
static int is_kdf_alg_supported(psa_algorithm_t kdf_alg)
{
#if defined(MBEDTLS_PSA_BUILTIN_ALG_HKDF)
    if (PSA_ALG_IS_HKDF(kdf_alg)) {
        return 1;
    }
#endif
#if defined(MBEDTLS_PSA_BUILTIN_ALG_TLS12_PRF)
    if (PSA_ALG_IS_TLS12_PRF(kdf_alg)) {
        return 1;
    }
#endif
#if defined(MBEDTLS_PSA_BUILTIN_ALG_TLS12_PSK_TO_MS)
    if (PSA_ALG_IS_TLS12_PSK_TO_MS(kdf_alg)) {
        return 1;
    }
#endif
    return 0;
}

static psa_status_t psa_hash_try_support(psa_algorithm_t alg)
{
    psa_hash_operation_t operation = PSA_HASH_OPERATION_INIT;
    psa_status_t status = psa_hash_setup(&operation, alg);
    psa_hash_abort(&operation);
    return status;
}

static psa_status_t psa_key_derivation_set_maximum_capacity(
    psa_key_derivation_operation_t *operation,
    psa_algorithm_t kdf_alg)
{
    psa_algorithm_t hash_alg = PSA_ALG_HKDF_GET_HASH(kdf_alg);
    size_t hash_size = PSA_HASH_LENGTH(hash_alg);
    if (hash_size == 0) {
        return PSA_ERROR_NOT_SUPPORTED;
    }

    /* Make sure that hash_alg is a supported hash algorithm. Otherwise
     * we might fail later, which is somewhat unfriendly and potentially
     * risk-prone. */
    psa_status_t status = psa_hash_try_support(hash_alg);
    if (status != PSA_SUCCESS) {
        return status;
    }

#if defined(PSA_WANT_ALG_HKDF)
    if (PSA_ALG_IS_HKDF(kdf_alg)) {
        operation->capacity = 255 * hash_size;
    } else
#endif
#if defined(PSA_WANT_ALG_TLS12_PRF)
    if (PSA_ALG_IS_TLS12_PRF(kdf_alg) &&
        (hash_alg == PSA_ALG_SHA_256 || hash_alg == PSA_ALG_SHA_384)) {
        operation->capacity = SIZE_MAX;
    } else
#endif
#if defined(PSA_WANT_ALG_TLS12_PSK_TO_MS)
    if (PSA_ALG_IS_TLS12_PSK_TO_MS(kdf_alg) &&
        (hash_alg == PSA_ALG_SHA_256 || hash_alg == PSA_ALG_SHA_384)) {
        /* Master Secret is always 48 bytes
         * https://datatracker.ietf.org/doc/html/rfc5246.html#section-8.1 */
        operation->capacity = 48U;
    } else
#endif
    {
        (void) hash_size;
        status = PSA_ERROR_NOT_SUPPORTED;
    }
    return status;
}


static psa_status_t psa_key_derivation_setup_kdf(
    psa_key_derivation_operation_t *operation,
    psa_algorithm_t kdf_alg)
{
    /* Make sure that operation->ctx is properly zero-initialised. (Macro
     * initialisers for this union leave some bytes unspecified.) */
    memset(&operation->ctx, 0, sizeof(operation->ctx));
    /* Make sure that kdf_alg is a supported key derivation algorithm. */
    if (!is_kdf_alg_supported(kdf_alg)) {
        return PSA_ERROR_NOT_SUPPORTED;
    }

    psa_status_t status = psa_key_derivation_set_maximum_capacity(operation,
                                                                  kdf_alg);
    return status;
}

static psa_status_t psa_key_agreement_try_support(psa_algorithm_t alg)
{
#if defined(PSA_WANT_ALG_ECDH)
    if (alg == PSA_ALG_ECDH) {
        return PSA_SUCCESS;
    }
#endif
    (void) alg;
    return PSA_ERROR_NOT_SUPPORTED;
}
#endif /* AT_LEAST_ONE_BUILTIN_KDF */

psa_status_t psa_key_derivation_setup(psa_key_derivation_operation_t *operation,
                                      psa_algorithm_t alg)
{
    psa_status_t status;

    if (operation->alg != 0) {
        return PSA_ERROR_BAD_STATE;
    }

    if (PSA_ALG_IS_RAW_KEY_AGREEMENT(alg)) {
        return PSA_ERROR_INVALID_ARGUMENT;
    } else if (PSA_ALG_IS_KEY_AGREEMENT(alg)) {
#if defined(AT_LEAST_ONE_BUILTIN_KDF)
        psa_algorithm_t kdf_alg = PSA_ALG_KEY_AGREEMENT_GET_KDF(alg);
        psa_algorithm_t ka_alg = PSA_ALG_KEY_AGREEMENT_GET_BASE(alg);
        status = psa_key_agreement_try_support(ka_alg);
        if (status != PSA_SUCCESS) {
            return status;
        }
        status = psa_key_derivation_setup_kdf(operation, kdf_alg);
#else
        return PSA_ERROR_NOT_SUPPORTED;
#endif /* AT_LEAST_ONE_BUILTIN_KDF */
    } else if (PSA_ALG_IS_KEY_DERIVATION(alg)) {
#if defined(AT_LEAST_ONE_BUILTIN_KDF)
        status = psa_key_derivation_setup_kdf(operation, alg);
#else
        return PSA_ERROR_NOT_SUPPORTED;
#endif /* AT_LEAST_ONE_BUILTIN_KDF */
    } else {
        return PSA_ERROR_INVALID_ARGUMENT;
    }

    if (status == PSA_SUCCESS) {
        operation->alg = alg;
    }
    return status;
}

#if defined(MBEDTLS_PSA_BUILTIN_ALG_HKDF)
static psa_status_t psa_hkdf_input(psa_hkdf_key_derivation_t *hkdf,
                                   psa_algorithm_t hash_alg,
                                   psa_key_derivation_step_t step,
                                   const uint8_t *data,
                                   size_t data_length)
{
    psa_status_t status;
    switch (step) {
        case PSA_KEY_DERIVATION_INPUT_SALT:
            if (hkdf->state != HKDF_STATE_INIT) {
                return PSA_ERROR_BAD_STATE;
            } else {
                status = psa_key_derivation_start_hmac(&hkdf->hmac,
                                                       hash_alg,
                                                       data, data_length);
                if (status != PSA_SUCCESS) {
                    return status;
                }
                hkdf->state = HKDF_STATE_STARTED;
                return PSA_SUCCESS;
            }
        case PSA_KEY_DERIVATION_INPUT_SECRET:
            /* If no salt was provided, use an empty salt. */
            if (hkdf->state == HKDF_STATE_INIT) {
                status = psa_key_derivation_start_hmac(&hkdf->hmac,
                                                       hash_alg,
                                                       NULL, 0);
                if (status != PSA_SUCCESS) {
                    return status;
                }
                hkdf->state = HKDF_STATE_STARTED;
            }
            if (hkdf->state != HKDF_STATE_STARTED) {
                return PSA_ERROR_BAD_STATE;
            }
            status = psa_mac_update(&hkdf->hmac,
                                    data, data_length);
            if (status != PSA_SUCCESS) {
                return status;
            }
            status = psa_mac_sign_finish(&hkdf->hmac,
                                         hkdf->prk,
                                         sizeof(hkdf->prk),
                                         &data_length);
            if (status != PSA_SUCCESS) {
                return status;
            }
            hkdf->offset_in_block = PSA_HASH_LENGTH(hash_alg);
            hkdf->block_number = 0;
            hkdf->state = HKDF_STATE_KEYED;
            return PSA_SUCCESS;
        case PSA_KEY_DERIVATION_INPUT_INFO:
            if (hkdf->state == HKDF_STATE_OUTPUT) {
                return PSA_ERROR_BAD_STATE;
            }
            if (hkdf->info_set) {
                return PSA_ERROR_BAD_STATE;
            }
            hkdf->info_length = data_length;
            if (data_length != 0) {
                hkdf->info = mbedtls_calloc(1, data_length);
                if (hkdf->info == NULL) {
                    return PSA_ERROR_INSUFFICIENT_MEMORY;
                }
                memcpy(hkdf->info, data, data_length);
            }
            hkdf->info_set = 1;
            return PSA_SUCCESS;
        default:
            return PSA_ERROR_INVALID_ARGUMENT;
    }
}
#endif /* MBEDTLS_PSA_BUILTIN_ALG_HKDF */

#if defined(MBEDTLS_PSA_BUILTIN_ALG_TLS12_PRF) || \
    defined(MBEDTLS_PSA_BUILTIN_ALG_TLS12_PSK_TO_MS)
static psa_status_t psa_tls12_prf_set_seed(psa_tls12_prf_key_derivation_t *prf,
                                           const uint8_t *data,
                                           size_t data_length)
{
    if (prf->state != PSA_TLS12_PRF_STATE_INIT) {
        return PSA_ERROR_BAD_STATE;
    }

    if (data_length != 0) {
        prf->seed = mbedtls_calloc(1, data_length);
        if (prf->seed == NULL) {
            return PSA_ERROR_INSUFFICIENT_MEMORY;
        }

        memcpy(prf->seed, data, data_length);
        prf->seed_length = data_length;
    }

    prf->state = PSA_TLS12_PRF_STATE_SEED_SET;

    return PSA_SUCCESS;
}

static psa_status_t psa_tls12_prf_set_key(psa_tls12_prf_key_derivation_t *prf,
                                          const uint8_t *data,
                                          size_t data_length)
{
    if (prf->state != PSA_TLS12_PRF_STATE_SEED_SET) {
        return PSA_ERROR_BAD_STATE;
    }

    if (data_length != 0) {
        prf->secret = mbedtls_calloc(1, data_length);
        if (prf->secret == NULL) {
            return PSA_ERROR_INSUFFICIENT_MEMORY;
        }

        memcpy(prf->secret, data, data_length);
        prf->secret_length = data_length;
    }

    prf->state = PSA_TLS12_PRF_STATE_KEY_SET;

    return PSA_SUCCESS;
}

static psa_status_t psa_tls12_prf_set_label(psa_tls12_prf_key_derivation_t *prf,
                                            const uint8_t *data,
                                            size_t data_length)
{
    if (prf->state != PSA_TLS12_PRF_STATE_KEY_SET) {
        return PSA_ERROR_BAD_STATE;
    }

    if (data_length != 0) {
        prf->label = mbedtls_calloc(1, data_length);
        if (prf->label == NULL) {
            return PSA_ERROR_INSUFFICIENT_MEMORY;
        }

        memcpy(prf->label, data, data_length);
        prf->label_length = data_length;
    }

    prf->state = PSA_TLS12_PRF_STATE_LABEL_SET;

    return PSA_SUCCESS;
}

static psa_status_t psa_tls12_prf_input(psa_tls12_prf_key_derivation_t *prf,
                                        psa_key_derivation_step_t step,
                                        const uint8_t *data,
                                        size_t data_length)
{
    switch (step) {
        case PSA_KEY_DERIVATION_INPUT_SEED:
            return psa_tls12_prf_set_seed(prf, data, data_length);
        case PSA_KEY_DERIVATION_INPUT_SECRET:
            return psa_tls12_prf_set_key(prf, data, data_length);
        case PSA_KEY_DERIVATION_INPUT_LABEL:
            return psa_tls12_prf_set_label(prf, data, data_length);
        default:
            return PSA_ERROR_INVALID_ARGUMENT;
    }
}
#endif /* MBEDTLS_PSA_BUILTIN_ALG_TLS12_PRF) ||
        * MBEDTLS_PSA_BUILTIN_ALG_TLS12_PSK_TO_MS */

#if defined(MBEDTLS_PSA_BUILTIN_ALG_TLS12_PSK_TO_MS)
static psa_status_t psa_tls12_prf_psk_to_ms_set_key(
    psa_tls12_prf_key_derivation_t *prf,
    const uint8_t *data,
    size_t data_length)
{
    psa_status_t status;
    uint8_t pms[4 + 2 * PSA_TLS12_PSK_TO_MS_PSK_MAX_SIZE];
    uint8_t *cur = pms;

    if (data_length > PSA_TLS12_PSK_TO_MS_PSK_MAX_SIZE) {
        return PSA_ERROR_INVALID_ARGUMENT;
    }

    /* Quoting RFC 4279, Section 2:
     *
     * The premaster secret is formed as follows: if the PSK is N octets
     * long, concatenate a uint16 with the value N, N zero octets, a second
     * uint16 with the value N, and the PSK itself.
     */

    *cur++ = MBEDTLS_BYTE_1(data_length);
    *cur++ = MBEDTLS_BYTE_0(data_length);
    memset(cur, 0, data_length);
    cur += data_length;
    *cur++ = pms[0];
    *cur++ = pms[1];
    memcpy(cur, data, data_length);
    cur += data_length;

    status = psa_tls12_prf_set_key(prf, pms, cur - pms);

    mbedtls_platform_zeroize(pms, sizeof(pms));
    return status;
}

static psa_status_t psa_tls12_prf_psk_to_ms_input(
    psa_tls12_prf_key_derivation_t *prf,
    psa_key_derivation_step_t step,
    const uint8_t *data,
    size_t data_length)
{
    if (step == PSA_KEY_DERIVATION_INPUT_SECRET) {
        return psa_tls12_prf_psk_to_ms_set_key(prf,
                                               data, data_length);
    }

    return psa_tls12_prf_input(prf, step, data, data_length);
}
#endif /* MBEDTLS_PSA_BUILTIN_ALG_TLS12_PSK_TO_MS */

/** Check whether the given key type is acceptable for the given
 * input step of a key derivation.
 *
 * Secret inputs must have the type #PSA_KEY_TYPE_DERIVE.
 * Non-secret inputs must have the type #PSA_KEY_TYPE_RAW_DATA.
 * Both secret and non-secret inputs can alternatively have the type
 * #PSA_KEY_TYPE_NONE, which is never the type of a key object, meaning
 * that the input was passed as a buffer rather than via a key object.
 */
static int psa_key_derivation_check_input_type(
    psa_key_derivation_step_t step,
    psa_key_type_t key_type)
{
    switch (step) {
        case PSA_KEY_DERIVATION_INPUT_SECRET:
            if (key_type == PSA_KEY_TYPE_DERIVE) {
                return PSA_SUCCESS;
            }
            if (key_type == PSA_KEY_TYPE_NONE) {
                return PSA_SUCCESS;
            }
            break;
        case PSA_KEY_DERIVATION_INPUT_LABEL:
        case PSA_KEY_DERIVATION_INPUT_SALT:
        case PSA_KEY_DERIVATION_INPUT_INFO:
        case PSA_KEY_DERIVATION_INPUT_SEED:
            if (key_type == PSA_KEY_TYPE_RAW_DATA) {
                return PSA_SUCCESS;
            }
            if (key_type == PSA_KEY_TYPE_NONE) {
                return PSA_SUCCESS;
            }
            break;
    }
    return PSA_ERROR_INVALID_ARGUMENT;
}

static psa_status_t psa_key_derivation_input_internal(
    psa_key_derivation_operation_t *operation,
    psa_key_derivation_step_t step,
    psa_key_type_t key_type,
    const uint8_t *data,
    size_t data_length)
{
    psa_status_t status;
    psa_algorithm_t kdf_alg = psa_key_derivation_get_kdf_alg(operation);

    if (kdf_alg == PSA_ALG_NONE) {
        /* This is a blank or aborted operation. */
        status = PSA_ERROR_BAD_STATE;
        goto exit;
    }

    status = psa_key_derivation_check_input_type(step, key_type);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

#if defined(MBEDTLS_PSA_BUILTIN_ALG_HKDF)
    if (PSA_ALG_IS_HKDF(kdf_alg)) {
        status = psa_hkdf_input(&operation->ctx.hkdf,
                                PSA_ALG_HKDF_GET_HASH(kdf_alg),
                                step, data, data_length);
    } else
#endif /* MBEDTLS_PSA_BUILTIN_ALG_HKDF */
#if defined(MBEDTLS_PSA_BUILTIN_ALG_TLS12_PRF)
    if (PSA_ALG_IS_TLS12_PRF(kdf_alg)) {
        status = psa_tls12_prf_input(&operation->ctx.tls12_prf,
                                     step, data, data_length);
    } else
#endif /* MBEDTLS_PSA_BUILTIN_ALG_TLS12_PRF */
#if defined(MBEDTLS_PSA_BUILTIN_ALG_TLS12_PSK_TO_MS)
    if (PSA_ALG_IS_TLS12_PSK_TO_MS(kdf_alg)) {
        status = psa_tls12_prf_psk_to_ms_input(&operation->ctx.tls12_prf,
                                               step, data, data_length);
    } else
#endif /* MBEDTLS_PSA_BUILTIN_ALG_TLS12_PSK_TO_MS */
    {
        /* This can't happen unless the operation object was not initialized */
        (void) data;
        (void) data_length;
        (void) kdf_alg;
        return PSA_ERROR_BAD_STATE;
    }

exit:
    if (status != PSA_SUCCESS) {
        psa_key_derivation_abort(operation);
    }
    return status;
}

psa_status_t psa_key_derivation_input_bytes(
    psa_key_derivation_operation_t *operation,
    psa_key_derivation_step_t step,
    const uint8_t *data_external,
    size_t data_length)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    LOCAL_INPUT_DECLARE(data_external, data);

    LOCAL_INPUT_ALLOC(data_external, data_length, data);

    status = psa_key_derivation_input_internal(operation, step,
                                               PSA_KEY_TYPE_NONE,
                                               data, data_length);
#if !defined(MBEDTLS_PSA_ASSUME_EXCLUSIVE_BUFFERS)
exit:
#endif
    LOCAL_INPUT_FREE(data_external, data);
    return status;
}

psa_status_t psa_key_derivation_input_key(
    psa_key_derivation_operation_t *operation,
    psa_key_derivation_step_t step,
    mbedtls_svc_key_id_t key)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    psa_status_t unlock_status = PSA_ERROR_CORRUPTION_DETECTED;
    psa_key_slot_t *slot;

    status = psa_get_and_lock_transparent_key_slot_with_policy(
        key, &slot, PSA_KEY_USAGE_DERIVE, operation->alg);
    if (status != PSA_SUCCESS) {
        psa_key_derivation_abort(operation);
        return status;
    }

    /* Passing a key object as a SECRET input unlocks the permission
     * to output to a key object. */
    if (step == PSA_KEY_DERIVATION_INPUT_SECRET) {
        operation->can_output_key = 1;
    }

    status = psa_key_derivation_input_internal(operation,
                                               step, slot->attr.type,
                                               slot->key.data,
                                               slot->key.bytes);

    unlock_status = psa_unlock_key_slot(slot);

    return (status == PSA_SUCCESS) ? unlock_status : status;
}



/****************************************************************/
/* Key agreement */
/****************************************************************/

#if defined(MBEDTLS_PSA_BUILTIN_ALG_ECDH)
static psa_status_t psa_key_agreement_ecdh(const uint8_t *peer_key,
                                           size_t peer_key_length,
                                           const mbedtls_ecp_keypair *our_key,
                                           uint8_t *shared_secret,
                                           size_t shared_secret_size,
                                           size_t *shared_secret_length)
{
    mbedtls_ecp_keypair *their_key = NULL;
    mbedtls_ecdh_context ecdh;
    psa_status_t status;
    size_t bits = 0;
    psa_ecc_family_t curve = mbedtls_ecc_group_to_psa(our_key->grp.id, &bits);
    mbedtls_ecdh_init(&ecdh);

    status = mbedtls_psa_ecp_load_representation(
        PSA_KEY_TYPE_ECC_PUBLIC_KEY(curve),
        bits,
        peer_key,
        peer_key_length,
        &their_key);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    status = mbedtls_to_psa_error(
        mbedtls_ecdh_get_params(&ecdh, their_key, MBEDTLS_ECDH_THEIRS));
    if (status != PSA_SUCCESS) {
        goto exit;
    }
    status = mbedtls_to_psa_error(
        mbedtls_ecdh_get_params(&ecdh, our_key, MBEDTLS_ECDH_OURS));
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    status = mbedtls_to_psa_error(
        mbedtls_ecdh_calc_secret(&ecdh,
                                 shared_secret_length,
                                 shared_secret, shared_secret_size,
                                 mbedtls_psa_get_random,
                                 MBEDTLS_PSA_RANDOM_STATE));
    if (status != PSA_SUCCESS) {
        goto exit;
    }
    if (PSA_BITS_TO_BYTES(bits) != *shared_secret_length) {
        status = PSA_ERROR_CORRUPTION_DETECTED;
    }

exit:
    if (status != PSA_SUCCESS) {
        mbedtls_platform_zeroize(shared_secret, shared_secret_size);
    }
    mbedtls_ecdh_free(&ecdh);
    mbedtls_ecp_keypair_free(their_key);
    mbedtls_free(their_key);

    return status;
}
#endif /* MBEDTLS_PSA_BUILTIN_ALG_ECDH */

#define PSA_KEY_AGREEMENT_MAX_SHARED_SECRET_SIZE MBEDTLS_ECP_MAX_BYTES

static psa_status_t psa_key_agreement_raw_internal(psa_algorithm_t alg,
                                                   psa_key_slot_t *private_key,
                                                   const uint8_t *peer_key,
                                                   size_t peer_key_length,
                                                   uint8_t *shared_secret,
                                                   size_t shared_secret_size,
                                                   size_t *shared_secret_length)
{
    mbedtls_ecp_keypair *ecp = NULL;
    psa_status_t status;

    switch (alg) {
#if defined(MBEDTLS_PSA_BUILTIN_ALG_ECDH)
        case PSA_ALG_ECDH:
            if (!PSA_KEY_TYPE_IS_ECC_KEY_PAIR(private_key->attr.type)) {
                return PSA_ERROR_INVALID_ARGUMENT;
            }
            status = mbedtls_psa_ecp_load_representation(
                private_key->attr.type,
                private_key->attr.bits,
                private_key->key.data,
                private_key->key.bytes,
                &ecp);
            if (status != PSA_SUCCESS) {
                return status;
            }
            status = psa_key_agreement_ecdh(peer_key, peer_key_length,
                                            ecp,
                                            shared_secret, shared_secret_size,
                                            shared_secret_length);
            mbedtls_ecp_keypair_free(ecp);
            mbedtls_free(ecp);
            return status;
#endif /* MBEDTLS_PSA_BUILTIN_ALG_ECDH */
        default:
            (void) ecp;
            (void) status;
            (void) private_key;
            (void) peer_key;
            (void) peer_key_length;
            (void) shared_secret;
            (void) shared_secret_size;
            (void) shared_secret_length;
            return PSA_ERROR_NOT_SUPPORTED;
    }
}

/* Note that if this function fails, you must call psa_key_derivation_abort()
 * to potentially free embedded data structures and wipe confidential data.
 */
static psa_status_t psa_key_agreement_internal(psa_key_derivation_operation_t *operation,
                                               psa_key_derivation_step_t step,
                                               psa_key_slot_t *private_key,
                                               const uint8_t *peer_key,
                                               size_t peer_key_length)
{
    psa_status_t status;
    uint8_t shared_secret[PSA_KEY_AGREEMENT_MAX_SHARED_SECRET_SIZE];
    size_t shared_secret_length = 0;
    psa_algorithm_t ka_alg = PSA_ALG_KEY_AGREEMENT_GET_BASE(operation->alg);

    /* Step 1: run the secret agreement algorithm to generate the shared
     * secret. */
    status = psa_key_agreement_raw_internal(ka_alg,
                                            private_key,
                                            peer_key, peer_key_length,
                                            shared_secret,
                                            sizeof(shared_secret),
                                            &shared_secret_length);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    /* Step 2: set up the key derivation to generate key material from
     * the shared secret. A shared secret is permitted wherever a key
     * of type DERIVE is permitted. */
    status = psa_key_derivation_input_internal(operation, step,
                                               PSA_KEY_TYPE_DERIVE,
                                               shared_secret,
                                               shared_secret_length);
exit:
    mbedtls_platform_zeroize(shared_secret, shared_secret_length);
    return status;
}

psa_status_t psa_key_derivation_key_agreement(psa_key_derivation_operation_t *operation,
                                              psa_key_derivation_step_t step,
                                              mbedtls_svc_key_id_t private_key,
                                              const uint8_t *peer_key_external,
                                              size_t peer_key_length)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    psa_status_t unlock_status = PSA_ERROR_CORRUPTION_DETECTED;
    psa_key_slot_t *slot;
    LOCAL_INPUT_DECLARE(peer_key_external, peer_key);

    if (!PSA_ALG_IS_KEY_AGREEMENT(operation->alg)) {
        return PSA_ERROR_INVALID_ARGUMENT;
    }
    status = psa_get_and_lock_transparent_key_slot_with_policy(
        private_key, &slot, PSA_KEY_USAGE_DERIVE, operation->alg);
    if (status != PSA_SUCCESS) {
        return status;
    }

    LOCAL_INPUT_ALLOC(peer_key_external, peer_key_length, peer_key);
    status = psa_key_agreement_internal(operation, step,
                                        slot,
                                        peer_key, peer_key_length);

#if !defined(MBEDTLS_PSA_ASSUME_EXCLUSIVE_BUFFERS)
exit:
#endif
    if (status != PSA_SUCCESS) {
        psa_key_derivation_abort(operation);
    } else {
        /* If a private key has been added as SECRET, we allow the derived
         * key material to be used as a key in PSA Crypto. */
        if (step == PSA_KEY_DERIVATION_INPUT_SECRET) {
            operation->can_output_key = 1;
        }
    }

    unlock_status = psa_unlock_key_slot(slot);

    LOCAL_INPUT_FREE(peer_key_external, peer_key);
    return (status == PSA_SUCCESS) ? unlock_status : status;
}

psa_status_t psa_raw_key_agreement(psa_algorithm_t alg,
                                   mbedtls_svc_key_id_t private_key,
                                   const uint8_t *peer_key_external,
                                   size_t peer_key_length,
                                   uint8_t *output_external,
                                   size_t output_size,
                                   size_t *output_length)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    psa_status_t unlock_status = PSA_ERROR_CORRUPTION_DETECTED;
    psa_key_slot_t *slot = NULL;
    size_t expected_length;
    LOCAL_INPUT_DECLARE(peer_key_external, peer_key);
    LOCAL_OUTPUT_DECLARE(output_external, output);
    LOCAL_OUTPUT_ALLOC(output_external, output_size, output);

    if (!PSA_ALG_IS_KEY_AGREEMENT(alg)) {
        status = PSA_ERROR_INVALID_ARGUMENT;
        goto exit;
    }
    status = psa_get_and_lock_transparent_key_slot_with_policy(
        private_key, &slot, PSA_KEY_USAGE_DERIVE, alg);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    /* PSA_RAW_KEY_AGREEMENT_OUTPUT_SIZE() is in general an upper bound
     * for the output size. The PSA specification only guarantees that this
     * function works if output_size >= PSA_RAW_KEY_AGREEMENT_OUTPUT_SIZE(...),
     * but it might be nice to allow smaller buffers if the output fits.
     * At the time of writing this comment, with only ECDH implemented,
     * PSA_RAW_KEY_AGREEMENT_OUTPUT_SIZE() is exact so the point is moot.
     * If FFDH is implemented, PSA_RAW_KEY_AGREEMENT_OUTPUT_SIZE() can easily
     * be exact for it as well. */
    expected_length =
        PSA_RAW_KEY_AGREEMENT_OUTPUT_SIZE(slot->attr.type, slot->attr.bits);
    if (output_size < expected_length) {
        status = PSA_ERROR_BUFFER_TOO_SMALL;
        goto exit;
    }

    LOCAL_INPUT_ALLOC(peer_key_external, peer_key_length, peer_key);
    status = psa_key_agreement_raw_internal(alg, slot,
                                            peer_key, peer_key_length,
                                            output, output_size,
                                            output_length);

exit:
    /* Check for successful allocation of output,
     * with an unsuccessful status. */
    if (output != NULL && status != PSA_SUCCESS) {
        /* If an error happens and is not handled properly, the output
         * may be used as a key to protect sensitive data. Arrange for such
         * a key to be random, which is likely to result in decryption or
         * verification errors. This is better than filling the buffer with
         * some constant data such as zeros, which would result in the data
         * being protected with a reproducible, easily knowable key.
         */
        psa_generate_random_internal(output, output_size);
        *output_length = output_size;
    }

    if (output == NULL) {
        /* output allocation failed. */
        *output_length = 0;
    }

    unlock_status = psa_unlock_key_slot(slot);

    LOCAL_INPUT_FREE(peer_key_external, peer_key);
    LOCAL_OUTPUT_FREE(output_external, output);
    return (status == PSA_SUCCESS) ? unlock_status : status;
}


/****************************************************************/
/* Random generation */
/****************************************************************/

#if defined(MBEDTLS_PSA_INJECT_ENTROPY)
#include "mbedtls/entropy_poll.h"
#endif

/** Initialize the PSA random generator.
 */
static void mbedtls_psa_random_init(mbedtls_psa_random_context_t *rng)
{
#if defined(MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG)
    memset(rng, 0, sizeof(*rng));
#else /* MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG */

    /* Set default configuration if
     * mbedtls_psa_crypto_configure_entropy_sources() hasn't been called. */
    if (rng->entropy_init == NULL) {
        rng->entropy_init = mbedtls_entropy_init;
    }
    if (rng->entropy_free == NULL) {
        rng->entropy_free = mbedtls_entropy_free;
    }

    rng->entropy_init(&rng->entropy);
#if defined(MBEDTLS_PSA_INJECT_ENTROPY) && \
    defined(MBEDTLS_NO_DEFAULT_ENTROPY_SOURCES)
    /* The PSA entropy injection feature depends on using NV seed as an entropy
     * source. Add NV seed as an entropy source for PSA entropy injection. */
    mbedtls_entropy_add_source(&rng->entropy,
                               mbedtls_nv_seed_poll, NULL,
                               MBEDTLS_ENTROPY_BLOCK_SIZE,
                               MBEDTLS_ENTROPY_SOURCE_STRONG);
#endif

    mbedtls_psa_drbg_init(MBEDTLS_PSA_RANDOM_STATE);
#endif /* MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG */
}

/** Deinitialize the PSA random generator.
 */
static void mbedtls_psa_random_free(mbedtls_psa_random_context_t *rng)
{
#if defined(MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG)
    memset(rng, 0, sizeof(*rng));
#else /* MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG */
    mbedtls_psa_drbg_free(MBEDTLS_PSA_RANDOM_STATE);
    rng->entropy_free(&rng->entropy);
#endif /* MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG */
}

/** Seed the PSA random generator.
 */
static psa_status_t mbedtls_psa_random_seed(mbedtls_psa_random_context_t *rng)
{
#if defined(MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG)
    /* Do nothing: the external RNG seeds itself. */
    (void) rng;
    return PSA_SUCCESS;
#else /* MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG */
    const unsigned char drbg_seed[] = "PSA";
    int ret = mbedtls_psa_drbg_seed(&rng->entropy,
                                    drbg_seed, sizeof(drbg_seed) - 1);
    return mbedtls_to_psa_error(ret);
#endif /* MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG */
}

psa_status_t psa_generate_random(uint8_t *output_external,
                                 size_t output_size)
{
    psa_status_t status;

    LOCAL_OUTPUT_DECLARE(output_external, output);
    LOCAL_OUTPUT_ALLOC(output_external, output_size, output);

    status = psa_generate_random_internal(output, output_size);

#if !defined(MBEDTLS_PSA_ASSUME_EXCLUSIVE_BUFFERS)
exit:
#endif
    LOCAL_OUTPUT_FREE(output_external, output);
    return status;
}

/* Wrapper function allowing the classic API to use the PSA RNG.
 *
 * `mbedtls_psa_get_random(MBEDTLS_PSA_RANDOM_STATE, ...)` calls
 * `psa_generate_random(...)`. The state parameter is ignored since the
 * PSA API doesn't support passing an explicit state.
 *
 * In the non-external case, psa_generate_random() calls an
 * `mbedtls_xxx_drbg_random` function which has exactly the same signature
 * and semantics as mbedtls_psa_get_random(). As an optimization,
 * instead of doing this back-and-forth between the PSA API and the
 * classic API, psa_crypto_random_impl.h defines `mbedtls_psa_get_random`
 * as a constant function pointer to `mbedtls_xxx_drbg_random`.
 */
#if defined(MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG)
int mbedtls_psa_get_random(void *p_rng,
                           unsigned char *output,
                           size_t output_size)
{
    /* This function takes a pointer to the RNG state because that's what
     * classic mbedtls functions using an RNG expect. The PSA RNG manages
     * its own state internally and doesn't let the caller access that state.
     * So we just ignore the state parameter, and in practice we'll pass
     * NULL. */
    (void) p_rng;
    psa_status_t status = psa_generate_random(output, output_size);
    if (status == PSA_SUCCESS) {
        return 0;
    } else {
        return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
    }
}
#endif /* MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG */

#if defined(MBEDTLS_PSA_INJECT_ENTROPY)
psa_status_t mbedtls_psa_inject_entropy(const uint8_t *seed,
                                        size_t seed_size)
{
    if (global_data.initialized) {
        return PSA_ERROR_NOT_PERMITTED;
    }

    if (((seed_size < MBEDTLS_ENTROPY_MIN_PLATFORM) ||
         (seed_size < MBEDTLS_ENTROPY_BLOCK_SIZE)) ||
        (seed_size > MBEDTLS_ENTROPY_MAX_SEED_SIZE)) {
        return PSA_ERROR_INVALID_ARGUMENT;
    }

    return mbedtls_psa_storage_inject_entropy(seed, seed_size);
}
#endif /* MBEDTLS_PSA_INJECT_ENTROPY */

/** Validate the key type and size for key generation
 *
 * \param  type  The key type
 * \param  bits  The number of bits of the key
 *
 * \retval #PSA_SUCCESS
 *         The key type and size are valid.
 * \retval #PSA_ERROR_INVALID_ARGUMENT
 *         The size in bits of the key is not valid.
 * \retval #PSA_ERROR_NOT_SUPPORTED
 *         The type and/or the size in bits of the key or the combination of
 *         the two is not supported.
 */
static psa_status_t psa_validate_key_type_and_size_for_key_generation(
    psa_key_type_t type, size_t bits)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;

    if (key_type_is_raw_bytes(type)) {
        status = validate_unstructured_key_bit_size(type, bits);
        if (status != PSA_SUCCESS) {
            return status;
        }
    } else
#if defined(PSA_WANT_KEY_TYPE_RSA_KEY_PAIR)
    if (PSA_KEY_TYPE_IS_RSA(type) && PSA_KEY_TYPE_IS_KEY_PAIR(type)) {
        if (bits > PSA_VENDOR_RSA_MAX_KEY_BITS) {
            return PSA_ERROR_NOT_SUPPORTED;
        }

        /* Accept only byte-aligned keys, for the same reasons as
         * in psa_import_rsa_key(). */
        if (bits % 8 != 0) {
            return PSA_ERROR_NOT_SUPPORTED;
        }
    } else
#endif /* defined(PSA_WANT_KEY_TYPE_RSA_KEY_PAIR) */

#if defined(PSA_WANT_KEY_TYPE_ECC_KEY_PAIR)
    if (PSA_KEY_TYPE_IS_ECC(type) && PSA_KEY_TYPE_IS_KEY_PAIR(type)) {
        /* To avoid empty block, return successfully here. */
        return PSA_SUCCESS;
    } else
#endif /* defined(PSA_WANT_KEY_TYPE_ECC_KEY_PAIR) */
    {
        return PSA_ERROR_NOT_SUPPORTED;
    }

    return PSA_SUCCESS;
}

psa_status_t psa_generate_key_internal(
    const psa_key_attributes_t *attributes,
    uint8_t *key_buffer, size_t key_buffer_size, size_t *key_buffer_length)
{
    psa_status_t status = PSA_ERROR_CORRUPTION_DETECTED;
    psa_key_type_t type = attributes->core.type;

    if ((attributes->domain_parameters == NULL) &&
        (attributes->domain_parameters_size != 0)) {
        return PSA_ERROR_INVALID_ARGUMENT;
    }

    if (key_type_is_raw_bytes(type)) {
        status = psa_generate_random(key_buffer, key_buffer_size);
        if (status != PSA_SUCCESS) {
            return status;
        }

#if defined(MBEDTLS_PSA_BUILTIN_KEY_TYPE_DES)
        if (type == PSA_KEY_TYPE_DES) {
            psa_des_set_key_parity(key_buffer, key_buffer_size);
        }
#endif /* MBEDTLS_PSA_BUILTIN_KEY_TYPE_DES */
    } else

#if defined(MBEDTLS_PSA_BUILTIN_KEY_TYPE_RSA_KEY_PAIR) && \
    defined(MBEDTLS_GENPRIME)
    if (type == PSA_KEY_TYPE_RSA_KEY_PAIR) {
        return mbedtls_psa_rsa_generate_key(attributes,
                                            key_buffer,
                                            key_buffer_size,
                                            key_buffer_length);
    } else
#endif /* defined(MBEDTLS_PSA_BUILTIN_KEY_TYPE_RSA_KEY_PAIR)
        * defined(MBEDTLS_GENPRIME) */

#if defined(MBEDTLS_PSA_BUILTIN_KEY_TYPE_ECC_KEY_PAIR)
    if (PSA_KEY_TYPE_IS_ECC(type) && PSA_KEY_TYPE_IS_KEY_PAIR(type)) {
        return mbedtls_psa_ecp_generate_key(attributes,
                                            key_buffer,
                                            key_buffer_size,
                                            key_buffer_length);
    } else
#endif /* defined(MBEDTLS_PSA_BUILTIN_KEY_TYPE_ECC_KEY_PAIR) */
    {
        (void) key_buffer_length;
        return PSA_ERROR_NOT_SUPPORTED;
    }

    return PSA_SUCCESS;
}

psa_status_t psa_generate_key(const psa_key_attributes_t *attributes,
                              mbedtls_svc_key_id_t *key)
{
    psa_status_t status;
    psa_key_slot_t *slot = NULL;
    psa_se_drv_table_entry_t *driver = NULL;
    size_t key_buffer_size;

    *key = MBEDTLS_SVC_KEY_ID_INIT;

    /* Reject any attempt to create a zero-length key so that we don't
     * risk tripping up later, e.g. on a malloc(0) that returns NULL. */
    if (psa_get_key_bits(attributes) == 0) {
        return PSA_ERROR_INVALID_ARGUMENT;
    }

    /* Reject any attempt to create a public key. */
    if (PSA_KEY_TYPE_IS_PUBLIC_KEY(attributes->core.type)) {
        return PSA_ERROR_INVALID_ARGUMENT;
    }

    status = psa_start_key_creation(PSA_KEY_CREATION_GENERATE, attributes,
                                    &slot, &driver);
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    /* In the case of a transparent key or an opaque key stored in local
     * storage (thus not in the case of generating a key in a secure element
     * or cryptoprocessor with storage), we have to allocate a buffer to
     * hold the generated key material. */
    if (slot->key.data == NULL) {
        if (PSA_KEY_LIFETIME_GET_LOCATION(attributes->core.lifetime) ==
            PSA_KEY_LOCATION_LOCAL_STORAGE) {
            status = psa_validate_key_type_and_size_for_key_generation(
                attributes->core.type, attributes->core.bits);
            if (status != PSA_SUCCESS) {
                goto exit;
            }

            key_buffer_size = PSA_EXPORT_KEY_OUTPUT_SIZE(
                attributes->core.type,
                attributes->core.bits);
        } else {
            status = psa_driver_wrapper_get_key_buffer_size(
                attributes, &key_buffer_size);
            if (status != PSA_SUCCESS) {
                goto exit;
            }
        }

        status = psa_allocate_buffer_to_slot(slot, key_buffer_size);
        if (status != PSA_SUCCESS) {
            goto exit;
        }
    }

    status = psa_driver_wrapper_generate_key(attributes,
                                             slot->key.data, slot->key.bytes, &slot->key.bytes);

    if (status != PSA_SUCCESS) {
        psa_remove_key_data_from_memory(slot);
    }

exit:
    if (status == PSA_SUCCESS) {
        status = psa_finish_key_creation(slot, driver, key);
    }
    if (status != PSA_SUCCESS) {
        psa_fail_key_creation(slot, driver);
    }

    return status;
}

/****************************************************************/
/* Module setup */
/****************************************************************/

#if !defined(MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG)
psa_status_t mbedtls_psa_crypto_configure_entropy_sources(
    void (* entropy_init)(mbedtls_entropy_context *ctx),
    void (* entropy_free)(mbedtls_entropy_context *ctx))
{
    if (global_data.rng_state != RNG_NOT_INITIALIZED) {
        return PSA_ERROR_BAD_STATE;
    }
    global_data.rng.entropy_init = entropy_init;
    global_data.rng.entropy_free = entropy_free;
    return PSA_SUCCESS;
}
#endif /* !defined(MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG) */

void mbedtls_psa_crypto_free(void)
{
    psa_wipe_all_key_slots();
    if (global_data.rng_state != RNG_NOT_INITIALIZED) {
        mbedtls_psa_random_free(&global_data.rng);
    }
    /* Wipe all remaining data, including configuration.
     * In particular, this sets all state indicator to the value
     * indicating "uninitialized". */
    mbedtls_platform_zeroize(&global_data, sizeof(global_data));

    /* Terminate drivers */
    psa_driver_wrapper_free();
}

#if defined(PSA_CRYPTO_STORAGE_HAS_TRANSACTIONS)
/** Recover a transaction that was interrupted by a power failure.
 *
 * This function is called during initialization, before psa_crypto_init()
 * returns. If this function returns a failure status, the initialization
 * fails.
 */
static psa_status_t psa_crypto_recover_transaction(
    const psa_crypto_transaction_t *transaction)
{
    switch (transaction->unknown.type) {
        case PSA_CRYPTO_TRANSACTION_CREATE_KEY:
        case PSA_CRYPTO_TRANSACTION_DESTROY_KEY:
        /* TODO - fall through to the failure case until this
         * is implemented.
         * https://github.com/ARMmbed/mbed-crypto/issues/218
         */
        default:
            /* We found an unsupported transaction in the storage.
             * We don't know what state the storage is in. Give up. */
            return PSA_ERROR_DATA_INVALID;
    }
}
#endif /* PSA_CRYPTO_STORAGE_HAS_TRANSACTIONS */

psa_status_t psa_crypto_init(void)
{
    psa_status_t status;

    /* Double initialization is explicitly allowed. */
    if (global_data.initialized != 0) {
        return PSA_SUCCESS;
    }

    /* Initialize and seed the random generator. */
    mbedtls_psa_random_init(&global_data.rng);
    global_data.rng_state = RNG_INITIALIZED;
    status = mbedtls_psa_random_seed(&global_data.rng);
    if (status != PSA_SUCCESS) {
        goto exit;
    }
    global_data.rng_state = RNG_SEEDED;

    status = psa_initialize_key_slots();
    if (status != PSA_SUCCESS) {
        goto exit;
    }

    /* Init drivers */
    status = psa_driver_wrapper_init();
    if (status != PSA_SUCCESS) {
        goto exit;
    }

#if defined(PSA_CRYPTO_STORAGE_HAS_TRANSACTIONS)
    status = psa_crypto_load_transaction();
    if (status == PSA_SUCCESS) {
        status = psa_crypto_recover_transaction(&psa_crypto_transaction);
        if (status != PSA_SUCCESS) {
            goto exit;
        }
        status = psa_crypto_stop_transaction();
    } else if (status == PSA_ERROR_DOES_NOT_EXIST) {
        /* There's no transaction to complete. It's all good. */
        status = PSA_SUCCESS;
    }
#endif /* PSA_CRYPTO_STORAGE_HAS_TRANSACTIONS */

    /* All done. */
    global_data.initialized = 1;

exit:
    if (status != PSA_SUCCESS) {
        mbedtls_psa_crypto_free();
    }
    return status;
}

/* Memory copying test hooks. These are called before input copy, after input
 * copy, before output copy and after output copy, respectively.
 * They are used by memory-poisoning tests to temporarily unpoison buffers
 * while they are copied. */
#if defined(MBEDTLS_TEST_HOOKS)
void (*psa_input_pre_copy_hook)(const uint8_t *input, size_t input_len) = NULL;
void (*psa_input_post_copy_hook)(const uint8_t *input, size_t input_len) = NULL;
void (*psa_output_pre_copy_hook)(const uint8_t *output, size_t output_len) = NULL;
void (*psa_output_post_copy_hook)(const uint8_t *output, size_t output_len) = NULL;
#endif

/** Copy from an input buffer to a local copy.
 *
 * \param[in] input             Pointer to input buffer.
 * \param[in] input_len         Length of the input buffer.
 * \param[out] input_copy       Pointer to a local copy in which to store the input data.
 * \param[out] input_copy_len   Length of the local copy buffer.
 * \return                      #PSA_SUCCESS, if the buffer was successfully
 *                              copied.
 * \return                      #PSA_ERROR_CORRUPTION_DETECTED, if the local
 *                              copy is too small to hold contents of the
 *                              input buffer.
 */
MBEDTLS_STATIC_TESTABLE
psa_status_t psa_crypto_copy_input(const uint8_t *input, size_t input_len,
                                   uint8_t *input_copy, size_t input_copy_len)
{
    if (input_len > input_copy_len) {
        return PSA_ERROR_CORRUPTION_DETECTED;
    }

#if defined(MBEDTLS_TEST_HOOKS)
    if (psa_input_pre_copy_hook != NULL) {
        psa_input_pre_copy_hook(input, input_len);
    }
#endif

    if (input_len > 0) {
        memcpy(input_copy, input, input_len);
    }

#if defined(MBEDTLS_TEST_HOOKS)
    if (psa_input_post_copy_hook != NULL) {
        psa_input_post_copy_hook(input, input_len);
    }
#endif

    return PSA_SUCCESS;
}

/** Copy from a local output buffer into a user-supplied one.
 *
 * \param[in] output_copy       Pointer to a local buffer containing the output.
 * \param[in] output_copy_len   Length of the local buffer.
 * \param[out] output           Pointer to user-supplied output buffer.
 * \param[out] output_len       Length of the user-supplied output buffer.
 * \return                      #PSA_SUCCESS, if the buffer was successfully
 *                              copied.
 * \return                      #PSA_ERROR_BUFFER_TOO_SMALL, if the
 *                              user-supplied output buffer is too small to
 *                              hold the contents of the local buffer.
 */
MBEDTLS_STATIC_TESTABLE
psa_status_t psa_crypto_copy_output(const uint8_t *output_copy, size_t output_copy_len,
                                    uint8_t *output, size_t output_len)
{
    if (output_len < output_copy_len) {
        return PSA_ERROR_BUFFER_TOO_SMALL;
    }

#if defined(MBEDTLS_TEST_HOOKS)
    if (psa_output_pre_copy_hook != NULL) {
        psa_output_pre_copy_hook(output, output_len);
    }
#endif

    if (output_copy_len > 0) {
        memcpy(output, output_copy, output_copy_len);
    }

#if defined(MBEDTLS_TEST_HOOKS)
    if (psa_output_post_copy_hook != NULL) {
        psa_output_post_copy_hook(output, output_len);
    }
#endif

    return PSA_SUCCESS;
}

psa_status_t psa_crypto_local_input_alloc(const uint8_t *input, size_t input_len,
                                          psa_crypto_local_input_t *local_input)
{
    psa_status_t status;

    *local_input = PSA_CRYPTO_LOCAL_INPUT_INIT;

    if (input_len == 0) {
        return PSA_SUCCESS;
    }

    local_input->buffer = mbedtls_calloc(input_len, 1);
    if (local_input->buffer == NULL) {
        /* Since we dealt with the zero-length case above, we know that
         * a NULL return value means a failure of allocation. */
        return PSA_ERROR_INSUFFICIENT_MEMORY;
    }
    /* From now on, we must free local_input->buffer on error. */

    local_input->length = input_len;

    status = psa_crypto_copy_input(input, input_len,
                                   local_input->buffer, local_input->length);
    if (status != PSA_SUCCESS) {
        goto error;
    }

    return PSA_SUCCESS;

error:
    if (local_input->buffer != NULL) {
        mbedtls_platform_zeroize(local_input->buffer, local_input->length);
        mbedtls_free(local_input->buffer);
        local_input->buffer = NULL;
    }
    local_input->length = 0;
    return status;
}

void psa_crypto_local_input_free(psa_crypto_local_input_t *local_input)
{
    if (local_input->buffer != NULL) {
        mbedtls_platform_zeroize(local_input->buffer, local_input->length);
        mbedtls_free(local_input->buffer);
        local_input->buffer = NULL;
    }
    local_input->length = 0;
}

psa_status_t psa_crypto_local_output_alloc(uint8_t *output, size_t output_len,
                                           psa_crypto_local_output_t *local_output)
{
    *local_output = PSA_CRYPTO_LOCAL_OUTPUT_INIT;

    if (output_len == 0) {
        return PSA_SUCCESS;
    }
    local_output->buffer = mbedtls_calloc(output_len, 1);
    if (local_output->buffer == NULL) {
        /* Since we dealt with the zero-length case above, we know that
         * a NULL return value means a failure of allocation. */
        return PSA_ERROR_INSUFFICIENT_MEMORY;
    }
    local_output->length = output_len;
    local_output->original = output;

    return PSA_SUCCESS;
}

psa_status_t psa_crypto_local_output_free(psa_crypto_local_output_t *local_output)
{
    psa_status_t status;

    if (local_output->buffer == NULL) {
        local_output->length = 0;
        return PSA_SUCCESS;
    }
    if (local_output->original == NULL) {
        /* We have an internal copy but nothing to copy back to. */
        return PSA_ERROR_CORRUPTION_DETECTED;
    }

    status = psa_crypto_copy_output(local_output->buffer, local_output->length,
                                    local_output->original, local_output->length);
    if (status != PSA_SUCCESS) {
        return status;
    }

    if (local_output->buffer != NULL) {
        mbedtls_platform_zeroize(local_output->buffer, local_output->length);
        mbedtls_free(local_output->buffer);
        local_output->buffer = NULL;
    }
    local_output->length = 0;

    return PSA_SUCCESS;
}

#endif /* MBEDTLS_PSA_CRYPTO_C */
