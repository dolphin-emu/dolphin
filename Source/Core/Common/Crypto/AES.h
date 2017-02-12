// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <vector>

#include "Common/CommonTypes.h"

namespace Common
{
namespace AES
{
std::vector<u8> Decrypt(const u8* key, u8* iv, const u8* src, size_t size);
}  // namespace AES
}  // namespace Common
