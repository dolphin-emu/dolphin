// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

namespace Titles
{
constexpr u64 BOOT2 = 0x0000000100000001;

constexpr u64 SYSTEM_MENU = 0x0000000100000002;

// IOS used by the latest System Menu (4.3). Corresponds to IOS80.
constexpr u64 SYSTEM_MENU_IOS = 0x0000000100000050;

constexpr u64 BC = 0x0000000100000100;
constexpr u64 MIOS = 0x0000000100000101;

constexpr u64 SHOP = 0x0001000248414241;
}  // namespace Titles
