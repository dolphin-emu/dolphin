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

// Extremely rare - actually, never seen.
// Star Wars : Rogue Leader spams that at some point :|
void Interpreter::Helper_UpdateCR1()
{
  PowerPC::SetCRField(1, (FPSCR.FX << 3) | (FPSCR.FEX << 2) | (FPSCR.VX << 1) | FPSCR.OX);
}

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

// Apply current rounding mode
void Interpreter::fctiwx(UGeckoInstruction inst)
{
  const double b = rPS0(inst.FB);
  u32 value;

  if (b > (double)0x7fffffff)
  {
    value = 0x7fffffff;
    SetFPException(FPSCR_VXCVI);
    FPSCR.FI = 0;
    FPSCR.FR = 0;
  }
  else if (b < -(double)0x80000000)
  {
    value = 0x80000000;
    SetFPException(FPSCR_VXCVI);
    FPSCR.FI = 0;
    FPSCR.FR = 0;
  }
  else
  {
    s32 i = 0;
    switch (FPSCR.RN)
    {
    case 0:  // nearest
    {
      double t = b + 0.5;
      i = (s32)t;

      if (t - i < 0 || (t - i == 0 && b > 0))
      {
        i--;
      }
      break;
    }
    case 1:  // zero
      i = (s32)b;
      break;
    case 2:  // +inf
      i = (s32)b;
      if (b - i > 0)
      {
        i++;
      }
      break;
    case 3:  // -inf
      i = (s32)b;
      if (b - i < 0)
      {
        i--;
      }
      break;
    }
    value = (u32)i;
    double di = i;
    if (di == b)
    {
      FPSCR.FI = 0;
      FPSCR.FR = 0;
    }
    else
    {
      SetFI(1);
      FPSCR.FR = fabs(di) > fabs(b);
    }
  }

  // based on HW tests
  // FPRF is not affected
  riPS0(inst.FD) = 0xfff8000000000000ull | value;
  if (value == 0 && std::signbit(b))
    riPS0(inst.FD) |= 0x100000000ull;
  if (inst.Rc)
    Helper_UpdateCR1();
}

// Always round toward zero
void Interpreter::fctiwzx(UGeckoInstruction inst)
{
  const double b = rPS0(inst.FB);
  u32 value;

  if (b > (double)0x7fffffff)
  {
    value = 0x7fffffff;
    SetFPException(FPSCR_VXCVI);
    FPSCR.FI = 0;
    FPSCR.FR = 0;
  }
  else if (b < -(double)0x80000000)
  {
    value = 0x80000000;
    SetFPException(FPSCR_VXCVI);
    FPSCR.FI = 0;
    FPSCR.FR = 0;
  }
  else
  {
    s32 i = (s32)b;
    double di = i;
    if (di == b)
    {
      FPSCR.FI = 0;
      FPSCR.FR = 0;
    }
    else
    {
      SetFI(1);
      FPSCR.FR = fabs(di) > fabs(b);
    }
    value = (u32)i;
  }

  // based on HW tests
  // FPRF is not affected
  riPS0(inst.FD) = 0xfff8000000000000ull | value;
  if (value == 0 && std::signbit(b))
    riPS0(inst.FD) |= 0x100000000ull;
  if (inst.Rc)
    Helper_UpdateCR1();
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

    SetFI(0);
    FPSCR.FR = 0;
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
  rPS0(inst.FD) = ForceDouble(NI_mul(rPS0(inst.FA), rPS0(inst.FC)));
  FPSCR.FI = 0;  // are these flags important?
  FPSCR.FR = 0;
  PowerPC::UpdateFPRF(rPS0(inst.FD));

  if (inst.Rc)
    Helper_UpdateCR1();
}
void Interpreter::fmulsx(UGeckoInstruction inst)
{
  double c_value = Force25Bit(rPS0(inst.FC));
  double d_value = NI_mul(rPS0(inst.FA), c_value);
  rPS0(inst.FD) = rPS1(inst.FD) = ForceSingle(d_value);
  // FPSCR.FI = d_value != rPS0(_inst.FD);
  FPSCR.FI = 0;
  FPSCR.FR = 0;
  PowerPC::UpdateFPRF(rPS0(inst.FD));

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::fmaddx(UGeckoInstruction inst)
{
  double result = ForceDouble(NI_madd(rPS0(inst.FA), rPS0(inst.FC), rPS0(inst.FB)));
  rPS0(inst.FD) = result;
  PowerPC::UpdateFPRF(result);

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::fmaddsx(UGeckoInstruction inst)
{
  double c_value = Force25Bit(rPS0(inst.FC));
  double d_value = NI_madd(rPS0(inst.FA), c_value, rPS0(inst.FB));
  rPS0(inst.FD) = rPS1(inst.FD) = ForceSingle(d_value);
  FPSCR.FI = d_value != rPS0(inst.FD);
  FPSCR.FR = 0;
  PowerPC::UpdateFPRF(rPS0(inst.FD));

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::faddx(UGeckoInstruction inst)
{
  rPS0(inst.FD) = ForceDouble(NI_add(rPS0(inst.FA), rPS0(inst.FB)));
  PowerPC::UpdateFPRF(rPS0(inst.FD));

  if (inst.Rc)
    Helper_UpdateCR1();
}
void Interpreter::faddsx(UGeckoInstruction inst)
{
  rPS0(inst.FD) = rPS1(inst.FD) = ForceSingle(NI_add(rPS0(inst.FA), rPS0(inst.FB)));
  PowerPC::UpdateFPRF(rPS0(inst.FD));

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::fdivx(UGeckoInstruction inst)
{
  rPS0(inst.FD) = ForceDouble(NI_div(rPS0(inst.FA), rPS0(inst.FB)));
  PowerPC::UpdateFPRF(rPS0(inst.FD));

  // FR,FI,OX,UX???
  if (inst.Rc)
    Helper_UpdateCR1();
}
void Interpreter::fdivsx(UGeckoInstruction inst)
{
  rPS0(inst.FD) = rPS1(inst.FD) = ForceSingle(NI_div(rPS0(inst.FA), rPS0(inst.FB)));
  PowerPC::UpdateFPRF(rPS0(inst.FD));

  if (inst.Rc)
    Helper_UpdateCR1();
}

// Single precision only.
void Interpreter::fresx(UGeckoInstruction inst)
{
  const double b = rPS0(inst.FB);
  const double result = Common::ApproximateReciprocal(b);

  rPS0(inst.FD) = rPS1(inst.FD) = result;

  if (b == 0.0)
  {
    SetFPException(FPSCR_ZX);

    if (FPSCR.ZE == 0)
      PowerPC::UpdateFPRF(result);
  }
  else if (Common::IsSNAN(b))
  {
    SetFPException(FPSCR_VXSNAN);

    if (FPSCR.VE == 0)
      PowerPC::UpdateFPRF(result);
  }
  else
  {
    PowerPC::UpdateFPRF(result);
  }

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::frsqrtex(UGeckoInstruction inst)
{
  const double b = rPS0(inst.FB);
  const double result = Common::ApproximateReciprocalSquareRoot(b);

  if (b < 0.0)
  {
    SetFPException(FPSCR_VXSQRT);

    if (FPSCR.VE == 0)
      PowerPC::UpdateFPRF(result);
  }
  else if (b == 0.0)
  {
    SetFPException(FPSCR_ZX);

    if (FPSCR.ZE == 0)
      PowerPC::UpdateFPRF(result);
  }
  else if (Common::IsSNAN(b))
  {
    SetFPException(FPSCR_VXSNAN);

    if (FPSCR.VE == 0)
      PowerPC::UpdateFPRF(result);
  }
  else
  {
    PowerPC::UpdateFPRF(result);
  }

  rPS0(inst.FD) = result;

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::fmsubx(UGeckoInstruction _inst)
{
  rPS0(_inst.FD) = ForceDouble(NI_msub(rPS0(_inst.FA), rPS0(_inst.FC), rPS0(_inst.FB)));
  PowerPC::UpdateFPRF(rPS0(_inst.FD));

  if (_inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::fmsubsx(UGeckoInstruction inst)
{
  double c_value = Force25Bit(rPS0(inst.FC));
  rPS0(inst.FD) = rPS1(inst.FD) = ForceSingle(NI_msub(rPS0(inst.FA), c_value, rPS0(inst.FB)));
  PowerPC::UpdateFPRF(rPS0(inst.FD));

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::fnmaddx(UGeckoInstruction inst)
{
  double result = ForceDouble(NI_madd(rPS0(inst.FA), rPS0(inst.FC), rPS0(inst.FB)));
  rPS0(inst.FD) = std::isnan(result) ? result : -result;
  PowerPC::UpdateFPRF(rPS0(inst.FD));

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::fnmaddsx(UGeckoInstruction inst)
{
  double c_value = Force25Bit(rPS0(inst.FC));
  double result = ForceSingle(NI_madd(rPS0(inst.FA), c_value, rPS0(inst.FB)));
  rPS0(inst.FD) = rPS1(inst.FD) = std::isnan(result) ? result : -result;
  PowerPC::UpdateFPRF(rPS0(inst.FD));

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::fnmsubx(UGeckoInstruction inst)
{
  double result = ForceDouble(NI_msub(rPS0(inst.FA), rPS0(inst.FC), rPS0(inst.FB)));
  rPS0(inst.FD) = std::isnan(result) ? result : -result;
  PowerPC::UpdateFPRF(rPS0(inst.FD));

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::fnmsubsx(UGeckoInstruction inst)
{
  double c_value = Force25Bit(rPS0(inst.FC));
  double result = ForceSingle(NI_msub(rPS0(inst.FA), c_value, rPS0(inst.FB)));
  rPS0(inst.FD) = rPS1(inst.FD) = std::isnan(result) ? result : -result;
  PowerPC::UpdateFPRF(rPS0(inst.FD));

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::fsubx(UGeckoInstruction inst)
{
  rPS0(inst.FD) = ForceDouble(NI_sub(rPS0(inst.FA), rPS0(inst.FB)));
  PowerPC::UpdateFPRF(rPS0(inst.FD));

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::fsubsx(UGeckoInstruction inst)
{
  rPS0(inst.FD) = rPS1(inst.FD) = ForceSingle(NI_sub(rPS0(inst.FA), rPS0(inst.FB)));
  PowerPC::UpdateFPRF(rPS0(inst.FD));

  if (inst.Rc)
    Helper_UpdateCR1();
}
