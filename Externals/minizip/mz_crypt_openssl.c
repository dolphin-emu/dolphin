/* mz_crypt_openssl.c -- Crypto/hash functions for OpenSSL
   part of the minizip-ng project

   Copyright (C) 2010-2021 Nathan Moinvaziri
     https://github.com/zlib-ng/minizip-ng

   This program is distributed under the terms of the same license as zlib.
   See the accompanying LICENSE file for the full text of the license.
*/


#include "mz.h"

#include <openssl/err.h>
#include <openssl/engine.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/aes.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>

#if defined(MZ_ZIP_SIGNING)
/* Note: https://www.imperialviolet.org/2015/10/17/boringssl.html says that
   BoringSSL does not support CMS. "#include <etc/cms.h>" will fail. See
   https://bugs.chromium.org/p/boringssl/issues/detail?id=421
*/
#include <openssl/cms.h>
#include <openssl/pkcs12.h>
#include <openssl/x509.h>
#endif

/***************************************************************************/

static void mz_crypt_init(void) {
    static int32_t openssl_initialized = 0;
    if (openssl_initialized == 0) {
        OpenSSL_add_all_algorithms();

        ERR_load_BIO_strings();
        ERR_load_crypto_strings();

        ENGINE_load_builtin_engines();
        ENGINE_register_all_complete();

        openssl_initialized = 1;
    }
}

int32_t mz_crypt_rand(uint8_t *buf, int32_t size) {
    int32_t result = 0;

    result = RAND_bytes(buf, size);

    if (!result)
        return MZ_CRYPT_ERROR;

    return size;
}

/***************************************************************************/

typedef struct mz_crypt_sha_s {
    SHA256_CTX ctx256;
    SHA_CTX    ctx1;
    int32_t    initialized;
    int32_t    error;
    uint16_t   algorithm;
} mz_crypt_sha;

/***************************************************************************/

void mz_crypt_sha_reset(void *handle) {
    mz_crypt_sha *sha = (mz_crypt_sha *)handle;

    sha->error = 0;
    sha->initialized = 0;

    mz_crypt_init();
}

int32_t mz_crypt_sha_begin(void *handle) {
    mz_crypt_sha *sha = (mz_crypt_sha *)handle;
    int32_t result = 0;


    if (sha == NULL)
        return MZ_PARAM_ERROR;

    mz_crypt_sha_reset(handle);

    if (sha->algorithm == MZ_HASH_SHA1)
        result = SHA1_Init(&sha->ctx1);
    else
        result = SHA256_Init(&sha->ctx256);

    if (!result) {
        sha->error = ERR_get_error();
        return MZ_HASH_ERROR;
    }

    sha->initialized = 1;
    return MZ_OK;
}

int32_t mz_crypt_sha_update(void *handle, const void *buf, int32_t size) {
    mz_crypt_sha *sha = (mz_crypt_sha *)handle;
    int32_t result = 0;

    if (sha == NULL || buf == NULL || !sha->initialized)
        return MZ_PARAM_ERROR;

    if (sha->algorithm == MZ_HASH_SHA1)
        result = SHA1_Update(&sha->ctx1, buf, size);
    else
        result = SHA256_Update(&sha->ctx256, buf, size);

    if (!result) {
        sha->error = ERR_get_error();
        return MZ_HASH_ERROR;
    }

    return size;
}

int32_t mz_crypt_sha_end(void *handle, uint8_t *digest, int32_t digest_size) {
    mz_crypt_sha *sha = (mz_crypt_sha *)handle;
    int32_t result = 0;

    if (sha == NULL || digest == NULL || !sha->initialized)
        return MZ_PARAM_ERROR;

    if (sha->algorithm == MZ_HASH_SHA1) {
        if (digest_size < MZ_HASH_SHA1_SIZE)
            return MZ_BUF_ERROR;
        result = SHA1_Final(digest, &sha->ctx1);
    } else {
        if (digest_size < MZ_HASH_SHA256_SIZE)
            return MZ_BUF_ERROR;
        result = SHA256_Final(digest, &sha->ctx256);
    }

    if (!result) {
        sha->error = ERR_get_error();
        return MZ_HASH_ERROR;
    }

    return MZ_OK;
}

void mz_crypt_sha_set_algorithm(void *handle, uint16_t algorithm) {
    mz_crypt_sha *sha = (mz_crypt_sha *)handle;
    sha->algorithm = algorithm;
}

void *mz_crypt_sha_create(void **handle) {
    mz_crypt_sha *sha = NULL;

    sha = (mz_crypt_sha *)MZ_ALLOC(sizeof(mz_crypt_sha));
    if (sha != NULL) {
        memset(sha, 0, sizeof(mz_crypt_sha));
        sha->algorithm = MZ_HASH_SHA256;
    }
    if (handle != NULL)
        *handle = sha;

    return sha;
}

void mz_crypt_sha_delete(void **handle) {
    mz_crypt_sha *sha = NULL;
    if (handle == NULL)
        return;
    sha = (mz_crypt_sha *)*handle;
    if (sha != NULL) {
        mz_crypt_sha_reset(*handle);
        MZ_FREE(sha);
    }
    *handle = NULL;
}

/***************************************************************************/

typedef struct mz_crypt_aes_s {
    AES_KEY    key;
    int32_t    mode;
    int32_t    error;
    uint8_t    *key_copy;
    int32_t    key_length;
} mz_crypt_aes;

/***************************************************************************/

void mz_crypt_aes_reset(void *handle) {
    MZ_UNUSED(handle);

    mz_crypt_init();
}

int32_t mz_crypt_aes_encrypt(void *handle, uint8_t *buf, int32_t size) {
    mz_crypt_aes *aes = (mz_crypt_aes *)handle;

    if (aes == NULL || buf == NULL)
        return MZ_PARAM_ERROR;
    if (size != MZ_AES_BLOCK_SIZE)
        return MZ_PARAM_ERROR;

    AES_encrypt(buf, buf, &aes->key);
    /* Equivalent to AES_ecb_encrypt with AES_ENCRYPT */
    return size;
}

int32_t mz_crypt_aes_decrypt(void *handle, uint8_t *buf, int32_t size) {
    mz_crypt_aes *aes = (mz_crypt_aes *)handle;
    if (aes == NULL || buf == NULL)
        return MZ_PARAM_ERROR;
    if (size != MZ_AES_BLOCK_SIZE)
        return MZ_PARAM_ERROR;

    AES_decrypt(buf, buf, &aes->key);
    /* Equivalent to AES_ecb_encrypt with AES_DECRYPT */
    return size;
}

int32_t mz_crypt_aes_set_encrypt_key(void *handle, const void *key, int32_t key_length) {
    mz_crypt_aes *aes = (mz_crypt_aes *)handle;
    int32_t result = 0;
    int32_t key_bits = 0;


    if (aes == NULL || key == NULL)
        return MZ_PARAM_ERROR;

    mz_crypt_aes_reset(handle);

    key_bits = key_length * 8;
    result = AES_set_encrypt_key(key, key_bits, &aes->key);
    if (result) {
        aes->error = ERR_get_error();
        return MZ_HASH_ERROR;
    }

    return MZ_OK;
}

int32_t mz_crypt_aes_set_decrypt_key(void *handle, const void *key, int32_t key_length) {
    mz_crypt_aes *aes = (mz_crypt_aes *)handle;
    int32_t result = 0;
    int32_t key_bits = 0;


    if (aes == NULL || key == NULL)
        return MZ_PARAM_ERROR;

    mz_crypt_aes_reset(handle);

    key_bits = key_length * 8;
    result = AES_set_decrypt_key(key, key_bits, &aes->key);
    if (result) {
        aes->error = ERR_get_error();
        return MZ_HASH_ERROR;
    }

    return MZ_OK;
}

void mz_crypt_aes_set_mode(void *handle, int32_t mode) {
    mz_crypt_aes *aes = (mz_crypt_aes *)handle;
    aes->mode = mode;
}

void *mz_crypt_aes_create(void **handle) {
    mz_crypt_aes *aes = NULL;

    aes = (mz_crypt_aes *)MZ_ALLOC(sizeof(mz_crypt_aes));
    if (aes != NULL)
        memset(aes, 0, sizeof(mz_crypt_aes));
    if (handle != NULL)
        *handle = aes;

    return aes;
}

void mz_crypt_aes_delete(void **handle) {
    mz_crypt_aes *aes = NULL;
    if (handle == NULL)
        return;
    aes = (mz_crypt_aes *)*handle;
    if (aes != NULL)
        MZ_FREE(aes);
    *handle = NULL;
}

/***************************************************************************/

typedef struct mz_crypt_hmac_s {
    HMAC_CTX   *ctx;
    int32_t    initialized;
    int32_t    error;
    uint16_t   algorithm;
} mz_crypt_hmac;

/***************************************************************************/

#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || (defined(LIBRESSL_VERSION_NUMBER) && (LIBRESSL_VERSION_NUMBER < 0x2070000fL))
static HMAC_CTX *HMAC_CTX_new(void) {
    HMAC_CTX *ctx = OPENSSL_malloc(sizeof(HMAC_CTX));
    if (ctx != NULL)
        HMAC_CTX_init(ctx);
    return ctx;
}

static void HMAC_CTX_free(HMAC_CTX *ctx) {
    if (ctx != NULL) {
        HMAC_CTX_cleanup(ctx);
        OPENSSL_free(ctx);
    }
}
#endif

/***************************************************************************/

void mz_crypt_hmac_reset(void *handle) {
    mz_crypt_hmac *hmac = (mz_crypt_hmac *)handle;

    HMAC_CTX_free(hmac->ctx);

    hmac->ctx = NULL;
    hmac->error = 0;

    mz_crypt_init();
}

int32_t mz_crypt_hmac_init(void *handle, const void *key, int32_t key_length) {
    mz_crypt_hmac *hmac = (mz_crypt_hmac *)handle;
    int32_t result = 0;
    const EVP_MD *evp_md = NULL;

    if (hmac == NULL || key == NULL)
        return MZ_PARAM_ERROR;

    mz_crypt_hmac_reset(handle);

    hmac->ctx = HMAC_CTX_new();

    if (hmac->algorithm == MZ_HASH_SHA1)
        evp_md = EVP_sha1();
    else
        evp_md = EVP_sha256();

    result = HMAC_Init_ex(hmac->ctx, key, key_length, evp_md, NULL);
    if (!result) {
        hmac->error = ERR_get_error();
        return MZ_HASH_ERROR;
    }

    return MZ_OK;
}

int32_t mz_crypt_hmac_update(void *handle, const void *buf, int32_t size) {
    mz_crypt_hmac *hmac = (mz_crypt_hmac *)handle;
    int32_t result = 0;

    if (hmac == NULL || buf == NULL)
        return MZ_PARAM_ERROR;

    result = HMAC_Update(hmac->ctx, buf, size);
    if (!result) {
        hmac->error = ERR_get_error();
        return MZ_HASH_ERROR;
    }

    return MZ_OK;
}

int32_t mz_crypt_hmac_end(void *handle, uint8_t *digest, int32_t digest_size) {
    mz_crypt_hmac *hmac = (mz_crypt_hmac *)handle;
    int32_t result = 0;

    if (hmac == NULL || digest == NULL)
        return MZ_PARAM_ERROR;

    if (hmac->algorithm == MZ_HASH_SHA1) {
        if (digest_size < MZ_HASH_SHA1_SIZE)
            return MZ_BUF_ERROR;

        result = HMAC_Final(hmac->ctx, digest, (uint32_t *)&digest_size);
    } else {
        if (digest_size < MZ_HASH_SHA256_SIZE)
            return MZ_BUF_ERROR;
        result = HMAC_Final(hmac->ctx, digest, (uint32_t *)&digest_size);
    }

    if (!result) {
        hmac->error = ERR_get_error();
        return MZ_HASH_ERROR;
    }

    return MZ_OK;
}

void mz_crypt_hmac_set_algorithm(void *handle, uint16_t algorithm) {
    mz_crypt_hmac *hmac = (mz_crypt_hmac *)handle;
    hmac->algorithm = algorithm;
}

int32_t mz_crypt_hmac_copy(void *src_handle, void *target_handle) {
    mz_crypt_hmac *source = (mz_crypt_hmac *)src_handle;
    mz_crypt_hmac *target = (mz_crypt_hmac *)target_handle;
    int32_t result = 0;

    if (source == NULL || target == NULL)
        return MZ_PARAM_ERROR;

    mz_crypt_hmac_reset(target_handle);

    if (target->ctx == NULL)
        target->ctx = HMAC_CTX_new();

    result = HMAC_CTX_copy(target->ctx, source->ctx);
    if (!result) {
        target->error = ERR_get_error();
        return MZ_HASH_ERROR;
    }

    return MZ_OK;
}

void *mz_crypt_hmac_create(void **handle) {
    mz_crypt_hmac *hmac = NULL;

    hmac = (mz_crypt_hmac *)MZ_ALLOC(sizeof(mz_crypt_hmac));
    if (hmac != NULL) {
        memset(hmac, 0, sizeof(mz_crypt_hmac));
        hmac->algorithm = MZ_HASH_SHA256;
    }
    if (handle != NULL)
        *handle = hmac;

    return hmac;
}

void mz_crypt_hmac_delete(void **handle) {
    mz_crypt_hmac *hmac = NULL;
    if (handle == NULL)
        return;
    hmac = (mz_crypt_hmac *)*handle;
    if (hmac != NULL) {
        mz_crypt_hmac_reset(*handle);
        MZ_FREE(hmac);
    }
    *handle = NULL;
}

/***************************************************************************/

#if defined(MZ_ZIP_SIGNING)
int32_t mz_crypt_sign(uint8_t *message, int32_t message_size, uint8_t *cert_data, int32_t cert_data_size,
    const char *cert_pwd, uint8_t **signature, int32_t *signature_size) {
    PKCS12 *p12 = NULL;
    EVP_PKEY *evp_pkey = NULL;
    BUF_MEM *buf_mem = NULL;
    BIO *cert_bio = NULL;
    BIO *message_bio = NULL;
    BIO *signature_bio = NULL;
    CMS_ContentInfo *cms = NULL;
    CMS_SignerInfo *signer_info = NULL;
    STACK_OF(X509) *ca_stack = NULL;
    X509 *cert = NULL;
    int32_t result = 0;
    int32_t err = MZ_OK;


    if (message == NULL || cert_data == NULL || signature == NULL || signature_size == NULL)
        return MZ_PARAM_ERROR;

    mz_crypt_init();

    *signature = NULL;
    *signature_size = 0;

    cert_bio = BIO_new_mem_buf(cert_data, cert_data_size);

    if (d2i_PKCS12_bio(cert_bio, &p12) == NULL)
        err = MZ_SIGN_ERROR;
    if (err == MZ_OK)
        result = PKCS12_parse(p12, cert_pwd, &evp_pkey, &cert, &ca_stack);
    if (result) {
        cms = CMS_sign(NULL, NULL, ca_stack, NULL, CMS_BINARY | CMS_PARTIAL);
        if (cms)
            signer_info = CMS_add1_signer(cms, cert, evp_pkey, EVP_sha256(), 0);
        if (signer_info == NULL) {
            err = MZ_SIGN_ERROR;
        } else {
            message_bio = BIO_new_mem_buf(message, message_size);
            signature_bio = BIO_new(BIO_s_mem());

            result = CMS_final(cms, message_bio, NULL, CMS_BINARY);
            if (result)
                result = i2d_CMS_bio(signature_bio, cms);
            if (result) {
                BIO_flush(signature_bio);
                BIO_get_mem_ptr(signature_bio, &buf_mem);

                *signature_size = buf_mem->length;
                *signature = MZ_ALLOC(buf_mem->length);

                memcpy(*signature, buf_mem->data, buf_mem->length);
            }
#if 0
            BIO *yy = BIO_new_file("xyz", "wb");
            BIO_write(yy, *signature, *signature_size);
            BIO_flush(yy);
            BIO_free(yy);
#endif
        }
    }

    if (!result)
        err = MZ_SIGN_ERROR;

    if (cms)
        CMS_ContentInfo_free(cms);
    if (signature_bio)
        BIO_free(signature_bio);
    if (cert_bio)
        BIO_free(cert_bio);
    if (message_bio)
        BIO_free(message_bio);
    if (p12)
        PKCS12_free(p12);

    if (err != MZ_OK && *signature != NULL) {
        MZ_FREE(*signature);
        *signature = NULL;
        *signature_size = 0;
    }

    return err;
}

int32_t mz_crypt_sign_verify(uint8_t *message, int32_t message_size, uint8_t *signature, int32_t signature_size) {
    CMS_ContentInfo *cms = NULL;
    STACK_OF(X509) *signers = NULL;
    STACK_OF(X509) *intercerts = NULL;
    X509_STORE *cert_store = NULL;
    X509_LOOKUP *lookup = NULL;
    X509_STORE_CTX *store_ctx = NULL;
    BIO *message_bio = NULL;
    BIO *signature_bio = NULL;
    BUF_MEM *buf_mem = NULL;
    int32_t signer_count = 0;
    int32_t result = 0;
    int32_t i = 0;
    int32_t err = MZ_SIGN_ERROR;


    if (message == NULL || message_size == 0 || signature == NULL || signature_size == 0)
        return MZ_PARAM_ERROR;

    mz_crypt_init();

    cert_store = X509_STORE_new();

    X509_STORE_load_locations(cert_store, "cacert.pem", NULL);
    X509_STORE_set_default_paths(cert_store);

#if 0
    BIO *yy = BIO_new_file("xyz", "wb");
    BIO_write(yy, signature, signature_size);
    BIO_flush(yy);
    BIO_free(yy);
#endif

    lookup = X509_STORE_add_lookup(cert_store, X509_LOOKUP_file());
    if (lookup != NULL)
        X509_LOOKUP_load_file(lookup, "cacert.pem", X509_FILETYPE_PEM);
    lookup = X509_STORE_add_lookup(cert_store, X509_LOOKUP_hash_dir());
    if (lookup != NULL)
        X509_LOOKUP_add_dir(lookup, NULL, X509_FILETYPE_DEFAULT);

    signature_bio = BIO_new_mem_buf(signature, signature_size);
    message_bio = BIO_new(BIO_s_mem());

    cms = d2i_CMS_bio(signature_bio, NULL);
    if (cms) {
        result = CMS_verify(cms, NULL, cert_store, NULL, message_bio, CMS_NO_SIGNER_CERT_VERIFY | CMS_BINARY);
        if (result)
            signers = CMS_get0_signers(cms);
        if (signers)
            intercerts = CMS_get1_certs(cms);
        if (intercerts) {
            /* Verify signer certificates */
            signer_count = sk_X509_num(signers);
            if (signer_count > 0)
                err = MZ_OK;

            for (i = 0; i < signer_count; i++) {
                store_ctx = X509_STORE_CTX_new();
                X509_STORE_CTX_init(store_ctx, cert_store, sk_X509_value(signers, i), intercerts);
                result = X509_verify_cert(store_ctx);
                if (store_ctx)
                    X509_STORE_CTX_free(store_ctx);

                if (!result) {
                    err = MZ_SIGN_ERROR;
                    break;
                }
            }
        }

        BIO_get_mem_ptr(message_bio, &buf_mem);

        if (err == MZ_OK) {
            /* Verify the message */
            if (((int32_t)buf_mem->length != message_size) ||
                (memcmp(buf_mem->data, message, message_size) != 0))
                err = MZ_SIGN_ERROR;
        }
    }

#if 0
    if (!result)
        printf(ERR_error_string(ERR_get_error(), NULL));
#endif

    if (cms)
        CMS_ContentInfo_free(cms);
    if (message_bio)
        BIO_free(message_bio);
    if (signature_bio)
        BIO_free(signature_bio);
    if (cert_store)
        X509_STORE_free(cert_store);

    return err;
}
#endif
