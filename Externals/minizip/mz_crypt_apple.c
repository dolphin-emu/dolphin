/* mz_crypt_apple.c -- Crypto/hash functions for Apple
   part of the minizip-ng project

   Copyright (C) Nathan Moinvaziri
     https://github.com/zlib-ng/minizip-ng

   This program is distributed under the terms of the same license as zlib.
   See the accompanying LICENSE file for the full text of the license.
*/

#include "mz.h"
#include "mz_crypt.h"

#include <CoreFoundation/CoreFoundation.h>
#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonHMAC.h>
#include <Security/Security.h>
#include <Security/SecPolicy.h>

/***************************************************************************/

/* Avoid use of private API for iOS, Apple does not allow it on App Store. Zip format doesn't need GCM. */
#if !TARGET_OS_IPHONE
enum {
    kCCModeGCM = 11,
};

CCCryptorStatus CCCryptorGCMReset(CCCryptorRef cryptorRef);
CCCryptorStatus CCCryptorGCMAddIV(CCCryptorRef cryptorRef, const void *iv, size_t ivLen);
CCCryptorStatus CCCryptorGCMAddAAD(CCCryptorRef cryptorRef, const void *aData, size_t aDataLen);
CCCryptorStatus CCCryptorGCMEncrypt(CCCryptorRef cryptorRef, const void *dataIn, size_t dataInLength, void *dataOut);
CCCryptorStatus CCCryptorGCMDecrypt(CCCryptorRef cryptorRef, const void *dataIn, size_t dataInLength, void *dataOut);
CCCryptorStatus CCCryptorGCMFinal(CCCryptorRef cryptorRef, void *tagOut, size_t *tagLength);
#endif

/***************************************************************************/

int32_t mz_crypt_rand(uint8_t *buf, int32_t size) {
    if (SecRandomCopyBytes(kSecRandomDefault, size, buf) != errSecSuccess)
        return 0;
    return size;
}

/***************************************************************************/

typedef struct mz_crypt_sha_s {
    union {
        CC_SHA1_CTX   ctx1;
        CC_SHA256_CTX ctx256;
        CC_SHA512_CTX ctx512;
    };
    int32_t           error;
    int32_t           initialized;
    uint16_t          algorithm;
} mz_crypt_sha;

/***************************************************************************/

static const uint8_t mz_crypt_sha_digest_size[] = {
    MZ_HASH_SHA1_SIZE,                     0, MZ_HASH_SHA224_SIZE,
    MZ_HASH_SHA256_SIZE, MZ_HASH_SHA384_SIZE, MZ_HASH_SHA512_SIZE
};

/***************************************************************************/

void mz_crypt_sha_reset(void *handle) {
    mz_crypt_sha *sha = (mz_crypt_sha *)handle;

    sha->error = 0;
    sha->initialized = 0;
}

int32_t mz_crypt_sha_begin(void *handle) {
    mz_crypt_sha *sha = (mz_crypt_sha *)handle;

    if (!sha)
        return MZ_PARAM_ERROR;

    mz_crypt_sha_reset(handle);

    switch (sha->algorithm) {
    case MZ_HASH_SHA1:
        sha->error = CC_SHA1_Init(&sha->ctx1);
        break;
    case MZ_HASH_SHA224:
        sha->error = CC_SHA224_Init(&sha->ctx256);
        break;
    case MZ_HASH_SHA256:
        sha->error = CC_SHA256_Init(&sha->ctx256);
        break;
    case MZ_HASH_SHA384:
        sha->error = CC_SHA384_Init(&sha->ctx512);
        break;
    case MZ_HASH_SHA512:
        sha->error = CC_SHA512_Init(&sha->ctx512);
        break;
    default:
        return MZ_PARAM_ERROR;
    }

    if (!sha->error)
        return MZ_HASH_ERROR;

    sha->initialized = 1;
    return MZ_OK;
}

int32_t mz_crypt_sha_update(void *handle, const void *buf, int32_t size) {
    mz_crypt_sha *sha = (mz_crypt_sha *)handle;

    if (!sha || !buf || !sha->initialized)
        return MZ_PARAM_ERROR;

    switch (sha->algorithm) {
    case MZ_HASH_SHA1:
        sha->error = CC_SHA1_Update(&sha->ctx1, buf, size);
        break;
    case MZ_HASH_SHA224:
        sha->error = CC_SHA224_Update(&sha->ctx256, buf, size);
        break;
    case MZ_HASH_SHA256:
        sha->error = CC_SHA256_Update(&sha->ctx256, buf, size);
        break;
    case MZ_HASH_SHA384:
        sha->error = CC_SHA384_Update(&sha->ctx512, buf, size);
        break;
    case MZ_HASH_SHA512:
        sha->error = CC_SHA512_Update(&sha->ctx512, buf, size);
        break;
    }

    if (!sha->error)
        return MZ_HASH_ERROR;

    return size;
}

int32_t mz_crypt_sha_end(void *handle, uint8_t *digest, int32_t digest_size) {
    mz_crypt_sha *sha = (mz_crypt_sha *)handle;

    if (!sha || !digest || !sha->initialized)
        return MZ_PARAM_ERROR;
    if (digest_size < mz_crypt_sha_digest_size[sha->algorithm - MZ_HASH_SHA1])
        return MZ_PARAM_ERROR;

    switch (sha->algorithm) {
    case MZ_HASH_SHA1:
        sha->error = CC_SHA1_Final(digest, &sha->ctx1);
        break;
    case MZ_HASH_SHA224:
        sha->error = CC_SHA224_Final(digest, &sha->ctx256);
        break;
    case MZ_HASH_SHA256:
        sha->error = CC_SHA256_Final(digest, &sha->ctx256);
        break;
    case MZ_HASH_SHA384:
        sha->error = CC_SHA384_Final(digest, &sha->ctx512);
        break;
    case MZ_HASH_SHA512:
        sha->error = CC_SHA512_Final(digest, &sha->ctx512);
        break;
    }

    if (!sha->error)
        return MZ_HASH_ERROR;

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
    if (sha) {
        memset(sha, 0, sizeof(mz_crypt_sha));
        sha->algorithm = MZ_HASH_SHA256;
    }
    return sha;
}

void mz_crypt_sha_delete(void **handle) {
    mz_crypt_sha *sha = NULL;
    if (!handle)
        return;
    sha = (mz_crypt_sha *)*handle;
    if (sha) {
        mz_crypt_sha_reset(*handle);
        free(sha);
    }
    *handle = NULL;
}

/***************************************************************************/

typedef struct mz_crypt_aes_s {
    CCCryptorRef crypt;
    int32_t      mode;
    int32_t      error;
} mz_crypt_aes;

/***************************************************************************/

static void mz_crypt_aes_free(void *handle) {
    mz_crypt_aes *aes = (mz_crypt_aes *)handle;

    if (aes->crypt)
        CCCryptorRelease(aes->crypt);
    aes->crypt = NULL;
}

void mz_crypt_aes_reset(void *handle) {
    mz_crypt_aes_free(handle);
}

int32_t mz_crypt_aes_encrypt(void *handle, const void *aad, int32_t aad_size, uint8_t *buf, int32_t size) {
    mz_crypt_aes *aes = (mz_crypt_aes *)handle;
    size_t data_moved = 0;

    if (!aes || !buf || size % MZ_AES_BLOCK_SIZE != 0 || !aes->crypt)
        return MZ_PARAM_ERROR;

    if (aes->mode == MZ_AES_MODE_GCM) {
#if TARGET_OS_IPHONE
        return MZ_SUPPORT_ERROR;
#else
        if (aad && aad_size > 0) {
            aes->error = CCCryptorGCMAddAAD(aes->crypt, aad, aad_size);
            if (aes->error != kCCSuccess)
                return MZ_CRYPT_ERROR;
        }
        aes->error = CCCryptorGCMEncrypt(aes->crypt, buf, size, buf);
#endif
    } else {
        if (aad && aad_size > 0)
            return MZ_PARAM_ERROR;
        aes->error = CCCryptorUpdate(aes->crypt, buf, size, buf, size, &data_moved);
    }

    if (aes->error != kCCSuccess)
        return MZ_CRYPT_ERROR;

    return size;
}

int32_t mz_crypt_aes_encrypt_final(void *handle, uint8_t *buf, int32_t size, uint8_t *tag, int32_t tag_size) {
    mz_crypt_aes *aes = (mz_crypt_aes *)handle;
#if !TARGET_OS_IPHONE
    size_t tag_outsize = tag_size;
#endif

    if (!aes || !tag || !tag_size || !aes->crypt || aes->mode != MZ_AES_MODE_GCM)
        return MZ_PARAM_ERROR;

#if TARGET_OS_IPHONE
    return MZ_SUPPORT_ERROR;
#else
    aes->error = CCCryptorGCMEncrypt(aes->crypt, buf, size, buf);
    if (aes->error != kCCSuccess)
        return MZ_CRYPT_ERROR;

    aes->error = CCCryptorGCMFinal(aes->crypt, tag, &tag_outsize);

    if (aes->error != kCCSuccess)
        return MZ_CRYPT_ERROR;

    return size;
#endif
}

int32_t mz_crypt_aes_decrypt(void *handle, const void *aad, int32_t aad_size, uint8_t *buf, int32_t size) {
    mz_crypt_aes *aes = (mz_crypt_aes *)handle;
    size_t data_moved = 0;

    if (!aes || !buf || size % MZ_AES_BLOCK_SIZE != 0 || !aes->crypt)
        return MZ_PARAM_ERROR;

    if (aes->mode == MZ_AES_MODE_GCM) {
#if TARGET_OS_IPHONE
        return MZ_SUPPORT_ERROR;
#else
        if (aad && aad_size > 0) {
            aes->error = CCCryptorGCMAddAAD(aes->crypt, aad, aad_size);
            if (aes->error != kCCSuccess)
                return MZ_CRYPT_ERROR;
        }
        aes->error = CCCryptorGCMDecrypt(aes->crypt, buf, size, buf);
#endif
    } else {
        if (aad && aad_size > 0)
            return MZ_PARAM_ERROR;
        aes->error = CCCryptorUpdate(aes->crypt, buf, size, buf, size, &data_moved);
    }

    if (aes->error != kCCSuccess)
        return MZ_CRYPT_ERROR;

    return size;
}

int32_t mz_crypt_aes_decrypt_final(void *handle, uint8_t *buf, int32_t size, const uint8_t *tag, int32_t tag_length) {
    mz_crypt_aes *aes = (mz_crypt_aes *)handle;
#if !TARGET_OS_IPHONE
    uint8_t tag_actual_buf[MZ_AES_BLOCK_SIZE];
    size_t tag_actual_len = sizeof(tag_actual_buf);
    uint8_t *tag_actual = tag_actual_buf;
    int32_t c = tag_length;
    int32_t is_ok = 0;
#endif

    if (!aes || !tag || !tag_length || !aes->crypt || aes->mode != MZ_AES_MODE_GCM)
        return MZ_PARAM_ERROR;

#if TARGET_OS_IPHONE
    return MZ_SUPPORT_ERROR;
#else
    aes->error = CCCryptorGCMDecrypt(aes->crypt, buf, size, buf);
    if (aes->error != kCCSuccess)
        return MZ_CRYPT_ERROR;

    /* CCCryptorGCMFinal does not verify tag */
    aes->error = CCCryptorGCMFinal(aes->crypt, tag_actual, &tag_actual_len);

    if (aes->error != kCCSuccess)
        return MZ_CRYPT_ERROR;
    if (tag_length != (int32_t)tag_actual_len)
        return MZ_CRYPT_ERROR;

    /* Timing safe comparison */
    for (; c > 0; c--)
        is_ok |= *tag++ ^ *tag_actual++;

    if (is_ok)
        return MZ_CRYPT_ERROR;

    return size;
#endif
}

static int32_t mz_crypt_aes_set_key(void *handle, const void *key, int32_t key_length,
    const void *iv, int32_t iv_length, CCOperation op) {
    mz_crypt_aes *aes = (mz_crypt_aes *)handle;
    CCMode mode;

    if (aes->mode == MZ_AES_MODE_CBC)
        mode = kCCModeCBC;
    else if (aes->mode == MZ_AES_MODE_ECB)
        mode = kCCModeECB;
    else if (aes->mode == MZ_AES_MODE_GCM)
#if !TARGET_OS_IPHONE
        mode = kCCModeGCM;
#else
        return MZ_SUPPORT_ERROR;
#endif
    else
        return MZ_PARAM_ERROR;

    mz_crypt_aes_reset(handle);

    aes->error = CCCryptorCreateWithMode(op, mode, kCCAlgorithmAES, ccNoPadding, iv, key, key_length,
        NULL, 0, 0, 0, &aes->crypt);

    if (aes->error != kCCSuccess)
        return MZ_HASH_ERROR;

#if !TARGET_OS_IPHONE
    if (aes->mode == MZ_AES_MODE_GCM) {
        aes->error = CCCryptorGCMAddIV(aes->crypt, iv, iv_length);

        if (aes->error != kCCSuccess)
            return MZ_HASH_ERROR;
    }
#endif

    return MZ_OK;
}

int32_t mz_crypt_aes_set_encrypt_key(void *handle, const void *key, int32_t key_length,
    const void *iv, int32_t iv_length) {
    return mz_crypt_aes_set_key(handle, key, key_length, iv, iv_length, kCCEncrypt);
}

int32_t mz_crypt_aes_set_decrypt_key(void *handle, const void *key, int32_t key_length,
    const void *iv, int32_t iv_length) {
    return mz_crypt_aes_set_key(handle, key, key_length, iv, iv_length, kCCDecrypt);
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
    CCHmacContext   ctx;
    int32_t         initialized;
    int32_t         error;
    uint16_t        algorithm;
} mz_crypt_hmac;

/***************************************************************************/

static void mz_crypt_hmac_free(void *handle) {
    mz_crypt_hmac *hmac = (mz_crypt_hmac *)handle;
    memset(&hmac->ctx, 0, sizeof(hmac->ctx));
}

void mz_crypt_hmac_reset(void *handle) {
    mz_crypt_hmac *hmac = (mz_crypt_hmac *)handle;
    mz_crypt_hmac_free(handle);
    hmac->error = 0;
}

int32_t mz_crypt_hmac_init(void *handle, const void *key, int32_t key_length) {
    mz_crypt_hmac *hmac = (mz_crypt_hmac *)handle;
    CCHmacAlgorithm algorithm = 0;

    if (!hmac || !key)
        return MZ_PARAM_ERROR;

    mz_crypt_hmac_reset(handle);

    if (hmac->algorithm == MZ_HASH_SHA1)
        algorithm = kCCHmacAlgSHA1;
    else if (hmac->algorithm == MZ_HASH_SHA256)
        algorithm = kCCHmacAlgSHA256;
    else
        return MZ_PARAM_ERROR;

    CCHmacInit(&hmac->ctx, algorithm, key, key_length);
    return MZ_OK;
}

int32_t mz_crypt_hmac_update(void *handle, const void *buf, int32_t size) {
    mz_crypt_hmac *hmac = (mz_crypt_hmac *)handle;

    if (!hmac || !buf)
        return MZ_PARAM_ERROR;

    CCHmacUpdate(&hmac->ctx, buf, size);
    return MZ_OK;
}

int32_t mz_crypt_hmac_end(void *handle, uint8_t *digest, int32_t digest_size) {
    mz_crypt_hmac *hmac = (mz_crypt_hmac *)handle;

    if (!hmac || !digest)
        return MZ_PARAM_ERROR;

    if (hmac->algorithm == MZ_HASH_SHA1) {
        if (digest_size < MZ_HASH_SHA1_SIZE)
            return MZ_BUF_ERROR;
        CCHmacFinal(&hmac->ctx, digest);
    } else {
        if (digest_size < MZ_HASH_SHA256_SIZE)
            return MZ_BUF_ERROR;
        CCHmacFinal(&hmac->ctx, digest);
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

    memcpy(&target->ctx, &source->ctx, sizeof(CCHmacContext));
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
