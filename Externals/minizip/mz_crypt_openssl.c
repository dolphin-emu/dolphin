/* mz_crypt_openssl.c -- Crypto/hash functions for OpenSSL
   part of the minizip-ng project

   Copyright (C) Nathan Moinvaziri
     https://github.com/zlib-ng/minizip-ng

   This program is distributed under the terms of the same license as zlib.
   See the accompanying LICENSE file for the full text of the license.
*/

#include "mz.h"
#include "mz_crypt.h"

#include <openssl/err.h>
#include <openssl/engine.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/aes.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#  include <openssl/core_names.h>
#endif

/***************************************************************************/

static void mz_crypt_init(void) {
    static int32_t openssl_initialized = 0;
    if (!openssl_initialized) {
#if OPENSSL_VERSION_NUMBER < 0x10100000L
        OpenSSL_add_all_algorithms();

        ERR_load_BIO_strings();
        ERR_load_crypto_strings();

        ENGINE_load_builtin_engines();
        ENGINE_register_all_complete();
#else
        OPENSSL_init_crypto(OPENSSL_INIT_ENGINE_ALL_BUILTIN, NULL);
#endif

        openssl_initialized = 1;
    }
}

int32_t mz_crypt_rand(uint8_t *buf, int32_t size) {
    if (!RAND_bytes(buf, size))
        return MZ_CRYPT_ERROR;

    return size;
}

/***************************************************************************/

typedef struct mz_crypt_sha_s {
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    union {
        SHA512_CTX ctx512;
        SHA256_CTX ctx256;
        SHA_CTX    ctx1;
    };
#else
    EVP_MD_CTX     *ctx;
#endif
    int32_t        initialized;
    int32_t        error;
    uint16_t       algorithm;
} mz_crypt_sha;

/***************************************************************************/

static const uint8_t mz_crypt_sha_digest_size[] = {
    MZ_HASH_SHA1_SIZE,                     0, MZ_HASH_SHA224_SIZE,
    MZ_HASH_SHA256_SIZE, MZ_HASH_SHA384_SIZE, MZ_HASH_SHA512_SIZE
};

/***************************************************************************/

static void mz_crypt_sha_free(void *handle) {
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
    mz_crypt_sha *sha = (mz_crypt_sha *)handle;
    if (sha->ctx)
        EVP_MD_CTX_free(sha->ctx);
    sha->ctx = NULL;
#else
    MZ_UNUSED(handle);
#endif
}

void mz_crypt_sha_reset(void *handle) {
    mz_crypt_sha *sha = (mz_crypt_sha *)handle;

    mz_crypt_init();
    mz_crypt_sha_free(handle);

    sha->error = 0;
    sha->initialized = 0;
}

int32_t mz_crypt_sha_begin(void *handle) {
    mz_crypt_sha *sha = (mz_crypt_sha *)handle;
    int32_t result = 0;

    if (!sha)
        return MZ_PARAM_ERROR;

    mz_crypt_sha_reset(handle);

#if OPENSSL_VERSION_NUMBER < 0x10100000L
    switch (sha->algorithm) {
    case MZ_HASH_SHA1:
        result = SHA1_Init(&sha->ctx1);
        break;
    case MZ_HASH_SHA224:
        result = SHA224_Init(&sha->ctx256);
        break;
    case MZ_HASH_SHA256:
        result = SHA256_Init(&sha->ctx256);
        break;
    case MZ_HASH_SHA384:
        result = SHA384_Init(&sha->ctx512);
        break;
    case MZ_HASH_SHA512:
        result = SHA512_Init(&sha->ctx512);
        break;
    }
#else
    const EVP_MD *md = NULL;
    switch (sha->algorithm) {
    case MZ_HASH_SHA1:
        md = EVP_sha1();
        break;
    case MZ_HASH_SHA224:
        md = EVP_sha224();
        break;
    case MZ_HASH_SHA256:
        md = EVP_sha256();
        break;
    case MZ_HASH_SHA384:
        md = EVP_sha384();
        break;
    case MZ_HASH_SHA512:
        md = EVP_sha512();
        break;
    }
    if (!md)
        return MZ_PARAM_ERROR;

    sha->ctx = EVP_MD_CTX_new();
    if (!sha->ctx)
        return MZ_MEM_ERROR;
    result = EVP_DigestInit_ex(sha->ctx, md, NULL);
#endif

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

    if (!sha || !buf || !sha->initialized)
        return MZ_PARAM_ERROR;

#if OPENSSL_VERSION_NUMBER < 0x10100000L
    switch (sha->algorithm) {
    case MZ_HASH_SHA1:
        result = SHA1_Update(&sha->ctx1, buf, size);
        break;
    case MZ_HASH_SHA224:
        result = SHA224_Update(&sha->ctx256, buf, size);
        break;
    case MZ_HASH_SHA256:
        result = SHA256_Update(&sha->ctx256, buf, size);
        break;
    case MZ_HASH_SHA384:
        result = SHA384_Update(&sha->ctx512, buf, size);
        break;
    case MZ_HASH_SHA512:
        result = SHA512_Update(&sha->ctx512, buf, size);
        break;
    }
#else
    result = EVP_DigestUpdate(sha->ctx, buf, size);
#endif

    if (!result) {
        sha->error = ERR_get_error();
        return MZ_HASH_ERROR;
    }

    return size;
}

int32_t mz_crypt_sha_end(void *handle, uint8_t *digest, int32_t digest_size) {
    mz_crypt_sha *sha = (mz_crypt_sha *)handle;
    int32_t result = 0;

    if (!sha || !digest || !sha->initialized)
        return MZ_PARAM_ERROR;
    if (digest_size < mz_crypt_sha_digest_size[sha->algorithm - MZ_HASH_SHA1])
        return MZ_PARAM_ERROR;

#if OPENSSL_VERSION_NUMBER < 0x10100000L
    switch (sha->algorithm) {
    case MZ_HASH_SHA1:
        result = SHA1_Final(digest, &sha->ctx1);
        break;
    case MZ_HASH_SHA224:
        result = SHA224_Final(digest, &sha->ctx256);
        break;
    case MZ_HASH_SHA256:
        result = SHA256_Final(digest, &sha->ctx256);
        break;
    case MZ_HASH_SHA384:
        result = SHA384_Final(digest, &sha->ctx512);
        break;
    case MZ_HASH_SHA512:
        result = SHA512_Final(digest, &sha->ctx512);
        break;
    }
#else
    result = EVP_DigestFinal_ex(sha->ctx, digest, NULL);
#endif

    if (!result) {
        sha->error = ERR_get_error();
        return MZ_HASH_ERROR;
    }

    return MZ_OK;
}

int32_t mz_crypt_sha_set_algorithm(void *handle, uint16_t algorithm) {
    mz_crypt_sha *sha = (mz_crypt_sha *)handle;
    if (algorithm < MZ_HASH_SHA1 || algorithm > MZ_HASH_SHA512)
        return MZ_PARAM_ERROR;
    sha->algorithm = algorithm;
    return MZ_OK;
}

void *mz_crypt_sha_create(void) {
    mz_crypt_sha *sha = (mz_crypt_sha *)calloc(1, sizeof(mz_crypt_sha));
    if (sha)
        sha->algorithm = MZ_HASH_SHA256;
    return sha;
}

void mz_crypt_sha_delete(void **handle) {
    mz_crypt_sha *sha = NULL;
    if (!handle)
        return;
    sha = (mz_crypt_sha *)*handle;
    if (sha) {
        mz_crypt_sha_free(*handle);
        free(sha);
    }
    *handle = NULL;
}

/***************************************************************************/

typedef struct mz_crypt_aes_s {
    int32_t    mode;
    int32_t    error;
    EVP_CIPHER_CTX *ctx;
} mz_crypt_aes;

/***************************************************************************/

static void mz_crypt_aes_free(void *handle) {
    mz_crypt_aes *aes = (mz_crypt_aes *)handle;
    if (aes->ctx)
        EVP_CIPHER_CTX_free(aes->ctx);
    aes->ctx = NULL;
}

void mz_crypt_aes_reset(void *handle) {
    mz_crypt_init();
    mz_crypt_aes_free(handle);
}

int32_t mz_crypt_aes_encrypt(void *handle, const void *aad, int32_t aad_size, uint8_t *buf, int32_t size) {
    mz_crypt_aes *aes = (mz_crypt_aes *)handle;

    if (!aes || !buf || size % MZ_AES_BLOCK_SIZE != 0 || !aes->ctx)
        return MZ_PARAM_ERROR;
    if (aes->mode != MZ_AES_MODE_GCM && aad && aad_size > 0)
        return MZ_PARAM_ERROR;

    if (aad && aad_size > 0) {
        int32_t how_many = 0;
        if (!EVP_EncryptUpdate(aes->ctx, NULL, &how_many, aad, aad_size))
            return MZ_CRYPT_ERROR;
    }

    if (!EVP_EncryptUpdate(aes->ctx, buf, &size, buf, size))
        return MZ_CRYPT_ERROR;

    return size;
}

int32_t mz_crypt_aes_encrypt_final(void *handle, uint8_t *buf, int32_t size, uint8_t *tag, int32_t tag_size) {
    mz_crypt_aes *aes = (mz_crypt_aes *)handle;
    int result = 0;
    int out_len = 0;

    if (!aes || !tag || !tag_size || !aes->ctx || aes->mode != MZ_AES_MODE_GCM)
        return MZ_PARAM_ERROR;

    if (buf && size) {
        if (!EVP_EncryptUpdate(aes->ctx, buf, &size, buf, size))
            return MZ_CRYPT_ERROR;
    }

    /* Must call EncryptFinal for tag to be calculated */
    result = EVP_EncryptFinal_ex(aes->ctx, NULL, &out_len);

    if (result)
        result = EVP_CIPHER_CTX_ctrl(aes->ctx, EVP_CTRL_GCM_GET_TAG, tag_size, tag);

    if (!result) {
        aes->error = ERR_get_error();
        return MZ_CRYPT_ERROR;
    }

    return size;
}

int32_t mz_crypt_aes_decrypt(void *handle, const void *aad, int32_t aad_size, uint8_t *buf, int32_t size) {
    mz_crypt_aes *aes = (mz_crypt_aes *)handle;

    if (!aes || !buf || size % MZ_AES_BLOCK_SIZE != 0 || !aes->ctx)
        return MZ_PARAM_ERROR;
    if (aes->mode != MZ_AES_MODE_GCM && aad && aad_size > 0)
        return MZ_PARAM_ERROR;

    if (aad && aad_size > 0) {
        int32_t how_many = 0;
        if (!EVP_DecryptUpdate(aes->ctx, NULL, &how_many, aad, aad_size))
            return MZ_CRYPT_ERROR;
    }

    if (!EVP_DecryptUpdate(aes->ctx, buf, &size, buf, size))
        return MZ_CRYPT_ERROR;

    return size;
}

int32_t mz_crypt_aes_decrypt_final(void *handle, uint8_t *buf, int32_t size, const uint8_t *tag, int32_t tag_length) {
    mz_crypt_aes *aes = (mz_crypt_aes *)handle;
    int out_len = 0;

    if (!aes || !tag || !tag_length || !aes->ctx || aes->mode != MZ_AES_MODE_GCM)
        return MZ_PARAM_ERROR;

    if (buf && size) {
        if (!EVP_DecryptUpdate(aes->ctx, buf, &size, buf, size))
            return MZ_CRYPT_ERROR;
    }

    /* Set expected tag */
    if (!EVP_CIPHER_CTX_ctrl(aes->ctx, EVP_CTRL_GCM_SET_TAG, tag_length, (void *)tag)) {
        aes->error = ERR_get_error();
        return MZ_CRYPT_ERROR;
    }

    /* Must call DecryptFinal for tag verification */
    if (!EVP_DecryptFinal_ex(aes->ctx, NULL, &out_len)) {
        aes->error = ERR_get_error();
        return MZ_CRYPT_ERROR;
    }

    return size;
}

static int32_t mz_crypt_aes_set_key(void *handle, const void *key, int32_t key_length,
    const void *iv, int32_t iv_length, int32_t encrypt) {
    mz_crypt_aes *aes = (mz_crypt_aes *)handle;
    const EVP_CIPHER *type = NULL;

    switch (aes->mode) {
    case MZ_AES_MODE_CBC:
        if (key_length == 16)
            type = EVP_aes_128_cbc();
        else if (key_length == 24)
            type = EVP_aes_192_cbc();
        else if (key_length == 32)
            type = EVP_aes_256_cbc();
        break;
    case MZ_AES_MODE_ECB:
        if (key_length == 16)
            type = EVP_aes_128_ecb();
        else if (key_length == 24)
            type = EVP_aes_192_ecb();
        else if (key_length == 32)
            type = EVP_aes_256_ecb();
        break;
    case MZ_AES_MODE_GCM:
        if (key_length == 16)
            type = EVP_aes_128_gcm();
        else if (key_length == 24)
            type = EVP_aes_192_gcm();
        else if (key_length == 32)
            type = EVP_aes_256_gcm();
        break;
    }
    if (!type)
        return MZ_PARAM_ERROR;

    aes->ctx = EVP_CIPHER_CTX_new();
    if (!aes->ctx)
        return MZ_MEM_ERROR;

    if (!EVP_CipherInit_ex(aes->ctx, type, NULL, key, iv, encrypt)) {
        aes->error = ERR_get_error();
        return MZ_HASH_ERROR;
    }

    EVP_CIPHER_CTX_set_padding(aes->ctx, aes->mode == MZ_AES_MODE_GCM);

    return MZ_OK;
}

int32_t mz_crypt_aes_set_encrypt_key(void *handle, const void *key, int32_t key_length,
    const void *iv, int32_t iv_length) {
    mz_crypt_aes *aes = (mz_crypt_aes *)handle;

    if (!aes || !key || !key_length)
        return MZ_PARAM_ERROR;
    if (key_length != 16 && key_length != 24 && key_length != 32)
        return MZ_PARAM_ERROR;
    if (iv && iv_length != MZ_AES_BLOCK_SIZE)
        return MZ_PARAM_ERROR;

    mz_crypt_aes_reset(handle);

    return mz_crypt_aes_set_key(handle, key, key_length, iv, iv_length, 1);
}

int32_t mz_crypt_aes_set_decrypt_key(void *handle, const void *key, int32_t key_length,
    const void *iv, int32_t iv_length) {
    mz_crypt_aes *aes = (mz_crypt_aes *)handle;

    if (!aes || !key || !key_length)
        return MZ_PARAM_ERROR;
    if (key_length != 16 && key_length != 24 && key_length != 32)
        return MZ_PARAM_ERROR;
    if (iv && iv_length > MZ_AES_BLOCK_SIZE)
        return MZ_PARAM_ERROR;

    mz_crypt_aes_reset(handle);

    return mz_crypt_aes_set_key(handle, key, key_length, iv, iv_length, 0);
}

void mz_crypt_aes_set_mode(void *handle, int32_t mode) {
    mz_crypt_aes *aes = (mz_crypt_aes *)handle;
    aes->mode = mode;
}

void *mz_crypt_aes_create(void) {
    mz_crypt_aes *aes = (mz_crypt_aes *)calloc(1, sizeof(mz_crypt_aes));
    return aes;
}

void mz_crypt_aes_delete(void **handle) {
    mz_crypt_aes *aes = NULL;
    if (!handle)
        return;
    aes = (mz_crypt_aes *)*handle;
    if (aes) {
        mz_crypt_aes_free(*handle);
        free(aes);
    }
    *handle = NULL;
}

/***************************************************************************/

typedef struct mz_crypt_hmac_s {
#if OPENSSL_VERSION_NUMBER < 0x30000000L
    HMAC_CTX    *ctx;
#else
    EVP_MAC     *mac;
    EVP_MAC_CTX *ctx;
#endif
    int32_t     initialized;
    int32_t     error;
    uint16_t    algorithm;
} mz_crypt_hmac;

/***************************************************************************/

#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || \
    (defined(LIBRESSL_VERSION_NUMBER) && (LIBRESSL_VERSION_NUMBER < 0x2070000fL))
static HMAC_CTX *HMAC_CTX_new(void) {
    HMAC_CTX *ctx = OPENSSL_malloc(sizeof(HMAC_CTX));
    if (ctx)
        HMAC_CTX_init(ctx);
    return ctx;
}

static void HMAC_CTX_free(HMAC_CTX *ctx) {
    if (ctx) {
        HMAC_CTX_cleanup(ctx);
        OPENSSL_free(ctx);
    }
}
#endif

/***************************************************************************/

static void mz_crypt_hmac_free(void *handle) {
    mz_crypt_hmac *hmac = (mz_crypt_hmac *)handle;

#if OPENSSL_VERSION_NUMBER < 0x30000000L
    HMAC_CTX_free(hmac->ctx);
#else
    if (hmac->ctx)
        EVP_MAC_CTX_free(hmac->ctx);
    if (hmac->mac)
        EVP_MAC_free(hmac->mac);
    hmac->mac = NULL;
#endif

    hmac->ctx = NULL;
}

void mz_crypt_hmac_reset(void *handle) {
    mz_crypt_hmac *hmac = (mz_crypt_hmac *)handle;

    mz_crypt_init();
    mz_crypt_hmac_free(handle);

    hmac->error = 0;
}

int32_t mz_crypt_hmac_init(void *handle, const void *key, int32_t key_length) {
    mz_crypt_hmac *hmac = (mz_crypt_hmac *)handle;
    int32_t result = 0;

    if (!hmac || !key)
        return MZ_PARAM_ERROR;

    mz_crypt_hmac_reset(handle);

#if OPENSSL_VERSION_NUMBER < 0x30000000L
    const EVP_MD *evp_md = NULL;

    if (hmac->algorithm == MZ_HASH_SHA1)
        evp_md = EVP_sha1();
    else
        evp_md = EVP_sha256();

    hmac->ctx = HMAC_CTX_new();
    if (!hmac->ctx)
        return MZ_MEM_ERROR;

    result = HMAC_Init_ex(hmac->ctx, key, key_length, evp_md, NULL);
#else
    char *digest_algorithm = NULL;
    OSSL_PARAM params[2];

    if (hmac->algorithm == MZ_HASH_SHA1)
        digest_algorithm = "sha1";
    else
        digest_algorithm = "sha256";

    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_DIGEST, digest_algorithm, 0);
    params[1] = OSSL_PARAM_construct_end();

    hmac->mac = EVP_MAC_fetch(NULL, "HMAC", NULL);
    if (!hmac->mac)
        return MZ_MEM_ERROR;
    hmac->ctx = EVP_MAC_CTX_new(hmac->mac);
    if (!hmac->ctx)
        return MZ_MEM_ERROR;
    result = EVP_MAC_init(hmac->ctx, key, key_length, params);
#endif

    if (!result) {
        hmac->error = ERR_get_error();
        return MZ_HASH_ERROR;
    }

    return MZ_OK;
}

int32_t mz_crypt_hmac_update(void *handle, const void *buf, int32_t size) {
    mz_crypt_hmac *hmac = (mz_crypt_hmac *)handle;
    int32_t result = 0;

    if (!hmac || !buf)
        return MZ_PARAM_ERROR;

#if OPENSSL_VERSION_NUMBER < 0x30000000L
    result = HMAC_Update(hmac->ctx, buf, size);
#else
    result = EVP_MAC_update(hmac->ctx, buf, size);
#endif
    if (!result) {
        hmac->error = ERR_get_error();
        return MZ_HASH_ERROR;
    }

    return MZ_OK;
}

int32_t mz_crypt_hmac_end(void *handle, uint8_t *digest, int32_t digest_size) {
    mz_crypt_hmac *hmac = (mz_crypt_hmac *)handle;
    int32_t result = 0;

    if (!hmac || !digest)
        return MZ_PARAM_ERROR;

#if OPENSSL_VERSION_NUMBER < 0x30000000L
    if (hmac->algorithm == MZ_HASH_SHA1) {
        if (digest_size < MZ_HASH_SHA1_SIZE)
            return MZ_BUF_ERROR;

        result = HMAC_Final(hmac->ctx, digest, (uint32_t *)&digest_size);
    } else {
        if (digest_size < MZ_HASH_SHA256_SIZE)
            return MZ_BUF_ERROR;
        result = HMAC_Final(hmac->ctx, digest, (uint32_t *)&digest_size);
    }
#else
    {
        size_t digest_outsize = digest_size;
        result = EVP_MAC_final(hmac->ctx, digest, &digest_outsize, digest_size);
    }
#endif

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

    if (!source || !target)
        return MZ_PARAM_ERROR;

    mz_crypt_hmac_reset(target_handle);

#if OPENSSL_VERSION_NUMBER < 0x30000000L
    if (!target->ctx)
        target->ctx = HMAC_CTX_new();

    if (!HMAC_CTX_copy(target->ctx, source->ctx)) {
        target->error = ERR_get_error();
        return MZ_HASH_ERROR;
    }
#else
    if (!target->ctx)
        target->ctx = EVP_MAC_CTX_dup(source->ctx);
    if (!target->ctx)
        return MZ_MEM_ERROR;
#endif

    return MZ_OK;
}

void *mz_crypt_hmac_create(void) {
    mz_crypt_hmac *hmac = (mz_crypt_hmac *)calloc(1, sizeof(mz_crypt_hmac));
    if (hmac)
        hmac->algorithm = MZ_HASH_SHA256;
    return hmac;
}

void mz_crypt_hmac_delete(void **handle) {
    mz_crypt_hmac *hmac = NULL;
    if (!handle)
        return;
    hmac = (mz_crypt_hmac *)*handle;
    if (hmac) {
        mz_crypt_hmac_free(*handle);
        free(hmac);
    }
    *handle = NULL;
}
