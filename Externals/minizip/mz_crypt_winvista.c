/* mz_crypt_winrt.c -- Crypto/hash functions for Windows Vista or later
   part of the minizip-ng project

   Copyright (C) 2010-2022 Nathan Moinvaziri
     https://github.com/zlib-ng/minizip-ng

   This program is distributed under the terms of the same license as zlib.
   See the accompanying LICENSE file for the full text of the license.
*/

#include "mz.h"
#include "mz_os.h"
#include "mz_crypt.h"

#include <windows.h>

#ifndef _WIN32_WINNT_VISTA
#  define _WIN32_WINNT_VISTA 0x0600
#endif

#if _WIN32_WINNT >= _WIN32_WINNT_VISTA
#include <bcrypt.h>

/***************************************************************************/

#define NT_SUCCESS(status) ((status) >= 0)

/***************************************************************************/

int32_t mz_crypt_rand(uint8_t *buf, int32_t size) {
    BCRYPT_ALG_HANDLE provider = NULL;
    NTSTATUS status = 0;

    status = BCryptOpenAlgorithmProvider(&provider, BCRYPT_RNG_ALGORITHM, NULL, 0);
    if (NT_SUCCESS(status)) {
        status = BCryptGenRandom(provider, buf, size, 0);
        BCryptCloseAlgorithmProvider(provider, 0);
    }
    if (NT_SUCCESS(status))
        return size;

    return mz_os_rand(buf, size);
}

/***************************************************************************/

typedef struct mz_crypt_sha_s {
    union {
        struct {
            BCRYPT_ALG_HANDLE  provider;
            BCRYPT_HASH_HANDLE hash;
            uint8_t            *buffer;
        };
    };
    int32_t                    error;
    uint16_t                   algorithm;
} mz_crypt_sha;

/***************************************************************************/

static void mz_crypt_sha_free(void *handle) {
    mz_crypt_sha *sha = (mz_crypt_sha *)handle;
    if (sha->hash)
        BCryptDestroyHash(sha->hash);
    sha->hash = NULL;
    if (sha->provider)
        BCryptCloseAlgorithmProvider(sha->provider, 0);
    sha->provider = NULL;
    free(sha->buffer);
    sha->buffer = NULL;
}

void mz_crypt_sha_reset(void *handle) {
    mz_crypt_sha *sha = (mz_crypt_sha *)handle;
    mz_crypt_sha_free(handle);
    sha->error = 0;
}

int32_t mz_crypt_sha_begin(void *handle) {
    mz_crypt_sha *sha = (mz_crypt_sha *)handle;
    NTSTATUS status = 0;
    const wchar_t *alg_id = BCRYPT_SHA256_ALGORITHM;
    ULONG buffer_size = 0;
    ULONG result_size = sizeof(buffer_size);
    int32_t err = MZ_OK;

    if (!sha)
        return MZ_PARAM_ERROR;

    if (sha->algorithm == MZ_HASH_SHA224)
        return MZ_SUPPORT_ERROR;

    switch (sha->algorithm) {
    case MZ_HASH_SHA1:
        alg_id = BCRYPT_SHA1_ALGORITHM;
        break;
    case MZ_HASH_SHA384:
        alg_id = BCRYPT_SHA384_ALGORITHM;
        break;
    case MZ_HASH_SHA512:
        alg_id = BCRYPT_SHA512_ALGORITHM;
        break;
    }

    status = BCryptOpenAlgorithmProvider(&sha->provider, alg_id, NULL, 0);
    if (NT_SUCCESS(status)) {
        status = BCryptGetProperty(sha->provider, BCRYPT_OBJECT_LENGTH, (PUCHAR)&buffer_size, result_size,
            &result_size, 0);
    }
    if (NT_SUCCESS(status)) {
        sha->buffer = malloc(buffer_size);
        if (!sha->buffer)
            return MZ_MEM_ERROR;
        status = BCryptCreateHash(sha->provider, &sha->hash, sha->buffer, buffer_size, NULL, 0, 0);
    }
    if (!NT_SUCCESS(status)) {
        sha->error = status;
        err = MZ_HASH_ERROR;
    }

    return err;
}

int32_t mz_crypt_sha_update(void *handle, const void *buf, int32_t size) {
    mz_crypt_sha *sha = (mz_crypt_sha *)handle;
    NTSTATUS status = 0;

    if (!sha || !buf || size < 0)
        return MZ_PARAM_ERROR;

    if (sha->algorithm == MZ_HASH_SHA224)
        return MZ_SUPPORT_ERROR;

    if (sha->hash == 0)
        return MZ_PARAM_ERROR;

    status = BCryptHashData(sha->hash, (uint8_t*)buf, size, 0);
    if (!NT_SUCCESS(status)) {
        sha->error = status;
        return MZ_HASH_ERROR;
    }
    return size;
}

int32_t mz_crypt_sha_end(void *handle, uint8_t *digest, int32_t digest_size) {
    mz_crypt_sha *sha = (mz_crypt_sha *)handle;
    NTSTATUS status = 0;
    ULONG expected_size = 0;
    ULONG result_size = sizeof(expected_size);

    if (!sha || !digest)
        return MZ_PARAM_ERROR;

    if (sha->algorithm == MZ_HASH_SHA224)
        return MZ_SUPPORT_ERROR;

    if (sha->hash == 0)
        return MZ_PARAM_ERROR;

    status = BCryptGetProperty(sha->hash, BCRYPT_HASH_LENGTH, (PUCHAR)&expected_size, result_size, &result_size, 0);
    if (!NT_SUCCESS(status))
        return MZ_HASH_ERROR;
    if ((int32_t)expected_size > digest_size)
        return MZ_BUF_ERROR;
    status = BCryptFinishHash(sha->hash, digest, expected_size, 0);
    if (!NT_SUCCESS(status)) {
        sha->error = status;
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

#define MZ_AES_MAX_TAG_SIZE     (16)
#define MZ_AES_MAX_NONCE_SIZE   (12)

typedef struct mz_crypt_aes_s {
    BCRYPT_ALG_HANDLE provider;
    BCRYPT_KEY_HANDLE key;
    uint8_t           *key_buffer;
    int32_t           mode;
    int32_t           error;
    uint8_t           *iv;
    uint32_t          iv_length;
    uint8_t           nonce[MZ_AES_MAX_NONCE_SIZE];
    uint32_t          nonce_length;
    uint8_t           mac[MZ_AES_MAX_TAG_SIZE];
    uint8_t           tag[MZ_AES_MAX_TAG_SIZE];
    uint32_t          tag_size;
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO *auth_info;
} mz_crypt_aes;

/***************************************************************************/

static void mz_crypt_aes_free(void *handle) {
    mz_crypt_aes *aes = (mz_crypt_aes *)handle;
    if (aes->key)
        BCryptDestroyKey(aes->key);
    aes->key = NULL;
    if (aes->provider)
        BCryptCloseAlgorithmProvider(aes->provider, 0);
    aes->provider = NULL;
    free(aes->key_buffer);
    aes->key_buffer = NULL;
    free(aes->iv);
    aes->iv = NULL;
    free(aes->auth_info);
    aes->auth_info = NULL;
}

void mz_crypt_aes_reset(void *handle) {
    mz_crypt_aes_free(handle);
}

int32_t mz_crypt_aes_encrypt(void *handle, const void *aad, int32_t aad_size, uint8_t *buf, int32_t size) {
    mz_crypt_aes *aes = (mz_crypt_aes *)handle;
    ULONG output_size = 0;
    NTSTATUS status = 0;

    if (!aes || !buf || size % MZ_AES_BLOCK_SIZE != 0)
        return MZ_PARAM_ERROR;
    if (aes->mode != MZ_AES_MODE_GCM && aad && aad_size > 0)
        return MZ_PARAM_ERROR;
    if (aes->mode == MZ_AES_MODE_GCM && !aes->auth_info)
        return MZ_PARAM_ERROR;

    if (aad && aes->auth_info && !(aes->auth_info->dwFlags & BCRYPT_AUTH_MODE_IN_PROGRESS_FLAG)) {
        aes->auth_info->pbAuthData = (uint8_t*)aad;
        aes->auth_info->cbAuthData = aad_size;
    }

    status = BCryptEncrypt(aes->key, buf, size, aes->auth_info, aes->iv, aes->iv_length, buf, size,
        &output_size, 0);

    if (aad && aes->auth_info) {
        aes->auth_info->pbAuthData = NULL;
        aes->auth_info->cbAuthData = 0;
    }

    if (!NT_SUCCESS(status)) {
        aes->error = status;
        return MZ_CRYPT_ERROR;
    }
    return size;
}

int32_t mz_crypt_aes_encrypt_final(void *handle, uint8_t *buf, int32_t size, uint8_t *tag, int32_t tag_size) {
    mz_crypt_aes *aes = (mz_crypt_aes *)handle;
    NTSTATUS status = 0;
    ULONG output_size = 0;

    if (!aes || !tag || !tag_size || aes->mode != MZ_AES_MODE_GCM || !aes->auth_info)
        return MZ_PARAM_ERROR;

    aes->auth_info->pbTag = tag;
    aes->auth_info->cbTag = tag_size;

    aes->auth_info->dwFlags &= ~BCRYPT_AUTH_MODE_CHAIN_CALLS_FLAG;

    status = BCryptEncrypt(aes->key, buf, size, aes->auth_info, aes->iv, aes->iv_length, buf, size,
        &output_size, 0);

    if (!NT_SUCCESS(status)) {
        aes->error = status;
        return MZ_CRYPT_ERROR;
    }

    return size;
}

int32_t mz_crypt_aes_decrypt(void *handle, const void *aad, int32_t aad_size, uint8_t *buf, int32_t size) {
    mz_crypt_aes *aes = (mz_crypt_aes *)handle;
    ULONG output_size = 0;
    NTSTATUS status = 0;

    if (!aes || !buf || size % MZ_AES_BLOCK_SIZE != 0)
        return MZ_PARAM_ERROR;
    if (aes->mode != MZ_AES_MODE_GCM && aad && aad_size > 0)
        return MZ_PARAM_ERROR;
    if (aes->mode == MZ_AES_MODE_GCM && !aes->auth_info)
        return MZ_PARAM_ERROR;

    if (aad && aes->auth_info && !(aes->auth_info->dwFlags & BCRYPT_AUTH_MODE_IN_PROGRESS_FLAG)) {
        aes->auth_info->pbAuthData = (uint8_t*)aad;
        aes->auth_info->cbAuthData = aad_size;
    }

    status = BCryptDecrypt(aes->key, buf, size, aes->auth_info, aes->iv, aes->iv_length, buf, size,
        &output_size, 0);

    if (aad && aes->auth_info) {
        aes->auth_info->pbAuthData = NULL;
        aes->auth_info->cbAuthData = 0;
    }

    if (!NT_SUCCESS(status)) {
        aes->error = status;
        return MZ_CRYPT_ERROR;
    }
    return size;
}

int32_t mz_crypt_aes_decrypt_final(void *handle, uint8_t *buf, int32_t size, const uint8_t *tag, int32_t tag_length) {
    mz_crypt_aes *aes = (mz_crypt_aes *)handle;
    NTSTATUS status = 0;
    ULONG output_size = 0;

    if (!aes || !tag || !tag_length || aes->mode != MZ_AES_MODE_GCM || !aes->auth_info)
        return MZ_PARAM_ERROR;

    aes->auth_info->pbTag = (uint8_t *)tag;
    aes->auth_info->cbTag = tag_length;

    aes->auth_info->dwFlags &= ~BCRYPT_AUTH_MODE_CHAIN_CALLS_FLAG;

    status = BCryptDecrypt(aes->key, buf, size, aes->auth_info, aes->iv, aes->iv_length, buf, size,
        &output_size, 0);

    if (!NT_SUCCESS(status)) {
        aes->error = status;
        return MZ_CRYPT_ERROR;
    }

    return size;
}

static int32_t mz_crypt_aes_set_key(void *handle, const void *key, int32_t key_length,
    const void *iv, int32_t iv_length) {
    mz_crypt_aes *aes = (mz_crypt_aes *)handle;
    BCRYPT_KEY_DATA_BLOB_HEADER *key_blob = NULL;
    int32_t key_blob_size = 0;
    ULONG key_size = 0;
    wchar_t *mode = NULL;
    NTSTATUS status = 0;
    int32_t err = MZ_OK;

    if (!aes || !key || !key_length)
        return MZ_PARAM_ERROR;
    if (key_length != 16 && key_length != 24 && key_length != 32)
        return MZ_PARAM_ERROR;
    if (iv && iv_length > MZ_AES_BLOCK_SIZE)
        return MZ_PARAM_ERROR;

    if (aes->mode == MZ_AES_MODE_ECB)
        mode = BCRYPT_CHAIN_MODE_ECB;
    else if (aes->mode == MZ_AES_MODE_CBC)
        mode = BCRYPT_CHAIN_MODE_CBC;
    else if (aes->mode == MZ_AES_MODE_GCM)
        mode = BCRYPT_CHAIN_MODE_GCM;
    else
        return MZ_PARAM_ERROR;

    mz_crypt_aes_reset(handle);

    if (iv && iv_length) {
        if (aes->mode == MZ_AES_MODE_ECB)
            return MZ_PARAM_ERROR;

        aes->iv_length = MZ_AES_BLOCK_SIZE;
        aes->iv = calloc(MZ_AES_BLOCK_SIZE, sizeof(uint8_t));
        if (!aes->iv)
            return MZ_MEM_ERROR;
        memcpy(aes->iv, iv, iv_length);
    }

    status = BCryptOpenAlgorithmProvider(&aes->provider, BCRYPT_AES_ALGORITHM, NULL, 0);

    if (NT_SUCCESS(status)) {
        status = BCryptSetProperty(aes->provider, BCRYPT_CHAINING_MODE, (PBYTE)mode, sizeof(mode), 0);
    }

    if (NT_SUCCESS(status)) {
        ULONG result_size;
        status = BCryptGetProperty(aes->provider, BCRYPT_OBJECT_LENGTH, (PUCHAR)&key_size,
            sizeof(key_size), &result_size, 0);
    }

    if (NT_SUCCESS(status)) {
        aes->key_buffer = malloc(key_size);
        if (!aes->key_buffer)
            return MZ_MEM_ERROR;
        key_blob_size = sizeof(*key_blob) + key_length;
        key_blob = malloc(key_blob_size);
        if (key_blob) {
            key_blob->dwMagic = BCRYPT_KEY_DATA_BLOB_MAGIC;
            key_blob->dwVersion = BCRYPT_KEY_DATA_BLOB_VERSION1;
            key_blob->cbKeyData = key_length;

            memcpy((uint8_t*)key_blob + sizeof(*key_blob), key, key_length);

            status = BCryptImportKey(aes->provider, NULL, BCRYPT_KEY_DATA_BLOB, &aes->key, aes->key_buffer,
                key_size, (PUCHAR)key_blob, key_blob_size, 0);
            SecureZeroMemory(key_blob, key_blob_size);
            free(key_blob);
        }
    }

    if (aes->mode == MZ_AES_MODE_GCM) {
        ULONG result_size;

        aes->auth_info = malloc(sizeof(BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO));
        if (!aes->auth_info)
            return MZ_MEM_ERROR;

        BCRYPT_INIT_AUTH_MODE_INFO(*aes->auth_info);
        aes->auth_info->dwFlags = BCRYPT_AUTH_MODE_CHAIN_CALLS_FLAG;

        if (NT_SUCCESS(status)) {
            BCRYPT_AUTH_TAG_LENGTHS_STRUCT tag_lengths;

            status = BCryptGetProperty(aes->provider, BCRYPT_AUTH_TAG_LENGTH, (PUCHAR)&tag_lengths,
                sizeof(tag_lengths), &result_size, 0);

            if (NT_SUCCESS(status)) {
                aes->tag_size = tag_lengths.dwMaxLength;
                if (aes->tag_size > sizeof(aes->tag))
                    return MZ_CRYPT_ERROR;

                aes->auth_info->cbTag = aes->tag_size;
                aes->auth_info->pbMacContext = aes->mac;
                aes->auth_info->cbMacContext = aes->tag_size;
            }
        }

        if (NT_SUCCESS(status)) {
            ULONG block_length;

            status = BCryptGetProperty(aes->provider, BCRYPT_BLOCK_LENGTH, (PUCHAR)&block_length,
                sizeof(block_length), &result_size, 0);

            if (NT_SUCCESS(status) && iv) {
                if (aes->iv_length > block_length)
                    return MZ_CRYPT_ERROR;

                aes->nonce_length = iv_length;
                if (aes->nonce_length > MZ_AES_MAX_NONCE_SIZE)
                    aes->nonce_length = MZ_AES_MAX_NONCE_SIZE;
                memcpy(aes->nonce, iv, aes->nonce_length);

                aes->auth_info->pbNonce = aes->nonce;
                aes->auth_info->cbNonce = aes->nonce_length;
            }
        }
    }

    if (!NT_SUCCESS(status)) {
        aes->error = status;
        err = MZ_CRYPT_ERROR;
    }

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
    BCRYPT_ALG_HANDLE  provider;
    BCRYPT_KEY_HANDLE  key;
    BCRYPT_HASH_HANDLE hash;
    uint8_t            *buffer;
    int32_t            error;
    uint16_t           algorithm;
} mz_crypt_hmac;

/***************************************************************************/

static void mz_crypt_hmac_free(void *handle) {
    mz_crypt_hmac *hmac = (mz_crypt_hmac *)handle;
    if (hmac->hash)
        BCryptDestroyHash(hmac->hash);
    hmac->hash = NULL;
    if (hmac->key)
        BCryptDestroyKey(hmac->key);
    hmac->key = NULL;
    if (hmac->provider)
        BCryptCloseAlgorithmProvider(hmac->provider, 0);
    hmac->provider = NULL;
    free(hmac->buffer);
    hmac->buffer = NULL;
}

void mz_crypt_hmac_reset(void *handle) {
    mz_crypt_hmac_free(handle);
}

int32_t mz_crypt_hmac_init(void *handle, const void *key, int32_t key_length) {
    mz_crypt_hmac *hmac = (mz_crypt_hmac *)handle;
    wchar_t *alg_id = BCRYPT_SHA256_ALGORITHM;
    ULONG buffer_size = 0;
    ULONG result_size = sizeof(buffer_size);
    NTSTATUS status = 0;
    int32_t err = MZ_OK;

    if (!hmac || !key)
        return MZ_PARAM_ERROR;

    mz_crypt_hmac_reset(handle);

    if (hmac->algorithm == MZ_HASH_SHA1)
        alg_id = BCRYPT_SHA1_ALGORITHM;

    status = BCryptOpenAlgorithmProvider(&hmac->provider, alg_id, NULL, BCRYPT_ALG_HANDLE_HMAC_FLAG);
    if (NT_SUCCESS(status)) {
        status = BCryptGetProperty(hmac->provider, BCRYPT_OBJECT_LENGTH, (PUCHAR)&buffer_size, result_size,
            &result_size, 0);
    }
    if (NT_SUCCESS(status)) {
        hmac->buffer = malloc(buffer_size);
        if (!hmac->buffer)
            return MZ_MEM_ERROR;
    }
    if (NT_SUCCESS(status))
        status = BCryptCreateHash(hmac->provider, &hmac->hash, hmac->buffer, buffer_size, (PUCHAR)key, key_length, 0);

    if (!NT_SUCCESS(status)) {
        hmac->error = status;
        err = MZ_CRYPT_ERROR;
    }

    if (err != MZ_OK)
        mz_crypt_hmac_free(handle);

    return err;
}

int32_t mz_crypt_hmac_update(void *handle, const void *buf, int32_t size) {
    mz_crypt_hmac *hmac = (mz_crypt_hmac *)handle;
    NTSTATUS status = 0;

    if (!hmac || !buf || !hmac->hash)
        return MZ_PARAM_ERROR;

    status = BCryptHashData(hmac->hash, (uint8_t*)buf, size, 0);
    if (!NT_SUCCESS(status)) {
        hmac->error = status;
        return MZ_HASH_ERROR;
    }
    return MZ_OK;
}

int32_t mz_crypt_hmac_end(void *handle, uint8_t *digest, int32_t digest_size) {
    mz_crypt_hmac *hmac = (mz_crypt_hmac *)handle;
    NTSTATUS status = 0;
    ULONG expected_size = 0;
    ULONG result_size = sizeof(expected_size);

    if (!hmac || !digest || !hmac->hash)
        return MZ_PARAM_ERROR;
    status = BCryptGetProperty(hmac->hash, BCRYPT_HASH_LENGTH, (PUCHAR)&expected_size, result_size, &result_size, 0);
    if (!NT_SUCCESS(status))
        return MZ_HASH_ERROR;
    if ((int32_t)expected_size > digest_size)
        return MZ_BUF_ERROR;
    status = BCryptFinishHash(hmac->hash, digest, expected_size, 0);
    if (!NT_SUCCESS(status)) {
        hmac->error = status;
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
    NTSTATUS status = 0;
    int32_t err = MZ_OK;

    if (target->hash) {
        BCryptDestroyHash(target->hash);
        target->hash = NULL;
    }

    status = BCryptDuplicateHash(source->hash, &target->hash, NULL, 0, 0);

    if (!NT_SUCCESS(status)) {
        target->error = status;
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
