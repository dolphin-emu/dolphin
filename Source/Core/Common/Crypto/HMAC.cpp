// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <mbedtls/hmac_drbg.h>

#include "Common/Crypto/HMAC.h"

namespace Common::HMAC
{
void HMACWithSHA1(const u8* key, const size_t key_len, const u8* msg, const size_t msg_len, u8* out)
{
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA1), 1);
  mbedtls_md_hmac_starts(&ctx, key, key_len);
  mbedtls_md_hmac_update(&ctx, msg, msg_len);
  mbedtls_md_hmac_finish(&ctx, out);
  mbedtls_md_free(&ctx);
}
}  // namespace Common::HMAC