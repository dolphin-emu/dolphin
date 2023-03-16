// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/PowerPC/Interpreter/Interpreter.h"

#include <cmath>

#include "Common/CommonTypes.h"
#include "Common/FloatUtils.h"
#include "Core/PowerPC/Interpreter/Interpreter_FPUtils.h"
#include "Core/PowerPC/PowerPC.h"

// These "binary instructions" do not alter FPSCR.
void Interpreter::ps_sel(Interpreter& interpreter, UGeckoInstruction inst)
{
  const auto& a = PowerPC::ppcState.ps[inst.FA];
  const auto& b = PowerPC::ppcState.ps[inst.FB];
  const auto& c = PowerPC::ppcState.ps[inst.FC];

  PowerPC::ppcState.ps[inst.FD].SetBoth(a.PS0AsDouble() >= -0.0 ? c.PS0AsDouble() : b.PS0AsDouble(),
                                        a.PS1AsDouble() >= -0.0 ? c.PS1AsDouble() :
                                                                  b.PS1AsDouble());

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_neg(Interpreter& interpreter, UGeckoInstruction inst)
{
  const auto& b = PowerPC::ppcState.ps[inst.FB];

  PowerPC::ppcState.ps[inst.FD].SetBoth(b.PS0AsU64() ^ (UINT64_C(1) << 63),
                                        b.PS1AsU64() ^ (UINT64_C(1) << 63));

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_mr(Interpreter& interpreter, UGeckoInstruction inst)
{
  PowerPC::ppcState.ps[inst.FD] = PowerPC::ppcState.ps[inst.FB];

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_nabs(Interpreter& interpreter, UGeckoInstruction inst)
{
  const auto& b = PowerPC::ppcState.ps[inst.FB];

  PowerPC::ppcState.ps[inst.FD].SetBoth(b.PS0AsU64() | (UINT64_C(1) << 63),
                                        b.PS1AsU64() | (UINT64_C(1) << 63));

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_abs(Interpreter& interpreter, UGeckoInstruction inst)
{
  const auto& b = PowerPC::ppcState.ps[inst.FB];

  PowerPC::ppcState.ps[inst.FD].SetBoth(b.PS0AsU64() & ~(UINT64_C(1) << 63),
                                        b.PS1AsU64() & ~(UINT64_C(1) << 63));

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

// These are just moves, double is OK.
void Interpreter::ps_merge00(Interpreter& interpreter, UGeckoInstruction inst)
{
  const auto& a = PowerPC::ppcState.ps[inst.FA];
  const auto& b = PowerPC::ppcState.ps[inst.FB];

  PowerPC::ppcState.ps[inst.FD].SetBoth(a.PS0AsDouble(), b.PS0AsDouble());

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_merge01(Interpreter& interpreter, UGeckoInstruction inst)
{
  const auto& a = PowerPC::ppcState.ps[inst.FA];
  const auto& b = PowerPC::ppcState.ps[inst.FB];

  PowerPC::ppcState.ps[inst.FD].SetBoth(a.PS0AsDouble(), b.PS1AsDouble());

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_merge10(Interpreter& interpreter, UGeckoInstruction inst)
{
  const auto& a = PowerPC::ppcState.ps[inst.FA];
  const auto& b = PowerPC::ppcState.ps[inst.FB];

  PowerPC::ppcState.ps[inst.FD].SetBoth(a.PS1AsDouble(), b.PS0AsDouble());

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_merge11(Interpreter& interpreter, UGeckoInstruction inst)
{
  const auto& a = PowerPC::ppcState.ps[inst.FA];
  const auto& b = PowerPC::ppcState.ps[inst.FB];

  PowerPC::ppcState.ps[inst.FD].SetBoth(a.PS1AsDouble(), b.PS1AsDouble());

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

// From here on, the real deal.
void Interpreter::ps_div(Interpreter& interpreter, UGeckoInstruction inst)
{
  const auto& a = PowerPC::ppcState.ps[inst.FA];
  const auto& b = PowerPC::ppcState.ps[inst.FB];

  const float ps0 =
      ForceSingle(PowerPC::ppcState.fpscr,
                  NI_div(&PowerPC::ppcState.fpscr, a.PS0AsDouble(), b.PS0AsDouble()).value);
  const float ps1 =
      ForceSingle(PowerPC::ppcState.fpscr,
                  NI_div(&PowerPC::ppcState.fpscr, a.PS1AsDouble(), b.PS1AsDouble()).value);

  PowerPC::ppcState.ps[inst.FD].SetBoth(ps0, ps1);
  PowerPC::ppcState.UpdateFPRFSingle(ps0);

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_res(Interpreter& interpreter, UGeckoInstruction inst)
{
  // this code is based on the real hardware tests
  const double a = PowerPC::ppcState.ps[inst.FB].PS0AsDouble();
  const double b = PowerPC::ppcState.ps[inst.FB].PS1AsDouble();

  if (a == 0.0 || b == 0.0)
  {
    SetFPException(&PowerPC::ppcState.fpscr, FPSCR_ZX);
    PowerPC::ppcState.fpscr.ClearFIFR();
  }

  if (std::isnan(a) || std::isinf(a) || std::isnan(b) || std::isinf(b))
    PowerPC::ppcState.fpscr.ClearFIFR();

  if (Common::IsSNAN(a) || Common::IsSNAN(b))
    SetFPException(&PowerPC::ppcState.fpscr, FPSCR_VXSNAN);

  const double ps0 = Common::ApproximateReciprocal(a);
  const double ps1 = Common::ApproximateReciprocal(b);

  PowerPC::ppcState.ps[inst.FD].SetBoth(ps0, ps1);
  PowerPC::ppcState.UpdateFPRFSingle(float(ps0));

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_rsqrte(Interpreter& interpreter, UGeckoInstruction inst)
{
  const double ps0 = PowerPC::ppcState.ps[inst.FB].PS0AsDouble();
  const double ps1 = PowerPC::ppcState.ps[inst.FB].PS1AsDouble();

  if (ps0 == 0.0 || ps1 == 0.0)
  {
    SetFPException(&PowerPC::ppcState.fpscr, FPSCR_ZX);
    PowerPC::ppcState.fpscr.ClearFIFR();
  }

  if (ps0 < 0.0 || ps1 < 0.0)
  {
    SetFPException(&PowerPC::ppcState.fpscr, FPSCR_VXSQRT);
    PowerPC::ppcState.fpscr.ClearFIFR();
  }

  if (std::isnan(ps0) || std::isinf(ps0) || std::isnan(ps1) || std::isinf(ps1))
    PowerPC::ppcState.fpscr.ClearFIFR();

  if (Common::IsSNAN(ps0) || Common::IsSNAN(ps1))
    SetFPException(&PowerPC::ppcState.fpscr, FPSCR_VXSNAN);

  const float dst_ps0 =
      ForceSingle(PowerPC::ppcState.fpscr, Common::ApproximateReciprocalSquareRoot(ps0));
  const float dst_ps1 =
      ForceSingle(PowerPC::ppcState.fpscr, Common::ApproximateReciprocalSquareRoot(ps1));

  PowerPC::ppcState.ps[inst.FD].SetBoth(dst_ps0, dst_ps1);
  PowerPC::ppcState.UpdateFPRFSingle(dst_ps0);

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_sub(Interpreter& interpreter, UGeckoInstruction inst)
{
  const auto& a = PowerPC::ppcState.ps[inst.FA];
  const auto& b = PowerPC::ppcState.ps[inst.FB];

  const float ps0 =
      ForceSingle(PowerPC::ppcState.fpscr,
                  NI_sub(&PowerPC::ppcState.fpscr, a.PS0AsDouble(), b.PS0AsDouble()).value);
  const float ps1 =
      ForceSingle(PowerPC::ppcState.fpscr,
                  NI_sub(&PowerPC::ppcState.fpscr, a.PS1AsDouble(), b.PS1AsDouble()).value);

  PowerPC::ppcState.ps[inst.FD].SetBoth(ps0, ps1);
  PowerPC::ppcState.UpdateFPRFSingle(ps0);

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_add(Interpreter& interpreter, UGeckoInstruction inst)
{
  const auto& a = PowerPC::ppcState.ps[inst.FA];
  const auto& b = PowerPC::ppcState.ps[inst.FB];

  const float ps0 =
      ForceSingle(PowerPC::ppcState.fpscr,
                  NI_add(&PowerPC::ppcState.fpscr, a.PS0AsDouble(), b.PS0AsDouble()).value);
  const float ps1 =
      ForceSingle(PowerPC::ppcState.fpscr,
                  NI_add(&PowerPC::ppcState.fpscr, a.PS1AsDouble(), b.PS1AsDouble()).value);

  PowerPC::ppcState.ps[inst.FD].SetBoth(ps0, ps1);
  PowerPC::ppcState.UpdateFPRFSingle(ps0);

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_mul(Interpreter& interpreter, UGeckoInstruction inst)
{
  const auto& a = PowerPC::ppcState.ps[inst.FA];
  const auto& c = PowerPC::ppcState.ps[inst.FC];

  const double c0 = Force25Bit(c.PS0AsDouble());
  const double c1 = Force25Bit(c.PS1AsDouble());

  const float ps0 = ForceSingle(PowerPC::ppcState.fpscr,
                                NI_mul(&PowerPC::ppcState.fpscr, a.PS0AsDouble(), c0).value);
  const float ps1 = ForceSingle(PowerPC::ppcState.fpscr,
                                NI_mul(&PowerPC::ppcState.fpscr, a.PS1AsDouble(), c1).value);

  PowerPC::ppcState.ps[inst.FD].SetBoth(ps0, ps1);
  PowerPC::ppcState.UpdateFPRFSingle(ps0);

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_msub(Interpreter& interpreter, UGeckoInstruction inst)
{
  const auto& a = PowerPC::ppcState.ps[inst.FA];
  const auto& b = PowerPC::ppcState.ps[inst.FB];
  const auto& c = PowerPC::ppcState.ps[inst.FC];

  const double c0 = Force25Bit(c.PS0AsDouble());
  const double c1 = Force25Bit(c.PS1AsDouble());

  const float ps0 =
      ForceSingle(PowerPC::ppcState.fpscr,
                  NI_msub(&PowerPC::ppcState.fpscr, a.PS0AsDouble(), c0, b.PS0AsDouble()).value);
  const float ps1 =
      ForceSingle(PowerPC::ppcState.fpscr,
                  NI_msub(&PowerPC::ppcState.fpscr, a.PS1AsDouble(), c1, b.PS1AsDouble()).value);

  PowerPC::ppcState.ps[inst.FD].SetBoth(ps0, ps1);
  PowerPC::ppcState.UpdateFPRFSingle(ps0);

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_madd(Interpreter& interpreter, UGeckoInstruction inst)
{
  const auto& a = PowerPC::ppcState.ps[inst.FA];
  const auto& b = PowerPC::ppcState.ps[inst.FB];
  const auto& c = PowerPC::ppcState.ps[inst.FC];

  const double c0 = Force25Bit(c.PS0AsDouble());
  const double c1 = Force25Bit(c.PS1AsDouble());

  const float ps0 =
      ForceSingle(PowerPC::ppcState.fpscr,
                  NI_madd(&PowerPC::ppcState.fpscr, a.PS0AsDouble(), c0, b.PS0AsDouble()).value);
  const float ps1 =
      ForceSingle(PowerPC::ppcState.fpscr,
                  NI_madd(&PowerPC::ppcState.fpscr, a.PS1AsDouble(), c1, b.PS1AsDouble()).value);

  PowerPC::ppcState.ps[inst.FD].SetBoth(ps0, ps1);
  PowerPC::ppcState.UpdateFPRFSingle(ps0);

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_nmsub(Interpreter& interpreter, UGeckoInstruction inst)
{
  const auto& a = PowerPC::ppcState.ps[inst.FA];
  const auto& b = PowerPC::ppcState.ps[inst.FB];
  const auto& c = PowerPC::ppcState.ps[inst.FC];

  const double c0 = Force25Bit(c.PS0AsDouble());
  const double c1 = Force25Bit(c.PS1AsDouble());

  const float tmp0 =
      ForceSingle(PowerPC::ppcState.fpscr,
                  NI_msub(&PowerPC::ppcState.fpscr, a.PS0AsDouble(), c0, b.PS0AsDouble()).value);
  const float tmp1 =
      ForceSingle(PowerPC::ppcState.fpscr,
                  NI_msub(&PowerPC::ppcState.fpscr, a.PS1AsDouble(), c1, b.PS1AsDouble()).value);

  const float ps0 = std::isnan(tmp0) ? tmp0 : -tmp0;
  const float ps1 = std::isnan(tmp1) ? tmp1 : -tmp1;

  PowerPC::ppcState.ps[inst.FD].SetBoth(ps0, ps1);
  PowerPC::ppcState.UpdateFPRFSingle(ps0);

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_nmadd(Interpreter& interpreter, UGeckoInstruction inst)
{
  const auto& a = PowerPC::ppcState.ps[inst.FA];
  const auto& b = PowerPC::ppcState.ps[inst.FB];
  const auto& c = PowerPC::ppcState.ps[inst.FC];

  const double c0 = Force25Bit(c.PS0AsDouble());
  const double c1 = Force25Bit(c.PS1AsDouble());

  const float tmp0 =
      ForceSingle(PowerPC::ppcState.fpscr,
                  NI_madd(&PowerPC::ppcState.fpscr, a.PS0AsDouble(), c0, b.PS0AsDouble()).value);
  const float tmp1 =
      ForceSingle(PowerPC::ppcState.fpscr,
                  NI_madd(&PowerPC::ppcState.fpscr, a.PS1AsDouble(), c1, b.PS1AsDouble()).value);

  const float ps0 = std::isnan(tmp0) ? tmp0 : -tmp0;
  const float ps1 = std::isnan(tmp1) ? tmp1 : -tmp1;

  PowerPC::ppcState.ps[inst.FD].SetBoth(ps0, ps1);
  PowerPC::ppcState.UpdateFPRFSingle(ps0);

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_sum0(Interpreter& interpreter, UGeckoInstruction inst)
{
  const auto& a = PowerPC::ppcState.ps[inst.FA];
  const auto& b = PowerPC::ppcState.ps[inst.FB];
  const auto& c = PowerPC::ppcState.ps[inst.FC];

  const float ps0 =
      ForceSingle(PowerPC::ppcState.fpscr,
                  NI_add(&PowerPC::ppcState.fpscr, a.PS0AsDouble(), b.PS1AsDouble()).value);
  const float ps1 = ForceSingle(PowerPC::ppcState.fpscr, c.PS1AsDouble());

  PowerPC::ppcState.ps[inst.FD].SetBoth(ps0, ps1);
  PowerPC::ppcState.UpdateFPRFSingle(ps0);

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_sum1(Interpreter& interpreter, UGeckoInstruction inst)
{
  const auto& a = PowerPC::ppcState.ps[inst.FA];
  const auto& b = PowerPC::ppcState.ps[inst.FB];
  const auto& c = PowerPC::ppcState.ps[inst.FC];

  const float ps0 = ForceSingle(PowerPC::ppcState.fpscr, c.PS0AsDouble());
  const float ps1 =
      ForceSingle(PowerPC::ppcState.fpscr,
                  NI_add(&PowerPC::ppcState.fpscr, a.PS0AsDouble(), b.PS1AsDouble()).value);

  PowerPC::ppcState.ps[inst.FD].SetBoth(ps0, ps1);
  PowerPC::ppcState.UpdateFPRFSingle(ps1);

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_muls0(Interpreter& interpreter, UGeckoInstruction inst)
{
  const auto& a = PowerPC::ppcState.ps[inst.FA];
  const auto& c = PowerPC::ppcState.ps[inst.FC];

  const double c0 = Force25Bit(c.PS0AsDouble());
  const float ps0 = ForceSingle(PowerPC::ppcState.fpscr,
                                NI_mul(&PowerPC::ppcState.fpscr, a.PS0AsDouble(), c0).value);
  const float ps1 = ForceSingle(PowerPC::ppcState.fpscr,
                                NI_mul(&PowerPC::ppcState.fpscr, a.PS1AsDouble(), c0).value);

  PowerPC::ppcState.ps[inst.FD].SetBoth(ps0, ps1);
  PowerPC::ppcState.UpdateFPRFSingle(ps0);

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_muls1(Interpreter& interpreter, UGeckoInstruction inst)
{
  const auto& a = PowerPC::ppcState.ps[inst.FA];
  const auto& c = PowerPC::ppcState.ps[inst.FC];

  const double c1 = Force25Bit(c.PS1AsDouble());
  const float ps0 = ForceSingle(PowerPC::ppcState.fpscr,
                                NI_mul(&PowerPC::ppcState.fpscr, a.PS0AsDouble(), c1).value);
  const float ps1 = ForceSingle(PowerPC::ppcState.fpscr,
                                NI_mul(&PowerPC::ppcState.fpscr, a.PS1AsDouble(), c1).value);

  PowerPC::ppcState.ps[inst.FD].SetBoth(ps0, ps1);
  PowerPC::ppcState.UpdateFPRFSingle(ps0);

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_madds0(Interpreter& interpreter, UGeckoInstruction inst)
{
  const auto& a = PowerPC::ppcState.ps[inst.FA];
  const auto& b = PowerPC::ppcState.ps[inst.FB];
  const auto& c = PowerPC::ppcState.ps[inst.FC];

  const double c0 = Force25Bit(c.PS0AsDouble());
  const float ps0 =
      ForceSingle(PowerPC::ppcState.fpscr,
                  NI_madd(&PowerPC::ppcState.fpscr, a.PS0AsDouble(), c0, b.PS0AsDouble()).value);
  const float ps1 =
      ForceSingle(PowerPC::ppcState.fpscr,
                  NI_madd(&PowerPC::ppcState.fpscr, a.PS1AsDouble(), c0, b.PS1AsDouble()).value);

  PowerPC::ppcState.ps[inst.FD].SetBoth(ps0, ps1);
  PowerPC::ppcState.UpdateFPRFSingle(ps0);

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_madds1(Interpreter& interpreter, UGeckoInstruction inst)
{
  const auto& a = PowerPC::ppcState.ps[inst.FA];
  const auto& b = PowerPC::ppcState.ps[inst.FB];
  const auto& c = PowerPC::ppcState.ps[inst.FC];

  const double c1 = Force25Bit(c.PS1AsDouble());
  const float ps0 =
      ForceSingle(PowerPC::ppcState.fpscr,
                  NI_madd(&PowerPC::ppcState.fpscr, a.PS0AsDouble(), c1, b.PS0AsDouble()).value);
  const float ps1 =
      ForceSingle(PowerPC::ppcState.fpscr,
                  NI_madd(&PowerPC::ppcState.fpscr, a.PS1AsDouble(), c1, b.PS1AsDouble()).value);

  PowerPC::ppcState.ps[inst.FD].SetBoth(ps0, ps1);
  PowerPC::ppcState.UpdateFPRFSingle(ps0);

  if (inst.Rc)
    PowerPC::ppcState.UpdateCR1();
}

void Interpreter::ps_cmpu0(Interpreter& interpreter, UGeckoInstruction inst)
{
  const auto& a = PowerPC::ppcState.ps[inst.FA];
  const auto& b = PowerPC::ppcState.ps[inst.FB];

  Helper_FloatCompareUnordered(inst, a.PS0AsDouble(), b.PS0AsDouble());
}

void Interpreter::ps_cmpo0(Interpreter& interpreter, UGeckoInstruction inst)
{
  const auto& a = PowerPC::ppcState.ps[inst.FA];
  const auto& b = PowerPC::ppcState.ps[inst.FB];

  Helper_FloatCompareOrdered(inst, a.PS0AsDouble(), b.PS0AsDouble());
}

void Interpreter::ps_cmpu1(Interpreter& interpreter, UGeckoInstruction inst)
{
  const auto& a = PowerPC::ppcState.ps[inst.FA];
  const auto& b = PowerPC::ppcState.ps[inst.FB];

  Helper_FloatCompareUnordered(inst, a.PS1AsDouble(), b.PS1AsDouble());
}

void Interpreter::ps_cmpo1(Interpreter& interpreter, UGeckoInstruction inst)
{
  const auto& a = PowerPC::ppcState.ps[inst.FA];
  const auto& b = PowerPC::ppcState.ps[inst.FB];

  Helper_FloatCompareOrdered(inst, a.PS1AsDouble(), b.PS1AsDouble());
}
