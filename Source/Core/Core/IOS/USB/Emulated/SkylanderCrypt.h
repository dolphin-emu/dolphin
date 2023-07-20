// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once
#include "Common/CommonTypes.h"
#include <array>

namespace IOS::HLE::USB
{
  class SkylanderCrypt final
  {
  public:
    static u16 ComputeCRC16(u16 init_value, const u8* buffer, u32 size);
    static u64 ComputeCRC48(const u8* data, u32 size);
    static u64 CalculateKeyA(u8 sector, const u8* nuid, u32 size);
  };
}
