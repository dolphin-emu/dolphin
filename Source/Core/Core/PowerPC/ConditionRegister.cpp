// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/PowerPC/ConditionRegister.h"

namespace PowerPC
{
const std::array<u64, 16> ConditionRegister::s_crTable = {{
    ConditionRegister::PPCToInternal(0x0),
    ConditionRegister::PPCToInternal(0x1),
    ConditionRegister::PPCToInternal(0x2),
    ConditionRegister::PPCToInternal(0x3),
    ConditionRegister::PPCToInternal(0x4),
    ConditionRegister::PPCToInternal(0x5),
    ConditionRegister::PPCToInternal(0x6),
    ConditionRegister::PPCToInternal(0x7),
    ConditionRegister::PPCToInternal(0x8),
    ConditionRegister::PPCToInternal(0x9),
    ConditionRegister::PPCToInternal(0xA),
    ConditionRegister::PPCToInternal(0xB),
    ConditionRegister::PPCToInternal(0xC),
    ConditionRegister::PPCToInternal(0xD),
    ConditionRegister::PPCToInternal(0xE),
    ConditionRegister::PPCToInternal(0xF),
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

void ConditionRegister::Set(u32 cr)
{
  for (u32 i = 0; i < 8; i++)
  {
    SetField(i, (cr >> (28 - i * 4)) & 0xF);
  }
}

}  // namespace PowerPC
