// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/Crypto/HMAC.h"

#include <limits>

#include <wolfssl/wolfcrypt/hmac.h>

#include "Common/ScopeGuard.h"

namespace Common::HMAC
{
bool HMACWithSHA1(std::span<const u8> key, std::span<const u8> msg, u8* out)
{
  if (key.size() > std::numeric_limits<word32>::max() ||
      msg.size() > std::numeric_limits<word32>::max())
  {
    return false;
  }

  Hmac hmac;
  if (wc_HmacInit(&hmac, nullptr, INVALID_DEVID))
    return false;
  Common::ScopeGuard guard{[&hmac] { wc_HmacFree(&hmac); }};

  return !wc_HmacSetKey(&hmac, WC_SHA, key.data(), static_cast<word32>(key.size())) &&
         !wc_HmacUpdate(&hmac, msg.data(), static_cast<word32>(msg.size())) &&
         !wc_HmacFinal(&hmac, out);
}
}  // namespace Common::HMAC
