// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <string_view>

#include "Common/CommonTypes.h"

namespace Common
{
u32 ComputeCRC32(std::string_view data);
}  // namespace Common
