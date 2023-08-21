// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"

namespace Titles
{
constexpr u64 BOOT2 = 0x0000000100000001;

constexpr u64 SYSTEM_MENU = 0x0000000100000002;

constexpr u64 SHOP = 0x0001000248414241;

constexpr u64 KOREAN_SHOP = 0x000100024841424b;

constexpr u64 IOS(u32 major_version)
{
  return 0x0000000100000000 | major_version;
}

// IOS used by the latest System Menu (4.3). Corresponds to IOS80.
constexpr u64 SYSTEM_MENU_IOS = IOS(80);

constexpr u64 BC = IOS(0x100);
constexpr u64 MIOS = IOS(0x101);
}  // namespace Titles
