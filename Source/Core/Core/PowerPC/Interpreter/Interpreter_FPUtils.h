// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cmath>
#include <limits>

#include "Common/BitUtils.h"
#include "Common/CPUDetect.h"
#include "Common/CommonTypes.h"
#include "Common/FloatUtils.h"
#include "Core/PowerPC/Gekko.h"
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

inline void SetFPException(UReg_FPSCR* fpscr, u32 mask)
{
  if ((fpscr->Hex & mask) != mask)
  {
    fpscr->FX = 1;
  }

  fpscr->Hex |= mask;
  fpscr->VX = (fpscr->Hex & FPSCR_VX_ANY) != 0;
}

inline double ForceSingle(const UReg_FPSCR& fpscr, double value)
{
  // convert to float...
  float x = (float)value;
  if (!cpu_info.bFlushToZero && fpscr.NI)
  {
    x = Common::FlushToZero(x);
  }
  // ...and back to double:
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
  u64 integral = Common::BitCast<u64>(d);

  integral = (integral & 0xFFFFFFFFF8000000ULL) + (integral & 0x8000000);

  return Common::BitCast<double>(integral);
}

inline double MakeQuiet(double d)
{
  const u64 integral = Common::BitCast<u64>(d) | Common::DOUBLE_QBIT;

  return Common::BitCast<double>(integral);
}

// these functions allow globally modify operations behaviour
// also, these may be used to set flags like FR, FI, OX, UX

struct FPResult
{
  bool HasNoInvalidExceptions() const { return (exception & FPSCR_VX_ANY) == 0; }

  void SetException(UReg_FPSCR* fpscr, FPSCRExceptionFlag flag)
  {
    exception = flag;
    SetFPException(fpscr, flag);
  }

  double value = 0.0;
  FPSCRExceptionFlag exception{};
};

inline FPResult NI_mul(UReg_FPSCR* fpscr, double a, double b)
{
  FPResult result{a * b};

  if (std::isnan(result.value))
  {
    if (Common::IsSNAN(a) || Common::IsSNAN(b))
    {
      result.SetException(fpscr, FPSCR_VXSNAN);
    }

    fpscr->ClearFIFR();

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
    result.SetException(fpscr, FPSCR_VXIMZ);
    return result;
  }

  return result;
}

inline FPResult NI_div(UReg_FPSCR* fpscr, double a, double b)
{
  FPResult result{a / b};

  if (std::isnan(result.value))
  {
    if (Common::IsSNAN(a) || Common::IsSNAN(b))
      result.SetException(fpscr, FPSCR_VXSNAN);

    fpscr->ClearFIFR();

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
    {
      if (a == 0.0)
      {
        result.SetException(fpscr, FPSCR_VXZDZ);
      }
      else
      {
        result.SetException(fpscr, FPSCR_ZX);
      }
    }
    else if (std::isinf(a) && std::isinf(b))
    {
      result.SetException(fpscr, FPSCR_VXIDI);
    }

    result.value = PPC_NAN;
    return result;
  }

  return result;
}

inline FPResult NI_add(UReg_FPSCR* fpscr, double a, double b)
{
  FPResult result{a + b};

  if (std::isnan(result.value))
  {
    if (Common::IsSNAN(a) || Common::IsSNAN(b))
      result.SetException(fpscr, FPSCR_VXSNAN);

    fpscr->ClearFIFR();

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

    result.SetException(fpscr, FPSCR_VXISI);
    result.value = PPC_NAN;
    return result;
  }

  if (std::isinf(a) || std::isinf(b))
    fpscr->ClearFIFR();

  return result;
}

inline FPResult NI_sub(UReg_FPSCR* fpscr, double a, double b)
{
  FPResult result{a - b};

  if (std::isnan(result.value))
  {
    if (Common::IsSNAN(a) || Common::IsSNAN(b))
      result.SetException(fpscr, FPSCR_VXSNAN);

    fpscr->ClearFIFR();

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

    result.SetException(fpscr, FPSCR_VXISI);
    result.value = PPC_NAN;
    return result;
  }

  if (std::isinf(a) || std::isinf(b))
    fpscr->ClearFIFR();

  return result;
}

// FMA instructions on PowerPC are weird:
// They calculate (a * c) + b, but the order in which
// inputs are checked for NaN is still a, b, c.
inline FPResult NI_madd(UReg_FPSCR* fpscr, double a, double c, double b)
{
  FPResult result{a * c};

  if (std::isnan(result.value))
  {
    if (Common::IsSNAN(a) || Common::IsSNAN(b) || Common::IsSNAN(c))
      result.SetException(fpscr, FPSCR_VXSNAN);

    fpscr->ClearFIFR();

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

    result.SetException(fpscr, FPSCR_VXIMZ);
    result.value = PPC_NAN;
    return result;
  }

  result.value += b;

  if (std::isnan(result.value))
  {
    if (Common::IsSNAN(b))
      result.SetException(fpscr, FPSCR_VXSNAN);

    fpscr->ClearFIFR();

    if (std::isnan(b))
    {
      result.value = MakeQuiet(b);
      return result;
    }

    result.SetException(fpscr, FPSCR_VXISI);
    result.value = PPC_NAN;
    return result;
  }

  if (std::isinf(a) || std::isinf(b) || std::isinf(c))
    fpscr->ClearFIFR();

  return result;
}

inline FPResult NI_msub(UReg_FPSCR* fpscr, double a, double c, double b)
{
  FPResult result{a * c};

  if (std::isnan(result.value))
  {
    if (Common::IsSNAN(a) || Common::IsSNAN(b) || Common::IsSNAN(c))
      result.SetException(fpscr, FPSCR_VXSNAN);

    fpscr->ClearFIFR();

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

    result.SetException(fpscr, FPSCR_VXIMZ);
    result.value = PPC_NAN;
    return result;
  }

  result.value -= b;

  if (std::isnan(result.value))
  {
    if (Common::IsSNAN(b))
      result.SetException(fpscr, FPSCR_VXSNAN);

    fpscr->ClearFIFR();

    if (std::isnan(b))
    {
      result.value = MakeQuiet(b);
      return result;
    }

    result.SetException(fpscr, FPSCR_VXISI);
    result.value = PPC_NAN;
    return result;
  }

  if (std::isinf(a) || std::isinf(b) || std::isinf(c))
    fpscr->ClearFIFR();

  return result;
}

// used by stfsXX instructions and ps_rsqrte
inline u32 ConvertToSingle(u64 x)
{
  u32 exp = (x >> 52) & 0x7ff;
  if (exp > 896 || (x & ~Common::DOUBLE_SIGN) == 0)
  {
    return ((x >> 32) & 0xc0000000) | ((x >> 29) & 0x3fffffff);
  }
  else if (exp >= 874)
  {
    u32 t = (u32)(0x80000000 | ((x & Common::DOUBLE_FRAC) >> 21));
    t = t >> (905 - exp);
    t |= (x >> 32) & 0x80000000;
    return t;
  }
  else
  {
    // This is said to be undefined.
    // The code is based on hardware tests.
    return ((x >> 32) & 0xc0000000) | ((x >> 29) & 0x3fffffff);
  }
}

// used by psq_stXX operations.
inline u32 ConvertToSingleFTZ(u64 x)
{
  u32 exp = (x >> 52) & 0x7ff;
  if (exp > 896 || (x & ~Common::DOUBLE_SIGN) == 0)
  {
    return ((x >> 32) & 0xc0000000) | ((x >> 29) & 0x3fffffff);
  }
  else
  {
    return (x >> 32) & 0x80000000;
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
