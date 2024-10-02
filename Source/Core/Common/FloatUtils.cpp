// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/FloatUtils.h"
#include "Core/PowerPC/Gekko.h"

#include <bit>
#include <cmath>

namespace Common
{
u32 ClassifyDouble(double dvalue)
{
  const u64 ivalue = std::bit_cast<u64>(dvalue);
  const u64 sign = ivalue & DOUBLE_SIGN;
  const u64 exp = ivalue & DOUBLE_EXP;

  if (exp > DOUBLE_ZERO && exp < DOUBLE_EXP)
  {
    // Nice normalized number.
    return sign ? PPC_FPCLASS_NN : PPC_FPCLASS_PN;
  }

  const u64 mantissa = ivalue & DOUBLE_FRAC;
  if (mantissa)
  {
    if (exp)
      return PPC_FPCLASS_QNAN;

    // Denormalized number.
    return sign ? PPC_FPCLASS_ND : PPC_FPCLASS_PD;
  }

  if (exp)
  {
    // Infinite
    return sign ? PPC_FPCLASS_NINF : PPC_FPCLASS_PINF;
  }

  // Zero
  return sign ? PPC_FPCLASS_NZ : PPC_FPCLASS_PZ;
}

u32 ClassifyFloat(float fvalue)
{
  const u32 ivalue = std::bit_cast<u32>(fvalue);
  const u32 sign = ivalue & FLOAT_SIGN;
  const u32 exp = ivalue & FLOAT_EXP;

  if (exp > FLOAT_ZERO && exp < FLOAT_EXP)
  {
    // Nice normalized number.
    return sign ? PPC_FPCLASS_NN : PPC_FPCLASS_PN;
  }

  const u32 mantissa = ivalue & FLOAT_FRAC;
  if (mantissa)
  {
    if (exp)
      return PPC_FPCLASS_QNAN;  // Quiet NAN

    // Denormalized number.
    return sign ? PPC_FPCLASS_ND : PPC_FPCLASS_PD;
  }

  if (exp)
  {
    // Infinite
    return sign ? PPC_FPCLASS_NINF : PPC_FPCLASS_PINF;
  }

  // Zero
  return sign ? PPC_FPCLASS_NZ : PPC_FPCLASS_PZ;
}

const std::array<BaseAndDec, 32> frsqrte_expected = {{
    {0x1a7e800, -0x568}, {0x17cb800, -0x4f3}, {0x1552800, -0x48d}, {0x130c000, -0x435},
    {0x10f2000, -0x3e7}, {0x0eff000, -0x3a2}, {0x0d2e000, -0x365}, {0x0b7c000, -0x32e},
    {0x09e5000, -0x2fc}, {0x0867000, -0x2d0}, {0x06ff000, -0x2a8}, {0x05ab800, -0x283},
    {0x046a000, -0x261}, {0x0339800, -0x243}, {0x0218800, -0x226}, {0x0105800, -0x20b},
    {0x3ffa000, -0x7a4}, {0x3c29000, -0x700}, {0x38aa000, -0x670}, {0x3572000, -0x5f2},
    {0x3279000, -0x584}, {0x2fb7000, -0x524}, {0x2d26000, -0x4cc}, {0x2ac0000, -0x47e},
    {0x2881000, -0x43a}, {0x2665000, -0x3fa}, {0x2468000, -0x3c2}, {0x2287000, -0x38e},
    {0x20c1000, -0x35e}, {0x1f12000, -0x332}, {0x1d79000, -0x30a}, {0x1bf4000, -0x2e6},
}};

double ApproximateReciprocalSquareRoot(double val)
{
  s64 integral = std::bit_cast<s64>(val);
  s64 mantissa = integral & ((1LL << 52) - 1);
  const s64 sign = integral & (1ULL << 63);
  s64 exponent = integral & (0x7FFLL << 52);

  // Special case 0
  if (mantissa == 0 && exponent == 0)
  {
    return sign ? -std::numeric_limits<double>::infinity() :
                  std::numeric_limits<double>::infinity();
  }

  // Special case NaN-ish numbers
  if (exponent == DOUBLE_EXP)
  {
    if (mantissa == 0)
    {
      if (sign)
        return std::numeric_limits<double>::quiet_NaN();

      return 0.0;
    }

    return 0.0 + val;
  }

  // Negative numbers return NaN
  if (sign)
    return std::numeric_limits<double>::quiet_NaN();

  if (!exponent)
  {
    // "Normalize" denormal values
    do
    {
      exponent -= 1LL << 52;
      mantissa <<= 1;
    } while (!(mantissa & (1LL << 52)));
    mantissa &= DOUBLE_FRAC;
    exponent += 1LL << 52;
  }

  const s64 exponent_lsb = exponent & (1LL << 52);
  exponent = ((0x3FFLL << 52) - ((exponent - (0x3FELL << 52)) / 2)) & (0x7FFLL << 52);
  integral = sign | exponent;

  const int i = static_cast<int>((exponent_lsb | mantissa) >> 37);
  const auto& entry = frsqrte_expected[i / 2048];
  integral |= static_cast<s64>(entry.m_base + entry.m_dec * (i % 2048)) << 26;

  return std::bit_cast<double>(integral);
}

const std::array<BaseAndDec, 32> fres_expected = {{
    {0xfff000, -0x3e1}, {0xf07000, -0x3a7}, {0xe1d400, -0x371}, {0xd41000, -0x340},
    {0xc71000, -0x313}, {0xbac400, -0x2ea}, {0xaf2000, -0x2c4}, {0xa41000, -0x2a0},
    {0x999000, -0x27f}, {0x8f9400, -0x261}, {0x861000, -0x245}, {0x7d0000, -0x22a},
    {0x745800, -0x212}, {0x6c1000, -0x1fb}, {0x642800, -0x1e5}, {0x5c9400, -0x1d1},
    {0x555000, -0x1be}, {0x4e5800, -0x1ac}, {0x47ac00, -0x19b}, {0x413c00, -0x18b},
    {0x3b1000, -0x17c}, {0x352000, -0x16e}, {0x2f5c00, -0x15b}, {0x29f000, -0x15b},
    {0x248800, -0x143}, {0x1f7c00, -0x143}, {0x1a7000, -0x12d}, {0x15bc00, -0x12d},
    {0x110800, -0x11a}, {0x0ca000, -0x11a}, {0x083800, -0x108}, {0x041800, -0x106},
}};

// Used by fres and ps_res.
double ApproximateReciprocal(const UReg_FPSCR& fpscr, double val)
{
  u64 integral = std::bit_cast<u64>(val);

  // Convert into a float when possible
  u64 signless = integral & ~DOUBLE_SIGN;
  const u32 mantissa =
      static_cast<u32>((integral & DOUBLE_FRAC) >> (DOUBLE_FRAC_WIDTH - FLOAT_FRAC_WIDTH));
  const u32 sign = static_cast<u32>((integral >> 32) & FLOAT_SIGN);
  s32 exponent = static_cast<s32>((integral & DOUBLE_EXP) >> DOUBLE_FRAC_WIDTH) - 0x380;

  // The largest floats possible just return 0
  const u64 huge_float = fpscr.NI ? 0x47d0000000000000ULL : 0x4940000000000000ULL;

  // Special case 0
  if (signless == 0)
    return std::copysign(std::numeric_limits<double>::infinity(), val);

  // Special case huge or NaN-ish numbers
  if (signless >= huge_float)
  {
    if (!std::isnan(val))
      return std::copysign(0.0, val);
    return 0.0 + val;
  }

  // Special case small inputs
  if (exponent < -1)
    return std::copysign(std::numeric_limits<float>::max(), val);

  exponent = 253 - exponent;

  const u32 i = static_cast<u32>(mantissa >> 8);
  const auto& entry = fres_expected[i / 1024];
  const u32 new_mantissa = static_cast<u32>(entry.m_base + entry.m_dec * (i % 1024)) / 2;

  u32 result = sign | (static_cast<u32>(exponent) << FLOAT_FRAC_WIDTH) | new_mantissa;
  if (exponent <= 0)
  {
    // Result is subnormal so format it properly!
    if (fpscr.NI)
    {
      // Flush to 0 if inexact
      result = sign;
    }
    else
    {
      // Shift by the exponent amount
      u32 shift = 1 + static_cast<u32>(-exponent);
      result = sign | (((1 << FLOAT_FRAC_WIDTH) | new_mantissa) >> shift);
    }
  }
  return static_cast<double>(std::bit_cast<float>(result));
}

}  // namespace Common
