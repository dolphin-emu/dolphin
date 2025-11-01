/**
 * \file check_crypto_config.h
 *
 * \brief Consistency checks for PSA configuration options
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

/*
 * It is recommended to include this file from your crypto_config.h
 * in order to catch dependency issues early.
 */

#ifndef MBEDTLS_CHECK_CRYPTO_CONFIG_H
#define MBEDTLS_CHECK_CRYPTO_CONFIG_H

#if defined(PSA_WANT_ALG_CCM) && \
    !(defined(PSA_WANT_KEY_TYPE_AES) || \
    defined(PSA_WANT_KEY_TYPE_CAMELLIA))
#error "PSA_WANT_ALG_CCM defined, but not all prerequisites"
#endif

#if defined(PSA_WANT_ALG_CMAC) && \
    !(defined(PSA_WANT_KEY_TYPE_AES) || \
    defined(PSA_WANT_KEY_TYPE_CAMELLIA) || \
    defined(PSA_WANT_KEY_TYPE_DES))
#error "PSA_WANT_ALG_CMAC defined, but not all prerequisites"
#endif

#if defined(PSA_WANT_ALG_DETERMINISTIC_ECDSA) && \
    !(defined(PSA_WANT_KEY_TYPE_ECC_KEY_PAIR) || \
    defined(PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY))
#error "PSA_WANT_ALG_DETERMINISTIC_ECDSA defined, but not all prerequisites"
#endif

#if defined(PSA_WANT_ALG_ECDSA) && \
    !(defined(PSA_WANT_KEY_TYPE_ECC_KEY_PAIR) || \
    defined(PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY))
#error "PSA_WANT_ALG_ECDSA defined, but not all prerequisites"
#endif

#if defined(PSA_WANT_ALG_GCM) && \
    !(defined(PSA_WANT_KEY_TYPE_AES) || \
    defined(PSA_WANT_KEY_TYPE_CAMELLIA))
#error "PSA_WANT_ALG_GCM defined, but not all prerequisites"
#endif

#if defined(PSA_WANT_ALG_RSA_PKCS1V15_CRYPT) && \
    !(defined(PSA_WANT_KEY_TYPE_RSA_KEY_PAIR) || \
    defined(PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY))
#error "PSA_WANT_ALG_RSA_PKCS1V15_CRYPT defined, but not all prerequisites"
#endif

#if defined(PSA_WANT_ALG_RSA_PKCS1V15_SIGN) && \
    !(defined(PSA_WANT_KEY_TYPE_RSA_KEY_PAIR) || \
    defined(PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY))
#error "PSA_WANT_ALG_RSA_PKCS1V15_SIGN defined, but not all prerequisites"
#endif

#if defined(PSA_WANT_ALG_RSA_OAEP) && \
    !(defined(PSA_WANT_KEY_TYPE_RSA_KEY_PAIR) || \
    defined(PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY))
#error "PSA_WANT_ALG_RSA_OAEP defined, but not all prerequisites"
#endif

#if defined(PSA_WANT_ALG_RSA_PSS) && \
    !(defined(PSA_WANT_KEY_TYPE_RSA_KEY_PAIR) || \
    defined(PSA_WANT_KEY_TYPE_RSA_PUBLIC_KEY))
#error "PSA_WANT_ALG_RSA_PSS defined, but not all prerequisites"
#endif

#if defined(PSA_WANT_KEY_TYPE_ECC_KEY_PAIR) && \
    !defined(PSA_WANT_KEY_TYPE_ECC_PUBLIC_KEY)
#error "PSA_WANT_KEY_TYPE_ECC_KEY_PAIR defined, but not all prerequisites"
#endif

#endif /* MBEDTLS_CHECK_CRYPTO_CONFIG_H */
