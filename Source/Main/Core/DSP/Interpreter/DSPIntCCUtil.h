// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
//
// Additional copyrights go to Duddie and Tratax (c) 2004

#pragma once

// Anything to do with SR and conditions goes here.

#include "Common/CommonTypes.h"

namespace DSP
{
namespace Interpreter
{
bool CheckCondition(u8 _Condition);

void Update_SR_Register16(s16 _Value, bool carry = false, bool overflow = false,
                          bool overS32 = false);
void Update_SR_Register64(s64 _Value, bool carry = false, bool overflow = false);
void Update_SR_LZ(bool value);

inline bool isCarry(u64 val, u64 result)
{
  return (val > result);
}

inline bool isCarry2(u64 val, u64 result)
{
  return (val >= result);
}

inline bool isOverflow(s64 val1, s64 val2, s64 res)
{
  return ((val1 ^ res) & (val2 ^ res)) < 0;
}

inline bool isOverS32(s64 acc)
{
  return (acc != (s32)acc) ? true : false;
}

}  // namespace Interpreter
}  // namespace DSP
