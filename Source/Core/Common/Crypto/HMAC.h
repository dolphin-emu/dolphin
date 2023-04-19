// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"

namespace Common::HMAC
{
bool HMACWithSHA1(const u8* key, const size_t key_len, const u8* msg, const size_t msg_len,
                  u8* out);
}  // namespace Common::HMAC
