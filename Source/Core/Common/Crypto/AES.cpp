// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <mbedtls/aes.h>

#include "Common/Crypto/AES.h"

namespace Common::AES
{
std::vector<u8> DecryptEncrypt(const u8* key, u8* iv, const u8* src, size_t size, Mode mode)
{
  mbedtls_aes_context aes_ctx;
  std::vector<u8> buffer(size);

  if (mode == Mode::Encrypt)
    mbedtls_aes_setkey_enc(&aes_ctx, key, 128);
  else
    mbedtls_aes_setkey_dec(&aes_ctx, key, 128);

  mbedtls_aes_crypt_cbc(&aes_ctx, mode == Mode::Encrypt ? MBEDTLS_AES_ENCRYPT : MBEDTLS_AES_DECRYPT,
                        size, iv, src, buffer.data());

  return buffer;
}

std::vector<u8> Decrypt(const u8* key, u8* iv, const u8* src, size_t size)
{
  return DecryptEncrypt(key, iv, src, size, Mode::Decrypt);
}

std::vector<u8> Encrypt(const u8* key, u8* iv, const u8* src, size_t size)
{
  return DecryptEncrypt(key, iv, src, size, Mode::Encrypt);
}
}  // namespace Common::AES
