// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"

#include <span>

namespace Common::HMAC
{
// HMAC with the SHA1 message digest. Excepted output length is 20 bytes.
bool HMACWithSHA1(std::span<const u8> key, std::span<const u8> msg, u8* out);
}  // namespace Common::HMAC
