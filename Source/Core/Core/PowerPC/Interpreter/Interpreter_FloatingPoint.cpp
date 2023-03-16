// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/Interpreter/Interpreter.h"

#include <cmath>
#include <limits>

#include "Common/CommonTypes.h"
#include "Common/FloatUtils.h"
#include "Core/PowerPC/Gekko.h"
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

void SetFI(UReg_FPSCR* fpscr, u32 FI)
{
  if (FI != 0)
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
  const double b = PowerPC::ppcState.ps[inst.FB].PS0AsDouble();
  u32 value;
  bool exception_occurred = false;

  if (std::isnan(b))
  {
    if (Common::IsSNAN(b))
      SetFPException(&PowerPC::ppcState.fpscr, FPSCR_VXSNAN);

    value = 0x80000000;
    SetFPException(&PowerPC::ppcState.fpscr, FPSCR_VXCVI);
    exception_occurred = true;
  }
  else if (b > static_cast<double>(0x7fffffff))
  {
    // Positive large operand or +inf
    value = 0x7fffffff;
    SetFPException(&PowerPC::ppcState.fpscr, FPSCR_VXCVI);
    exception_occurred = true;
  }
  else if (b < -static_cast<double>(0x80000000))
  {
    // Negative large operand or -inf
    value = 0x80000000;
    SetFPException(&PowerPC::ppcState.fpscr, FPSCR_VXCVI);
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
      PowerPC::ppcState.fpscr.ClearFIFR();
    }
    else
    {
      // Also sets FPSCR[XX]
      SetFI(&PowerPC::ppcState.fpscr, 1);
      PowerPC::ppcState.fpscr.FR = fabs(di) > fabs(b);
    }
  }

  if (exception_occurred)
  {
    PowerPC::ppcState.fpscr.ClearFIFR();
  }

  if (!exception_occurred || PowerPC::ppcState.fpscr.VE == 0)
  {
    // Based on HW tests
    // FPRF is not affected
    u64 result = 0xfff8000000000000ull | value;
    if (value == 0 && std::signbit(b))
      result |= 0x100000000ull;

    PowerPC::ppcState.ps[inst.FD].SetPS0(result);
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
      SetFPException(&PowerPC::ppcState.fpscr, FPSCR_VXSNAN);
      if (PowerPC::ppcState.fpscr.VE == 0)
      {
        SetFPException(&PowerPC::ppcState.fpscr, FPSCR_VXVC);
      }
    }
    else  // QNaN
    {
      SetFPException(&PowerPC::ppcState.fpscr, FPSCR_VXVC);
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
  PowerPC::ppcState.fpscr.FPRF = (PowerPC::ppcState.fpscr.FPRF & ~FPCC_MASK) | compare_value;

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
      SetFPException(&PowerPC::ppcState.fpscr, FPSCR_VXSNAN);
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
  PowerPC::ppcState.fpscr.FPRF = (PowerPC::ppcState.fpscr.FPRF & ~FPCC_MASK) | compare_value;

  PowerPC::ppcState.cr.SetField(inst.CRFD, compare_value);
}

void Interpreter::fcmpo(Interpreter& interpreter, UGeckoInstruction inst)
{
  const auto& a = PowerPC::ppcState.ps[inst.FA];
  const auto& b = PowerPC::ppcState.ps[inst.FB];

  Helper_FloatCompareOrdered(inst, a.PS0AsDouble(), b.PS0AsDouble());
}

void Interpreter::fcmpu(Interpreter& interpreter, UGeckoInstruction inst)
{
  const auto& a = PowerPC::ppcState.ps[inst.FA];
  const auto& b = PowerPC::ppcState.ps[inst.FB];

  Helper_FloatCompareUnordered(inst, a.PS0AsDouble(), b.PS0AsDouble());
}

void Interpreter::fctiwx(Interpreter& interpreter, UGeckoInstruction inst)
{
  ConvertToInteger(inst, static_cast<RoundingMode>(PowerPC::ppcState.fpscr.RN.Value()));
}

void Interpreter::fctiwzx(Interpreter& interpreter, UGeckoInstruction inst)
{
  ConvertToInteger(inst, RoundingMode::TowardsZero);
}

void Interpreter::fmrx(Interpreter& interpreter, UGeckoInstruction inst)
{
  PowerPC::ppcState.ps[inst.FD].SetPS0(PowerPC::ppcState.ps[inst.FB].PS0AsU64());

  // This is a binary instruction. Does not alter FPSCR
  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fabsx(Interpreter& interpreter, UGeckoInstruction inst)
{
  PowerPC::ppcState.ps[inst.FD].SetPS0(fabs(PowerPC::ppcState.ps[inst.FB].PS0AsDouble()));

  // This is a binary instruction. Does not alter FPSCR
  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fnabsx(Interpreter& interpreter, UGeckoInstruction inst)
{
  PowerPC::ppcState.ps[inst.FD].SetPS0(PowerPC::ppcState.ps[inst.FB].PS0AsU64() |
                                       (UINT64_C(1) << 63));

  // This is a binary instruction. Does not alter FPSCR
  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fnegx(Interpreter& interpreter, UGeckoInstruction inst)
{
  PowerPC::ppcState.ps[inst.FD].SetPS0(PowerPC::ppcState.ps[inst.FB].PS0AsU64() ^
                                       (UINT64_C(1) << 63));

  // This is a binary instruction. Does not alter FPSCR
  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fselx(Interpreter& interpreter, UGeckoInstruction inst)
{
  const auto& a = PowerPC::ppcState.ps[inst.FA];
  const auto& b = PowerPC::ppcState.ps[inst.FB];
  const auto& c = PowerPC::ppcState.ps[inst.FC];

  PowerPC::ppcState.ps[inst.FD].SetPS0((a.PS0AsDouble() >= -0.0) ? c.PS0AsDouble() :
                                                                   b.PS0AsDouble());

  // This is a binary instruction. Does not alter FPSCR
  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

// !!! warning !!!
// PS1 must be set to the value of PS0 or DragonballZ will be f**ked up
// PS1 is said to be undefined
void Interpreter::frspx(Interpreter& interpreter, UGeckoInstruction inst)  // round to single
{
  const double b = PowerPC::ppcState.ps[inst.FB].PS0AsDouble();
  const float rounded = ForceSingle(PowerPC::ppcState.fpscr, b);

  if (std::isnan(b))
  {
    const bool is_snan = Common::IsSNAN(b);

    if (is_snan)
      SetFPException(&PowerPC::ppcState.fpscr, FPSCR_VXSNAN);

    if (!is_snan || PowerPC::ppcState.fpscr.VE == 0)
    {
      PowerPC::ppcState.ps[inst.FD].Fill(rounded);
      PowerPC::ppcState.UpdateFPRFSingle(rounded);
    }

    PowerPC::ppcState.fpscr.ClearFIFR();
  }
  else
  {
    SetFI(&PowerPC::ppcState.fpscr, b != rounded);
    PowerPC::ppcState.fpscr.FR = fabs(rounded) > fabs(b);
    PowerPC::ppcState.UpdateFPRFSingle(rounded);
    PowerPC::ppcState.ps[inst.FD].Fill(rounded);
  }

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fmulx(Interpreter& interpreter, UGeckoInstruction inst)
{
  const auto& a = PowerPC::ppcState.ps[inst.FA];
  const auto& c = PowerPC::ppcState.ps[inst.FC];

  const FPResult product = NI_mul(&PowerPC::ppcState.fpscr, a.PS0AsDouble(), c.PS0AsDouble());

  if (PowerPC::ppcState.fpscr.VE == 0 || product.HasNoInvalidExceptions())
  {
    const double result = ForceDouble(PowerPC::ppcState.fpscr, product.value);

    PowerPC::ppcState.ps[inst.FD].SetPS0(result);
    PowerPC::ppcState.fpscr.FI = 0;  // are these flags important?
    PowerPC::ppcState.fpscr.FR = 0;
    PowerPC::ppcState.UpdateFPRFDouble(result);
  }

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}
void Interpreter::fmulsx(Interpreter& interpreter, UGeckoInstruction inst)
{
  const auto& a = PowerPC::ppcState.ps[inst.FA];
  const auto& c = PowerPC::ppcState.ps[inst.FC];

  const double c_value = Force25Bit(c.PS0AsDouble());
  const FPResult d_value = NI_mul(&PowerPC::ppcState.fpscr, a.PS0AsDouble(), c_value);

  if (PowerPC::ppcState.fpscr.VE == 0 || d_value.HasNoInvalidExceptions())
  {
    const float result = ForceSingle(PowerPC::ppcState.fpscr, d_value.value);

    PowerPC::ppcState.ps[inst.FD].Fill(result);
    PowerPC::ppcState.fpscr.FI = 0;
    PowerPC::ppcState.fpscr.FR = 0;
    PowerPC::ppcState.UpdateFPRFSingle(result);
  }

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fmaddx(Interpreter& interpreter, UGeckoInstruction inst)
{
  const auto& a = PowerPC::ppcState.ps[inst.FA];
  const auto& b = PowerPC::ppcState.ps[inst.FB];
  const auto& c = PowerPC::ppcState.ps[inst.FC];
  const FPResult product =
      NI_madd(&PowerPC::ppcState.fpscr, a.PS0AsDouble(), c.PS0AsDouble(), b.PS0AsDouble());

  if (PowerPC::ppcState.fpscr.VE == 0 || product.HasNoInvalidExceptions())
  {
    const double result = ForceDouble(PowerPC::ppcState.fpscr, product.value);
    PowerPC::ppcState.ps[inst.FD].SetPS0(result);
    PowerPC::ppcState.UpdateFPRFDouble(result);
  }

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fmaddsx(Interpreter& interpreter, UGeckoInstruction inst)
{
  const auto& a = PowerPC::ppcState.ps[inst.FA];
  const auto& b = PowerPC::ppcState.ps[inst.FB];
  const auto& c = PowerPC::ppcState.ps[inst.FC];

  const double c_value = Force25Bit(c.PS0AsDouble());
  const FPResult d_value =
      NI_madd(&PowerPC::ppcState.fpscr, a.PS0AsDouble(), c_value, b.PS0AsDouble());

  if (PowerPC::ppcState.fpscr.VE == 0 || d_value.HasNoInvalidExceptions())
  {
    const float result = ForceSingle(PowerPC::ppcState.fpscr, d_value.value);

    PowerPC::ppcState.ps[inst.FD].Fill(result);
    PowerPC::ppcState.fpscr.FI = d_value.value != result;
    PowerPC::ppcState.fpscr.FR = 0;
    PowerPC::ppcState.UpdateFPRFSingle(result);
  }

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::faddx(Interpreter& interpreter, UGeckoInstruction inst)
{
  const auto& a = PowerPC::ppcState.ps[inst.FA];
  const auto& b = PowerPC::ppcState.ps[inst.FB];

  const FPResult sum = NI_add(&PowerPC::ppcState.fpscr, a.PS0AsDouble(), b.PS0AsDouble());

  if (PowerPC::ppcState.fpscr.VE == 0 || sum.HasNoInvalidExceptions())
  {
    const double result = ForceDouble(PowerPC::ppcState.fpscr, sum.value);
    PowerPC::ppcState.ps[inst.FD].SetPS0(result);
    PowerPC::ppcState.UpdateFPRFDouble(result);
  }

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}
void Interpreter::faddsx(Interpreter& interpreter, UGeckoInstruction inst)
{
  const auto& a = PowerPC::ppcState.ps[inst.FA];
  const auto& b = PowerPC::ppcState.ps[inst.FB];

  const FPResult sum = NI_add(&PowerPC::ppcState.fpscr, a.PS0AsDouble(), b.PS0AsDouble());

  if (PowerPC::ppcState.fpscr.VE == 0 || sum.HasNoInvalidExceptions())
  {
    const float result = ForceSingle(PowerPC::ppcState.fpscr, sum.value);
    PowerPC::ppcState.ps[inst.FD].Fill(result);
    PowerPC::ppcState.UpdateFPRFSingle(result);
  }

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fdivx(Interpreter& interpreter, UGeckoInstruction inst)
{
  const auto& a = PowerPC::ppcState.ps[inst.FA];
  const auto& b = PowerPC::ppcState.ps[inst.FB];

  const FPResult quotient = NI_div(&PowerPC::ppcState.fpscr, a.PS0AsDouble(), b.PS0AsDouble());
  const bool not_divide_by_zero = PowerPC::ppcState.fpscr.ZE == 0 || quotient.exception != FPSCR_ZX;
  const bool not_invalid = PowerPC::ppcState.fpscr.VE == 0 || quotient.HasNoInvalidExceptions();

  if (not_divide_by_zero && not_invalid)
  {
    const double result = ForceDouble(PowerPC::ppcState.fpscr, quotient.value);
    PowerPC::ppcState.ps[inst.FD].SetPS0(result);
    PowerPC::ppcState.UpdateFPRFDouble(result);
  }

  // FR,FI,OX,UX???
  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}
void Interpreter::fdivsx(Interpreter& interpreter, UGeckoInstruction inst)
{
  const auto& a = PowerPC::ppcState.ps[inst.FA];
  const auto& b = PowerPC::ppcState.ps[inst.FB];

  const FPResult quotient = NI_div(&PowerPC::ppcState.fpscr, a.PS0AsDouble(), b.PS0AsDouble());
  const bool not_divide_by_zero = PowerPC::ppcState.fpscr.ZE == 0 || quotient.exception != FPSCR_ZX;
  const bool not_invalid = PowerPC::ppcState.fpscr.VE == 0 || quotient.HasNoInvalidExceptions();

  if (not_divide_by_zero && not_invalid)
  {
    const float result = ForceSingle(PowerPC::ppcState.fpscr, quotient.value);
    PowerPC::ppcState.ps[inst.FD].Fill(result);
    PowerPC::ppcState.UpdateFPRFSingle(result);
  }

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

// Single precision only.
void Interpreter::fresx(Interpreter& interpreter, UGeckoInstruction inst)
{
  const double b = PowerPC::ppcState.ps[inst.FB].PS0AsDouble();

  const auto compute_result = [inst](double value) {
    const double result = Common::ApproximateReciprocal(value);
    PowerPC::ppcState.ps[inst.FD].Fill(result);
    PowerPC::ppcState.UpdateFPRFSingle(float(result));
  };

  if (b == 0.0)
  {
    SetFPException(&PowerPC::ppcState.fpscr, FPSCR_ZX);
    PowerPC::ppcState.fpscr.ClearFIFR();

    if (PowerPC::ppcState.fpscr.ZE == 0)
      compute_result(b);
  }
  else if (Common::IsSNAN(b))
  {
    SetFPException(&PowerPC::ppcState.fpscr, FPSCR_VXSNAN);
    PowerPC::ppcState.fpscr.ClearFIFR();

    if (PowerPC::ppcState.fpscr.VE == 0)
      compute_result(b);
  }
  else
  {
    if (std::isnan(b) || std::isinf(b))
      PowerPC::ppcState.fpscr.ClearFIFR();

    compute_result(b);
  }

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::frsqrtex(Interpreter& interpreter, UGeckoInstruction inst)
{
  const double b = PowerPC::ppcState.ps[inst.FB].PS0AsDouble();

  const auto compute_result = [inst](double value) {
    const double result = Common::ApproximateReciprocalSquareRoot(value);
    PowerPC::ppcState.ps[inst.FD].SetPS0(result);
    PowerPC::ppcState.UpdateFPRFDouble(result);
  };

  if (b < 0.0)
  {
    SetFPException(&PowerPC::ppcState.fpscr, FPSCR_VXSQRT);
    PowerPC::ppcState.fpscr.ClearFIFR();

    if (PowerPC::ppcState.fpscr.VE == 0)
      compute_result(b);
  }
  else if (b == 0.0)
  {
    SetFPException(&PowerPC::ppcState.fpscr, FPSCR_ZX);
    PowerPC::ppcState.fpscr.ClearFIFR();

    if (PowerPC::ppcState.fpscr.ZE == 0)
      compute_result(b);
  }
  else if (Common::IsSNAN(b))
  {
    SetFPException(&PowerPC::ppcState.fpscr, FPSCR_VXSNAN);
    PowerPC::ppcState.fpscr.ClearFIFR();

    if (PowerPC::ppcState.fpscr.VE == 0)
      compute_result(b);
  }
  else
  {
    if (std::isnan(b) || std::isinf(b))
      PowerPC::ppcState.fpscr.ClearFIFR();

    compute_result(b);
  }

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fmsubx(Interpreter& interpreter, UGeckoInstruction inst)
{
  const auto& a = PowerPC::ppcState.ps[inst.FA];
  const auto& b = PowerPC::ppcState.ps[inst.FB];
  const auto& c = PowerPC::ppcState.ps[inst.FC];

  const FPResult product =
      NI_msub(&PowerPC::ppcState.fpscr, a.PS0AsDouble(), c.PS0AsDouble(), b.PS0AsDouble());

  if (PowerPC::ppcState.fpscr.VE == 0 || product.HasNoInvalidExceptions())
  {
    const double result = ForceDouble(PowerPC::ppcState.fpscr, product.value);
    PowerPC::ppcState.ps[inst.FD].SetPS0(result);
    PowerPC::ppcState.UpdateFPRFDouble(result);
  }

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fmsubsx(Interpreter& interpreter, UGeckoInstruction inst)
{
  const auto& a = PowerPC::ppcState.ps[inst.FA];
  const auto& b = PowerPC::ppcState.ps[inst.FB];
  const auto& c = PowerPC::ppcState.ps[inst.FC];

  const double c_value = Force25Bit(c.PS0AsDouble());
  const FPResult product =
      NI_msub(&PowerPC::ppcState.fpscr, a.PS0AsDouble(), c_value, b.PS0AsDouble());

  if (PowerPC::ppcState.fpscr.VE == 0 || product.HasNoInvalidExceptions())
  {
    const float result = ForceSingle(PowerPC::ppcState.fpscr, product.value);
    PowerPC::ppcState.ps[inst.FD].Fill(result);
    PowerPC::ppcState.UpdateFPRFSingle(result);
  }

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fnmaddx(Interpreter& interpreter, UGeckoInstruction inst)
{
  const auto& a = PowerPC::ppcState.ps[inst.FA];
  const auto& b = PowerPC::ppcState.ps[inst.FB];
  const auto& c = PowerPC::ppcState.ps[inst.FC];

  const FPResult product =
      NI_madd(&PowerPC::ppcState.fpscr, a.PS0AsDouble(), c.PS0AsDouble(), b.PS0AsDouble());

  if (PowerPC::ppcState.fpscr.VE == 0 || product.HasNoInvalidExceptions())
  {
    const double tmp = ForceDouble(PowerPC::ppcState.fpscr, product.value);
    const double result = std::isnan(tmp) ? tmp : -tmp;

    PowerPC::ppcState.ps[inst.FD].SetPS0(result);
    PowerPC::ppcState.UpdateFPRFDouble(result);
  }

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fnmaddsx(Interpreter& interpreter, UGeckoInstruction inst)
{
  const auto& a = PowerPC::ppcState.ps[inst.FA];
  const auto& b = PowerPC::ppcState.ps[inst.FB];
  const auto& c = PowerPC::ppcState.ps[inst.FC];

  const double c_value = Force25Bit(c.PS0AsDouble());
  const FPResult product =
      NI_madd(&PowerPC::ppcState.fpscr, a.PS0AsDouble(), c_value, b.PS0AsDouble());

  if (PowerPC::ppcState.fpscr.VE == 0 || product.HasNoInvalidExceptions())
  {
    const float tmp = ForceSingle(PowerPC::ppcState.fpscr, product.value);
    const float result = std::isnan(tmp) ? tmp : -tmp;

    PowerPC::ppcState.ps[inst.FD].Fill(result);
    PowerPC::ppcState.UpdateFPRFSingle(result);
  }

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fnmsubx(Interpreter& interpreter, UGeckoInstruction inst)
{
  const auto& a = PowerPC::ppcState.ps[inst.FA];
  const auto& b = PowerPC::ppcState.ps[inst.FB];
  const auto& c = PowerPC::ppcState.ps[inst.FC];

  const FPResult product =
      NI_msub(&PowerPC::ppcState.fpscr, a.PS0AsDouble(), c.PS0AsDouble(), b.PS0AsDouble());

  if (PowerPC::ppcState.fpscr.VE == 0 || product.HasNoInvalidExceptions())
  {
    const double tmp = ForceDouble(PowerPC::ppcState.fpscr, product.value);
    const double result = std::isnan(tmp) ? tmp : -tmp;

    PowerPC::ppcState.ps[inst.FD].SetPS0(result);
    PowerPC::ppcState.UpdateFPRFDouble(result);
  }

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fnmsubsx(Interpreter& interpreter, UGeckoInstruction inst)
{
  const auto& a = PowerPC::ppcState.ps[inst.FA];
  const auto& b = PowerPC::ppcState.ps[inst.FB];
  const auto& c = PowerPC::ppcState.ps[inst.FC];

  const double c_value = Force25Bit(c.PS0AsDouble());
  const FPResult product =
      NI_msub(&PowerPC::ppcState.fpscr, a.PS0AsDouble(), c_value, b.PS0AsDouble());

  if (PowerPC::ppcState.fpscr.VE == 0 || product.HasNoInvalidExceptions())
  {
    const float tmp = ForceSingle(PowerPC::ppcState.fpscr, product.value);
    const float result = std::isnan(tmp) ? tmp : -tmp;

    PowerPC::ppcState.ps[inst.FD].Fill(result);
    PowerPC::ppcState.UpdateFPRFSingle(result);
  }

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fsubx(Interpreter& interpreter, UGeckoInstruction inst)
{
  const auto& a = PowerPC::ppcState.ps[inst.FA];
  const auto& b = PowerPC::ppcState.ps[inst.FB];

  const FPResult difference = NI_sub(&PowerPC::ppcState.fpscr, a.PS0AsDouble(), b.PS0AsDouble());

  if (PowerPC::ppcState.fpscr.VE == 0 || difference.HasNoInvalidExceptions())
  {
    const double result = ForceDouble(PowerPC::ppcState.fpscr, difference.value);
    PowerPC::ppcState.ps[inst.FD].SetPS0(result);
    PowerPC::ppcState.UpdateFPRFDouble(result);
  }

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::fsubsx(Interpreter& interpreter, UGeckoInstruction inst)
{
  const auto& a = PowerPC::ppcState.ps[inst.FA];
  const auto& b = PowerPC::ppcState.ps[inst.FB];

  const FPResult difference = NI_sub(&PowerPC::ppcState.fpscr, a.PS0AsDouble(), b.PS0AsDouble());

  if (PowerPC::ppcState.fpscr.VE == 0 || difference.HasNoInvalidExceptions())
  {
    const float result = ForceSingle(PowerPC::ppcState.fpscr, difference.value);
    PowerPC::ppcState.ps[inst.FD].Fill(result);
    PowerPC::ppcState.UpdateFPRFSingle(result);
  }

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}
