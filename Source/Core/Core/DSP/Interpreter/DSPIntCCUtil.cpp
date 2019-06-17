// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
//
// Additional copyrights go to Duddie and Tratax (c) 2004

// HELPER FUNCTIONS

#include "Core/DSP/Interpreter/DSPIntCCUtil.h"
#include "Core/DSP/DSPCore.h"

namespace DSP::Interpreter
{
void Update_SR_Register64(s64 _Value, bool carry, bool overflow)
{
  g_dsp.r.sr &= ~SR_CMP_MASK;

  // 0x01
  if (carry)
  {
    g_dsp.r.sr |= SR_CARRY;
  }

  // 0x02 and 0x80
  if (overflow)
  {
    g_dsp.r.sr |= SR_OVERFLOW;
    g_dsp.r.sr |= SR_OVERFLOW_STICKY;
  }

  // 0x04
  if (_Value == 0)
  {
    g_dsp.r.sr |= SR_ARITH_ZERO;
  }

  // 0x08
  if (_Value < 0)
  {
    g_dsp.r.sr |= SR_SIGN;
  }

  // 0x10
  if (_Value != (s32)_Value)
  {
    g_dsp.r.sr |= SR_OVER_S32;
  }

  // 0x20 - Checks if top bits of m are equal
  if (((_Value & 0xc0000000) == 0) || ((_Value & 0xc0000000) == 0xc0000000))
  {
    g_dsp.r.sr |= SR_TOP2BITS;
  }
}

void Update_SR_Register16(s16 _Value, bool carry, bool overflow, bool overS32)
{
  g_dsp.r.sr &= ~SR_CMP_MASK;

  // 0x01
  if (carry)
  {
    g_dsp.r.sr |= SR_CARRY;
  }

  // 0x02 and 0x80
  if (overflow)
  {
    g_dsp.r.sr |= SR_OVERFLOW;
    g_dsp.r.sr |= SR_OVERFLOW_STICKY;
  }

  // 0x04
  if (_Value == 0)
  {
    g_dsp.r.sr |= SR_ARITH_ZERO;
  }

  // 0x08
  if (_Value < 0)
  {
    g_dsp.r.sr |= SR_SIGN;
  }

  // 0x10
  if (overS32)
  {
    g_dsp.r.sr |= SR_OVER_S32;
  }

  // 0x20 - Checks if top bits of m are equal
  if ((((u16)_Value >> 14) == 0) || (((u16)_Value >> 14) == 3))
  {
    g_dsp.r.sr |= SR_TOP2BITS;
  }
}

void Update_SR_LZ(bool value)
{
  if (value == true)
    g_dsp.r.sr |= SR_LOGIC_ZERO;
  else
    g_dsp.r.sr &= ~SR_LOGIC_ZERO;
}

static bool IsCarry()
{
  return (g_dsp.r.sr & SR_CARRY) != 0;
}

static bool IsOverflow()
{
  return (g_dsp.r.sr & SR_OVERFLOW) != 0;
}

static bool IsOverS32()
{
  return (g_dsp.r.sr & SR_OVER_S32) != 0;
}

static bool IsLess()
{
  return (!(g_dsp.r.sr & SR_OVERFLOW) != !(g_dsp.r.sr & SR_SIGN));
}

static bool IsZero()
{
  return (g_dsp.r.sr & SR_ARITH_ZERO) != 0;
}

static bool IsLogicZero()
{
  return (g_dsp.r.sr & SR_LOGIC_ZERO) != 0;
}

static bool IsConditionA()
{
  return (((g_dsp.r.sr & SR_OVER_S32) || (g_dsp.r.sr & SR_TOP2BITS)) &&
          !(g_dsp.r.sr & SR_ARITH_ZERO)) != 0;
}

// see DSPCore.h for flags
bool CheckCondition(u8 _Condition)
{
  switch (_Condition & 0xf)
  {
  case 0xf:  // Always true.
    return true;
  case 0x0:  // GE - Greater Equal
    return !IsLess();
  case 0x1:  // L - Less
    return IsLess();
  case 0x2:  // G - Greater
    return !IsLess() && !IsZero();
  case 0x3:  // LE - Less Equal
    return IsLess() || IsZero();
  case 0x4:  // NZ - Not Zero
    return !IsZero();
  case 0x5:  // Z - Zero
    return IsZero();
  case 0x6:  // NC - Not carry
    return !IsCarry();
  case 0x7:  // C - Carry
    return IsCarry();
  case 0x8:  // ? - Not over s32
    return !IsOverS32();
  case 0x9:  // ? - Over s32
    return IsOverS32();
  case 0xa:  // ?
    return IsConditionA();
  case 0xb:  // ?
    return !IsConditionA();
  case 0xc:  // LNZ  - Logic Not Zero
    return !IsLogicZero();
  case 0xd:  // LZ - Logic Zero
    return IsLogicZero();
  case 0xe:  // 0 - Overflow
    return IsOverflow();
  default:
    return true;
  }
}
}  // namespace DSP::Interpreter
