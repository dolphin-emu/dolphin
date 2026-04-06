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
      result.value = Common::MakeQuiet(a);
      return result;
    }
    if (std::isnan(b))
    {
      result.value = Common::MakeQuiet(b);
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
      result.value = Common::MakeQuiet(a);
      return result;
    }
    if (std::isnan(b))
    {
      result.value = Common::MakeQuiet(b);
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
      result.value = Common::MakeQuiet(a);
      return result;
    }
    if (std::isnan(b))
    {
      result.value = Common::MakeQuiet(b);
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
      result.value = Common::MakeQuiet(a);
      return result;
    }
    if (std::isnan(b))
    {
      result.value = Common::MakeQuiet(b);
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
  // the only ones with the unfortunate side effect of completely accurately emulating them
  // requiring either software floats or the manual checking of the individual float
  // operations performed, down to a precision greater than that of subnormals.
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
  // 4. CPUs, unsurprisingly, don't tend to support 64-bit float inputs to an operation with a
  //    32-bit result.
  //    One quirk of PowerPC is that instead of just not caring about handling the precision
  //    in the registers of the operands of single precision instructions, it instead
  //    takes into account that extra precision and *only rounds once* to 32-bit.
  //    This means that you can have double precision inputs such that the result rounds
  //    differently than if you did a double precision FMA and rounded the result to 32-bit.
  //    - What makes FMA so special here is that it's the only basic operation which, upon being
  //      converted to a 64-bit operation then rounded back to a 32-bit result, does *not* give
  //      the same result when rounding to nearest!
  //      Addition, subtraction, multiplication, and division do not have this issue and will give
  //      the correct result for all inputs!
  //    - In fact, rounding to nearest is the *only* rounding mode where it ends up having issues!
  //      The reason being, if the result was to round in one direction, it would
  //      always direct the double precision output in a way such that rounding to single precision
  //      would end up having that same transformation (or it would already be exactly
  //      representable as a single precision value).
  //
  // It is relatively easy to find 32-bit values which do not round properly if one performs
  // f32(fma(f64(a), f64(c), f64(b))), despite the rarity.
  // The requirements can be shown fairly easily as well:
  // - Final Result = sign * (1.fffffffffffffffffffffffddddddddddddddddddddddddddddd * 2^exponent
  //                          + c * 2^(exponent - 52))
  // What we need is some form of discrepancy which occurs from rounding twice,
  // such that rounding from the perspective `d` just being in front of `c` (like in the actual
  // operation which only rounds once) will give a different result than rounding `d` then
  // rounding again to single precision.
  // There are a few ways which this discrepancy from rounding twice can be caused, with all
  // of them relating to rounding to nearest ties even:
  // 1. Tying down to even because `c` is too small
  //    a. The highest bit of `d` is 1, the rest of the bits of `d` are 0 (this means it ties)
  //    b. The lowest bit of `f` is 0 (this means it ties to even downwards)
  //    c. `c` is positive (nonzero) and does not round `d` upwards
  //    -  This means while a single round would round up,
  //       instead this rounds down because of tying to even.
  // 2. Tying up because `d` rounded up
  //    a. The highest bit of `d` is 0, the rest of the bits of `d` are 1
  //    b. The lowest bit of `f` is 1 (this means it ties to even upwards)
  //    c. `c` is positive, and the highest bit of c is 1
  //    - This will cause `d` to round to 100...00, meaning it will tie then round upwards.
  // 3. Tying up to even because `c` is too small
  //    a. The highest bit of `d` is 1, the rest of the bits of `d` are 0 (this means it ties)
  //    b. The lowest bit of `f` is 1 (this means it ties to even upwards)
  //    c. `c` is negative and does not round `d` downwards
  //    -  This is similar to the first one but in reverse, rounding up instead of down.
  // 4. Tying down because `d` rounded down
  //    a. The highest and lowest bits of `d` are 1, the rest of the bits of `d` are 0
  //    b. The lowest bit of `f` is 0 (this means it ties to even downwards)
  //    c. `c` is negative, and the highest bit of c is 1,
  //       and at least one other bit of c is nonzero
  //    - The backwards counterpart to case 2, this will cause `d` to round back down to 100..00,
  //      where the tie down will cause it to round down instead of up.
  //
  // The first values found which were shown to definitively cause issues appeared
  // in Mario Strikers Charged, where:
  // a = 0x42480000 (50.0)
  // c = 0xbc88cc38 (-0.01669894158840179443359375)
  // b = 0x1b1c72a0
  //     (1.294105489087172032066277841712287344222431784146465361118316650390625 * 10^-22)
  //
  // Performing the FMADDS we get:
  //            1.fffffffffffffffffffffffddddddddddddddddddddddddddddd * 2^exp
  //              +/- c
  // Result = -(1.1010101101111110001011110000000000000000000000000000 * 2^-1
  //               -  1.001110001110010101 * 2^-73)
  // This exactly matches case 3 as shown above, so while the result should be 0xbf55bf17,
  // Dolphin was returning 0xbf55bf18!
  // Due to being able to choose any value of `c` easily to counter the value of `d`,
  // it's not particularly difficult to make your own examples as well,
  // but of course these happening in practice is going to be absurdly uncommon most of the time.
  //
  // Currently Dolphin supports:
  // - Correct ordering of NaN checking (for both double and single precision)
  // - Rounding frC up
  // - Rounding only once for single precision inputs (this will be the large majority of cases!)
  // - Rounding only once for double precision inputs
  //   - This is a side effect of how we handle single-precision inputs: By doing
  //     error calculations rather than checking if every input is a float, we ensure that we know
  //     at the very least the rounding direction that was taken and that would need to be taken.
  //
  // Currently it does not support:
  // - Handling frC overflowing to an unreachable value
  //   - This is simple enough to check for and handle properly, but the likelihood of it occurring
  //     is so low that it's not worth it to check for it for the rare accuracy improvement.
  // - Dealing with every 64-bit subnormal possibility correctly
  //   - Double precision subnormals are what cause requiring more precision than double precision
  //     to be a thing if you want a correct implementation for every possible inputs.
  //     If a case where this was necessary came up it'd just be more worth it to fall back to
  //     a software floating point implementation instead.
  //
  // All of these can be resolved in a software float emulation method, or by using things such as
  // error-free float algorithms, but the nature of both of these lead to incredible speed costs,
  // and with the extreme rarity of anything beyond what's currently handled mattering, the other
  // cases don't seem to have any reason to be implemented as of now.

  FPResult result;

  // In double precision, just doing the normal operation will be exact with no issues.
  if (!single)
  {
    result.value = std::fma(a, c, sub ? -b : b);
  }
  else
  {
    // For single precision inputs, we never actually cast to a float -- we instead compute the
    // result using a 64-bit FMA, and if the bits end up being an even tie when converting to
    // a float, we approximate (for single-precision-only inputs this will be exact) the
    // amount rounded by the FMA, and use that to manually fix which direction we round!
    // We of course still properly round `c` first, though.
    const double c_round = Force25Bit(c);

    // First, we compute the 64-bit FMA forwards
    const double b_sign = sub ? -b : b;
    result.value = std::fma(a, c_round, b_sign);

    // We then check if we're currently tying in rounding direction
    const u64 result_bits = std::bit_cast<u64>(result.value);

    // The mask of the `d` bits as shown in the above comments
    const u64 D_MASK = 0x000000001fffffff;
    // The mask of `d` which would force a tie to even, which is the only case where there
    // can be potentially be differences compared to just casting to an f32 directly.
    const u64 EVEN_TIE = 0x0000000010000000;

    // Because we check this entire mask which includes a 1 bit, we can be sure that
    // if this result passes, the input is not an infinity that would become a NaN.
    // If we had only checked for a subset of these bits (e.g. only checking if the last
    // one was 0), we would have needed to also check if the exponent was all ones.
    if ((result_bits & D_MASK) == EVEN_TIE)
    {
      // Because we have a tie, we now compute any error in the FMA calculation
      // via an error-free transformation (Ole Møller's 2Sum algorithm)
      // s  := a  + b
      // a' := s  - b
      // b' := s  - a'
      // da := a  - a'
      // db := b  - b'
      // t  := da + db
      // But for these calculations, we assume "a" := a * c_round, allowing the usage of FMA,
      // both being shorter and allowing for likely necessary increased precision!
      // We also switch up the signs a bit so we don't introduce an instruction simply to
      // negate one of the operands of an FMA
      const double a_prime = b_sign - result.value;
      const double b_prime = result.value + a_prime;
      const double delta_a = std::fma(a, c_round, a_prime);
      const double delta_b = b_sign - b_prime;
      const double error = delta_a + delta_b;

      // `error` will properly match the direction for rounding *even for 64-bit inputs*.
      // Thoroughly proving that this works for even all normal values isn't entirely trivial,
      // nor are the exact details really important, but the basic logic is:
      // result.value = roundf64(a * c_round + b_sign) = a * c_round + b_sign - e0
      // a_prime = roundf64(b_sign - a * c_round - b_sign + e0)
      //         = -a * c_round + e0 - e1
      // b_prime = roundf64(a * c_round + b_sign - e0 - a * c_round + e0 - e1)
      //         = b_sign - e1 - e2
      // delta_a = roundf64(a * c_round - a * c_round + e0 - e1)
      //         = e0 - e1 - e3
      // delta_b = roundf64(b_sign - b_sign + e1 + e2)
      //         = e1 + e2 - e4
      // error   = roundf64(delta_a + delta_b)
      //         = roundf64(e0 + e2 - e3 - e4)
      // Then showing that e2 - e3 - e4 is tiny enough to not change the sign of
      // e0 (the true error value, as `error` can't capture all of the possible precision),
      // including that if the true e0 = 0 then e1 = e2 = e3 = e4 = error = 0.

      // This "error" value represents the number such that `result.value - error == exact_result`.
      if (error != 0.0)
      {
        // Because the error is nonzero here, we actually do need to round a specific direction
        // and don't want to just tie to even!

        // Note that it should never be possible for the error to be NaN if the result isn't either
        // infinite or NaN itself. It would require:
        // da == inf, db == -inf
        // Which expanded out is:
        // a - ((a + b) - b) == inf, b - ((a + b) - ((a + b) - b)) == -inf, where
        // a + b isn't infinite. This means (a + b) - b must be infinite on the left,
        // but this will end up giving the right hand side the same sign of infinity.
        // All this to say we don't check for `if (!std::isnan(error))` for the `else` statement.
        // Also note that we do not cast to a float here,
        // as individual instructions using this function will on their own afterwards.

        if ((error > 0.0) == (result.value > 0.0))
          result.value = std::bit_cast<double>(result_bits + 1);  // Tie is too small, round up.
        else
          result.value = std::bit_cast<double>(result_bits - 1);  // Tie is too large, round down.
      }
    }
  }

  if (std::isnan(result.value))
  {
    if (Common::IsSNAN(a) || Common::IsSNAN(b) || Common::IsSNAN(c))
      result.SetException(ppc_state, FPSCR_VXSNAN);

    ppc_state.fpscr.ClearFIFR();

    if (std::isnan(a))
    {
      result.value = Common::MakeQuiet(a);
      return result;
    }
    if (std::isnan(b))
    {
      result.value = Common::MakeQuiet(b);  // !
      return result;
    }
    if (std::isnan(c))
    {
      result.value = Common::MakeQuiet(c);
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
