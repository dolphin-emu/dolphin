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

static void SetFI(UReg_FPSCR* fpscr, int FI)
{
  if (FI)
  {
    SetFPException(fpscr, FPSCR_XX);
  }
  fpscr->FI = FI;
}

// Note that the convert to integer operation is defined
// in Appendix C.4.2 in PowerPC Microprocessor Family:
// The Programming Environments Manual for 32 and 64-bit Microprocessors
void ConvertToInteger(UGeckoInstruction inst, RoundingMode rounding_mode)
{
  const double b = rPS(inst.FB).PS0AsDouble();
  u32 value;
  bool exception_occurred = false;

  if (std::isnan(b))
  {
    if (Common::IsSNAN(b))
      SetFPException(&FPSCR, FPSCR_VXSNAN);

    value = 0x80000000;
    SetFPException(&FPSCR, FPSCR_VXCVI);
    exception_occurred = true;
  }
  else if (b > static_cast<double>(0x7fffffff))
  {
    // Positive large operand or +inf
    value = 0x7fffffff;
    SetFPException(&FPSCR, FPSCR_VXCVI);
    exception_occurred = true;
  }
  else if (b < -static_cast<double>(0x80000000))
  {
    // Negative large operand or -inf
    value = 0x80000000;
    SetFPException(&FPSCR, FPSCR_VXCVI);
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
      SetFI(&FPSCR, 1);
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
    u64 result = 0xfff8000000000000ull | value;
    if (value == 0 && std::signbit(b))
      result |= 0x100000000ull;

    rPS(inst.FD).SetPS0(result);
  }

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
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
      SetFPException(&FPSCR, FPSCR_VXSNAN);
      if (FPSCR.VE == 0)
      {
        SetFPException(&FPSCR, FPSCR_VXVC);
      }
    }
    else  // QNaN
    {
      SetFPException(&FPSCR, FPSCR_VXVC);
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

  PowerPC::ppcState.cr.SetField(inst.CRFD, compare_value);
}

void Interpreter::Helper_FloatCompareUnordered(UGeckoInstruction inst, double fa, double fb)
{
  FPCC compare_result;

  if (std::isnan(fa) || std::isnan(fb))
  {
    compare_result = FPCC::FU;

    if (Common::IsSNAN(fa) || Common::IsSNAN(fb))
    {
      SetFPException(&FPSCR, FPSCR_VXSNAN);
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

  PowerPC::ppcState.cr.SetField(inst.CRFD, compare_value);
}

void Interpreter::fcmpo(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& b = rPS(inst.FB);

  Helper_FloatCompareOrdered(inst, a.PS0AsDouble(), b.PS0AsDouble());
}

void Interpreter::fcmpu(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& b = rPS(inst.FB);

  Helper_FloatCompareUnordered(inst, a.PS0AsDouble(), b.PS0AsDouble());
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
  rPS(inst.FD).SetPS0(rPS(inst.FB).PS0AsU64());

  // This is a binary instruction. Does not alter FPSCR
  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fabsx(UGeckoInstruction inst)
{
  rPS(inst.FD).SetPS0(fabs(rPS(inst.FB).PS0AsDouble()));

  // This is a binary instruction. Does not alter FPSCR
  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fnabsx(UGeckoInstruction inst)
{
  rPS(inst.FD).SetPS0(rPS(inst.FB).PS0AsU64() | (UINT64_C(1) << 63));

  // This is a binary instruction. Does not alter FPSCR
  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fnegx(UGeckoInstruction inst)
{
  rPS(inst.FD).SetPS0(rPS(inst.FB).PS0AsU64() ^ (UINT64_C(1) << 63));

  // This is a binary instruction. Does not alter FPSCR
  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fselx(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& b = rPS(inst.FB);
  const auto& c = rPS(inst.FC);

  rPS(inst.FD).SetPS0((a.PS0AsDouble() >= -0.0) ? c.PS0AsDouble() : b.PS0AsDouble());

  // This is a binary instruction. Does not alter FPSCR
  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

// !!! warning !!!
// PS1 must be set to the value of PS0 or DragonballZ will be f**ked up
// PS1 is said to be undefined
void Interpreter::frspx(UGeckoInstruction inst)  // round to single
{
  const double b = rPS(inst.FB).PS0AsDouble();
  const double rounded = ForceSingle(FPSCR, b);

  if (std::isnan(b))
  {
    const bool is_snan = Common::IsSNAN(b);

    if (is_snan)
      SetFPException(&FPSCR, FPSCR_VXSNAN);

    if (!is_snan || FPSCR.VE == 0)
    {
      rPS(inst.FD).Fill(rounded);
      PowerPC::UpdateFPRF(b);
    }

    FPSCR.ClearFIFR();
  }
  else
  {
    SetFI(&FPSCR, b != rounded);
    FPSCR.FR = fabs(rounded) > fabs(b);
    PowerPC::UpdateFPRF(rounded);
    rPS(inst.FD).Fill(rounded);
  }

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fmulx(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& c = rPS(inst.FC);

  const FPResult product = NI_mul(&FPSCR, a.PS0AsDouble(), c.PS0AsDouble());

  if (FPSCR.VE == 0 || product.HasNoInvalidExceptions())
  {
    const double result = ForceDouble(FPSCR, product.value);

    rPS(inst.FD).SetPS0(result);
    FPSCR.FI = 0;  // are these flags important?
    FPSCR.FR = 0;
    PowerPC::UpdateFPRF(result);
  }

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}
void Interpreter::fmulsx(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& c = rPS(inst.FC);

  const double c_value = Force25Bit(c.PS0AsDouble());
  const FPResult d_value = NI_mul(&FPSCR, a.PS0AsDouble(), c_value);

  if (FPSCR.VE == 0 || d_value.HasNoInvalidExceptions())
  {
    const double result = ForceSingle(FPSCR, d_value.value);

    rPS(inst.FD).Fill(result);
    FPSCR.FI = 0;
    FPSCR.FR = 0;
    PowerPC::UpdateFPRF(result);
  }

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fmaddx(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& b = rPS(inst.FB);
  const auto& c = rPS(inst.FC);
  const FPResult product = NI_madd(&FPSCR, a.PS0AsDouble(), c.PS0AsDouble(), b.PS0AsDouble());

  if (FPSCR.VE == 0 || product.HasNoInvalidExceptions())
  {
    const double result = ForceDouble(FPSCR, product.value);
    rPS(inst.FD).SetPS0(result);
    PowerPC::UpdateFPRF(result);
  }

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fmaddsx(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& b = rPS(inst.FB);
  const auto& c = rPS(inst.FC);

  const double c_value = Force25Bit(c.PS0AsDouble());
  const FPResult d_value = NI_madd(&FPSCR, a.PS0AsDouble(), c_value, b.PS0AsDouble());

  if (FPSCR.VE == 0 || d_value.HasNoInvalidExceptions())
  {
    const double result = ForceSingle(FPSCR, d_value.value);

    rPS(inst.FD).Fill(result);
    FPSCR.FI = d_value.value != result;
    FPSCR.FR = 0;
    PowerPC::UpdateFPRF(result);
  }

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::faddx(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& b = rPS(inst.FB);

  const FPResult sum = NI_add(&FPSCR, a.PS0AsDouble(), b.PS0AsDouble());

  if (FPSCR.VE == 0 || sum.HasNoInvalidExceptions())
  {
    const double result = ForceDouble(FPSCR, sum.value);
    rPS(inst.FD).SetPS0(result);
    PowerPC::UpdateFPRF(result);
  }

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}
void Interpreter::faddsx(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& b = rPS(inst.FB);

  const FPResult sum = NI_add(&FPSCR, a.PS0AsDouble(), b.PS0AsDouble());

  if (FPSCR.VE == 0 || sum.HasNoInvalidExceptions())
  {
    const double result = ForceSingle(FPSCR, sum.value);
    rPS(inst.FD).Fill(result);
    PowerPC::UpdateFPRF(result);
  }

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fdivx(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& b = rPS(inst.FB);

  const FPResult quotient = NI_div(&FPSCR, a.PS0AsDouble(), b.PS0AsDouble());
  const bool not_divide_by_zero = FPSCR.ZE == 0 || quotient.exception != FPSCR_ZX;
  const bool not_invalid = FPSCR.VE == 0 || quotient.HasNoInvalidExceptions();

  if (not_divide_by_zero && not_invalid)
  {
    const double result = ForceDouble(FPSCR, quotient.value);
    rPS(inst.FD).SetPS0(result);
    PowerPC::UpdateFPRF(result);
  }

  // FR,FI,OX,UX???
  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}
void Interpreter::fdivsx(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& b = rPS(inst.FB);

  const FPResult quotient = NI_div(&FPSCR, a.PS0AsDouble(), b.PS0AsDouble());
  const bool not_divide_by_zero = FPSCR.ZE == 0 || quotient.exception != FPSCR_ZX;
  const bool not_invalid = FPSCR.VE == 0 || quotient.HasNoInvalidExceptions();

  if (not_divide_by_zero && not_invalid)
  {
    const double result = ForceSingle(FPSCR, quotient.value);
    rPS(inst.FD).Fill(result);
    PowerPC::UpdateFPRF(result);
  }

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

// Single precision only.
void Interpreter::fresx(UGeckoInstruction inst)
{
  const double b = rPS(inst.FB).PS0AsDouble();

  const auto compute_result = [inst](double value) {
    const double result = Common::ApproximateReciprocal(value);
    rPS(inst.FD).Fill(result);
    PowerPC::UpdateFPRF(result);
  };

  if (b == 0.0)
  {
    SetFPException(&FPSCR, FPSCR_ZX);
    FPSCR.ClearFIFR();

    if (FPSCR.ZE == 0)
      compute_result(b);
  }
  else if (Common::IsSNAN(b))
  {
    SetFPException(&FPSCR, FPSCR_VXSNAN);
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
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::frsqrtex(UGeckoInstruction inst)
{
  const double b = rPS(inst.FB).PS0AsDouble();

  const auto compute_result = [inst](double value) {
    const double result = Common::ApproximateReciprocalSquareRoot(value);
    rPS(inst.FD).SetPS0(result);
    PowerPC::UpdateFPRF(result);
  };

  if (b < 0.0)
  {
    SetFPException(&FPSCR, FPSCR_VXSQRT);
    FPSCR.ClearFIFR();

    if (FPSCR.VE == 0)
      compute_result(b);
  }
  else if (b == 0.0)
  {
    SetFPException(&FPSCR, FPSCR_ZX);
    FPSCR.ClearFIFR();

    if (FPSCR.ZE == 0)
      compute_result(b);
  }
  else if (Common::IsSNAN(b))
  {
    SetFPException(&FPSCR, FPSCR_VXSNAN);
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
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fmsubx(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& b = rPS(inst.FB);
  const auto& c = rPS(inst.FC);

  const FPResult product = NI_msub(&FPSCR, a.PS0AsDouble(), c.PS0AsDouble(), b.PS0AsDouble());

  if (FPSCR.VE == 0 || product.HasNoInvalidExceptions())
  {
    const double result = ForceDouble(FPSCR, product.value);
    rPS(inst.FD).SetPS0(result);
    PowerPC::UpdateFPRF(result);
  }

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fmsubsx(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& b = rPS(inst.FB);
  const auto& c = rPS(inst.FC);

  const double c_value = Force25Bit(c.PS0AsDouble());
  const FPResult product = NI_msub(&FPSCR, a.PS0AsDouble(), c_value, b.PS0AsDouble());

  if (FPSCR.VE == 0 || product.HasNoInvalidExceptions())
  {
    const double result = ForceSingle(FPSCR, product.value);
    rPS(inst.FD).Fill(result);
    PowerPC::UpdateFPRF(result);
  }

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fnmaddx(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& b = rPS(inst.FB);
  const auto& c = rPS(inst.FC);

  const FPResult product = NI_madd(&FPSCR, a.PS0AsDouble(), c.PS0AsDouble(), b.PS0AsDouble());

  if (FPSCR.VE == 0 || product.HasNoInvalidExceptions())
  {
    const double tmp = ForceDouble(FPSCR, product.value);
    const double result = std::isnan(tmp) ? tmp : -tmp;

    rPS(inst.FD).SetPS0(result);
    PowerPC::UpdateFPRF(result);
  }

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fnmaddsx(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& b = rPS(inst.FB);
  const auto& c = rPS(inst.FC);

  const double c_value = Force25Bit(c.PS0AsDouble());
  const FPResult product = NI_madd(&FPSCR, a.PS0AsDouble(), c_value, b.PS0AsDouble());

  if (FPSCR.VE == 0 || product.HasNoInvalidExceptions())
  {
    const double tmp = ForceSingle(FPSCR, product.value);
    const double result = std::isnan(tmp) ? tmp : -tmp;

    rPS(inst.FD).Fill(result);
    PowerPC::UpdateFPRF(result);
  }

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fnmsubx(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& b = rPS(inst.FB);
  const auto& c = rPS(inst.FC);

  const FPResult product = NI_msub(&FPSCR, a.PS0AsDouble(), c.PS0AsDouble(), b.PS0AsDouble());

  if (FPSCR.VE == 0 || product.HasNoInvalidExceptions())
  {
    const double tmp = ForceDouble(FPSCR, product.value);
    const double result = std::isnan(tmp) ? tmp : -tmp;

    rPS(inst.FD).SetPS0(result);
    PowerPC::UpdateFPRF(result);
  }

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fnmsubsx(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& b = rPS(inst.FB);
  const auto& c = rPS(inst.FC);

  const double c_value = Force25Bit(c.PS0AsDouble());
  const FPResult product = NI_msub(&FPSCR, a.PS0AsDouble(), c_value, b.PS0AsDouble());

  if (FPSCR.VE == 0 || product.HasNoInvalidExceptions())
  {
    const double tmp = ForceSingle(FPSCR, product.value);
    const double result = std::isnan(tmp) ? tmp : -tmp;

    rPS(inst.FD).Fill(result);
    PowerPC::UpdateFPRF(result);
  }

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fsubx(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& b = rPS(inst.FB);

  const FPResult difference = NI_sub(&FPSCR, a.PS0AsDouble(), b.PS0AsDouble());

  if (FPSCR.VE == 0 || difference.HasNoInvalidExceptions())
  {
    const double result = ForceDouble(FPSCR, difference.value);
    rPS(inst.FD).SetPS0(result);
    PowerPC::UpdateFPRF(result);
  }

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fsubsx(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& b = rPS(inst.FB);

  const FPResult difference = NI_sub(&FPSCR, a.PS0AsDouble(), b.PS0AsDouble());

  if (FPSCR.VE == 0 || difference.HasNoInvalidExceptions())
  {
    const double result = ForceSingle(FPSCR, difference.value);
    rPS(inst.FD).Fill(result);
    PowerPC::UpdateFPRF(result);
  }

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}
