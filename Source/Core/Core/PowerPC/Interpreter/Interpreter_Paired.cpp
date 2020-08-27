// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cmath>

#include "Common/CommonTypes.h"
#include "Common/FloatUtils.h"
#include "Core/PowerPC/Interpreter/Interpreter.h"
#include "Core/PowerPC/Interpreter/Interpreter_FPUtils.h"
#include "Core/PowerPC/PowerPC.h"

// These "binary instructions" do not alter FPSCR.
void Interpreter::ps_sel(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& b = rPS(inst.FB);
  const auto& c = rPS(inst.FC);

  rPS(inst.FD).SetBoth(a.PS0AsDouble() >= -0.0 ? c.PS0AsDouble() : b.PS0AsDouble(),
                       a.PS1AsDouble() >= -0.0 ? c.PS1AsDouble() : b.PS1AsDouble());

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_neg(UGeckoInstruction inst)
{
  const auto& b = rPS(inst.FB);

  rPS(inst.FD).SetBoth(b.PS0AsU64() ^ (UINT64_C(1) << 63), b.PS1AsU64() ^ (UINT64_C(1) << 63));

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_mr(UGeckoInstruction inst)
{
  rPS(inst.FD) = rPS(inst.FB);

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_nabs(UGeckoInstruction inst)
{
  const auto& b = rPS(inst.FB);

  rPS(inst.FD).SetBoth(b.PS0AsU64() | (UINT64_C(1) << 63), b.PS1AsU64() | (UINT64_C(1) << 63));

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_abs(UGeckoInstruction inst)
{
  const auto& b = rPS(inst.FB);

  rPS(inst.FD).SetBoth(b.PS0AsU64() & ~(UINT64_C(1) << 63), b.PS1AsU64() & ~(UINT64_C(1) << 63));

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

// These are just moves, double is OK.
void Interpreter::ps_merge00(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& b = rPS(inst.FB);

  rPS(inst.FD).SetBoth(a.PS0AsDouble(), b.PS0AsDouble());

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_merge01(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& b = rPS(inst.FB);

  rPS(inst.FD).SetBoth(a.PS0AsDouble(), b.PS1AsDouble());

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_merge10(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& b = rPS(inst.FB);

  rPS(inst.FD).SetBoth(a.PS1AsDouble(), b.PS0AsDouble());

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_merge11(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& b = rPS(inst.FB);

  rPS(inst.FD).SetBoth(a.PS1AsDouble(), b.PS1AsDouble());

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

// From here on, the real deal.
void Interpreter::ps_div(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& b = rPS(inst.FB);

  const double ps0 = ForceSingle(FPSCR, NI_div(&FPSCR, a.PS0AsDouble(), b.PS0AsDouble()).value);
  const double ps1 = ForceSingle(FPSCR, NI_div(&FPSCR, a.PS1AsDouble(), b.PS1AsDouble()).value);

  rPS(inst.FD).SetBoth(ps0, ps1);
  PowerPC::UpdateFPRF(ps0);

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_res(UGeckoInstruction inst)
{
  // this code is based on the real hardware tests
  const double a = rPS(inst.FB).PS0AsDouble();
  const double b = rPS(inst.FB).PS1AsDouble();

  if (a == 0.0 || b == 0.0)
  {
    SetFPException(&FPSCR, FPSCR_ZX);
    FPSCR.ClearFIFR();
  }

  if (std::isnan(a) || std::isinf(a) || std::isnan(b) || std::isinf(b))
    FPSCR.ClearFIFR();

  if (Common::IsSNAN(a) || Common::IsSNAN(b))
    SetFPException(&FPSCR, FPSCR_VXSNAN);

  const double ps0 = Common::ApproximateReciprocal(a);
  const double ps1 = Common::ApproximateReciprocal(b);

  rPS(inst.FD).SetBoth(ps0, ps1);
  PowerPC::UpdateFPRF(ps0);

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_rsqrte(UGeckoInstruction inst)
{
  const double ps0 = rPS(inst.FB).PS0AsDouble();
  const double ps1 = rPS(inst.FB).PS1AsDouble();

  if (ps0 == 0.0 || ps1 == 0.0)
  {
    SetFPException(&FPSCR, FPSCR_ZX);
    FPSCR.ClearFIFR();
  }

  if (ps0 < 0.0 || ps1 < 0.0)
  {
    SetFPException(&FPSCR, FPSCR_VXSQRT);
    FPSCR.ClearFIFR();
  }

  if (std::isnan(ps0) || std::isinf(ps0) || std::isnan(ps1) || std::isinf(ps1))
    FPSCR.ClearFIFR();

  if (Common::IsSNAN(ps0) || Common::IsSNAN(ps1))
    SetFPException(&FPSCR, FPSCR_VXSNAN);

  const double dst_ps0 = ForceSingle(FPSCR, Common::ApproximateReciprocalSquareRoot(ps0));
  const double dst_ps1 = ForceSingle(FPSCR, Common::ApproximateReciprocalSquareRoot(ps1));

  rPS(inst.FD).SetBoth(dst_ps0, dst_ps1);
  PowerPC::UpdateFPRF(dst_ps0);

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_sub(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& b = rPS(inst.FB);

  const double ps0 = ForceSingle(FPSCR, NI_sub(&FPSCR, a.PS0AsDouble(), b.PS0AsDouble()).value);
  const double ps1 = ForceSingle(FPSCR, NI_sub(&FPSCR, a.PS1AsDouble(), b.PS1AsDouble()).value);

  rPS(inst.FD).SetBoth(ps0, ps1);
  PowerPC::UpdateFPRF(ps0);

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_add(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& b = rPS(inst.FB);

  const double ps0 = ForceSingle(FPSCR, NI_add(&FPSCR, a.PS0AsDouble(), b.PS0AsDouble()).value);
  const double ps1 = ForceSingle(FPSCR, NI_add(&FPSCR, a.PS1AsDouble(), b.PS1AsDouble()).value);

  rPS(inst.FD).SetBoth(ps0, ps1);
  PowerPC::UpdateFPRF(ps0);

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_mul(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& c = rPS(inst.FC);

  const double c0 = Force25Bit(c.PS0AsDouble());
  const double c1 = Force25Bit(c.PS1AsDouble());

  const double ps0 = ForceSingle(FPSCR, NI_mul(&FPSCR, a.PS0AsDouble(), c0).value);
  const double ps1 = ForceSingle(FPSCR, NI_mul(&FPSCR, a.PS1AsDouble(), c1).value);

  rPS(inst.FD).SetBoth(ps0, ps1);
  PowerPC::UpdateFPRF(ps0);

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_msub(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& b = rPS(inst.FB);
  const auto& c = rPS(inst.FC);

  const double c0 = Force25Bit(c.PS0AsDouble());
  const double c1 = Force25Bit(c.PS1AsDouble());

  const double ps0 =
      ForceSingle(FPSCR, NI_msub(&FPSCR, a.PS0AsDouble(), c0, b.PS0AsDouble()).value);
  const double ps1 =
      ForceSingle(FPSCR, NI_msub(&FPSCR, a.PS1AsDouble(), c1, b.PS1AsDouble()).value);

  rPS(inst.FD).SetBoth(ps0, ps1);
  PowerPC::UpdateFPRF(ps0);

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_madd(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& b = rPS(inst.FB);
  const auto& c = rPS(inst.FC);

  const double c0 = Force25Bit(c.PS0AsDouble());
  const double c1 = Force25Bit(c.PS1AsDouble());

  const double ps0 =
      ForceSingle(FPSCR, NI_madd(&FPSCR, a.PS0AsDouble(), c0, b.PS0AsDouble()).value);
  const double ps1 =
      ForceSingle(FPSCR, NI_madd(&FPSCR, a.PS1AsDouble(), c1, b.PS1AsDouble()).value);

  rPS(inst.FD).SetBoth(ps0, ps1);
  PowerPC::UpdateFPRF(ps0);

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_nmsub(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& b = rPS(inst.FB);
  const auto& c = rPS(inst.FC);

  const double c0 = Force25Bit(c.PS0AsDouble());
  const double c1 = Force25Bit(c.PS1AsDouble());

  const double tmp0 =
      ForceSingle(FPSCR, NI_msub(&FPSCR, a.PS0AsDouble(), c0, b.PS0AsDouble()).value);
  const double tmp1 =
      ForceSingle(FPSCR, NI_msub(&FPSCR, a.PS1AsDouble(), c1, b.PS1AsDouble()).value);

  const double ps0 = std::isnan(tmp0) ? tmp0 : -tmp0;
  const double ps1 = std::isnan(tmp1) ? tmp1 : -tmp1;

  rPS(inst.FD).SetBoth(ps0, ps1);
  PowerPC::UpdateFPRF(ps0);

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_nmadd(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& b = rPS(inst.FB);
  const auto& c = rPS(inst.FC);

  const double c0 = Force25Bit(c.PS0AsDouble());
  const double c1 = Force25Bit(c.PS1AsDouble());

  const double tmp0 =
      ForceSingle(FPSCR, NI_madd(&FPSCR, a.PS0AsDouble(), c0, b.PS0AsDouble()).value);
  const double tmp1 =
      ForceSingle(FPSCR, NI_madd(&FPSCR, a.PS1AsDouble(), c1, b.PS1AsDouble()).value);

  const double ps0 = std::isnan(tmp0) ? tmp0 : -tmp0;
  const double ps1 = std::isnan(tmp1) ? tmp1 : -tmp1;

  rPS(inst.FD).SetBoth(ps0, ps1);
  PowerPC::UpdateFPRF(ps0);

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_sum0(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& b = rPS(inst.FB);
  const auto& c = rPS(inst.FC);

  const double ps0 = ForceSingle(FPSCR, NI_add(&FPSCR, a.PS0AsDouble(), b.PS1AsDouble()).value);
  const double ps1 = ForceSingle(FPSCR, c.PS1AsDouble());

  rPS(inst.FD).SetBoth(ps0, ps1);
  PowerPC::UpdateFPRF(ps0);

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_sum1(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& b = rPS(inst.FB);
  const auto& c = rPS(inst.FC);

  const double ps0 = ForceSingle(FPSCR, c.PS0AsDouble());
  const double ps1 = ForceSingle(FPSCR, NI_add(&FPSCR, a.PS0AsDouble(), b.PS1AsDouble()).value);

  rPS(inst.FD).SetBoth(ps0, ps1);
  PowerPC::UpdateFPRF(ps1);

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_muls0(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& c = rPS(inst.FC);

  const double c0 = Force25Bit(c.PS0AsDouble());
  const double ps0 = ForceSingle(FPSCR, NI_mul(&FPSCR, a.PS0AsDouble(), c0).value);
  const double ps1 = ForceSingle(FPSCR, NI_mul(&FPSCR, a.PS1AsDouble(), c0).value);

  rPS(inst.FD).SetBoth(ps0, ps1);
  PowerPC::UpdateFPRF(ps0);

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_muls1(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& c = rPS(inst.FC);

  const double c1 = Force25Bit(c.PS1AsDouble());
  const double ps0 = ForceSingle(FPSCR, NI_mul(&FPSCR, a.PS0AsDouble(), c1).value);
  const double ps1 = ForceSingle(FPSCR, NI_mul(&FPSCR, a.PS1AsDouble(), c1).value);

  rPS(inst.FD).SetBoth(ps0, ps1);
  PowerPC::UpdateFPRF(ps0);

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_madds0(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& b = rPS(inst.FB);
  const auto& c = rPS(inst.FC);

  const double c0 = Force25Bit(c.PS0AsDouble());
  const double ps0 =
      ForceSingle(FPSCR, NI_madd(&FPSCR, a.PS0AsDouble(), c0, b.PS0AsDouble()).value);
  const double ps1 =
      ForceSingle(FPSCR, NI_madd(&FPSCR, a.PS1AsDouble(), c0, b.PS1AsDouble()).value);

  rPS(inst.FD).SetBoth(ps0, ps1);
  PowerPC::UpdateFPRF(ps0);

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_madds1(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& b = rPS(inst.FB);
  const auto& c = rPS(inst.FC);

  const double c1 = Force25Bit(c.PS1AsDouble());
  const double ps0 =
      ForceSingle(FPSCR, NI_madd(&FPSCR, a.PS0AsDouble(), c1, b.PS0AsDouble()).value);
  const double ps1 =
      ForceSingle(FPSCR, NI_madd(&FPSCR, a.PS1AsDouble(), c1, b.PS1AsDouble()).value);

  rPS(inst.FD).SetBoth(ps0, ps1);
  PowerPC::UpdateFPRF(ps0);

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_cmpu0(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& b = rPS(inst.FB);

  Helper_FloatCompareUnordered(inst, a.PS0AsDouble(), b.PS0AsDouble());
}

void Interpreter::ps_cmpo0(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& b = rPS(inst.FB);

  Helper_FloatCompareOrdered(inst, a.PS0AsDouble(), b.PS0AsDouble());
}

void Interpreter::ps_cmpu1(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& b = rPS(inst.FB);

  Helper_FloatCompareUnordered(inst, a.PS1AsDouble(), b.PS1AsDouble());
}

void Interpreter::ps_cmpo1(UGeckoInstruction inst)
{
  const auto& a = rPS(inst.FA);
  const auto& b = rPS(inst.FB);

  Helper_FloatCompareOrdered(inst, a.PS1AsDouble(), b.PS1AsDouble());
}
