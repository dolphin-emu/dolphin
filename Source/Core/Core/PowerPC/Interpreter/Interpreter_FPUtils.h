// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <bit>
#include <cmath>
#include <limits>

#include "Common/CPUDetect.h"
#include "Common/CommonTypes.h"
#include "Common/FloatUtils.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/Interpreter/ExceptionUtils.h"
#include "Core/PowerPC/PowerPC.h"

constexpr double PPC_NAN = std::numeric_limits<double>::quiet_NaN();

// the 4 less-significand bits in FPSCR[FPRF]
enum class FPCC
{
  FL = 8,  // <
  FG = 4,  // >
  FE = 2,  // =
  FU = 1,  // ?
};

inline void CheckFPExceptions(PowerPC::PowerPCState& ppc_state)
{
  if (ppc_state.fpscr.FEX && (ppc_state.msr.FE0 || ppc_state.msr.FE1))
    GenerateProgramException(ppc_state, ProgramExceptionCause::FloatingPoint);
}

inline void UpdateFPExceptionSummary(PowerPC::PowerPCState& ppc_state)
{
  ppc_state.fpscr.VX = (ppc_state.fpscr.Hex & FPSCR_VX_ANY) != 0;
  ppc_state.fpscr.FEX = ((ppc_state.fpscr.Hex >> 22) & (ppc_state.fpscr.Hex & FPSCR_ANY_E)) != 0;

  CheckFPExceptions(ppc_state);
}

inline void SetFPException(PowerPC::PowerPCState& ppc_state, u32 mask)
{
  if ((ppc_state.fpscr.Hex & mask) != mask)
  {
    ppc_state.fpscr.FX = 1;
  }

  ppc_state.fpscr.Hex |= mask;
  UpdateFPExceptionSummary(ppc_state);
}

inline float ForceSingle(const UReg_FPSCR& fpscr, double value)
{
  if (fpscr.NI)
  {
    // Emulate a rounding quirk. If the conversion result before rounding is a subnormal single,
    // it's always flushed to zero, even if rounding would have caused it to become normal.

    constexpr u64 smallest_normal_single = 0x3810000000000000;
    const u64 value_without_sign =
        std::bit_cast<u64>(value) & (Common::DOUBLE_EXP | Common::DOUBLE_FRAC);

    if (value_without_sign < smallest_normal_single)
    {
      const u64 flushed_double = std::bit_cast<u64>(value) & Common::DOUBLE_SIGN;
      const u32 flushed_single = static_cast<u32>(flushed_double >> 32);
      return std::bit_cast<float>(flushed_single);
    }
  }

  // Emulate standard conversion to single precision.

  float x = static_cast<float>(value);
  if (!cpu_info.bFlushToZero && fpscr.NI)
  {
    x = Common::FlushToZero(x);
  }
  return x;
}

inline double ForceDouble(const UReg_FPSCR& fpscr, double d)
{
  if (!cpu_info.bFlushToZero && fpscr.NI)
  {
    d = Common::FlushToZero(d);
  }
  return d;
}

inline double Force25Bit(double d)
{
  u64 integral = std::bit_cast<u64>(d);

  u64 exponent = integral & Common::DOUBLE_EXP;
  u64 fraction = integral & Common::DOUBLE_FRAC;

  if (exponent == 0 && fraction != 0)
  {
    // Subnormals get "normalized" before they're rounded
    // In the end, this practically just means that the rounding is
    // at a different bit

    s64 keep_mask = 0xFFFFFFFFF8000000LL;
    u64 round = 0x8000000;

    // Shift the mask and rounding bit to the right until
    // the fraction is "normal"
    // That is to say shifting it until the MSB of the fraction
    // would escape into the exponent
    u32 shift = std::countl_zero(fraction) - (63 - Common::DOUBLE_FRAC_WIDTH);
    keep_mask >>= shift;
    round >>= shift;

    // Round using these shifted values
    integral = (integral & keep_mask) + (integral & round);
  }
  else
  {
    integral = (integral & 0xFFFFFFFFF8000000ULL) + (integral & 0x8000000);
  }

  return std::bit_cast<double>(integral);
}

inline double MakeQuiet(double d)
{
  const u64 integral = std::bit_cast<u64>(d) | Common::DOUBLE_QBIT;

  return std::bit_cast<double>(integral);
}

// these functions allow globally modify operations behaviour
// also, these may be used to set flags like FR, FI, OX, UX

struct FPResult
{
  bool HasNoInvalidExceptions() const { return (exception & FPSCR_VX_ANY) == 0; }

  void SetException(PowerPC::PowerPCState& ppc_state, FPSCRExceptionFlag flag)
  {
    exception = flag;
    SetFPException(ppc_state, flag);
  }

  double value = 0.0;
  FPSCRExceptionFlag exception{};
};

inline FPResult NI_mul(PowerPC::PowerPCState& ppc_state, double a, double b)
{
  FPResult result{a * b};

  if (std::isnan(result.value))
  {
    if (Common::IsSNAN(a) || Common::IsSNAN(b))
    {
      result.SetException(ppc_state, FPSCR_VXSNAN);
    }

    ppc_state.fpscr.ClearFIFR();

    if (std::isnan(a))
    {
      result.value = MakeQuiet(a);
      return result;
    }
    if (std::isnan(b))
    {
      result.value = MakeQuiet(b);
      return result;
    }

    result.value = PPC_NAN;
    result.SetException(ppc_state, FPSCR_VXIMZ);
    return result;
  }

  return result;
}

inline FPResult NI_div(PowerPC::PowerPCState& ppc_state, double a, double b)
{
  FPResult result{a / b};

  if (std::isinf(result.value))
  {
    if (b == 0.0)
    {
      result.SetException(ppc_state, FPSCR_ZX);
      return result;
    }
  }
  else if (std::isnan(result.value))
  {
    if (Common::IsSNAN(a) || Common::IsSNAN(b))
      result.SetException(ppc_state, FPSCR_VXSNAN);

    ppc_state.fpscr.ClearFIFR();

    if (std::isnan(a))
    {
      result.value = MakeQuiet(a);
      return result;
    }
    if (std::isnan(b))
    {
      result.value = MakeQuiet(b);
      return result;
    }

    if (b == 0.0)
      result.SetException(ppc_state, FPSCR_VXZDZ);
    else if (std::isinf(a) && std::isinf(b))
      result.SetException(ppc_state, FPSCR_VXIDI);

    result.value = PPC_NAN;
    return result;
  }

  return result;
}

inline FPResult NI_add(PowerPC::PowerPCState& ppc_state, double a, double b)
{
  FPResult result{a + b};

  if (std::isnan(result.value))
  {
    if (Common::IsSNAN(a) || Common::IsSNAN(b))
      result.SetException(ppc_state, FPSCR_VXSNAN);

    ppc_state.fpscr.ClearFIFR();

    if (std::isnan(a))
    {
      result.value = MakeQuiet(a);
      return result;
    }
    if (std::isnan(b))
    {
      result.value = MakeQuiet(b);
      return result;
    }

    result.SetException(ppc_state, FPSCR_VXISI);
    result.value = PPC_NAN;
    return result;
  }

  if (std::isinf(a) || std::isinf(b))
    ppc_state.fpscr.ClearFIFR();

  return result;
}

inline FPResult NI_sub(PowerPC::PowerPCState& ppc_state, double a, double b)
{
  FPResult result{a - b};

  if (std::isnan(result.value))
  {
    if (Common::IsSNAN(a) || Common::IsSNAN(b))
      result.SetException(ppc_state, FPSCR_VXSNAN);

    ppc_state.fpscr.ClearFIFR();

    if (std::isnan(a))
    {
      result.value = MakeQuiet(a);
      return result;
    }
    if (std::isnan(b))
    {
      result.value = MakeQuiet(b);
      return result;
    }

    result.SetException(ppc_state, FPSCR_VXISI);
    result.value = PPC_NAN;
    return result;
  }

  if (std::isinf(a) || std::isinf(b))
    ppc_state.fpscr.ClearFIFR();

  return result;
}

// FMA instructions on PowerPC are weird:
// They calculate (a * c) + b, but the order in which
// inputs are checked for NaN is still a, b, c.
inline FPResult NI_madd(PowerPC::PowerPCState& ppc_state, double a, double c, double b)
{
  FPResult result{std::fma(a, c, b)};

  if (std::isnan(result.value))
  {
    if (Common::IsSNAN(a) || Common::IsSNAN(b) || Common::IsSNAN(c))
      result.SetException(ppc_state, FPSCR_VXSNAN);

    ppc_state.fpscr.ClearFIFR();

    if (std::isnan(a))
    {
      result.value = MakeQuiet(a);
      return result;
    }
    if (std::isnan(b))
    {
      result.value = MakeQuiet(b);  // !
      return result;
    }
    if (std::isnan(c))
    {
      result.value = MakeQuiet(c);
      return result;
    }

    result.SetException(ppc_state, std::isnan(a * c) ? FPSCR_VXIMZ : FPSCR_VXISI);
    result.value = PPC_NAN;
    return result;
  }

  if (std::isinf(a) || std::isinf(b) || std::isinf(c))
    ppc_state.fpscr.ClearFIFR();

  return result;
}

inline FPResult NI_msub(PowerPC::PowerPCState& ppc_state, double a, double c, double b)
{
  FPResult result{std::fma(a, c, -b)};

  if (std::isnan(result.value))
  {
    if (Common::IsSNAN(a) || Common::IsSNAN(b) || Common::IsSNAN(c))
      result.SetException(ppc_state, FPSCR_VXSNAN);

    ppc_state.fpscr.ClearFIFR();

    if (std::isnan(a))
    {
      result.value = MakeQuiet(a);
      return result;
    }
    if (std::isnan(b))
    {
      result.value = MakeQuiet(b);  // !
      return result;
    }
    if (std::isnan(c))
    {
      result.value = MakeQuiet(c);
      return result;
    }

    result.SetException(ppc_state, std::isnan(a * c) ? FPSCR_VXIMZ : FPSCR_VXISI);
    result.value = PPC_NAN;
    return result;
  }

  if (std::isinf(a) || std::isinf(b) || std::isinf(c))
    ppc_state.fpscr.ClearFIFR();

  return result;
}

// used by stfsXX instructions and ps_rsqrte
inline u32 ConvertToSingle(u64 x)
{
  const u32 exp = u32((x >> 52) & 0x7ff);

  if (exp > 896 || (x & ~Common::DOUBLE_SIGN) == 0)
  {
    return u32(((x >> 32) & 0xc0000000) | ((x >> 29) & 0x3fffffff));
  }
  else if (exp >= 874)
  {
    u32 t = u32(0x80000000 | ((x & Common::DOUBLE_FRAC) >> 21));
    t = t >> (905 - exp);
    t |= u32((x >> 32) & 0x80000000);
    return t;
  }
  else
  {
    // This is said to be undefined.
    // The code is based on hardware tests.
    return u32(((x >> 32) & 0xc0000000) | ((x >> 29) & 0x3fffffff));
  }
}

// used by psq_stXX operations.
inline u32 ConvertToSingleFTZ(u64 x)
{
  const u32 exp = u32((x >> 52) & 0x7ff);

  if (exp > 896 || (x & ~Common::DOUBLE_SIGN) == 0)
  {
    return u32(((x >> 32) & 0xc0000000) | ((x >> 29) & 0x3fffffff));
  }
  else
  {
    return u32((x >> 32) & 0x80000000);
  }
}

inline u64 ConvertToDouble(u32 value)
{
  // This is a little-endian re-implementation of the algorithm described in
  // the PowerPC Programming Environments Manual for loading single
  // precision floating point numbers.
  // See page 566 of http://www.freescale.com/files/product/doc/MPCFPE32B.pdf

  u64 x = value;
  u64 exp = (x >> 23) & 0xff;
  u64 frac = x & 0x007fffff;

  if (exp > 0 && exp < 255)  // Normal number
  {
    u64 y = !(exp >> 7);
    u64 z = y << 61 | y << 60 | y << 59;
    return ((x & 0xc0000000) << 32) | z | ((x & 0x3fffffff) << 29);
  }
  else if (exp == 0 && frac != 0)  // Subnormal number
  {
    exp = 1023 - 126;
    do
    {
      frac <<= 1;
      exp -= 1;
    } while ((frac & 0x00800000) == 0);

    return ((x & 0x80000000) << 32) | (exp << 52) | ((frac & 0x007fffff) << 29);
  }
  else  // QNaN, SNaN or Zero
  {
    u64 y = exp >> 7;
    u64 z = y << 61 | y << 60 | y << 59;
    return ((x & 0xc0000000) << 32) | z | ((x & 0x3fffffff) << 29);
  }
}
