// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <limits>

#include "Common/BitUtils.h"
#include "Common/CommonTypes.h"

#ifdef _MSC_VER

// MSVC needs a workaround, because its std::numeric_limits<double>::signaling_NaN()
// will use __builtin_nans, which is improperly handled by the compiler and generates
// a bad constant. Here we go back to the version MSVC used before the builtin.
// TODO: Remove this and use numeric_limits directly whenever this bug is fixed.
// See Visual Studio bug # 128935 "std::numeric_limits<float>::signaling_NaN() is broken"

#include <ymath.h>

#endif  // _MSC_VER

namespace Common
{
template <typename T>
constexpr T SNANConstant()
{
  return std::numeric_limits<T>::signaling_NaN();
}

#ifdef _MSC_VER

// See workaround note above.

template <>
constexpr double SNANConstant()
{
  return (_CSTD _Snan._Double);
}
template <>
constexpr float SNANConstant()
{
  return (_CSTD _Snan._Float);
}

#endif  // _MSC_VER

// The most significant bit of the fraction is an is-quiet bit on all architectures we care about.
enum : u64
{
  DOUBLE_SIGN = 0x8000000000000000ULL,
  DOUBLE_EXP = 0x7FF0000000000000ULL,
  DOUBLE_FRAC = 0x000FFFFFFFFFFFFFULL,
  DOUBLE_ZERO = 0x0000000000000000ULL,
  DOUBLE_QBIT = 0x0008000000000000ULL
};

enum : u32
{
  FLOAT_SIGN = 0x80000000,
  FLOAT_EXP = 0x7F800000,
  FLOAT_FRAC = 0x007FFFFF,
  FLOAT_ZERO = 0x00000000
};

inline bool IsQNAN(double d)
{
  const u64 i = BitCast<u64>(d);
  return ((i & DOUBLE_EXP) == DOUBLE_EXP) && ((i & DOUBLE_QBIT) == DOUBLE_QBIT);
}

inline bool IsSNAN(double d)
{
  const u64 i = BitCast<u64>(d);
  return ((i & DOUBLE_EXP) == DOUBLE_EXP) && ((i & DOUBLE_FRAC) != DOUBLE_ZERO) &&
         ((i & DOUBLE_QBIT) == DOUBLE_ZERO);
}

inline float FlushToZero(float f)
{
  u32 i = BitCast<u32>(f);
  if ((i & FLOAT_EXP) == 0)
  {
    // Turn into signed zero
    i &= FLOAT_SIGN;
  }
  return BitCast<float>(i);
}

inline double FlushToZero(double d)
{
  u64 i = BitCast<u64>(d);
  if ((i & DOUBLE_EXP) == 0)
  {
    // Turn into signed zero
    i &= DOUBLE_SIGN;
  }
  return BitCast<double>(i);
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
// More efficient float version.
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
double ApproximateReciprocal(double val);

}  // namespace Common
