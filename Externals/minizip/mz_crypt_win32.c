/* mz_crypt_win32.c -- Crypto/hash functions for Windows
   part of the minizip-ng project

   Copyright (C) 2010-2021 Nathan Moinvaziri
     https://github.com/zlib-ng/minizip-ng

   This program is distributed under the terms of the same license as zlib.
   See the accompanying LICENSE file for the full text of the license.
*/

#include "mz.h"
#include "mz_os.h"
#include "mz_crypt.h"

#include <windows.h>
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
    HCRYPTPROV provider;
    HCRYPTHASH hash;
    int32_t    error;
    uint16_t   algorithm;
} mz_crypt_sha;

/***************************************************************************/

void mz_crypt_sha_reset(void *handle) {
    mz_crypt_sha *sha = (mz_crypt_sha *)handle;
    if (sha->hash)
        CryptDestroyHash(sha->hash);
    sha->hash = 0;
    if (sha->provider)
        CryptReleaseContext(sha->provider, 0);
    sha->provider = 0;
    sha->error = 0;
}

int32_t mz_crypt_sha_begin(void *handle) {
    mz_crypt_sha *sha = (mz_crypt_sha *)handle;
    ALG_ID alg_id = 0;
    int32_t result = 0;
    int32_t err = MZ_OK;


    if (sha == NULL)
        return MZ_PARAM_ERROR;

    if (sha->algorithm == MZ_HASH_SHA1)
        alg_id = CALG_SHA1;
    else
        alg_id = CALG_SHA_256;

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

    if (sha == NULL || buf == NULL || sha->hash == 0)
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

    if (sha == NULL || digest == NULL || sha->hash == 0)
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

int32_t mz_crypt_aes_encrypt(void *handle, uint8_t *buf, int32_t size) {
    mz_crypt_aes *aes = (mz_crypt_aes *)handle;
    int32_t result = 0;

    if (aes == NULL || buf == NULL)
        return MZ_PARAM_ERROR;
    if (size != MZ_AES_BLOCK_SIZE)
        return MZ_PARAM_ERROR;
    result = CryptEncrypt(aes->key, 0, 0, 0, buf, (DWORD *)&size, size);
    if (!result) {
        aes->error = GetLastError();
        return MZ_CRYPT_ERROR;
    }
    return size;
}

int32_t mz_crypt_aes_decrypt(void *handle, uint8_t *buf, int32_t size) {
    mz_crypt_aes *aes = (mz_crypt_aes *)handle;
    int32_t result = 0;
    if (aes == NULL || buf == NULL)
        return MZ_PARAM_ERROR;
    if (size != MZ_AES_BLOCK_SIZE)
        return MZ_PARAM_ERROR;
    result = CryptDecrypt(aes->key, 0, 0, 0, buf, (DWORD *)&size);
    if (!result) {
        aes->error = GetLastError();
        return MZ_CRYPT_ERROR;
    }
    return size;
}

static int32_t mz_crypt_aes_set_key(void *handle, const void *key, int32_t key_length) {
    mz_crypt_aes *aes = (mz_crypt_aes *)handle;
    HCRYPTHASH hash = 0;
    ALG_ID alg_id = 0;
    typedef struct key_blob_header_s {
        BLOBHEADER hdr;
        uint32_t   key_length;
    } key_blob_header_s;
    key_blob_header_s *key_blob_s = NULL;
    uint32_t mode = CRYPT_MODE_ECB;
    uint8_t *key_blob = NULL;
    int32_t key_blob_size = 0;
    int32_t result = 0;
    int32_t err = MZ_OK;


    if (aes == NULL || key == NULL)
        return MZ_PARAM_ERROR;

    mz_crypt_aes_reset(handle);

    if (key_length == MZ_AES_KEY_LENGTH(MZ_AES_ENCRYPTION_MODE_128))
        alg_id = CALG_AES_128;
    else if (key_length == MZ_AES_KEY_LENGTH(MZ_AES_ENCRYPTION_MODE_192))
        alg_id = CALG_AES_192;
    else if (key_length == MZ_AES_KEY_LENGTH(MZ_AES_ENCRYPTION_MODE_256))
        alg_id = CALG_AES_256;
    else
        return MZ_PARAM_ERROR;

    result = CryptAcquireContext(&aes->provider, NULL, MS_ENH_RSA_AES_PROV, PROV_RSA_AES, CRYPT_VERIFYCONTEXT | CRYPT_SILENT);
    if (result) {
        key_blob_size = sizeof(key_blob_header_s) + key_length;
        key_blob = (uint8_t *)MZ_ALLOC(key_blob_size);
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
            MZ_FREE(key_blob);
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

    if (hash)
        CryptDestroyHash(hash);

    return err;
}

int32_t mz_crypt_aes_set_encrypt_key(void *handle, const void *key, int32_t key_length) {
    return mz_crypt_aes_set_key(handle, key, key_length);
}

int32_t mz_crypt_aes_set_decrypt_key(void *handle, const void *key, int32_t key_length) {
    return mz_crypt_aes_set_key(handle, key, key_length);
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
    if (aes != NULL) {
        mz_crypt_aes_free(*handle);
        MZ_FREE(aes);
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
    int32_t result = 0;
    int32_t err = MZ_OK;


    if (hmac == NULL || key == NULL)
        return MZ_PARAM_ERROR;

    mz_crypt_hmac_reset(handle);

    if (hmac->algorithm == MZ_HASH_SHA1)
        alg_id = CALG_SHA1;
    else
        alg_id = CALG_SHA_256;

    hmac->info.HashAlgid = alg_id;

    result = CryptAcquireContext(&hmac->provider, NULL, MS_ENHANCED_PROV, PROV_RSA_FULL,
        CRYPT_VERIFYCONTEXT | CRYPT_SILENT);

    if (!result) {
        hmac->error = GetLastError();
        err = MZ_CRYPT_ERROR;
    } else {
        /* Zero-pad odd key lengths */
        if (key_length % 2 == 1)
            key_length += 1;
        key_blob_size = sizeof(key_blob_header_s) + key_length;
        key_blob = (uint8_t *)MZ_ALLOC(key_blob_size);
    }

    if (key_blob) {
        memset(key_blob, 0, key_blob_size);
        key_blob_s = (key_blob_header_s *)key_blob;
        key_blob_s->hdr.bType = PLAINTEXTKEYBLOB;
        key_blob_s->hdr.bVersion = CUR_BLOB_VERSION;
        key_blob_s->hdr.aiKeyAlg = CALG_RC2;
        key_blob_s->hdr.reserved = 0;
        key_blob_s->key_length = key_length;

        memcpy(key_blob + sizeof(key_blob_header_s), key, key_length);

        result = CryptImportKey(hmac->provider, key_blob, key_blob_size, 0, CRYPT_IPSEC_HMAC_KEY, &hmac->key);
        if (result)
            result = CryptCreateHash(hmac->provider, CALG_HMAC, hmac->key, 0, &hmac->hash);
        if (result)
            result = CryptSetHashParam(hmac->hash, HP_HMAC_INFO, (uint8_t *)&hmac->info, 0);

        SecureZeroMemory(key_blob, key_blob_size);
        MZ_FREE(key_blob);
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

    if (hmac == NULL || buf == NULL || hmac->hash == 0)
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

    if (hmac == NULL || digest == NULL || hmac->hash == 0)
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
        mz_crypt_hmac_free(*handle);
        MZ_FREE(hmac);
    }
    *handle = NULL;
}

/***************************************************************************/

#if defined(MZ_ZIP_SIGNING)
int32_t mz_crypt_sign(uint8_t *message, int32_t message_size, uint8_t *cert_data, int32_t cert_data_size,
    const char *cert_pwd, uint8_t **signature, int32_t *signature_size) {
    CRYPT_SIGN_MESSAGE_PARA sign_params;
    CRYPT_DATA_BLOB cert_data_blob;
    PCCERT_CONTEXT cert_context = NULL;
    HCERTSTORE cert_store = 0;
    wchar_t *password_wide = NULL;
    int32_t result = 0;
    int32_t err = MZ_OK;
    uint32_t messages_sizes[1];
    uint8_t *messages[1];


    if (message == NULL || cert_data == NULL || signature == NULL || signature_size == NULL)
        return MZ_PARAM_ERROR;

    *signature = NULL;
    *signature_size = 0;

    cert_data_blob.pbData = cert_data;
    cert_data_blob.cbData = cert_data_size;

    password_wide = mz_os_unicode_string_create(cert_pwd, MZ_ENCODING_UTF8);
    if (password_wide) {
        cert_store = PFXImportCertStore(&cert_data_blob, password_wide, 0);
        mz_os_unicode_string_delete(&password_wide);
    }

    if (cert_store == NULL)
        cert_store = PFXImportCertStore(&cert_data_blob, L"", 0);
    if (cert_store == NULL)
        cert_store = PFXImportCertStore(&cert_data_blob, NULL, 0);
    if (cert_store == NULL)
        return MZ_PARAM_ERROR;

    if (err == MZ_OK) {
        cert_context = CertFindCertificateInStore(cert_store,
            X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, CERT_FIND_HAS_PRIVATE_KEY, NULL, NULL);
        if (cert_context == NULL)
            err = MZ_PARAM_ERROR;
    }
    if (err == MZ_OK) {
        memset(&sign_params, 0, sizeof(sign_params));

        sign_params.cbSize = sizeof(sign_params);
        sign_params.dwMsgEncodingType = PKCS_7_ASN_ENCODING | X509_ASN_ENCODING;
        sign_params.pSigningCert = cert_context;
        sign_params.HashAlgorithm.pszObjId = szOID_NIST_sha256;
        sign_params.cMsgCert = 1;
        sign_params.rgpMsgCert = &cert_context;

        messages[0] = message;
        messages_sizes[0] = message_size;

#if 0 /* Timestamp support */
        CRYPT_ATTR_BLOB crypt_blob;
        CRYPT_TIMESTAMP_CONTEXT *ts_context = NULL;
        CRYPT_ATTRIBUTE unauth_attribs[1];
        wchar_t *timestamp_url_wide = NULL;
        const char *timestamp_url = NULL;

        if (timestamp_url != NULL)
            timestamp_url_wide = mz_os_unicode_string_create(timestamp_url);
        if (timestamp_url_wide != NULL) {
            result = CryptRetrieveTimeStamp(timestamp_url_wide,
                TIMESTAMP_NO_AUTH_RETRIEVAL | TIMESTAMP_VERIFY_CONTEXT_SIGNATURE, 0, szOID_NIST_sha256,
                NULL, message, message_size, &ts_context, NULL, NULL);

            mz_os_unicode_string_delete(&timestamp_url_wide);

            if ((result) && (ts_context != NULL)) {
                crypt_blob.cbData = ts_context->cbEncoded;
                crypt_blob.pbData = ts_context->pbEncoded;

                unauth_attribs[0].pszObjId = "1.2.840.113549.1.9.16.2.14"; //id-smime-aa-timeStampToken
                unauth_attribs[0].cValue = 1;
                unauth_attribs[0].rgValue = &crypt_blob;

                sign_params.rgUnauthAttr = &unauth_attribs[0];
                sign_params.cUnauthAttr = 1;
            }
        }

        if (ts_context != NULL)
            CryptMemFree(ts_context);

        if (result)
#endif

            result = CryptSignMessage(&sign_params, FALSE, 1, (const BYTE **)messages, (DWORD *)messages_sizes,
                NULL, (DWORD *)signature_size);

        if (result && *signature_size > 0)
            *signature = (uint8_t *)MZ_ALLOC(*signature_size);

        if (result && *signature != NULL)
            result = CryptSignMessage(&sign_params, FALSE, 1, (const BYTE **)messages, (DWORD *)messages_sizes,
                *signature, (DWORD *)signature_size);

        if (!result)
            err = MZ_SIGN_ERROR;
    }

    if (cert_context != NULL)
        CertFreeCertificateContext(cert_context);
    if (cert_store != NULL)
        CertCloseStore(cert_store, 0);

    return err;
}

int32_t mz_crypt_sign_verify(uint8_t *message, int32_t message_size, uint8_t *signature, int32_t signature_size) {
    CRYPT_VERIFY_MESSAGE_PARA verify_params;
    CERT_CONTEXT *signer_cert = NULL;
    CERT_CHAIN_PARA  chain_para;
    CERT_CHAIN_CONTEXT *chain_context = NULL;
    CERT_CHAIN_POLICY_PARA chain_policy;
    CERT_CHAIN_POLICY_STATUS chain_policy_status;
    HCRYPTMSG crypt_msg = 0;
    int32_t result = 0;
    int32_t err = MZ_SIGN_ERROR;
    uint8_t *decoded = NULL;
    int32_t decoded_size = 0;


    memset(&verify_params, 0, sizeof(verify_params));

    verify_params.cbSize = sizeof(verify_params);
    verify_params.dwMsgAndCertEncodingType = PKCS_7_ASN_ENCODING | X509_ASN_ENCODING;

    result = CryptVerifyMessageSignature(&verify_params, 0, signature, signature_size,
        NULL, (DWORD *)&decoded_size, NULL);

    if (result && decoded_size > 0)
        decoded = (uint8_t *)MZ_ALLOC(decoded_size);

    if (result && decoded != NULL)
        result = CryptVerifyMessageSignature(&verify_params, 0, signature, signature_size,
            decoded, (DWORD *)&decoded_size, (const CERT_CONTEXT **)&signer_cert);

    /* Get and validate certificate chain */
    memset(&chain_para, 0, sizeof(chain_para));

    if (result && signer_cert != NULL)
        result = CertGetCertificateChain(NULL, signer_cert, NULL, NULL, &chain_para,
            CERT_CHAIN_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT, NULL, (const CERT_CHAIN_CONTEXT **)&chain_context);

    memset(&chain_policy, 0, sizeof(chain_policy));
    chain_policy.cbSize = sizeof(CERT_CHAIN_POLICY_PARA);

    memset(&chain_policy_status, 0, sizeof(chain_policy_status));
    chain_policy_status.cbSize = sizeof(CERT_CHAIN_POLICY_STATUS);

    if (result && chain_context != NULL)
        result = CertVerifyCertificateChainPolicy(CERT_CHAIN_POLICY_BASE, chain_context,
            &chain_policy, &chain_policy_status);

    if (chain_policy_status.dwError != S_OK)
        result = 0;

#if 0
    crypt_msg = CryptMsgOpenToDecode(PKCS_7_ASN_ENCODING | X509_ASN_ENCODING, 0, 0, 0, NULL, NULL);
    if (crypt_msg != NULL) {
        /* Timestamp support */
        PCRYPT_ATTRIBUTES unauth_attribs = NULL;
        HCRYPTMSG ts_msg = 0;
        uint8_t *ts_content = NULL;
        int32_t ts_content_size = 0;
        uint8_t *ts_signature = NULL;
        int32_t ts_signature_size = 0;

        result = CryptMsgUpdate(crypt_msg, signature, signature_size, 1);

        if (result)
            CryptMsgGetParam(crypt_msg, CMSG_SIGNER_UNAUTH_ATTR_PARAM, 0, NULL, &ts_signature_size);

        if ((result) && (ts_signature_size > 0))
            ts_signature = (uint8_t *)MZ_ALLOC(ts_signature_size);

        if ((result) && (ts_signature != NULL)) {
            result = CryptMsgGetParam(crypt_msg, CMSG_SIGNER_UNAUTH_ATTR_PARAM, 0, ts_signature,
                &ts_signature_size);
            if (result)
            {
                unauth_attribs = (PCRYPT_ATTRIBUTES)ts_signature;

                if ((unauth_attribs->cAttr > 0) && (unauth_attribs->rgAttr[0].cValue > 0))
                {
                    ts_content = unauth_attribs->rgAttr[0].rgValue->pbData;
                    ts_content_size = unauth_attribs->rgAttr[0].rgValue->cbData;
                }
            }

            if ((result) && (ts_content != NULL))
                result = CryptVerifyTimeStampSignature(ts_content, ts_content_size, decoded,
                    decoded_size, 0, &crypt_context, NULL, NULL);

            if (result)
                err = MZ_OK;
        }

        if (ts_signature != NULL)
            MZ_FREE(ts_signature);

        if (crypt_context != NULL)
            CryptMemFree(crypt_context);
    } else {
        result = 0;
    }
#endif

    if ((result) && (decoded != NULL) && (decoded_size == message_size)) {
        /* Verify cms message with our stored message */
        if (memcmp(decoded, message, message_size) == 0)
            err = MZ_OK;
    }

    if (chain_context != NULL)
        CertFreeCertificateChain(chain_context);
    if (signer_cert != NULL)
        CertFreeCertificateContext(signer_cert);
    if (crypt_msg != NULL)
        CryptMsgClose(crypt_msg);

    if (decoded != NULL)
        MZ_FREE(decoded);

    return err;
}
#endif
