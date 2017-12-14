// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <mbedtls/aes.h>

#include "Common/Crypto/AES.h"

namespace Common
{
namespace AES
{
std::vector<u8> Decrypt(const u8* key, u8* iv, const u8* src, size_t size)
{
  mbedtls_aes_context aes_ctx;
  std::vector<u8> buffer(size);

  mbedtls_aes_setkey_dec(&aes_ctx, key, 128);
  mbedtls_aes_crypt_cbc(&aes_ctx, MBEDTLS_AES_DECRYPT, size, iv, src, buffer.data());

  return buffer;
}
}  // namespace AES
}  // namespace Common
