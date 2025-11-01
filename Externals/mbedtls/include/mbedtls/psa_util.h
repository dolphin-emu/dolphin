/**
 * \file psa_util.h
 *
 * \brief Utility functions for the use of the PSA Crypto library.
 *
 * \warning This function is not part of the public API and may
 *          change at any time.
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef MBEDTLS_PSA_UTIL_H
#define MBEDTLS_PSA_UTIL_H

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_USE_PSA_CRYPTO)

#include "psa/crypto.h"

#include "mbedtls/ecp.h"
#include "mbedtls/md.h"
#include "mbedtls/pk.h"
#include "mbedtls/oid.h"

#include <string.h>

/* Translations for symmetric crypto. */

static inline psa_key_type_t mbedtls_psa_translate_cipher_type(
    mbedtls_cipher_type_t cipher)
{
    switch (cipher) {
        case MBEDTLS_CIPHER_AES_128_CCM:
        case MBEDTLS_CIPHER_AES_192_CCM:
        case MBEDTLS_CIPHER_AES_256_CCM:
        case MBEDTLS_CIPHER_AES_128_GCM:
        case MBEDTLS_CIPHER_AES_192_GCM:
        case MBEDTLS_CIPHER_AES_256_GCM:
        case MBEDTLS_CIPHER_AES_128_CBC:
        case MBEDTLS_CIPHER_AES_192_CBC:
        case MBEDTLS_CIPHER_AES_256_CBC:
        case MBEDTLS_CIPHER_AES_128_ECB:
        case MBEDTLS_CIPHER_AES_192_ECB:
        case MBEDTLS_CIPHER_AES_256_ECB:
            return PSA_KEY_TYPE_AES;

        /* ARIA not yet supported in PSA. */
        /* case MBEDTLS_CIPHER_ARIA_128_CCM:
           case MBEDTLS_CIPHER_ARIA_192_CCM:
           case MBEDTLS_CIPHER_ARIA_256_CCM:
           case MBEDTLS_CIPHER_ARIA_128_GCM:
           case MBEDTLS_CIPHER_ARIA_192_GCM:
           case MBEDTLS_CIPHER_ARIA_256_GCM:
           case MBEDTLS_CIPHER_ARIA_128_CBC:
           case MBEDTLS_CIPHER_ARIA_192_CBC:
           case MBEDTLS_CIPHER_ARIA_256_CBC:
               return( PSA_KEY_TYPE_ARIA ); */

        default:
            return 0;
    }
}

static inline psa_algorithm_t mbedtls_psa_translate_cipher_mode(
    mbedtls_cipher_mode_t mode, size_t taglen)
{
    switch (mode) {
        case MBEDTLS_MODE_ECB:
            return PSA_ALG_ECB_NO_PADDING;
        case MBEDTLS_MODE_GCM:
            return PSA_ALG_AEAD_WITH_SHORTENED_TAG(PSA_ALG_GCM, taglen);
        case MBEDTLS_MODE_CCM:
            return PSA_ALG_AEAD_WITH_SHORTENED_TAG(PSA_ALG_CCM, taglen);
        case MBEDTLS_MODE_CBC:
            if (taglen == 0) {
                return PSA_ALG_CBC_NO_PADDING;
            } else {
                return 0;
            }
        default:
            return 0;
    }
}

static inline psa_key_usage_t mbedtls_psa_translate_cipher_operation(
    mbedtls_operation_t op)
{
    switch (op) {
        case MBEDTLS_ENCRYPT:
            return PSA_KEY_USAGE_ENCRYPT;
        case MBEDTLS_DECRYPT:
            return PSA_KEY_USAGE_DECRYPT;
        default:
            return 0;
    }
}

/* Translations for hashing. */

static inline psa_algorithm_t mbedtls_psa_translate_md(mbedtls_md_type_t md_alg)
{
    switch (md_alg) {
#if defined(MBEDTLS_MD2_C)
        case MBEDTLS_MD_MD2:
            return PSA_ALG_MD2;
#endif
#if defined(MBEDTLS_MD4_C)
        case MBEDTLS_MD_MD4:
            return PSA_ALG_MD4;
#endif
#if defined(MBEDTLS_MD5_C)
        case MBEDTLS_MD_MD5:
            return PSA_ALG_MD5;
#endif
#if defined(MBEDTLS_SHA1_C)
        case MBEDTLS_MD_SHA1:
            return PSA_ALG_SHA_1;
#endif
#if defined(MBEDTLS_SHA256_C)
        case MBEDTLS_MD_SHA224:
            return PSA_ALG_SHA_224;
        case MBEDTLS_MD_SHA256:
            return PSA_ALG_SHA_256;
#endif
#if defined(MBEDTLS_SHA512_C)
        case MBEDTLS_MD_SHA384:
            return PSA_ALG_SHA_384;
        case MBEDTLS_MD_SHA512:
            return PSA_ALG_SHA_512;
#endif
#if defined(MBEDTLS_RIPEMD160_C)
        case MBEDTLS_MD_RIPEMD160:
            return PSA_ALG_RIPEMD160;
#endif
        case MBEDTLS_MD_NONE:
            return 0;
        default:
            return 0;
    }
}

/* Translations for ECC. */

static inline int mbedtls_psa_get_ecc_oid_from_id(
    psa_ecc_family_t curve, size_t bits,
    char const **oid, size_t *oid_len)
{
    switch (curve) {
        case PSA_ECC_FAMILY_SECP_R1:
            switch (bits) {
#if defined(MBEDTLS_ECP_DP_SECP192R1_ENABLED)
                case 192:
                    *oid = MBEDTLS_OID_EC_GRP_SECP192R1;
                    *oid_len = MBEDTLS_OID_SIZE(MBEDTLS_OID_EC_GRP_SECP192R1);
                    return 0;
#endif /* MBEDTLS_ECP_DP_SECP192R1_ENABLED */
#if defined(MBEDTLS_ECP_DP_SECP224R1_ENABLED)
                case 224:
                    *oid = MBEDTLS_OID_EC_GRP_SECP224R1;
                    *oid_len = MBEDTLS_OID_SIZE(MBEDTLS_OID_EC_GRP_SECP224R1);
                    return 0;
#endif /* MBEDTLS_ECP_DP_SECP224R1_ENABLED */
#if defined(MBEDTLS_ECP_DP_SECP256R1_ENABLED)
                case 256:
                    *oid = MBEDTLS_OID_EC_GRP_SECP256R1;
                    *oid_len = MBEDTLS_OID_SIZE(MBEDTLS_OID_EC_GRP_SECP256R1);
                    return 0;
#endif /* MBEDTLS_ECP_DP_SECP256R1_ENABLED */
#if defined(MBEDTLS_ECP_DP_SECP384R1_ENABLED)
                case 384:
                    *oid = MBEDTLS_OID_EC_GRP_SECP384R1;
                    *oid_len = MBEDTLS_OID_SIZE(MBEDTLS_OID_EC_GRP_SECP384R1);
                    return 0;
#endif /* MBEDTLS_ECP_DP_SECP384R1_ENABLED */
#if defined(MBEDTLS_ECP_DP_SECP521R1_ENABLED)
                case 521:
                    *oid = MBEDTLS_OID_EC_GRP_SECP521R1;
                    *oid_len = MBEDTLS_OID_SIZE(MBEDTLS_OID_EC_GRP_SECP521R1);
                    return 0;
#endif /* MBEDTLS_ECP_DP_SECP521R1_ENABLED */
            }
            break;
        case PSA_ECC_FAMILY_SECP_K1:
            switch (bits) {
#if defined(MBEDTLS_ECP_DP_SECP192K1_ENABLED)
                case 192:
                    *oid = MBEDTLS_OID_EC_GRP_SECP192K1;
                    *oid_len = MBEDTLS_OID_SIZE(MBEDTLS_OID_EC_GRP_SECP192K1);
                    return 0;
#endif /* MBEDTLS_ECP_DP_SECP192K1_ENABLED */
#if defined(MBEDTLS_ECP_DP_SECP224K1_ENABLED)
                case 224:
                    *oid = MBEDTLS_OID_EC_GRP_SECP224K1;
                    *oid_len = MBEDTLS_OID_SIZE(MBEDTLS_OID_EC_GRP_SECP224K1);
                    return 0;
#endif /* MBEDTLS_ECP_DP_SECP224K1_ENABLED */
#if defined(MBEDTLS_ECP_DP_SECP256K1_ENABLED)
                case 256:
                    *oid = MBEDTLS_OID_EC_GRP_SECP256K1;
                    *oid_len = MBEDTLS_OID_SIZE(MBEDTLS_OID_EC_GRP_SECP256K1);
                    return 0;
#endif /* MBEDTLS_ECP_DP_SECP256K1_ENABLED */
            }
            break;
        case PSA_ECC_FAMILY_BRAINPOOL_P_R1:
            switch (bits) {
#if defined(MBEDTLS_ECP_DP_BP256R1_ENABLED)
                case 256:
                    *oid = MBEDTLS_OID_EC_GRP_BP256R1;
                    *oid_len = MBEDTLS_OID_SIZE(MBEDTLS_OID_EC_GRP_BP256R1);
                    return 0;
#endif /* MBEDTLS_ECP_DP_BP256R1_ENABLED */
#if defined(MBEDTLS_ECP_DP_BP384R1_ENABLED)
                case 384:
                    *oid = MBEDTLS_OID_EC_GRP_BP384R1;
                    *oid_len = MBEDTLS_OID_SIZE(MBEDTLS_OID_EC_GRP_BP384R1);
                    return 0;
#endif /* MBEDTLS_ECP_DP_BP384R1_ENABLED */
#if defined(MBEDTLS_ECP_DP_BP512R1_ENABLED)
                case 512:
                    *oid = MBEDTLS_OID_EC_GRP_BP512R1;
                    *oid_len = MBEDTLS_OID_SIZE(MBEDTLS_OID_EC_GRP_BP512R1);
                    return 0;
#endif /* MBEDTLS_ECP_DP_BP512R1_ENABLED */
            }
            break;
    }
    (void) oid;
    (void) oid_len;
    return -1;
}

#define MBEDTLS_PSA_MAX_EC_PUBKEY_LENGTH 1

#if defined(MBEDTLS_ECP_DP_SECP192R1_ENABLED)
#if MBEDTLS_PSA_MAX_EC_PUBKEY_LENGTH < (2 * ((192 + 7) / 8) + 1)
#undef MBEDTLS_PSA_MAX_EC_PUBKEY_LENGTH
#define MBEDTLS_PSA_MAX_EC_PUBKEY_LENGTH (2 * ((192 + 7) / 8) + 1)
#endif
#endif /* MBEDTLS_ECP_DP_SECP192R1_ENABLED */

#if defined(MBEDTLS_ECP_DP_SECP224R1_ENABLED)
#if MBEDTLS_PSA_MAX_EC_PUBKEY_LENGTH < (2 * ((224 + 7) / 8) + 1)
#undef MBEDTLS_PSA_MAX_EC_PUBKEY_LENGTH
#define MBEDTLS_PSA_MAX_EC_PUBKEY_LENGTH (2 * ((224 + 7) / 8) + 1)
#endif
#endif /* MBEDTLS_ECP_DP_SECP224R1_ENABLED */

#if defined(MBEDTLS_ECP_DP_SECP256R1_ENABLED)
#if MBEDTLS_PSA_MAX_EC_PUBKEY_LENGTH < (2 * ((256 + 7) / 8) + 1)
#undef MBEDTLS_PSA_MAX_EC_PUBKEY_LENGTH
#define MBEDTLS_PSA_MAX_EC_PUBKEY_LENGTH (2 * ((256 + 7) / 8) + 1)
#endif
#endif /* MBEDTLS_ECP_DP_SECP256R1_ENABLED */

#if defined(MBEDTLS_ECP_DP_SECP384R1_ENABLED)
#if MBEDTLS_PSA_MAX_EC_PUBKEY_LENGTH < (2 * ((384 + 7) / 8) + 1)
#undef MBEDTLS_PSA_MAX_EC_PUBKEY_LENGTH
#define MBEDTLS_PSA_MAX_EC_PUBKEY_LENGTH (2 * ((384 + 7) / 8) + 1)
#endif
#endif /* MBEDTLS_ECP_DP_SECP384R1_ENABLED */

#if defined(MBEDTLS_ECP_DP_SECP521R1_ENABLED)
#if MBEDTLS_PSA_MAX_EC_PUBKEY_LENGTH < (2 * ((521 + 7) / 8) + 1)
#undef MBEDTLS_PSA_MAX_EC_PUBKEY_LENGTH
#define MBEDTLS_PSA_MAX_EC_PUBKEY_LENGTH (2 * ((521 + 7) / 8) + 1)
#endif
#endif /* MBEDTLS_ECP_DP_SECP521R1_ENABLED */

#if defined(MBEDTLS_ECP_DP_SECP192K1_ENABLED)
#if MBEDTLS_PSA_MAX_EC_PUBKEY_LENGTH < (2 * ((192 + 7) / 8) + 1)
#undef MBEDTLS_PSA_MAX_EC_PUBKEY_LENGTH
#define MBEDTLS_PSA_MAX_EC_PUBKEY_LENGTH (2 * ((192 + 7) / 8) + 1)
#endif
#endif /* MBEDTLS_ECP_DP_SECP192K1_ENABLED */

#if defined(MBEDTLS_ECP_DP_SECP224K1_ENABLED)
#if MBEDTLS_PSA_MAX_EC_PUBKEY_LENGTH < (2 * ((224 + 7) / 8) + 1)
#undef MBEDTLS_PSA_MAX_EC_PUBKEY_LENGTH
#define MBEDTLS_PSA_MAX_EC_PUBKEY_LENGTH (2 * ((224 + 7) / 8) + 1)
#endif
#endif /* MBEDTLS_ECP_DP_SECP224K1_ENABLED */

#if defined(MBEDTLS_ECP_DP_SECP256K1_ENABLED)
#if MBEDTLS_PSA_MAX_EC_PUBKEY_LENGTH < (2 * ((256 + 7) / 8) + 1)
#undef MBEDTLS_PSA_MAX_EC_PUBKEY_LENGTH
#define MBEDTLS_PSA_MAX_EC_PUBKEY_LENGTH (2 * ((256 + 7) / 8) + 1)
#endif
#endif /* MBEDTLS_ECP_DP_SECP256K1_ENABLED */

#if defined(MBEDTLS_ECP_DP_BP256R1_ENABLED)
#if MBEDTLS_PSA_MAX_EC_PUBKEY_LENGTH < (2 * ((256 + 7) / 8) + 1)
#undef MBEDTLS_PSA_MAX_EC_PUBKEY_LENGTH
#define MBEDTLS_PSA_MAX_EC_PUBKEY_LENGTH (2 * ((256 + 7) / 8) + 1)
#endif
#endif /* MBEDTLS_ECP_DP_BP256R1_ENABLED */

#if defined(MBEDTLS_ECP_DP_BP384R1_ENABLED)
#if MBEDTLS_PSA_MAX_EC_PUBKEY_LENGTH < (2 * ((384 + 7) / 8) + 1)
#undef MBEDTLS_PSA_MAX_EC_PUBKEY_LENGTH
#define MBEDTLS_PSA_MAX_EC_PUBKEY_LENGTH (2 * ((384 + 7) / 8) + 1)
#endif
#endif /* MBEDTLS_ECP_DP_BP384R1_ENABLED */

#if defined(MBEDTLS_ECP_DP_BP512R1_ENABLED)
#if MBEDTLS_PSA_MAX_EC_PUBKEY_LENGTH < (2 * ((512 + 7) / 8) + 1)
#undef MBEDTLS_PSA_MAX_EC_PUBKEY_LENGTH
#define MBEDTLS_PSA_MAX_EC_PUBKEY_LENGTH (2 * ((512 + 7) / 8) + 1)
#endif
#endif /* MBEDTLS_ECP_DP_BP512R1_ENABLED */


/* Translations for PK layer */

static inline int mbedtls_psa_err_translate_pk(psa_status_t status)
{
    switch (status) {
        case PSA_SUCCESS:
            return 0;
        case PSA_ERROR_NOT_SUPPORTED:
            return MBEDTLS_ERR_PK_FEATURE_UNAVAILABLE;
        case PSA_ERROR_INSUFFICIENT_MEMORY:
            return MBEDTLS_ERR_PK_ALLOC_FAILED;
        case PSA_ERROR_INSUFFICIENT_ENTROPY:
            return MBEDTLS_ERR_ECP_RANDOM_FAILED;
        case PSA_ERROR_BAD_STATE:
            return MBEDTLS_ERR_PK_BAD_INPUT_DATA;
        /* All other failures */
        case PSA_ERROR_COMMUNICATION_FAILURE:
        case PSA_ERROR_HARDWARE_FAILURE:
        case PSA_ERROR_CORRUPTION_DETECTED:
            return MBEDTLS_ERR_PK_HW_ACCEL_FAILED;
        default: /* We return the same as for the 'other failures',
                  * but list them separately nonetheless to indicate
                  * which failure conditions we have considered. */
            return MBEDTLS_ERR_PK_HW_ACCEL_FAILED;
    }
}

/* Translations for ECC */

/* This function transforms an ECC group identifier from
 * https://www.iana.org/assignments/tls-parameters/tls-parameters.xhtml#tls-parameters-8
 * into a PSA ECC group identifier. */
#if defined(MBEDTLS_ECP_C)
static inline psa_key_type_t mbedtls_psa_parse_tls_ecc_group(
    uint16_t tls_ecc_grp_reg_id, size_t *bits)
{
    const mbedtls_ecp_curve_info *curve_info =
        mbedtls_ecp_curve_info_from_tls_id(tls_ecc_grp_reg_id);
    if (curve_info == NULL) {
        return 0;
    }
    return PSA_KEY_TYPE_ECC_KEY_PAIR(
        mbedtls_ecc_group_to_psa(curve_info->grp_id, bits));
}
#endif /* MBEDTLS_ECP_C */

/* This function takes a buffer holding an EC public key
 * exported through psa_export_public_key(), and converts
 * it into an ECPoint structure to be put into a ClientKeyExchange
 * message in an ECDHE exchange.
 *
 * Both the present and the foreseeable future format of EC public keys
 * used by PSA have the ECPoint structure contained in the exported key
 * as a subbuffer, and the function merely selects this subbuffer instead
 * of making a copy.
 */
static inline int mbedtls_psa_tls_psa_ec_to_ecpoint(unsigned char *src,
                                                    size_t srclen,
                                                    unsigned char **dst,
                                                    size_t *dstlen)
{
    *dst = src;
    *dstlen = srclen;
    return 0;
}

/* This function takes a buffer holding an ECPoint structure
 * (as contained in a TLS ServerKeyExchange message for ECDHE
 * exchanges) and converts it into a format that the PSA key
 * agreement API understands.
 */
static inline int mbedtls_psa_tls_ecpoint_to_psa_ec(unsigned char const *src,
                                                    size_t srclen,
                                                    unsigned char *dst,
                                                    size_t dstlen,
                                                    size_t *olen)
{
    if (srclen > dstlen) {
        return MBEDTLS_ERR_ECP_BUFFER_TOO_SMALL;
    }

    memcpy(dst, src, srclen);
    *olen = srclen;
    return 0;
}

#endif /* MBEDTLS_USE_PSA_CRYPTO */

/* Expose whatever RNG the PSA subsystem uses to applications using the
 * mbedtls_xxx API. The declarations and definitions here need to be
 * consistent with the implementation in library/psa_crypto_random_impl.h.
 * See that file for implementation documentation. */
#if defined(MBEDTLS_PSA_CRYPTO_C)

/* The type of a `f_rng` random generator function that many library functions
 * take.
 *
 * This type name is not part of the Mbed TLS stable API. It may be renamed
 * or moved without warning.
 */
typedef int mbedtls_f_rng_t(void *p_rng, unsigned char *output, size_t output_size);

#if defined(MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG)

/** The random generator function for the PSA subsystem.
 *
 * This function is suitable as the `f_rng` random generator function
 * parameter of many `mbedtls_xxx` functions. Use #MBEDTLS_PSA_RANDOM_STATE
 * to obtain the \p p_rng parameter.
 *
 * The implementation of this function depends on the configuration of the
 * library.
 *
 * \note Depending on the configuration, this may be a function or
 *       a pointer to a function.
 *
 * \note This function may only be used if the PSA crypto subsystem is active.
 *       This means that you must call psa_crypto_init() before any call to
 *       this function, and you must not call this function after calling
 *       mbedtls_psa_crypto_free().
 *
 * \param p_rng         The random generator context. This must be
 *                      #MBEDTLS_PSA_RANDOM_STATE. No other state is
 *                      supported.
 * \param output        The buffer to fill. It must have room for
 *                      \c output_size bytes.
 * \param output_size   The number of bytes to write to \p output.
 *                      This function may fail if \p output_size is too
 *                      large. It is guaranteed to accept any output size
 *                      requested by Mbed TLS library functions. The
 *                      maximum request size depends on the library
 *                      configuration.
 *
 * \return              \c 0 on success.
 * \return              An `MBEDTLS_ERR_ENTROPY_xxx`,
 *                      `MBEDTLS_ERR_PLATFORM_xxx,
 *                      `MBEDTLS_ERR_CTR_DRBG_xxx` or
 *                      `MBEDTLS_ERR_HMAC_DRBG_xxx` on error.
 */
int mbedtls_psa_get_random(void *p_rng,
                           unsigned char *output,
                           size_t output_size);

/** The random generator state for the PSA subsystem.
 *
 * This macro expands to an expression which is suitable as the `p_rng`
 * random generator state parameter of many `mbedtls_xxx` functions.
 * It must be used in combination with the random generator function
 * mbedtls_psa_get_random().
 *
 * The implementation of this macro depends on the configuration of the
 * library. Do not make any assumption on its nature.
 */
#define MBEDTLS_PSA_RANDOM_STATE NULL

#else /* !defined(MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG) */

#if defined(MBEDTLS_CTR_DRBG_C)
#include "mbedtls/ctr_drbg.h"
typedef mbedtls_ctr_drbg_context mbedtls_psa_drbg_context_t;
static mbedtls_f_rng_t *const mbedtls_psa_get_random = mbedtls_ctr_drbg_random;
#elif defined(MBEDTLS_HMAC_DRBG_C)
#include "mbedtls/hmac_drbg.h"
typedef mbedtls_hmac_drbg_context mbedtls_psa_drbg_context_t;
static mbedtls_f_rng_t *const mbedtls_psa_get_random = mbedtls_hmac_drbg_random;
#endif
extern mbedtls_psa_drbg_context_t *const mbedtls_psa_random_state;

#define MBEDTLS_PSA_RANDOM_STATE mbedtls_psa_random_state

#endif /* !defined(MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG) */

#endif /* MBEDTLS_PSA_CRYPTO_C */

#endif /* MBEDTLS_PSA_UTIL_H */
