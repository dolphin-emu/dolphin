/* mz_crypt_win32.c -- Crypto/hash functions for Windows XP
   part of the minizip-ng project

   Copyright (C) Nathan Moinvaziri
     https://github.com/zlib-ng/minizip-ng

   This program is distributed under the terms of the same license as zlib.
   See the accompanying LICENSE file for the full text of the license.
*/

#include "mz.h"
#include "mz_os.h"
#include "mz_crypt.h"

#include <windows.h>

#if _WIN32_WINNT <= _WIN32_WINNT_WINXP
#include <wincrypt.h>

/***************************************************************************/

int32_t mz_crypt_rand(uint8_t *buf, int32_t size) {
    HCRYPTPROV provider;
    int32_t result = 0;

    result = CryptAcquireContext(&provider, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT);
    if (result) {
        result = CryptGenRandom(provider, size, buf);
        CryptReleaseContext(provider, 0);
        if (result)
            return size;
    }

    return mz_os_rand(buf, size);
}

/***************************************************************************/

typedef struct mz_crypt_sha_s {
    union {
        struct {
            HCRYPTPROV  provider;
            HCRYPTHASH  hash;
        };
    };
    int32_t             error;
    uint16_t            algorithm;
} mz_crypt_sha;

/***************************************************************************/

static void mz_crypt_sha_free(void *handle) {
    mz_crypt_sha *sha = (mz_crypt_sha *)handle;
    if (sha->hash)
        CryptDestroyHash(sha->hash);
    sha->hash = 0;
    if (sha->provider)
        CryptReleaseContext(sha->provider, 0);
    sha->provider = 0;
}

void mz_crypt_sha_reset(void *handle) {
    mz_crypt_sha *sha = (mz_crypt_sha *)handle;
    mz_crypt_sha_free(handle);
    sha->error = 0;
}

int32_t mz_crypt_sha_begin(void *handle) {
    mz_crypt_sha *sha = (mz_crypt_sha *)handle;
    ALG_ID alg_id = 0;
    int32_t result = 0;
    int32_t err = MZ_OK;

    if (!sha)
        return MZ_PARAM_ERROR;

    if (sha->algorithm == MZ_HASH_SHA224)
        return MZ_SUPPORT_ERROR;

    switch (sha->algorithm) {
    case MZ_HASH_SHA1:
        alg_id = CALG_SHA1;
        break;
#if NTDDI_VERSION > NTDDI_WINXPSP2
    case MZ_HASH_SHA256:
        alg_id = CALG_SHA_256;
        break;
    case MZ_HASH_SHA384:
        alg_id = CALG_SHA_384;
        break;
    case MZ_HASH_SHA512:
        alg_id = CALG_SHA_512;
        break;
#else
    case MZ_HASH_SHA256:
    case MZ_HASH_SHA384:
    case MZ_HASH_SHA512:
        return MZ_SUPPORT_ERROR;
#endif
    default:
        return MZ_PARAM_ERROR;
    }

    result = CryptAcquireContext(&sha->provider, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT | CRYPT_SILENT);
    if (!result) {
        sha->error = GetLastError();
        err = MZ_CRYPT_ERROR;
    }

    if (result) {
        result = CryptCreateHash(sha->provider, alg_id, 0, 0, &sha->hash);
        if (!result) {
            sha->error = GetLastError();
            err = MZ_HASH_ERROR;
        }
    }

    return err;
}

int32_t mz_crypt_sha_update(void *handle, const void *buf, int32_t size) {
    mz_crypt_sha *sha = (mz_crypt_sha *)handle;
    int32_t result = 0;

    if (!sha || !buf || size < 0)
        return MZ_PARAM_ERROR;

    if (sha->algorithm == MZ_HASH_SHA224)
        return MZ_SUPPORT_ERROR;

    if (sha->hash == 0)
        return MZ_PARAM_ERROR;

    result = CryptHashData(sha->hash, buf, size, 0);
    if (!result) {
        sha->error = GetLastError();
        return MZ_HASH_ERROR;
    }
    return size;
}

int32_t mz_crypt_sha_end(void *handle, uint8_t *digest, int32_t digest_size) {
    mz_crypt_sha *sha = (mz_crypt_sha *)handle;
    int32_t result = 0;
    int32_t expected_size = 0;

    if (!sha || !digest)
        return MZ_PARAM_ERROR;

    if (sha->algorithm == MZ_HASH_SHA224)
        return MZ_SUPPORT_ERROR;

    if (sha->hash == 0)
        return MZ_PARAM_ERROR;

    result = CryptGetHashParam(sha->hash, HP_HASHVAL, NULL, (DWORD *)&expected_size, 0);
    if (expected_size > digest_size)
        return MZ_BUF_ERROR;
    if (!result)
        return MZ_HASH_ERROR;
    result = CryptGetHashParam(sha->hash, HP_HASHVAL, digest, (DWORD *)&digest_size, 0);
    if (!result) {
        sha->error = GetLastError();
        return MZ_HASH_ERROR;
    }
    return MZ_OK;
}

int32_t mz_crypt_sha_set_algorithm(void *handle, uint16_t algorithm) {
    mz_crypt_sha *sha = (mz_crypt_sha *)handle;
    if (algorithm != MZ_HASH_SHA1)
        return MZ_SUPPORT_ERROR;
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
    HCRYPTPROV provider;
    HCRYPTKEY  key;
    int32_t    mode;
    int32_t    error;
} mz_crypt_aes;

/***************************************************************************/

static void mz_crypt_aes_free(void *handle) {
    mz_crypt_aes *aes = (mz_crypt_aes *)handle;
    if (aes->key)
        CryptDestroyKey(aes->key);
    aes->key = 0;
    if (aes->provider)
        CryptReleaseContext(aes->provider, 0);
    aes->provider = 0;
}

void mz_crypt_aes_reset(void *handle) {
    mz_crypt_aes_free(handle);
}

int32_t mz_crypt_aes_encrypt(void *handle, const void *aad, int32_t aad_size, uint8_t *buf, int32_t size) {
    mz_crypt_aes *aes = (mz_crypt_aes *)handle;
    int32_t result = 0;

    if (!aes || !buf || size % MZ_AES_BLOCK_SIZE != 0 || (aad && aad_size > 0))
        return MZ_PARAM_ERROR;
    result = CryptEncrypt(aes->key, 0, 0, 0, buf, (DWORD *)&size, size);
    if (!result) {
        aes->error = GetLastError();
        return MZ_CRYPT_ERROR;
    }
    return size;
}

int32_t mz_crypt_aes_encrypt_final(void *handle, uint8_t *buf, int32_t size, uint8_t *tag, int32_t tag_size) {
    return MZ_SUPPORT_ERROR;
}

int32_t mz_crypt_aes_decrypt(void *handle, const void *aad, int32_t aad_size, uint8_t *buf, int32_t size) {
    mz_crypt_aes *aes = (mz_crypt_aes *)handle;
    int32_t result = 0;
    if (!aes || !buf || size % MZ_AES_BLOCK_SIZE != 0 || (aad && aad_size > 0))
        return MZ_PARAM_ERROR;
    result = CryptDecrypt(aes->key, 0, 0, 0, buf, (DWORD *)&size);
    if (!result) {
        aes->error = GetLastError();
        return MZ_CRYPT_ERROR;
    }
    return size;
}

int32_t mz_crypt_aes_decrypt_final(void *handle, uint8_t *buf, int32_t size, const uint8_t *tag, int32_t tag_length) {
    return MZ_SUPPORT_ERROR;
}

static int32_t mz_crypt_aes_set_key(void *handle, const void *key, int32_t key_length,
    const void *iv, int32_t iv_length) {
    mz_crypt_aes *aes = (mz_crypt_aes *)handle;
    HCRYPTHASH hash = 0;
    ALG_ID alg_id = 0;
    typedef struct key_blob_header_s {
        BLOBHEADER hdr;
        uint32_t   key_length;
    } key_blob_header_s;
    key_blob_header_s *key_blob_s = NULL;
    uint32_t mode;
    uint8_t *key_blob = NULL;
    int32_t key_blob_size = 0;
    int32_t result = 0;
    int32_t err = MZ_OK;

    if (!aes || !key || !key_length)
        return MZ_PARAM_ERROR;
    if (iv && iv_length < MZ_AES_BLOCK_SIZE)
        return MZ_PARAM_ERROR;

    mz_crypt_aes_reset(handle);

    if (aes->mode == MZ_AES_MODE_CBC)
        mode = CRYPT_MODE_CBC;
    else if (aes->mode == MZ_AES_MODE_ECB)
        mode = CRYPT_MODE_ECB;
    else if (aes->mode == MZ_AES_MODE_GCM)
        return MZ_SUPPORT_ERROR;
    else
        return MZ_PARAM_ERROR;

    if (key_length == 16)
        alg_id = CALG_AES_128;
    else if (key_length == 24)
        alg_id = CALG_AES_192;
    else if (key_length == 32)
        alg_id = CALG_AES_256;
    else
        return MZ_PARAM_ERROR;

    result = CryptAcquireContext(&aes->provider, NULL, MS_ENH_RSA_AES_PROV, PROV_RSA_AES,
        CRYPT_VERIFYCONTEXT | CRYPT_SILENT);
    if (result) {
        key_blob_size = sizeof(key_blob_header_s) + key_length;
        key_blob = (uint8_t *)malloc(key_blob_size);
        if (key_blob) {
            key_blob_s = (key_blob_header_s *)key_blob;
            key_blob_s->hdr.bType = PLAINTEXTKEYBLOB;
            key_blob_s->hdr.bVersion = CUR_BLOB_VERSION;
            key_blob_s->hdr.aiKeyAlg = alg_id;
            key_blob_s->hdr.reserved = 0;
            key_blob_s->key_length = key_length;

            memcpy(key_blob + sizeof(key_blob_header_s), key, key_length);

            result = CryptImportKey(aes->provider, key_blob, key_blob_size, 0, 0, &aes->key);

            SecureZeroMemory(key_blob, key_blob_size);
            free(key_blob);
        } else {
            err = MZ_MEM_ERROR;
        }
    }

    if (result && err == MZ_OK)
        result = CryptSetKeyParam(aes->key, KP_MODE, (const uint8_t *)&mode, 0);


    if (!result && err == MZ_OK) {
        aes->error = GetLastError();
        err = MZ_CRYPT_ERROR;
    }

    if (result && err == MZ_OK && iv) {
        if (aes->mode == MZ_AES_MODE_ECB)
            return MZ_PARAM_ERROR;

        result = CryptSetKeyParam(aes->key, KP_IV, iv, 0);

        if (!result) {
            aes->error = GetLastError();
            return MZ_CRYPT_ERROR;
        }
    }

    if (hash)
        CryptDestroyHash(hash);

    return err;
}

int32_t mz_crypt_aes_set_encrypt_key(void *handle, const void *key, int32_t key_length,
    const void *iv, int32_t iv_length) {
    return mz_crypt_aes_set_key(handle, key, key_length, iv, iv_length);
}

int32_t mz_crypt_aes_set_decrypt_key(void *handle, const void *key, int32_t key_length,
    const void *iv, int32_t iv_length) {
    return mz_crypt_aes_set_key(handle, key, key_length, iv, iv_length);
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
    HCRYPTPROV provider;
    HCRYPTHASH hash;
    HCRYPTKEY  key;
    HMAC_INFO  info;
    int32_t    mode;
    int32_t    error;
    uint16_t   algorithm;
} mz_crypt_hmac;

/***************************************************************************/

static void mz_crypt_hmac_free(void *handle) {
    mz_crypt_hmac *hmac = (mz_crypt_hmac *)handle;
    if (hmac->key)
        CryptDestroyKey(hmac->key);
    hmac->key = 0;
    if (hmac->hash)
        CryptDestroyHash(hmac->hash);
    hmac->hash = 0;
    if (hmac->provider)
        CryptReleaseContext(hmac->provider, 0);
    hmac->provider = 0;
    memset(&hmac->info, 0, sizeof(hmac->info));
}

void mz_crypt_hmac_reset(void *handle) {
    mz_crypt_hmac_free(handle);
}

int32_t mz_crypt_hmac_init(void *handle, const void *key, int32_t key_length) {
    mz_crypt_hmac *hmac = (mz_crypt_hmac *)handle;
    ALG_ID alg_id = 0;
    typedef struct key_blob_header_s {
        BLOBHEADER hdr;
        uint32_t   key_length;
    } key_blob_header_s;
    key_blob_header_s *key_blob_s = NULL;
    uint8_t *key_blob = NULL;
    int32_t key_blob_size = 0;
    int32_t pad_key_length = key_length;
    int32_t result = 0;
    int32_t err = MZ_OK;

    if (!hmac || !key)
        return MZ_PARAM_ERROR;

    mz_crypt_hmac_reset(handle);

    if (hmac->algorithm == MZ_HASH_SHA1)
        alg_id = CALG_SHA1;
    else
#ifdef CALG_SHA_256
        alg_id = CALG_SHA_256;
#else
        return MZ_SUPPORT_ERROR;
#endif

    hmac->info.HashAlgid = alg_id;

    result = CryptAcquireContext(&hmac->provider, NULL, MS_ENHANCED_PROV, PROV_RSA_FULL,
        CRYPT_VERIFYCONTEXT | CRYPT_SILENT);

    if (!result) {
        hmac->error = GetLastError();
        err = MZ_CRYPT_ERROR;
    } else {
        /* Pad single char key to work around CryptImportKey returning ERROR_INVALID_PARAMETER */
        if (pad_key_length == 1)
            pad_key_length += 1;
        key_blob_size = sizeof(key_blob_header_s) + pad_key_length;
        key_blob = (uint8_t *)malloc(key_blob_size);
    }

    if (key_blob) {
        memset(key_blob, 0, key_blob_size);
        key_blob_s = (key_blob_header_s *)key_blob;
        key_blob_s->hdr.bType = PLAINTEXTKEYBLOB;
        key_blob_s->hdr.bVersion = CUR_BLOB_VERSION;
        key_blob_s->hdr.aiKeyAlg = CALG_RC2;
        key_blob_s->hdr.reserved = 0;
        key_blob_s->key_length = pad_key_length;

        memcpy(key_blob + sizeof(key_blob_header_s), key, key_length);

        result = CryptImportKey(hmac->provider, key_blob, key_blob_size, 0, CRYPT_IPSEC_HMAC_KEY, &hmac->key);
        if (result)
            result = CryptCreateHash(hmac->provider, CALG_HMAC, hmac->key, 0, &hmac->hash);
        if (result)
            result = CryptSetHashParam(hmac->hash, HP_HMAC_INFO, (uint8_t *)&hmac->info, 0);

        SecureZeroMemory(key_blob, key_blob_size);
        free(key_blob);
    } else if (err == MZ_OK) {
        err = MZ_MEM_ERROR;
    }

    if (!result) {
        hmac->error = GetLastError();
        err = MZ_CRYPT_ERROR;
    }

    if (err != MZ_OK)
        mz_crypt_hmac_free(handle);

    return err;
}

int32_t mz_crypt_hmac_update(void *handle, const void *buf, int32_t size) {
    mz_crypt_hmac *hmac = (mz_crypt_hmac *)handle;
    int32_t result = 0;

    if (!hmac || !buf || !hmac->hash)
        return MZ_PARAM_ERROR;

    result = CryptHashData(hmac->hash, buf, size, 0);
    if (!result) {
        hmac->error = GetLastError();
        return MZ_HASH_ERROR;
    }
    return MZ_OK;
}

int32_t mz_crypt_hmac_end(void *handle, uint8_t *digest, int32_t digest_size) {
    mz_crypt_hmac *hmac = (mz_crypt_hmac *)handle;
    int32_t result = 0;
    int32_t expected_size = 0;

    if (!hmac || !digest || !hmac->hash)
        return MZ_PARAM_ERROR;
    result = CryptGetHashParam(hmac->hash, HP_HASHVAL, NULL, (DWORD *)&expected_size, 0);
    if (expected_size > digest_size)
        return MZ_BUF_ERROR;
    if (!result)
        return MZ_HASH_ERROR;
    result = CryptGetHashParam(hmac->hash, HP_HASHVAL, digest, (DWORD *)&digest_size, 0);
    if (!result) {
        hmac->error = GetLastError();
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
    int32_t err = MZ_OK;

    if (target->hash) {
        CryptDestroyHash(target->hash);
        target->hash = 0;
    }

    result = CryptDuplicateHash(source->hash, NULL, 0, &target->hash);

    if (!result) {
        target->error = GetLastError();
        err = MZ_HASH_ERROR;
    }
    return err;
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

#endif
