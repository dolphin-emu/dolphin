// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/Interpreter/Interpreter.h"

#include <cmath>

#include "Common/CommonTypes.h"
#include "Common/FloatUtils.h"
#include "Core/PowerPC/Interpreter/Interpreter_FPUtils.h"
#include "Core/PowerPC/PowerPC.h"

// These "binary instructions" do not alter FPSCR.
void Interpreter::ps_sel(GeckoInstruction inst)
{
  const auto& a = rPS(inst.FA());
  const auto& b = rPS(inst.FB());
  const auto& c = rPS(inst.FC());

  rPS(inst.FD()).SetBoth(a.PS0AsDouble() >= -0.0 ? c.PS0AsDouble() : b.PS0AsDouble(),
                         a.PS1AsDouble() >= -0.0 ? c.PS1AsDouble() : b.PS1AsDouble());

  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_neg(GeckoInstruction inst)
{
  const auto& b = rPS(inst.FB());

  rPS(inst.FD()).SetBoth(b.PS0AsU64() ^ (UINT64_C(1) << 63), b.PS1AsU64() ^ (UINT64_C(1) << 63));

  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_mr(GeckoInstruction inst)
{
  rPS(inst.FD()) = rPS(inst.FB());

  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_nabs(GeckoInstruction inst)
{
  const auto& b = rPS(inst.FB());

  rPS(inst.FD()).SetBoth(b.PS0AsU64() | (UINT64_C(1) << 63), b.PS1AsU64() | (UINT64_C(1) << 63));

  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_abs(GeckoInstruction inst)
{
  const auto& b = rPS(inst.FB());

  rPS(inst.FD()).SetBoth(b.PS0AsU64() & ~(UINT64_C(1) << 63), b.PS1AsU64() & ~(UINT64_C(1) << 63));

  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}

// These are just moves, double is OK.
void Interpreter::ps_merge00(GeckoInstruction inst)
{
  const auto& a = rPS(inst.FA());
  const auto& b = rPS(inst.FB());

  rPS(inst.FD()).SetBoth(a.PS0AsDouble(), b.PS0AsDouble());

  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_merge01(GeckoInstruction inst)
{
  const auto& a = rPS(inst.FA());
  const auto& b = rPS(inst.FB());

  rPS(inst.FD()).SetBoth(a.PS0AsDouble(), b.PS1AsDouble());

  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_merge10(GeckoInstruction inst)
{
  const auto& a = rPS(inst.FA());
  const auto& b = rPS(inst.FB());

  rPS(inst.FD()).SetBoth(a.PS1AsDouble(), b.PS0AsDouble());

  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_merge11(GeckoInstruction inst)
{
  const auto& a = rPS(inst.FA());
  const auto& b = rPS(inst.FB());

  rPS(inst.FD()).SetBoth(a.PS1AsDouble(), b.PS1AsDouble());

  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}

// From here on, the real deal.
void Interpreter::ps_div(GeckoInstruction inst)
{
  const auto& a = rPS(inst.FA());
  const auto& b = rPS(inst.FB());

  const float ps0 = ForceSingle(FPSCR, NI_div(&FPSCR, a.PS0AsDouble(), b.PS0AsDouble()).value);
  const float ps1 = ForceSingle(FPSCR, NI_div(&FPSCR, a.PS1AsDouble(), b.PS1AsDouble()).value);

  rPS(inst.FD()).SetBoth(ps0, ps1);
  PowerPC::UpdateFPRFSingle(ps0);

  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_res(GeckoInstruction inst)
{
  // this code is based on the real hardware tests
  const double a = rPS(inst.FB()).PS0AsDouble();
  const double b = rPS(inst.FB()).PS1AsDouble();

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

  rPS(inst.FD()).SetBoth(ps0, ps1);
  PowerPC::UpdateFPRFSingle(float(ps0));

  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_rsqrte(GeckoInstruction inst)
{
  const double ps0 = rPS(inst.FB()).PS0AsDouble();
  const double ps1 = rPS(inst.FB()).PS1AsDouble();

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

  const float dst_ps0 = ForceSingle(FPSCR, Common::ApproximateReciprocalSquareRoot(ps0));
  const float dst_ps1 = ForceSingle(FPSCR, Common::ApproximateReciprocalSquareRoot(ps1));

  rPS(inst.FD()).SetBoth(dst_ps0, dst_ps1);
  PowerPC::UpdateFPRFSingle(dst_ps0);

  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_sub(GeckoInstruction inst)
{
  const auto& a = rPS(inst.FA());
  const auto& b = rPS(inst.FB());

  const float ps0 = ForceSingle(FPSCR, NI_sub(&FPSCR, a.PS0AsDouble(), b.PS0AsDouble()).value);
  const float ps1 = ForceSingle(FPSCR, NI_sub(&FPSCR, a.PS1AsDouble(), b.PS1AsDouble()).value);

  rPS(inst.FD()).SetBoth(ps0, ps1);
  PowerPC::UpdateFPRFSingle(ps0);

  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_add(GeckoInstruction inst)
{
  const auto& a = rPS(inst.FA());
  const auto& b = rPS(inst.FB());

  const float ps0 = ForceSingle(FPSCR, NI_add(&FPSCR, a.PS0AsDouble(), b.PS0AsDouble()).value);
  const float ps1 = ForceSingle(FPSCR, NI_add(&FPSCR, a.PS1AsDouble(), b.PS1AsDouble()).value);

  rPS(inst.FD()).SetBoth(ps0, ps1);
  PowerPC::UpdateFPRFSingle(ps0);

  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_mul(GeckoInstruction inst)
{
  const auto& a = rPS(inst.FA());
  const auto& c = rPS(inst.FC());

  const double c0 = Force25Bit(c.PS0AsDouble());
  const double c1 = Force25Bit(c.PS1AsDouble());

  const float ps0 = ForceSingle(FPSCR, NI_mul(&FPSCR, a.PS0AsDouble(), c0).value);
  const float ps1 = ForceSingle(FPSCR, NI_mul(&FPSCR, a.PS1AsDouble(), c1).value);

  rPS(inst.FD()).SetBoth(ps0, ps1);
  PowerPC::UpdateFPRFSingle(ps0);

  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_msub(GeckoInstruction inst)
{
  const auto& a = rPS(inst.FA());
  const auto& b = rPS(inst.FB());
  const auto& c = rPS(inst.FC());

  const double c0 = Force25Bit(c.PS0AsDouble());
  const double c1 = Force25Bit(c.PS1AsDouble());

  const float ps0 = ForceSingle(FPSCR, NI_msub(&FPSCR, a.PS0AsDouble(), c0, b.PS0AsDouble()).value);
  const float ps1 = ForceSingle(FPSCR, NI_msub(&FPSCR, a.PS1AsDouble(), c1, b.PS1AsDouble()).value);

  rPS(inst.FD()).SetBoth(ps0, ps1);
  PowerPC::UpdateFPRFSingle(ps0);

  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_madd(GeckoInstruction inst)
{
  const auto& a = rPS(inst.FA());
  const auto& b = rPS(inst.FB());
  const auto& c = rPS(inst.FC());

  const double c0 = Force25Bit(c.PS0AsDouble());
  const double c1 = Force25Bit(c.PS1AsDouble());

  const float ps0 = ForceSingle(FPSCR, NI_madd(&FPSCR, a.PS0AsDouble(), c0, b.PS0AsDouble()).value);
  const float ps1 = ForceSingle(FPSCR, NI_madd(&FPSCR, a.PS1AsDouble(), c1, b.PS1AsDouble()).value);

  rPS(inst.FD()).SetBoth(ps0, ps1);
  PowerPC::UpdateFPRFSingle(ps0);

  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_nmsub(GeckoInstruction inst)
{
  const auto& a = rPS(inst.FA());
  const auto& b = rPS(inst.FB());
  const auto& c = rPS(inst.FC());

  const double c0 = Force25Bit(c.PS0AsDouble());
  const double c1 = Force25Bit(c.PS1AsDouble());

  const float tmp0 =
      ForceSingle(FPSCR, NI_msub(&FPSCR, a.PS0AsDouble(), c0, b.PS0AsDouble()).value);
  const float tmp1 =
      ForceSingle(FPSCR, NI_msub(&FPSCR, a.PS1AsDouble(), c1, b.PS1AsDouble()).value);

  const float ps0 = std::isnan(tmp0) ? tmp0 : -tmp0;
  const float ps1 = std::isnan(tmp1) ? tmp1 : -tmp1;

  rPS(inst.FD()).SetBoth(ps0, ps1);
  PowerPC::UpdateFPRFSingle(ps0);

  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_nmadd(GeckoInstruction inst)
{
  const auto& a = rPS(inst.FA());
  const auto& b = rPS(inst.FB());
  const auto& c = rPS(inst.FC());

  const double c0 = Force25Bit(c.PS0AsDouble());
  const double c1 = Force25Bit(c.PS1AsDouble());

  const float tmp0 =
      ForceSingle(FPSCR, NI_madd(&FPSCR, a.PS0AsDouble(), c0, b.PS0AsDouble()).value);
  const float tmp1 =
      ForceSingle(FPSCR, NI_madd(&FPSCR, a.PS1AsDouble(), c1, b.PS1AsDouble()).value);

  const float ps0 = std::isnan(tmp0) ? tmp0 : -tmp0;
  const float ps1 = std::isnan(tmp1) ? tmp1 : -tmp1;

  rPS(inst.FD()).SetBoth(ps0, ps1);
  PowerPC::UpdateFPRFSingle(ps0);

  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_sum0(GeckoInstruction inst)
{
  const auto& a = rPS(inst.FA());
  const auto& b = rPS(inst.FB());
  const auto& c = rPS(inst.FC());

  const float ps0 = ForceSingle(FPSCR, NI_add(&FPSCR, a.PS0AsDouble(), b.PS1AsDouble()).value);
  const float ps1 = ForceSingle(FPSCR, c.PS1AsDouble());

  rPS(inst.FD()).SetBoth(ps0, ps1);
  PowerPC::UpdateFPRFSingle(ps0);

  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_sum1(GeckoInstruction inst)
{
  const auto& a = rPS(inst.FA());
  const auto& b = rPS(inst.FB());
  const auto& c = rPS(inst.FC());

  const float ps0 = ForceSingle(FPSCR, c.PS0AsDouble());
  const float ps1 = ForceSingle(FPSCR, NI_add(&FPSCR, a.PS0AsDouble(), b.PS1AsDouble()).value);

  rPS(inst.FD()).SetBoth(ps0, ps1);
  PowerPC::UpdateFPRFSingle(ps1);

  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_muls0(GeckoInstruction inst)
{
  const auto& a = rPS(inst.FA());
  const auto& c = rPS(inst.FC());

  const double c0 = Force25Bit(c.PS0AsDouble());
  const float ps0 = ForceSingle(FPSCR, NI_mul(&FPSCR, a.PS0AsDouble(), c0).value);
  const float ps1 = ForceSingle(FPSCR, NI_mul(&FPSCR, a.PS1AsDouble(), c0).value);

  rPS(inst.FD()).SetBoth(ps0, ps1);
  PowerPC::UpdateFPRFSingle(ps0);

  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_muls1(GeckoInstruction inst)
{
  const auto& a = rPS(inst.FA());
  const auto& c = rPS(inst.FC());

  const double c1 = Force25Bit(c.PS1AsDouble());
  const float ps0 = ForceSingle(FPSCR, NI_mul(&FPSCR, a.PS0AsDouble(), c1).value);
  const float ps1 = ForceSingle(FPSCR, NI_mul(&FPSCR, a.PS1AsDouble(), c1).value);

  rPS(inst.FD()).SetBoth(ps0, ps1);
  PowerPC::UpdateFPRFSingle(ps0);

  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_madds0(GeckoInstruction inst)
{
  const auto& a = rPS(inst.FA());
  const auto& b = rPS(inst.FB());
  const auto& c = rPS(inst.FC());

  const double c0 = Force25Bit(c.PS0AsDouble());
  const float ps0 = ForceSingle(FPSCR, NI_madd(&FPSCR, a.PS0AsDouble(), c0, b.PS0AsDouble()).value);
  const float ps1 = ForceSingle(FPSCR, NI_madd(&FPSCR, a.PS1AsDouble(), c0, b.PS1AsDouble()).value);

  rPS(inst.FD()).SetBoth(ps0, ps1);
  PowerPC::UpdateFPRFSingle(ps0);

  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_madds1(GeckoInstruction inst)
{
  const auto& a = rPS(inst.FA());
  const auto& b = rPS(inst.FB());
  const auto& c = rPS(inst.FC());

  const double c1 = Force25Bit(c.PS1AsDouble());
  const float ps0 = ForceSingle(FPSCR, NI_madd(&FPSCR, a.PS0AsDouble(), c1, b.PS0AsDouble()).value);
  const float ps1 = ForceSingle(FPSCR, NI_madd(&FPSCR, a.PS1AsDouble(), c1, b.PS1AsDouble()).value);

  rPS(inst.FD()).SetBoth(ps0, ps1);
  PowerPC::UpdateFPRFSingle(ps0);

  if (inst.Rc())
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_cmpu0(GeckoInstruction inst)
{
  const auto& a = rPS(inst.FA());
  const auto& b = rPS(inst.FB());

  Helper_FloatCompareUnordered(inst, a.PS0AsDouble(), b.PS0AsDouble());
}

void Interpreter::ps_cmpo0(GeckoInstruction inst)
{
  const auto& a = rPS(inst.FA());
  const auto& b = rPS(inst.FB());

  Helper_FloatCompareOrdered(inst, a.PS0AsDouble(), b.PS0AsDouble());
}

void Interpreter::ps_cmpu1(GeckoInstruction inst)
{
  const auto& a = rPS(inst.FA());
  const auto& b = rPS(inst.FB());

  Helper_FloatCompareUnordered(inst, a.PS1AsDouble(), b.PS1AsDouble());
}

void Interpreter::ps_cmpo1(GeckoInstruction inst)
{
  const auto& a = rPS(inst.FA());
  const auto& b = rPS(inst.FB());

  Helper_FloatCompareOrdered(inst, a.PS1AsDouble(), b.PS1AsDouble());
}
