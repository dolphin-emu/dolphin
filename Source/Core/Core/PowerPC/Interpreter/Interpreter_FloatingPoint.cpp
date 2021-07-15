// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cmath>
#include <limits>

#include "Common/CommonTypes.h"
#include "Common/FloatUtils.h"
#include "Core/PowerPC/Gekko.h"
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
    fpscr->SetException(FPSCR_XX);
  }
  fpscr->FI = FI;
}

// Note that the convert to integer operation is defined
// in Appendix C.4.2 in PowerPC Microprocessor Family:
// The Programming Environments Manual for 32 and 64-bit Microprocessors
void ConvertToInteger(PowerPC::PowerPCState* ppcs, UGeckoInstruction inst,
                      RoundingMode rounding_mode)
{
  UReg_FPSCR& fpscr = ppcs->fpscr;
  const double b = ppcs->ps[inst.FB].PS0AsDouble();
  u32 value;
  bool exception_occurred = false;

  if (std::isnan(b))
  {
    if (Common::IsSNAN(b))
      fpscr.SetException(FPSCR_VXSNAN);

    value = 0x80000000;
    fpscr.SetException(FPSCR_VXCVI);
    exception_occurred = true;
  }
  else if (b > static_cast<double>(0x7fffffff))
  {
    // Positive large operand or +inf
    value = 0x7fffffff;
    fpscr.SetException(FPSCR_VXCVI);
    exception_occurred = true;
  }
  else if (b < -static_cast<double>(0x80000000))
  {
    // Negative large operand or -inf
    value = 0x80000000;
    fpscr.SetException(FPSCR_VXCVI);
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

      // Ties to even
      if (t - i < 0 || (t - i == 0 && (i & 1)))
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
      fpscr.ClearFIFR();
    }
    else
    {
      // Also sets FPSCR[XX]
      SetFI(&fpscr, 1);
      fpscr.FR = fabs(di) > fabs(b);
    }
  }

  if (exception_occurred)
  {
    fpscr.ClearFIFR();
  }

  if (!exception_occurred || fpscr.VE == 0)
  {
    // Based on HW tests
    // FPRF is not affected
    u64 result = 0xfff8000000000000ull | value;
    if (value == 0 && std::signbit(b))
      result |= 0x100000000ull;

    ppcs->ps[inst.FD].SetPS0(result);
  }

  if (inst.Rc)
    ppcs->UpdateCR1();
}
}  // Anonymous namespace

void Interpreter::Helper_FloatCompareOrdered(PowerPC::PowerPCState* ppcs, UGeckoInstruction inst,
                                             double fa, double fb)
{
  UReg_FPSCR& fpscr = ppcs->fpscr;
  FPCC compare_result;

  if (std::isnan(fa) || std::isnan(fb))
  {
    compare_result = FPCC::FU;
    if (Common::IsSNAN(fa) || Common::IsSNAN(fb))
    {
      fpscr.SetException(FPSCR_VXSNAN);
      if (FPSCR.VE == 0)
      {
        fpscr.SetException(FPSCR_VXVC);
      }
    }
    else  // QNaN
    {
      fpscr.SetException(FPSCR_VXVC);
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
  fpscr.FPRF = (fpscr.FPRF & ~FPCC_MASK) | compare_value;

  ppcs->cr.SetField(inst.CRFD, compare_value);
}

void Interpreter::Helper_FloatCompareUnordered(PowerPC::PowerPCState* ppcs, UGeckoInstruction inst,
                                               double fa, double fb)
{
  UReg_FPSCR& fpscr = ppcs->fpscr;
  FPCC compare_result;

  if (std::isnan(fa) || std::isnan(fb))
  {
    compare_result = FPCC::FU;

    if (Common::IsSNAN(fa) || Common::IsSNAN(fb))
    {
      fpscr.SetException(FPSCR_VXSNAN);
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
  fpscr.FPRF = (fpscr.FPRF & ~FPCC_MASK) | compare_value;

  ppcs->cr.SetField(inst.CRFD, compare_value);
}

void Interpreter::fcmpo(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& b = rPS(inst.FB);

  Helper_FloatCompareOrdered(&PowerPC::ppcState, inst, a.PS0AsDouble(), b.PS0AsDouble());
}

void Interpreter::fcmpu(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& b = rPS(inst.FB);

  Helper_FloatCompareUnordered(&PowerPC::ppcState, inst, a.PS0AsDouble(), b.PS0AsDouble());
}

void Interpreter::fctiwx(UGeckoInstruction inst)
{
  ConvertToInteger(&PowerPC::ppcState, inst, static_cast<RoundingMode>(FPSCR.RN));
}

void Interpreter::fctiwzx(UGeckoInstruction inst)
{
  ConvertToInteger(&PowerPC::ppcState, inst, RoundingMode::TowardsZero);
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
  const float rounded = ForceSingle(FPSCR, b);

  if (std::isnan(b))
  {
    const bool is_snan = Common::IsSNAN(b);

    if (is_snan)
      FPSCR.SetException(FPSCR_VXSNAN);

    if (!is_snan || FPSCR.VE == 0)
    {
      rPS(inst.FD).Fill(rounded);
      PowerPC::UpdateFPRFSingle(rounded);
    }

    FPSCR.ClearFIFR();
  }
  else
  {
    SetFI(&FPSCR, b != rounded);
    FPSCR.FR = fabs(rounded) > fabs(b);
    PowerPC::UpdateFPRFSingle(rounded);
    rPS(inst.FD).Fill(rounded);
  }

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fmulx(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& c = rPS(inst.FC);

  const FPResult product = NI_mul(a.PS0AsDouble(), c.PS0AsDouble());

  if (product.ApplyAndCheck(&FPSCR))
  {
    const double result = product.GetDouble(FPSCR);

    rPS(inst.FD).SetPS0(result);
    FPSCR.FI = 0;  // are these flags important?
    FPSCR.FR = 0;
    PowerPC::UpdateFPRFDouble(result);
  }

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}
void Interpreter::fmulsx(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& c = rPS(inst.FC);

  const double c_value = Force25Bit(c.PS0AsDouble());
  const FPResult d_value = NI_mul(a.PS0AsDouble(), c_value);

  if (d_value.ApplyAndCheck(&FPSCR))
  {
    const float result = d_value.GetSingle(FPSCR);

    rPS(inst.FD).Fill(result);
    FPSCR.FI = 0;
    FPSCR.FR = 0;
    PowerPC::UpdateFPRFSingle(result);
  }

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fmaddx(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& b = rPS(inst.FB);
  const auto& c = rPS(inst.FC);
  const FPResult product = NI_madd(a.PS0AsDouble(), c.PS0AsDouble(), b.PS0AsDouble());

  if (product.ApplyAndCheck(&FPSCR))
  {
    const double result = product.GetDouble(FPSCR);
    rPS(inst.FD).SetPS0(result);
    PowerPC::UpdateFPRFDouble(result);
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
  const FPResult d_value = NI_madd(a.PS0AsDouble(), c_value, b.PS0AsDouble());

  if (d_value.ApplyAndCheck(&FPSCR))
  {
    const float result = d_value.GetSingle(FPSCR);

    rPS(inst.FD).Fill(result);
    FPSCR.FI = d_value.value != result;
    FPSCR.FR = 0;
    PowerPC::UpdateFPRFSingle(result);
  }

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::faddx(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& b = rPS(inst.FB);

  const FPResult sum = NI_add(a.PS0AsDouble(), b.PS0AsDouble());

  if (sum.ApplyAndCheck(&FPSCR))
  {
    const double result = sum.GetDouble(FPSCR);
    rPS(inst.FD).SetPS0(result);
    PowerPC::UpdateFPRFDouble(result);
  }

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}
void Interpreter::faddsx(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& b = rPS(inst.FB);

  const FPResult sum = NI_add(a.PS0AsDouble(), b.PS0AsDouble());

  if (sum.ApplyAndCheck(&FPSCR))
  {
    const float result = sum.GetSingle(FPSCR);
    rPS(inst.FD).Fill(result);
    PowerPC::UpdateFPRFSingle(result);
  }

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fdivx(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& b = rPS(inst.FB);

  const FPResult quotient = NI_div(a.PS0AsDouble(), b.PS0AsDouble());
  const bool not_divide_by_zero = FPSCR.ZE == 0 || quotient.exception != FPSCR_ZX;

  if (quotient.ApplyAndCheck(&FPSCR) && not_divide_by_zero)
  {
    const double result = quotient.GetDouble(FPSCR);
    rPS(inst.FD).SetPS0(result);
    PowerPC::UpdateFPRFDouble(result);
  }

  // FR,FI,OX,UX???
  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}
void Interpreter::fdivsx(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& b = rPS(inst.FB);

  const FPResult quotient = NI_div(a.PS0AsDouble(), b.PS0AsDouble());
  const bool not_divide_by_zero = FPSCR.ZE == 0 || quotient.exception != FPSCR_ZX;

  if (quotient.ApplyAndCheck(&FPSCR) && not_divide_by_zero)
  {
    const float result = quotient.GetSingle(FPSCR);
    rPS(inst.FD).Fill(result);
    PowerPC::UpdateFPRFSingle(result);
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
    PowerPC::UpdateFPRFSingle(result);
  };

  if (b == 0.0)
  {
    FPSCR.SetException(FPSCR_ZX);
    FPSCR.ClearFIFR();

    if (FPSCR.ZE == 0)
      compute_result(b);
  }
  else if (Common::IsSNAN(b))
  {
    FPSCR.SetException(FPSCR_VXSNAN);
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
    PowerPC::UpdateFPRFDouble(result);
  };

  if (b < 0.0)
  {
    FPSCR.SetException(FPSCR_VXSQRT);
    FPSCR.ClearFIFR();

    if (FPSCR.VE == 0)
      compute_result(b);
  }
  else if (b == 0.0)
  {
    FPSCR.SetException(FPSCR_ZX);
    FPSCR.ClearFIFR();

    if (FPSCR.ZE == 0)
      compute_result(b);
  }
  else if (Common::IsSNAN(b))
  {
    FPSCR.SetException(FPSCR_VXSNAN);
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

  const FPResult product = NI_msub(a.PS0AsDouble(), c.PS0AsDouble(), b.PS0AsDouble());

  if (product.ApplyAndCheck(&FPSCR))
  {
    const double result = product.GetDouble(FPSCR);
    rPS(inst.FD).SetPS0(result);
    PowerPC::UpdateFPRFDouble(result);
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
  const FPResult product = NI_msub(a.PS0AsDouble(), c_value, b.PS0AsDouble());

  if (product.ApplyAndCheck(&FPSCR))
  {
    const float result = product.GetSingle(FPSCR);
    rPS(inst.FD).Fill(result);
    PowerPC::UpdateFPRFSingle(result);
  }

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fnmaddx(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& b = rPS(inst.FB);
  const auto& c = rPS(inst.FC);

  const FPResult product = NI_madd(a.PS0AsDouble(), c.PS0AsDouble(), b.PS0AsDouble());

  if (product.ApplyAndCheck(&FPSCR))
  {
    const double tmp = product.GetDouble(FPSCR);
    const double result = std::isnan(tmp) ? tmp : -tmp;

    rPS(inst.FD).SetPS0(result);
    PowerPC::UpdateFPRFDouble(result);
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
  const FPResult product = NI_madd(a.PS0AsDouble(), c_value, b.PS0AsDouble());

  if (product.ApplyAndCheck(&FPSCR))
  {
    const float tmp = product.GetSingle(FPSCR);
    const float result = std::isnan(tmp) ? tmp : -tmp;

    rPS(inst.FD).Fill(result);
    PowerPC::UpdateFPRFSingle(result);
  }

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fnmsubx(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& b = rPS(inst.FB);
  const auto& c = rPS(inst.FC);

  const FPResult product = NI_msub(a.PS0AsDouble(), c.PS0AsDouble(), b.PS0AsDouble());

  if (product.ApplyAndCheck(&FPSCR))
  {
    const double tmp = product.GetDouble(FPSCR);
    const double result = std::isnan(tmp) ? tmp : -tmp;

    rPS(inst.FD).SetPS0(result);
    PowerPC::UpdateFPRFDouble(result);
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
  const FPResult product = NI_msub(a.PS0AsDouble(), c_value, b.PS0AsDouble());

  if (product.ApplyAndCheck(&FPSCR))
  {
    const float tmp = product.GetSingle(FPSCR);
    const float result = std::isnan(tmp) ? tmp : -tmp;

    rPS(inst.FD).Fill(result);
    PowerPC::UpdateFPRFSingle(result);
  }

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fsubx(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& b = rPS(inst.FB);

  const FPResult difference = NI_sub(a.PS0AsDouble(), b.PS0AsDouble());

  if (difference.ApplyAndCheck(&FPSCR))
  {
    const double result = difference.GetDouble(FPSCR);
    rPS(inst.FD).SetPS0(result);
    PowerPC::UpdateFPRFDouble(result);
  }

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fsubsx(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& b = rPS(inst.FB);

  const FPResult difference = NI_sub(a.PS0AsDouble(), b.PS0AsDouble());

  if (difference.ApplyAndCheck(&FPSCR))
  {
    const float result = difference.GetSingle(FPSCR);
    rPS(inst.FD).Fill(result);
    PowerPC::UpdateFPRFSingle(result);
  }

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}
