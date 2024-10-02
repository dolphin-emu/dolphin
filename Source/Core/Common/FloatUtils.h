// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <bit>
#include <limits>

#include "Common/CommonTypes.h"
#include "Core/PowerPC/Gekko.h"

namespace Common
{
template <typename T>
constexpr T SNANConstant()
{
  return std::numeric_limits<T>::signaling_NaN();
}

// The most significant bit of the fraction is an is-quiet bit on all architectures we care about.
static constexpr u64 DOUBLE_QBIT = 0x0008000000000000ULL;
static constexpr u64 DOUBLE_SIGN = 0x8000000000000000ULL;
static constexpr u64 DOUBLE_EXP = 0x7FF0000000000000ULL;
static constexpr u64 DOUBLE_FRAC = 0x000FFFFFFFFFFFFFULL;
static constexpr u64 DOUBLE_ZERO = 0x0000000000000000ULL;
static constexpr int DOUBLE_EXP_WIDTH = 11;
static constexpr int DOUBLE_FRAC_WIDTH = 52;

static constexpr u32 FLOAT_SIGN = 0x80000000;
static constexpr u32 FLOAT_EXP = 0x7F800000;
static constexpr u32 FLOAT_FRAC = 0x007FFFFF;
static constexpr u32 FLOAT_ZERO = 0x00000000;
static constexpr int FLOAT_EXP_WIDTH = 8;
static constexpr int FLOAT_FRAC_WIDTH = 23;

inline bool IsQNAN(double d)
{
  const u64 i = std::bit_cast<u64>(d);
  return ((i & DOUBLE_EXP) == DOUBLE_EXP) && ((i & DOUBLE_QBIT) == DOUBLE_QBIT);
}

inline bool IsSNAN(double d)
{
  const u64 i = std::bit_cast<u64>(d);
  return ((i & DOUBLE_EXP) == DOUBLE_EXP) && ((i & DOUBLE_FRAC) != DOUBLE_ZERO) &&
         ((i & DOUBLE_QBIT) == DOUBLE_ZERO);
}

inline float FlushToZero(float f)
{
  u32 i = std::bit_cast<u32>(f);
  if ((i & FLOAT_EXP) == 0)
  {
    // Turn into signed zero
    i &= FLOAT_SIGN;
  }
  return std::bit_cast<float>(i);
}

inline double FlushToZero(double d)
{
  u64 i = std::bit_cast<u64>(d);
  if ((i & DOUBLE_EXP) == 0)
  {
    // Turn into signed zero
    i &= DOUBLE_SIGN;
  }
  return std::bit_cast<double>(i);
}

enum PPCFpClass
{
  PPC_FPCLASS_QNAN = 0x11,
  PPC_FPCLASS_NINF = 0x9,
  PPC_FPCLASS_NN = 0x8,
  PPC_FPCLASS_ND = 0x18,
  PPC_FPCLASS_NZ = 0x12,
  PPC_FPCLASS_PZ = 0x2,
  PPC_FPCLASS_PD = 0x14,
  PPC_FPCLASS_PN = 0x4,
  PPC_FPCLASS_PINF = 0x5,
};

// Uses PowerPC conventions for the return value, so it can be easily
// used directly in CPU emulation.
u32 ClassifyDouble(double dvalue);
u32 ClassifyFloat(float fvalue);

struct BaseAndDec
{
  int m_base;
  int m_dec;
};
extern const std::array<BaseAndDec, 32> frsqrte_expected;
extern const std::array<BaseAndDec, 32> fres_expected;

// PowerPC approximation algorithms
double ApproximateReciprocalSquareRoot(double val);
double ApproximateReciprocal(const UReg_FPSCR& fpscr, double val);

// Instructions which move data without performing operations round a bit weirdly
// Specifically they rounding the mantissa to be like that of a 32-bit float,
// going as far as to focus on the rounding mode, but never actually care about
// making sure the exponent becomes 32-bit
// Either this, or they'll truncate the mantissa down, which will always happen to
// PS1 OR PS0 in ps_rsqrte
inline u64 TruncateMantissaBits(u64 bits)
{
  // Truncation can be done by simply cutting off the mantissa bits that don't
  // exist in a single precision float
  constexpr u64 remove_bits = Common::DOUBLE_FRAC_WIDTH - Common::FLOAT_FRAC_WIDTH;
  constexpr u64 remove_mask = (1 << remove_bits) - 1;
  return bits & ~remove_mask;
}

inline double TruncateMantissa(double value)
{
  u64 bits = std::bit_cast<u64>(value);
  u64 trunc_bits = TruncateMantissaBits(bits);
  return std::bit_cast<double>(trunc_bits);
}

inline u64 RoundMantissaBitsFinite(u64 bits)
{
  const u64 replacement_exp = 0x4000000000000000ull;

  // To round only the mantissa, we assume the CPU can change the rounding mode,
  // create new double with an exponent that won't cause issues, round to a single,
  // and convert back to a double while restoring the original exponent again!
  // The removing the exponent is done via subtraction instead of bitwise
  // operations due to the possibility that the rounding will cause an overflow
  // into the exponent
  u64 resized_bits = (bits & (Common::DOUBLE_FRAC | Common::DOUBLE_SIGN)) | replacement_exp;

  float rounded_float = static_cast<float>(std::bit_cast<double>(resized_bits));
  double extended_float = static_cast<double>(rounded_float);
  u64 rounded_bits = std::bit_cast<u64>(extended_float);

  u64 orig_exp_bits = bits & Common::DOUBLE_EXP;

  if (orig_exp_bits == 0)
  {
    // The exponent isn't incremented for double subnormals
    return rounded_bits & ~Common::DOUBLE_EXP;
  }

  // Handle the change accordingly otherwise!
  rounded_bits = (rounded_bits - replacement_exp) + orig_exp_bits;
  return rounded_bits;
}

inline u64 RoundMantissaBits(u64 bits)
{
  // Checking if the value is non-finite
  if ((bits & Common::DOUBLE_EXP) == Common::DOUBLE_EXP)
  {
    // For infinite and NaN values, the mantissa is simply truncated
    return TruncateMantissaBits(bits);
  }

  return RoundMantissaBitsFinite(bits);
}

inline double RoundMantissaFinite(double value)
{
  // This function is only ever used by ps_sum1, because
  // for some reason it assumes that ps0 should be rounded with
  // finite values rather than checking if they might be infinite
  u64 bits = std::bit_cast<u64>(value);
  u64 rounded_bits = RoundMantissaBitsFinite(bits);
  return std::bit_cast<double>(rounded_bits);
}

inline double RoundMantissa(double value)
{
  // The double version of the function just converts to and from bits again
  // This would be a necessary step anyways, so it just simplifies code
  u64 bits = std::bit_cast<u64>(value);
  u64 rounded_bits = RoundMantissaBits(bits);
  return std::bit_cast<double>(rounded_bits);
}

}  // namespace Common
