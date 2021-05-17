// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string_view>

#include "Common/CommonTypes.h"

namespace Common
{
u32 ComputeCRC32(std::string_view data);
}  // namespace Common
