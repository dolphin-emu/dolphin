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

// The bulk work for fm(add/sub)(s)x operations
template <bool sub, bool single>
inline FPResult NI_madd_msub(PowerPC::PowerPCState& ppc_state, double a, double c, double b)
{
  // The FMA instructions on PowerPC are incredibly weird, and the single precision variations are
  // the only ones with the unfortunate side effect of completely accurately emulating them requiring
  // either software floats or the manual checking of the individual float operations performed,
  // down to a precision greater than that of subnormals.
  // The first oddity to be found is that they calculate (a * c) + b, but in the case of NaNs,
  // they're still checked in the order a, b, c.
  // The rest generally come from the single precision variation.
  // 1. The arguments are *not* forced to 32-bit precision in all ways, meaning you can end
  //    up with results that rely on a 64-bit precision mantissa.
  // 2. The only argument which *is* forced to 32-bit precision in a way is frC, and even that
  //    is only forced to have a 32-bit mantissa.
  // 3. fC is forced to have a 32-bit mantissa (rounding in the same way no matter what the
  //    rounding mode is set to), while keeping the exponent at any 64-bit value.
  //    But rather than the highest values rounding to infinity, because PowerPC internally uses
  //    a higher precision exponent, it rounds up to a value normally unreachable with even
  //    double precision floats.
  // 4. CPUs, unsurprisingly, don't tend to support 64-bit float inputs to an operation with a 32-bit
  //    result. One quirk of PowerPC is that instead of just not caring about the 32-bit precision
  //    mantissas, it includes them and *only rounds once* to 32-bit. This means that you can have
  //    double precision inputs that round differently than if you do a double precision FMA then
  //    round the result to 32-bit.
  //    - What makes FMA so special here is that it's the only basic operation which, upon being 
  //      converted to a 64-bit operation then rounded back to a 32-bit result, does *not* give
  //      the same result when rounding to nearest!
  //      Addition, subtraction, multiplication, and division do not have this issue and will give
  //      the correct result for all inputs!
  //    - In fact, rounding to nearest is the *only* rounding mode where it ends up having issues!
  //      The reason being, if the result was to round in one direction, it would
  //      always direct the double precision output in a way such that rounding to single precision
  //      would end up having that same transformation (or it would already be exactly representable
  //      as a single precision value).
  //
  // It is relatively easy to find 32-bit values which do not round properly if one performs
  // f32(fma(f64(a), f64(c), f64(b))), despite the rarity. The requirements can be shown fairly easily as well:
  // - Final Result = sign * (1.fffffffffffffffffffffffddddddddddddddddddddddddddddd * 2^exponent + c * 2^(exponent - 52))
  // What we need is some form of discrepency occurs from rounding twice, such that moving `d` over to be part of `c` (and
  // adjusting the exponent multiplied by the concatenated values)
  // There are a few ways which a discrepency from rounding twice can be caused:
  // 1. Tying down to even because `c` is too small
  //    a. The highest bit of `d` is 1, the rest of the bits of `d` are 0 (this means it ties)
  //    b. The lowest bit of `f` is 0 (this means it ties to even downwards)
  //    c. `c` is positive (nonzero) and does not round `d` upwards
  //    -  This means while a single round would round up, instead this rounds down because of tying to even.
  // 2. Tying up because `d` rounded up
  //    a. The highest bit of `d` is 0, the rest of the bits of `d` are 1
  //    b. The lowest bit of `f` is 1 (this means it ties to even upwards)
  //    c. `c` is positive, and the highest bit of c is 1
  //    - This will cause `d` to round to 100...00, meaning it will tie then round upwards.
  // 3. Tying up to even because `c` is too small
  //    a. The highest bit of `d` is 1, the rest of the bits of `d` are 0 (this means it ties)
  //    b. The lowest bit of `f` is 1 (this means it ties to even downwards)
  //    c. `c` is negative and does not round `d` downwards
  //    -  This is similar to the first one but in reverse, rounding up instead of down.
  // 4. Tying down because `d` rounded down
  //    a. The highest and lowest bits of `d` are 1, the rest of the bits of `d` are 0
  //    b. The lowest bit of `f` is 0 (this means it ties to even upwards)
  //    c. `c` is negative, and the highest bit of c is 1 and at least one other bit of c is nonzero
  //    - The backwards counterpart to case 2, this will cause `d` to round back down to 100..00,
  //      where the tie down will cause it to round down instead of up.
  //
  // The first values found which were shown to definitively cause issues appeared in Mario Strikers Charged, where:
  // a = 0x42480000 (50.0)
  // c = 0xbc88cc38 (-0.01669894158840179443359375)
  // b = 0x1b1c72a0 (0.0000000000000000000001294105489087172032066277841712287344222431784146465361118316650390625)
  // 
  //   1.fffffffffffffffffffffffddddddddddddddddddddddddddddd * 2^exp +/- c
  // -(1.1010101101111110001011110000000000000000000000000000 * 2^-1   -  1.001110001110010101 * 2^-73)
  // This exactly matches case 3 as shown above, so while the result should be 0xbf55bf17, Dolphin was returning 0xbf55bf18!
  // Due to being able to choose any value of `c` easily to counter the value in the multiplication, it's not particularly difficult
  // to make your own examples as well, but of course these happening in practice is going to be absurdly uncommon most of the time.
  // 
  // Currently Dolphin supports:
  // - Correct ordering of NaN checking (for both double and single precision)
  // - Rounding frC up
  // - Rounding only once for single precision inputs (this will be the large, large majority of cases!)
  //   - Currently this is interpreter-only. This can be implemented in the JIT just as easily, though.
  //     Eventually the JITs should hopefully support detecting back to back single-precision operations,
  //     
  // Currently it does not support:
  // - Handling frC overflowing to an unreachable value
  //   - This is simple enough to check for and handle properly, but the likelihood of it occuring is
  //     so low that it's not worth it to check for it for the extremely rare accuracy improvement
  // - Rounding only once for inputs with single precision mantissas but double precision exponents
  //   - This one is also very simple again is not really something that would happen.
  //     It's also the most likely one to occur, as paired singles similarly only round the mantissa,
  //     not the exponent, and there are games which which do in fact utilize this (for example, any
  //     games which use nw4r -- this includes Mario Kart Wii, although no ghosts are known which
  //     desync on Dolphin because of this or even the single precision -> double precision FMA issue)
  // - Rounding only once for inputs with double precision mantissas
  //
  // All of these can be resolved in a software float emulation method, or by using things such as
  // error-free float algorithms, but the nature of both of these lead to incredible speed costs,
  // and with the extreme rarity of anything beyond what's currently handled mattering, the other
  // cases don't seem to have any reason to be implemented as of now.

  FPResult result;
  
  // In double precision, just doing the normal operation will be exact with no issues.
  if (!single)
    result.value = std::fma(a, c, sub ? -b : b);
  else {
    // For the single precision case, all we currently do is rounding frC properly,
    // then check if the single precision case will work,
    // and if it does, we perform a single precision fma instead.
    const double c_round = Force25Bit(c);

    const float a_float = static_cast<float>(a);
    const float b_float = static_cast<float>(b);
    const float c_float = static_cast<float>(c_round);

    if (static_cast<double>(a_float) == a && static_cast<double>(b_float) == b && static_cast<double>(c_float) == c_round)
      result.value = static_cast<double>(std::fma(a_float, c_float, sub ? -b_float : b_float));
    else
      result.value = std::fma(a, c_round, sub ? -b : b);
  }

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

template <bool single>
inline FPResult NI_madd(PowerPC::PowerPCState& ppc_state, double a, double c, double b)
{
  return NI_madd_msub<false, single>(ppc_state, a, c, b);
}

template <bool single>
inline FPResult NI_msub(PowerPC::PowerPCState& ppc_state, double a, double c, double b)
{
  return NI_madd_msub<true, single>(ppc_state, a, c, b);
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
