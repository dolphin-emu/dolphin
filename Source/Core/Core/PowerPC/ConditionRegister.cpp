// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/ConditionRegister.h"

namespace PowerPC
{
const std::array<u64, 16> ConditionRegister::s_crTable = {{
    PPCToInternal(0x0),
    PPCToInternal(0x1),
    PPCToInternal(0x2),
    PPCToInternal(0x3),
    PPCToInternal(0x4),
    PPCToInternal(0x5),
    PPCToInternal(0x6),
    PPCToInternal(0x7),
    PPCToInternal(0x8),
    PPCToInternal(0x9),
    PPCToInternal(0xA),
    PPCToInternal(0xB),
    PPCToInternal(0xC),
    PPCToInternal(0xD),
    PPCToInternal(0xE),
    PPCToInternal(0xF),
}};

u32 ConditionRegister::Get() const
{
  u32 new_cr = 0;
  for (u32 i = 0; i < 8; i++)
  {
    new_cr |= GetField(i) << (28 - i * 4);
  }
  return new_cr;
}

void ConditionRegister::Set(const u32 cr)
{
  for (u32 i = 0; i < 8; i++)
  {
    SetField(i, (cr >> (28 - i * 4)) & 0xF);
  }
}

}  // namespace PowerPC
