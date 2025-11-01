/**
 * \file pk_internal.h
 *
 * \brief Public Key abstraction layer: wrapper functions
 */
/*
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef MBEDTLS_PK_WRAP_H
#define MBEDTLS_PK_WRAP_H

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#include "mbedtls/pk.h"

struct mbedtls_pk_info_t {
    /** Public key type */
    mbedtls_pk_type_t type;

    /** Type name */
    const char *name;

    /** Get key size in bits */
    size_t (*get_bitlen)(const void *);

    /** Tell if the context implements this type (e.g. ECKEY can do ECDSA) */
    int (*can_do)(mbedtls_pk_type_t type);

    /** Verify signature */
    int (*verify_func)(void *ctx, mbedtls_md_type_t md_alg,
                       const unsigned char *hash, size_t hash_len,
                       const unsigned char *sig, size_t sig_len);

    /** Make signature */
    int (*sign_func)(void *ctx, mbedtls_md_type_t md_alg,
                     const unsigned char *hash, size_t hash_len,
                     unsigned char *sig, size_t *sig_len,
                     int (*f_rng)(void *, unsigned char *, size_t),
                     void *p_rng);

#if defined(MBEDTLS_ECDSA_C) && defined(MBEDTLS_ECP_RESTARTABLE)
    /** Verify signature (restartable) */
    int (*verify_rs_func)(void *ctx, mbedtls_md_type_t md_alg,
                          const unsigned char *hash, size_t hash_len,
                          const unsigned char *sig, size_t sig_len,
                          void *rs_ctx);

    /** Make signature (restartable) */
    int (*sign_rs_func)(void *ctx, mbedtls_md_type_t md_alg,
                        const unsigned char *hash, size_t hash_len,
                        unsigned char *sig, size_t *sig_len,
                        int (*f_rng)(void *, unsigned char *, size_t),
                        void *p_rng, void *rs_ctx);
#endif /* MBEDTLS_ECDSA_C && MBEDTLS_ECP_RESTARTABLE */

    /** Decrypt message */
    int (*decrypt_func)(void *ctx, const unsigned char *input, size_t ilen,
                        unsigned char *output, size_t *olen, size_t osize,
                        int (*f_rng)(void *, unsigned char *, size_t),
                        void *p_rng);

    /** Encrypt message */
    int (*encrypt_func)(void *ctx, const unsigned char *input, size_t ilen,
                        unsigned char *output, size_t *olen, size_t osize,
                        int (*f_rng)(void *, unsigned char *, size_t),
                        void *p_rng);

    /** Check public-private key pair */
    int (*check_pair_func)(const void *pub, const void *prv);

    /** Allocate a new context */
    void * (*ctx_alloc_func)(void);

    /** Free the given context */
    void (*ctx_free_func)(void *ctx);

#if defined(MBEDTLS_ECDSA_C) && defined(MBEDTLS_ECP_RESTARTABLE)
    /** Allocate the restart context */
    void *(*rs_alloc_func)(void);

    /** Free the restart context */
    void (*rs_free_func)(void *rs_ctx);
#endif /* MBEDTLS_ECDSA_C && MBEDTLS_ECP_RESTARTABLE */

    /** Interface with the debug module */
    void (*debug_func)(const void *ctx, mbedtls_pk_debug_item *items);

};
#if defined(MBEDTLS_PK_RSA_ALT_SUPPORT)
/* Container for RSA-alt */
typedef struct {
    void *key;
    mbedtls_pk_rsa_alt_decrypt_func decrypt_func;
    mbedtls_pk_rsa_alt_sign_func sign_func;
    mbedtls_pk_rsa_alt_key_len_func key_len_func;
} mbedtls_rsa_alt_context;
#endif

#if defined(MBEDTLS_RSA_C)
extern const mbedtls_pk_info_t mbedtls_rsa_info;
#endif

#if defined(MBEDTLS_ECP_C)
extern const mbedtls_pk_info_t mbedtls_eckey_info;
extern const mbedtls_pk_info_t mbedtls_eckeydh_info;
#endif

#if defined(MBEDTLS_ECDSA_C)
extern const mbedtls_pk_info_t mbedtls_ecdsa_info;
#endif

#if defined(MBEDTLS_PK_RSA_ALT_SUPPORT)
extern const mbedtls_pk_info_t mbedtls_rsa_alt_info;
#endif

#if defined(MBEDTLS_USE_PSA_CRYPTO)
extern const mbedtls_pk_info_t mbedtls_pk_opaque_info;
#endif

#endif /* MBEDTLS_PK_WRAP_H */
