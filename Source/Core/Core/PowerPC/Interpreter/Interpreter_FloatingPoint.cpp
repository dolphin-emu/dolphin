// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cmath>
#include <limits>

#include "Common/CommonTypes.h"
#include "Common/FloatUtils.h"
#include "Core/PowerPC/Interpreter/Interpreter.h"
#include "Core/PowerPC/Interpreter/Interpreter_FPUtils.h"
#include "Core/PowerPC/PowerPC.h"

namespace
{
// Apply current rounding mode
enum class RoundingMode
{
  Nearest = 0b00,
  TowardsZero = 0b01,
  TowardsPositiveInfinity = 0b10,
  TowardsNegativeInfinity = 0b11
};

// Note that the convert to integer operation is defined
// in Appendix C.4.2 in PowerPC Microprocessor Family:
// The Programming Environments Manual for 32 and 64-bit Microprocessors
void ConvertToInteger(UGeckoInstruction inst, RoundingMode rounding_mode)
{
  const double b = rPS0(inst.FB);
  u32 value;
  bool exception_occurred = false;

  if (std::isnan(b))
  {
    if (Common::IsSNAN(b))
      SetFPException(FPSCR_VXSNAN);

    value = 0x80000000;
    SetFPException(FPSCR_VXCVI);
    exception_occurred = true;
  }
  else if (b > static_cast<double>(0x7fffffff))
  {
    // Positive large operand or +inf
    value = 0x7fffffff;
    SetFPException(FPSCR_VXCVI);
    exception_occurred = true;
  }
  else if (b < -static_cast<double>(0x80000000))
  {
    // Negative large operand or -inf
    value = 0x80000000;
    SetFPException(FPSCR_VXCVI);
    exception_occurred = true;
  }
  else
  {
    s32 i = 0;
    switch (rounding_mode)
    {
    case RoundingMode::Nearest:
    {
      const double t = b + 0.5;
      i = static_cast<s32>(t);

      if (t - i < 0 || (t - i == 0 && b > 0))
      {
        i--;
      }
      break;
    }
    case RoundingMode::TowardsZero:
      i = static_cast<s32>(b);
      break;
    case RoundingMode::TowardsPositiveInfinity:
      i = static_cast<s32>(b);
      if (b - i > 0)
      {
        i++;
      }
      break;
    case RoundingMode::TowardsNegativeInfinity:
      i = static_cast<s32>(b);
      if (b - i < 0)
      {
        i--;
      }
      break;
    }
    value = static_cast<u32>(i);
    const double di = i;
    if (di == b)
    {
      FPSCR.ClearFIFR();
    }
    else
    {
      // Also sets FPSCR[XX]
      SetFI(1);
      FPSCR.FR = fabs(di) > fabs(b);
    }
  }

  if (exception_occurred)
  {
    FPSCR.ClearFIFR();
  }

  if (!exception_occurred || FPSCR.VE == 0)
  {
    // Based on HW tests
    // FPRF is not affected
    riPS0(inst.FD) = 0xfff8000000000000ull | value;
    if (value == 0 && std::signbit(b))
      riPS0(inst.FD) |= 0x100000000ull;
  }

  if (inst.Rc)
    Helper_UpdateCR1();
}
}  // Anonymous namespace

void Interpreter::Helper_FloatCompareOrdered(UGeckoInstruction inst, double fa, double fb)
{
  FPCC compare_result;

  if (std::isnan(fa) || std::isnan(fb))
  {
    compare_result = FPCC::FU;
    if (Common::IsSNAN(fa) || Common::IsSNAN(fb))
    {
      SetFPException(FPSCR_VXSNAN);
      if (FPSCR.VE == 0)
      {
        SetFPException(FPSCR_VXVC);
      }
    }
    else  // QNaN
    {
      SetFPException(FPSCR_VXVC);
    }
  }
  else if (fa < fb)
  {
    compare_result = FPCC::FL;
  }
  else if (fa > fb)
  {
    compare_result = FPCC::FG;
  }
  else  // Equals
  {
    compare_result = FPCC::FE;
  }

  const u32 compare_value = static_cast<u32>(compare_result);

  // Clear and set the FPCC bits accordingly.
  FPSCR.FPRF = (FPSCR.FPRF & ~0xF) | compare_value;

  PowerPC::SetCRField(inst.CRFD, compare_value);
}

void Interpreter::Helper_FloatCompareUnordered(UGeckoInstruction inst, double fa, double fb)
{
  FPCC compare_result;

  if (std::isnan(fa) || std::isnan(fb))
  {
    compare_result = FPCC::FU;

    if (Common::IsSNAN(fa) || Common::IsSNAN(fb))
    {
      SetFPException(FPSCR_VXSNAN);
    }
  }
  else if (fa < fb)
  {
    compare_result = FPCC::FL;
  }
  else if (fa > fb)
  {
    compare_result = FPCC::FG;
  }
  else  // Equals
  {
    compare_result = FPCC::FE;
  }

  const u32 compare_value = static_cast<u32>(compare_result);

  // Clear and set the FPCC bits accordingly.
  FPSCR.FPRF = (FPSCR.FPRF & ~0xF) | compare_value;

  PowerPC::SetCRField(inst.CRFD, compare_value);
}

void Interpreter::fcmpo(UGeckoInstruction inst)
{
  Helper_FloatCompareOrdered(inst, rPS0(inst.FA), rPS0(inst.FB));
}

void Interpreter::fcmpu(UGeckoInstruction inst)
{
  Helper_FloatCompareUnordered(inst, rPS0(inst.FA), rPS0(inst.FB));
}

void Interpreter::fctiwx(UGeckoInstruction inst)
{
  ConvertToInteger(inst, static_cast<RoundingMode>(FPSCR.RN));
}

void Interpreter::fctiwzx(UGeckoInstruction inst)
{
  ConvertToInteger(inst, RoundingMode::TowardsZero);
}

void Interpreter::fmrx(UGeckoInstruction inst)
{
  riPS0(inst.FD) = riPS0(inst.FB);

  // This is a binary instruction. Does not alter FPSCR
  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::fabsx(UGeckoInstruction inst)
{
  rPS0(inst.FD) = fabs(rPS0(inst.FB));

  // This is a binary instruction. Does not alter FPSCR
  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::fnabsx(UGeckoInstruction inst)
{
  riPS0(inst.FD) = riPS0(inst.FB) | (1ULL << 63);

  // This is a binary instruction. Does not alter FPSCR
  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::fnegx(UGeckoInstruction inst)
{
  riPS0(inst.FD) = riPS0(inst.FB) ^ (1ULL << 63);

  // This is a binary instruction. Does not alter FPSCR
  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::fselx(UGeckoInstruction inst)
{
  rPS0(inst.FD) = (rPS0(inst.FA) >= -0.0) ? rPS0(inst.FC) : rPS0(inst.FB);

  // This is a binary instruction. Does not alter FPSCR
  if (inst.Rc)
    Helper_UpdateCR1();
}

// !!! warning !!!
// PS1 must be set to the value of PS0 or DragonballZ will be f**ked up
// PS1 is said to be undefined
void Interpreter::frspx(UGeckoInstruction inst)  // round to single
{
  const double b = rPS0(inst.FB);
  const double rounded = ForceSingle(b);

  if (std::isnan(b))
  {
    const bool is_snan = Common::IsSNAN(b);

    if (is_snan)
      SetFPException(FPSCR_VXSNAN);

    if (!is_snan || FPSCR.VE == 0)
    {
      rPS0(inst.FD) = rounded;
      rPS1(inst.FD) = rounded;
      PowerPC::UpdateFPRF(b);
    }

    FPSCR.ClearFIFR();
  }
  else
  {
    SetFI(b != rounded);
    FPSCR.FR = fabs(rounded) > fabs(b);
    PowerPC::UpdateFPRF(rounded);
    rPS0(inst.FD) = rounded;
    rPS1(inst.FD) = rounded;
  }

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::fmulx(UGeckoInstruction inst)
{
  const FPResult product = NI_mul(rPS0(inst.FA), rPS0(inst.FC));

  if (FPSCR.VE == 0 || product.HasNoInvalidExceptions())
  {
    const double result = ForceDouble(product.value);

    rPS0(inst.FD) = result;
    FPSCR.FI = 0;  // are these flags important?
    FPSCR.FR = 0;
    PowerPC::UpdateFPRF(result);
  }

  if (inst.Rc)
    Helper_UpdateCR1();
}
void Interpreter::fmulsx(UGeckoInstruction inst)
{
  const double c_value = Force25Bit(rPS0(inst.FC));
  const FPResult d_value = NI_mul(rPS0(inst.FA), c_value);

  if (FPSCR.VE == 0 || d_value.HasNoInvalidExceptions())
  {
    const double result = ForceSingle(d_value.value);

    rPS0(inst.FD) = rPS1(inst.FD) = result;
    FPSCR.FI = 0;
    FPSCR.FR = 0;
    PowerPC::UpdateFPRF(result);
  }

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::fmaddx(UGeckoInstruction inst)
{
  const FPResult product = NI_madd(rPS0(inst.FA), rPS0(inst.FC), rPS0(inst.FB));

  if (FPSCR.VE == 0 || product.HasNoInvalidExceptions())
  {
    const double result = ForceDouble(product.value);
    rPS0(inst.FD) = result;
    PowerPC::UpdateFPRF(result);
  }

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::fmaddsx(UGeckoInstruction inst)
{
  const double c_value = Force25Bit(rPS0(inst.FC));
  const FPResult d_value = NI_madd(rPS0(inst.FA), c_value, rPS0(inst.FB));

  if (FPSCR.VE == 0 || d_value.HasNoInvalidExceptions())
  {
    const double result = ForceSingle(d_value.value);

    rPS0(inst.FD) = rPS1(inst.FD) = result;
    FPSCR.FI = d_value.value != result;
    FPSCR.FR = 0;
    PowerPC::UpdateFPRF(result);
  }

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::faddx(UGeckoInstruction inst)
{
  const FPResult sum = NI_add(rPS0(inst.FA), rPS0(inst.FB));

  if (FPSCR.VE == 0 || sum.HasNoInvalidExceptions())
  {
    const double result = ForceDouble(sum.value);
    rPS0(inst.FD) = result;
    PowerPC::UpdateFPRF(result);
  }

  if (inst.Rc)
    Helper_UpdateCR1();
}
void Interpreter::faddsx(UGeckoInstruction inst)
{
  const FPResult sum = NI_add(rPS0(inst.FA), rPS0(inst.FB));

  if (FPSCR.VE == 0 || sum.HasNoInvalidExceptions())
  {
    const double result = ForceSingle(sum.value);
    rPS0(inst.FD) = rPS1(inst.FD) = result;
    PowerPC::UpdateFPRF(result);
  }

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::fdivx(UGeckoInstruction inst)
{
  const FPResult quotient = NI_div(rPS0(inst.FA), rPS0(inst.FB));
  const bool not_divide_by_zero = FPSCR.ZE == 0 || quotient.exception != FPSCR_ZX;
  const bool not_invalid = FPSCR.VE == 0 || quotient.HasNoInvalidExceptions();

  if (not_divide_by_zero && not_invalid)
  {
    const double result = ForceDouble(quotient.value);
    rPS0(inst.FD) = result;
    PowerPC::UpdateFPRF(result);
  }

  // FR,FI,OX,UX???
  if (inst.Rc)
    Helper_UpdateCR1();
}
void Interpreter::fdivsx(UGeckoInstruction inst)
{
  const FPResult quotient = NI_div(rPS0(inst.FA), rPS0(inst.FB));
  const bool not_divide_by_zero = FPSCR.ZE == 0 || quotient.exception != FPSCR_ZX;
  const bool not_invalid = FPSCR.VE == 0 || quotient.HasNoInvalidExceptions();

  if (not_divide_by_zero && not_invalid)
  {
    const double result = ForceSingle(quotient.value);
    rPS0(inst.FD) = rPS1(inst.FD) = result;
    PowerPC::UpdateFPRF(result);
  }

  if (inst.Rc)
    Helper_UpdateCR1();
}

// Single precision only.
void Interpreter::fresx(UGeckoInstruction inst)
{
  const double b = rPS0(inst.FB);

  const auto compute_result = [inst](double value) {
    const double result = Common::ApproximateReciprocal(value);
    rPS0(inst.FD) = rPS1(inst.FD) = result;
    PowerPC::UpdateFPRF(result);
  };

  if (b == 0.0)
  {
    SetFPException(FPSCR_ZX);
    FPSCR.ClearFIFR();

    if (FPSCR.ZE == 0)
      compute_result(b);
  }
  else if (Common::IsSNAN(b))
  {
    SetFPException(FPSCR_VXSNAN);
    FPSCR.ClearFIFR();

    if (FPSCR.VE == 0)
      compute_result(b);
  }
  else
  {
    if (std::isnan(b) || std::isinf(b))
      FPSCR.ClearFIFR();

    compute_result(b);
  }

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::frsqrtex(UGeckoInstruction inst)
{
  const double b = rPS0(inst.FB);

  const auto compute_result = [inst](double value) {
    const double result = Common::ApproximateReciprocalSquareRoot(value);
    rPS0(inst.FD) = result;
    PowerPC::UpdateFPRF(result);
  };

  if (b < 0.0)
  {
    SetFPException(FPSCR_VXSQRT);
    FPSCR.ClearFIFR();

    if (FPSCR.VE == 0)
      compute_result(b);
  }
  else if (b == 0.0)
  {
    SetFPException(FPSCR_ZX);
    FPSCR.ClearFIFR();

    if (FPSCR.ZE == 0)
      compute_result(b);
  }
  else if (Common::IsSNAN(b))
  {
    SetFPException(FPSCR_VXSNAN);
    FPSCR.ClearFIFR();

    if (FPSCR.VE == 0)
      compute_result(b);
  }
  else
  {
    if (std::isnan(b) || std::isinf(b))
      FPSCR.ClearFIFR();

    compute_result(b);
  }

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::fmsubx(UGeckoInstruction inst)
{
  const FPResult product = NI_msub(rPS0(inst.FA), rPS0(inst.FC), rPS0(inst.FB));

  if (FPSCR.VE == 0 || product.HasNoInvalidExceptions())
  {
    const double result = ForceDouble(product.value);
    rPS0(inst.FD) = result;
    PowerPC::UpdateFPRF(result);
  }

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::fmsubsx(UGeckoInstruction inst)
{
  const double c_value = Force25Bit(rPS0(inst.FC));
  const FPResult product = NI_msub(rPS0(inst.FA), c_value, rPS0(inst.FB));

  if (FPSCR.VE == 0 || product.HasNoInvalidExceptions())
  {
    const double result = ForceSingle(product.value);
    rPS0(inst.FD) = rPS1(inst.FD) = result;
    PowerPC::UpdateFPRF(result);
  }

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::fnmaddx(UGeckoInstruction inst)
{
  const FPResult product = NI_madd(rPS0(inst.FA), rPS0(inst.FC), rPS0(inst.FB));

  if (FPSCR.VE == 0 || product.HasNoInvalidExceptions())
  {
    const double result = ForceDouble(product.value);
    rPS0(inst.FD) = std::isnan(result) ? result : -result;
    PowerPC::UpdateFPRF(rPS0(inst.FD));
  }

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::fnmaddsx(UGeckoInstruction inst)
{
  const double c_value = Force25Bit(rPS0(inst.FC));
  const FPResult product = NI_madd(rPS0(inst.FA), c_value, rPS0(inst.FB));

  if (FPSCR.VE == 0 || product.HasNoInvalidExceptions())
  {
    const double result = ForceSingle(product.value);
    rPS0(inst.FD) = rPS1(inst.FD) = std::isnan(result) ? result : -result;
    PowerPC::UpdateFPRF(rPS0(inst.FD));
  }

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::fnmsubx(UGeckoInstruction inst)
{
  const FPResult product = NI_msub(rPS0(inst.FA), rPS0(inst.FC), rPS0(inst.FB));

  if (FPSCR.VE == 0 || product.HasNoInvalidExceptions())
  {
    const double result = ForceDouble(product.value);
    rPS0(inst.FD) = std::isnan(result) ? result : -result;
    PowerPC::UpdateFPRF(rPS0(inst.FD));
  }

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::fnmsubsx(UGeckoInstruction inst)
{
  const double c_value = Force25Bit(rPS0(inst.FC));
  const FPResult product = NI_msub(rPS0(inst.FA), c_value, rPS0(inst.FB));

  if (FPSCR.VE == 0 || product.HasNoInvalidExceptions())
  {
    const double result = ForceSingle(product.value);
    rPS0(inst.FD) = rPS1(inst.FD) = std::isnan(result) ? result : -result;
    PowerPC::UpdateFPRF(rPS0(inst.FD));
  }

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::fsubx(UGeckoInstruction inst)
{
  const FPResult difference = NI_sub(rPS0(inst.FA), rPS0(inst.FB));

  if (FPSCR.VE == 0 || difference.HasNoInvalidExceptions())
  {
    const double result = ForceDouble(difference.value);
    rPS0(inst.FD) = result;
    PowerPC::UpdateFPRF(result);
  }

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::fsubsx(UGeckoInstruction inst)
{
  const FPResult difference = NI_sub(rPS0(inst.FA), rPS0(inst.FB));

  if (FPSCR.VE == 0 || difference.HasNoInvalidExceptions())
  {
    const double result = ForceSingle(difference.value);
    rPS0(inst.FD) = rPS1(inst.FD) = result;
    PowerPC::UpdateFPRF(result);
  }

  if (inst.Rc)
    Helper_UpdateCR1();
}
