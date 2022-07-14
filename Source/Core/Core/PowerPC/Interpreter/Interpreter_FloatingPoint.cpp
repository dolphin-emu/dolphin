// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/Interpreter/Interpreter.h"

#include <cmath>
#include <limits>

#include "Common/CommonTypes.h"
#include "Common/FPURoundMode.h"
#include "Common/FloatUtils.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/Interpreter/Interpreter_FPUtils.h"
#include "Core/PowerPC/PowerPC.h"

namespace
{
void SetFI(Reg_FPSCR* fpscr, bool FI)
{
  if (FI)
  {
    SetFPException(fpscr, FPSCR_XX);
  }
  fpscr->FI() = FI;
}

// Note that the convert to integer operation is defined
// in Appendix C.4.2 in PowerPC Microprocessor Family:
// The Programming Environments Manual for 32 and 64-bit Microprocessors
void ConvertToInteger(GeckoInstruction inst, FPURoundMode::RoundMode rounding_mode)
{
  const double b = rPS(inst.FB()).PS0AsDouble();
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
    case FPURoundMode::RoundMode::Nearest:
    {
      const double t = b + 0.5;
      i = static_cast<s32>(t);

      // Ties to even
      if (t - i < 0 || (t - i == 0 && (i & 1)))
      {
        i--;
      }
      break;
    }
    case FPURoundMode::RoundMode::TowardsZero:
      i = static_cast<s32>(b);
      break;
    case FPURoundMode::RoundMode::TowardsPositiveInfinity:
      i = static_cast<s32>(b);
      if (b - i > 0)
      {
        i++;
      }
      break;
    case FPURoundMode::RoundMode::TowardsNegativeInfinity:
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
      SetFI(&FPSCR, true);
      FPSCR.FR() = fabs(di) > fabs(b);
    }
  }

  if (exception_occurred)
  {
    FPSCR.ClearFIFR();
  }

  if (!exception_occurred || !FPSCR.VE())
  {
    // Based on HW tests
    // FPRF is not affected
    u64 result = 0xfff8000000000000ull | value;
    if (value == 0 && std::signbit(b))
      result |= 0x100000000ull;

    rPS(inst.FD()).SetPS0(result);
  }

  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}
}  // Anonymous namespace

void Interpreter::Helper_FloatCompareOrdered(GeckoInstruction inst, double fa, double fb)
{
  FPCC compare_result;

  if (std::isnan(fa) || std::isnan(fb))
  {
    compare_result = FPCC::FU;
    if (Common::IsSNAN(fa) || Common::IsSNAN(fb))
    {
      SetFPException(&FPSCR, FPSCR_VXSNAN);
      if (!FPSCR.VE())
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
  FPSCR.FPRF() = (FPSCR.FPRF() & ~FPCC_MASK) | compare_value;

  PowerPC::ppcState.cr.SetField(inst.CRFD(), compare_value);
}

void Interpreter::Helper_FloatCompareUnordered(GeckoInstruction inst, double fa, double fb)
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
  FPSCR.FPRF() = (FPSCR.FPRF() & ~FPCC_MASK) | compare_value;

  PowerPC::ppcState.cr.SetField(inst.CRFD(), compare_value);
}

void Interpreter::fcmpo(GeckoInstruction inst)
{
  const auto& a = rPS(inst.FA());
  const auto& b = rPS(inst.FB());

  Helper_FloatCompareOrdered(inst, a.PS0AsDouble(), b.PS0AsDouble());
}

void Interpreter::fcmpu(GeckoInstruction inst)
{
  const auto& a = rPS(inst.FA());
  const auto& b = rPS(inst.FB());

  Helper_FloatCompareUnordered(inst, a.PS0AsDouble(), b.PS0AsDouble());
}

void Interpreter::fctiwx(GeckoInstruction inst)
{
  ConvertToInteger(inst, FPSCR.RN());
}

void Interpreter::fctiwzx(GeckoInstruction inst)
{
  ConvertToInteger(inst, FPURoundMode::RoundMode::TowardsZero);
}

void Interpreter::fmrx(GeckoInstruction inst)
{
  rPS(inst.FD()).SetPS0(rPS(inst.FB()).PS0AsU64());

  // This is a binary instruction. Does not alter FPSCR
  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fabsx(GeckoInstruction inst)
{
  rPS(inst.FD()).SetPS0(fabs(rPS(inst.FB()).PS0AsDouble()));

  // This is a binary instruction. Does not alter FPSCR
  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fnabsx(GeckoInstruction inst)
{
  rPS(inst.FD()).SetPS0(rPS(inst.FB()).PS0AsU64() | (UINT64_C(1) << 63));

  // This is a binary instruction. Does not alter FPSCR
  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fnegx(GeckoInstruction inst)
{
  rPS(inst.FD()).SetPS0(rPS(inst.FB()).PS0AsU64() ^ (UINT64_C(1) << 63));

  // This is a binary instruction. Does not alter FPSCR
  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fselx(GeckoInstruction inst)
{
  const auto& a = rPS(inst.FA());
  const auto& b = rPS(inst.FB());
  const auto& c = rPS(inst.FC());

  rPS(inst.FD()).SetPS0((a.PS0AsDouble() >= -0.0) ? c.PS0AsDouble() : b.PS0AsDouble());

  // This is a binary instruction. Does not alter FPSCR
  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}

// !!! warning !!!
// PS1 must be set to the value of PS0 or DragonballZ will be f**ked up
// PS1 is said to be undefined
void Interpreter::frspx(GeckoInstruction inst)  // round to single
{
  const double b = rPS(inst.FB()).PS0AsDouble();
  const float rounded = ForceSingle(FPSCR, b);

  if (std::isnan(b))
  {
    const bool is_snan = Common::IsSNAN(b);

    if (is_snan)
      SetFPException(&FPSCR, FPSCR_VXSNAN);

    if (!is_snan || !FPSCR.VE())
    {
      rPS(inst.FD()).Fill(rounded);
      PowerPC::UpdateFPRFSingle(rounded);
    }

    FPSCR.ClearFIFR();
  }
  else
  {
    SetFI(&FPSCR, b != rounded);
    FPSCR.FR() = fabs(rounded) > fabs(b);
    PowerPC::UpdateFPRFSingle(rounded);
    rPS(inst.FD()).Fill(rounded);
  }

  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fmulx(GeckoInstruction inst)
{
  const auto& a = rPS(inst.FA());
  const auto& c = rPS(inst.FC());

  const FPResult product = NI_mul(&FPSCR, a.PS0AsDouble(), c.PS0AsDouble());

  if (!FPSCR.VE() || product.HasNoInvalidExceptions())
  {
    const double result = ForceDouble(FPSCR, product.value);

    rPS(inst.FD()).SetPS0(result);
    FPSCR.FI() = false;  // are these flags important?
    FPSCR.FR() = false;
    PowerPC::UpdateFPRFDouble(result);
  }

  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}
void Interpreter::fmulsx(GeckoInstruction inst)
{
  const auto& a = rPS(inst.FA());
  const auto& c = rPS(inst.FC());

  const double c_value = Force25Bit(c.PS0AsDouble());
  const FPResult d_value = NI_mul(&FPSCR, a.PS0AsDouble(), c_value);

  if (!FPSCR.VE() || d_value.HasNoInvalidExceptions())
  {
    const float result = ForceSingle(FPSCR, d_value.value);

    rPS(inst.FD()).Fill(result);
    FPSCR.FI() = false;
    FPSCR.FR() = false;
    PowerPC::UpdateFPRFSingle(result);
  }

  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fmaddx(GeckoInstruction inst)
{
  const auto& a = rPS(inst.FA());
  const auto& b = rPS(inst.FB());
  const auto& c = rPS(inst.FC());
  const FPResult product = NI_madd(&FPSCR, a.PS0AsDouble(), c.PS0AsDouble(), b.PS0AsDouble());

  if (!FPSCR.VE() || product.HasNoInvalidExceptions())
  {
    const double result = ForceDouble(FPSCR, product.value);
    rPS(inst.FD()).SetPS0(result);
    PowerPC::UpdateFPRFDouble(result);
  }

  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fmaddsx(GeckoInstruction inst)
{
  const auto& a = rPS(inst.FA());
  const auto& b = rPS(inst.FB());
  const auto& c = rPS(inst.FC());

  const double c_value = Force25Bit(c.PS0AsDouble());
  const FPResult d_value = NI_madd(&FPSCR, a.PS0AsDouble(), c_value, b.PS0AsDouble());

  if (!FPSCR.VE() || d_value.HasNoInvalidExceptions())
  {
    const float result = ForceSingle(FPSCR, d_value.value);

    rPS(inst.FD()).Fill(result);
    FPSCR.FI() = d_value.value != result;
    FPSCR.FR() = false;
    PowerPC::UpdateFPRFSingle(result);
  }

  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::faddx(GeckoInstruction inst)
{
  const auto& a = rPS(inst.FA());
  const auto& b = rPS(inst.FB());

  const FPResult sum = NI_add(&FPSCR, a.PS0AsDouble(), b.PS0AsDouble());

  if (!FPSCR.VE() || sum.HasNoInvalidExceptions())
  {
    const double result = ForceDouble(FPSCR, sum.value);
    rPS(inst.FD()).SetPS0(result);
    PowerPC::UpdateFPRFDouble(result);
  }

  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}
void Interpreter::faddsx(GeckoInstruction inst)
{
  const auto& a = rPS(inst.FA());
  const auto& b = rPS(inst.FB());

  const FPResult sum = NI_add(&FPSCR, a.PS0AsDouble(), b.PS0AsDouble());

  if (!FPSCR.VE() || sum.HasNoInvalidExceptions())
  {
    const float result = ForceSingle(FPSCR, sum.value);
    rPS(inst.FD()).Fill(result);
    PowerPC::UpdateFPRFSingle(result);
  }

  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fdivx(GeckoInstruction inst)
{
  const auto& a = rPS(inst.FA());
  const auto& b = rPS(inst.FB());

  const FPResult quotient = NI_div(&FPSCR, a.PS0AsDouble(), b.PS0AsDouble());
  const bool not_divide_by_zero = !FPSCR.ZE() || quotient.exception != FPSCR_ZX;
  const bool not_invalid = !FPSCR.VE() || quotient.HasNoInvalidExceptions();

  if (not_divide_by_zero && not_invalid)
  {
    const double result = ForceDouble(FPSCR, quotient.value);
    rPS(inst.FD()).SetPS0(result);
    PowerPC::UpdateFPRFDouble(result);
  }

  // FR,FI,OX,UX???
  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}
void Interpreter::fdivsx(GeckoInstruction inst)
{
  const auto& a = rPS(inst.FA());
  const auto& b = rPS(inst.FB());

  const FPResult quotient = NI_div(&FPSCR, a.PS0AsDouble(), b.PS0AsDouble());
  const bool not_divide_by_zero = !FPSCR.ZE() || quotient.exception != FPSCR_ZX;
  const bool not_invalid = !FPSCR.VE() || quotient.HasNoInvalidExceptions();

  if (not_divide_by_zero && not_invalid)
  {
    const float result = ForceSingle(FPSCR, quotient.value);
    rPS(inst.FD()).Fill(result);
    PowerPC::UpdateFPRFSingle(result);
  }

  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}

// Single precision only.
void Interpreter::fresx(GeckoInstruction inst)
{
  const double b = rPS(inst.FB()).PS0AsDouble();

  const auto compute_result = [inst](double value) {
    const double result = Common::ApproximateReciprocal(value);
    rPS(inst.FD()).Fill(result);
    PowerPC::UpdateFPRFSingle(float(result));
  };

  if (b == 0.0)
  {
    SetFPException(&FPSCR, FPSCR_ZX);
    FPSCR.ClearFIFR();

    if (!FPSCR.ZE())
      compute_result(b);
  }
  else if (Common::IsSNAN(b))
  {
    SetFPException(&FPSCR, FPSCR_VXSNAN);
    FPSCR.ClearFIFR();

    if (!FPSCR.VE())
      compute_result(b);
  }
  else
  {
    if (std::isnan(b) || std::isinf(b))
      FPSCR.ClearFIFR();

    compute_result(b);
  }

  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::frsqrtex(GeckoInstruction inst)
{
  const double b = rPS(inst.FB()).PS0AsDouble();

  const auto compute_result = [inst](double value) {
    const double result = Common::ApproximateReciprocalSquareRoot(value);
    rPS(inst.FD()).SetPS0(result);
    PowerPC::UpdateFPRFDouble(result);
  };

  if (b < 0.0)
  {
    SetFPException(&FPSCR, FPSCR_VXSQRT);
    FPSCR.ClearFIFR();

    if (!FPSCR.VE())
      compute_result(b);
  }
  else if (b == 0.0)
  {
    SetFPException(&FPSCR, FPSCR_ZX);
    FPSCR.ClearFIFR();

    if (!FPSCR.ZE())
      compute_result(b);
  }
  else if (Common::IsSNAN(b))
  {
    SetFPException(&FPSCR, FPSCR_VXSNAN);
    FPSCR.ClearFIFR();

    if (!FPSCR.VE())
      compute_result(b);
  }
  else
  {
    if (std::isnan(b) || std::isinf(b))
      FPSCR.ClearFIFR();

    compute_result(b);
  }

  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fmsubx(GeckoInstruction inst)
{
  const auto& a = rPS(inst.FA());
  const auto& b = rPS(inst.FB());
  const auto& c = rPS(inst.FC());

  const FPResult product = NI_msub(&FPSCR, a.PS0AsDouble(), c.PS0AsDouble(), b.PS0AsDouble());

  if (!FPSCR.VE() || product.HasNoInvalidExceptions())
  {
    const double result = ForceDouble(FPSCR, product.value);
    rPS(inst.FD()).SetPS0(result);
    PowerPC::UpdateFPRFDouble(result);
  }

  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fmsubsx(GeckoInstruction inst)
{
  const auto& a = rPS(inst.FA());
  const auto& b = rPS(inst.FB());
  const auto& c = rPS(inst.FC());

  const double c_value = Force25Bit(c.PS0AsDouble());
  const FPResult product = NI_msub(&FPSCR, a.PS0AsDouble(), c_value, b.PS0AsDouble());

  if (!FPSCR.VE() || product.HasNoInvalidExceptions())
  {
    const float result = ForceSingle(FPSCR, product.value);
    rPS(inst.FD()).Fill(result);
    PowerPC::UpdateFPRFSingle(result);
  }

  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fnmaddx(GeckoInstruction inst)
{
  const auto& a = rPS(inst.FA());
  const auto& b = rPS(inst.FB());
  const auto& c = rPS(inst.FC());

  const FPResult product = NI_madd(&FPSCR, a.PS0AsDouble(), c.PS0AsDouble(), b.PS0AsDouble());

  if (!FPSCR.VE() || product.HasNoInvalidExceptions())
  {
    const double tmp = ForceDouble(FPSCR, product.value);
    const double result = std::isnan(tmp) ? tmp : -tmp;

    rPS(inst.FD()).SetPS0(result);
    PowerPC::UpdateFPRFDouble(result);
  }

  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fnmaddsx(GeckoInstruction inst)
{
  const auto& a = rPS(inst.FA());
  const auto& b = rPS(inst.FB());
  const auto& c = rPS(inst.FC());

  const double c_value = Force25Bit(c.PS0AsDouble());
  const FPResult product = NI_madd(&FPSCR, a.PS0AsDouble(), c_value, b.PS0AsDouble());

  if (!FPSCR.VE() || product.HasNoInvalidExceptions())
  {
    const float tmp = ForceSingle(FPSCR, product.value);
    const float result = std::isnan(tmp) ? tmp : -tmp;

    rPS(inst.FD()).Fill(result);
    PowerPC::UpdateFPRFSingle(result);
  }

  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fnmsubx(GeckoInstruction inst)
{
  const auto& a = rPS(inst.FA());
  const auto& b = rPS(inst.FB());
  const auto& c = rPS(inst.FC());

  const FPResult product = NI_msub(&FPSCR, a.PS0AsDouble(), c.PS0AsDouble(), b.PS0AsDouble());

  if (!FPSCR.VE() || product.HasNoInvalidExceptions())
  {
    const double tmp = ForceDouble(FPSCR, product.value);
    const double result = std::isnan(tmp) ? tmp : -tmp;

    rPS(inst.FD()).SetPS0(result);
    PowerPC::UpdateFPRFDouble(result);
  }

  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fnmsubsx(GeckoInstruction inst)
{
  const auto& a = rPS(inst.FA());
  const auto& b = rPS(inst.FB());
  const auto& c = rPS(inst.FC());

  const double c_value = Force25Bit(c.PS0AsDouble());
  const FPResult product = NI_msub(&FPSCR, a.PS0AsDouble(), c_value, b.PS0AsDouble());

  if (!FPSCR.VE() || product.HasNoInvalidExceptions())
  {
    const float tmp = ForceSingle(FPSCR, product.value);
    const float result = std::isnan(tmp) ? tmp : -tmp;

    rPS(inst.FD()).Fill(result);
    PowerPC::UpdateFPRFSingle(result);
  }

  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fsubx(GeckoInstruction inst)
{
  const auto& a = rPS(inst.FA());
  const auto& b = rPS(inst.FB());

  const FPResult difference = NI_sub(&FPSCR, a.PS0AsDouble(), b.PS0AsDouble());

  if (!FPSCR.VE() || difference.HasNoInvalidExceptions())
  {
    const double result = ForceDouble(FPSCR, difference.value);
    rPS(inst.FD()).SetPS0(result);
    PowerPC::UpdateFPRFDouble(result);
  }

  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fsubsx(GeckoInstruction inst)
{
  const auto& a = rPS(inst.FA());
  const auto& b = rPS(inst.FB());

  const FPResult difference = NI_sub(&FPSCR, a.PS0AsDouble(), b.PS0AsDouble());

  if (!FPSCR.VE() || difference.HasNoInvalidExceptions())
  {
    const float result = ForceSingle(FPSCR, difference.value);
    rPS(inst.FD()).Fill(result);
    PowerPC::UpdateFPRFSingle(result);
  }

  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}
