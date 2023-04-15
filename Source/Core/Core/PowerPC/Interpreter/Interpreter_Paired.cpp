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
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& a = ppc_state.ps[inst.FA];
  const auto& b = ppc_state.ps[inst.FB];
  const auto& c = ppc_state.ps[inst.FC];

  ppc_state.ps[inst.FD].SetBoth(a.PS0AsDouble() >= -0.0 ? c.PS0AsDouble() : b.PS0AsDouble(),
                                a.PS1AsDouble() >= -0.0 ? c.PS1AsDouble() : b.PS1AsDouble());

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::ps_neg(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& b = ppc_state.ps[inst.FB];

  ppc_state.ps[inst.FD].SetBoth(b.PS0AsU64() ^ (UINT64_C(1) << 63),
                                b.PS1AsU64() ^ (UINT64_C(1) << 63));

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::ps_mr(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  ppc_state.ps[inst.FD] = ppc_state.ps[inst.FB];

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::ps_nabs(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& b = ppc_state.ps[inst.FB];

  ppc_state.ps[inst.FD].SetBoth(b.PS0AsU64() | (UINT64_C(1) << 63),
                                b.PS1AsU64() | (UINT64_C(1) << 63));

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::ps_abs(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& b = ppc_state.ps[inst.FB];

  ppc_state.ps[inst.FD].SetBoth(b.PS0AsU64() & ~(UINT64_C(1) << 63),
                                b.PS1AsU64() & ~(UINT64_C(1) << 63));

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

// These are just moves, double is OK.
void Interpreter::ps_merge00(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& a = ppc_state.ps[inst.FA];
  const auto& b = ppc_state.ps[inst.FB];

  ppc_state.ps[inst.FD].SetBoth(a.PS0AsDouble(), b.PS0AsDouble());

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::ps_merge01(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& a = ppc_state.ps[inst.FA];
  const auto& b = ppc_state.ps[inst.FB];

  ppc_state.ps[inst.FD].SetBoth(a.PS0AsDouble(), b.PS1AsDouble());

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::ps_merge10(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& a = ppc_state.ps[inst.FA];
  const auto& b = ppc_state.ps[inst.FB];

  ppc_state.ps[inst.FD].SetBoth(a.PS1AsDouble(), b.PS0AsDouble());

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::ps_merge11(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& a = ppc_state.ps[inst.FA];
  const auto& b = ppc_state.ps[inst.FB];

  ppc_state.ps[inst.FD].SetBoth(a.PS1AsDouble(), b.PS1AsDouble());

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

// From here on, the real deal.
void Interpreter::ps_div(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& a = ppc_state.ps[inst.FA];
  const auto& b = ppc_state.ps[inst.FB];

  const float ps0 =
      ForceSingle(ppc_state.fpscr, NI_div(ppc_state, a.PS0AsDouble(), b.PS0AsDouble()).value);
  const float ps1 =
      ForceSingle(ppc_state.fpscr, NI_div(ppc_state, a.PS1AsDouble(), b.PS1AsDouble()).value);

  ppc_state.ps[inst.FD].SetBoth(ps0, ps1);
  ppc_state.UpdateFPRFSingle(ps0);

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::ps_res(Interpreter& interpreter, UGeckoInstruction inst)
{
  // this code is based on the real hardware tests
  auto& ppc_state = interpreter.m_ppc_state;
  const double a = ppc_state.ps[inst.FB].PS0AsDouble();
  const double b = ppc_state.ps[inst.FB].PS1AsDouble();

  if (a == 0.0 || b == 0.0)
  {
    SetFPException(ppc_state, FPSCR_ZX);
    ppc_state.fpscr.ClearFIFR();
  }

  if (std::isnan(a) || std::isinf(a) || std::isnan(b) || std::isinf(b))
    ppc_state.fpscr.ClearFIFR();

  if (Common::IsSNAN(a) || Common::IsSNAN(b))
    SetFPException(ppc_state, FPSCR_VXSNAN);

  const double ps0 = Common::ApproximateReciprocal(a);
  const double ps1 = Common::ApproximateReciprocal(b);

  ppc_state.ps[inst.FD].SetBoth(ps0, ps1);
  ppc_state.UpdateFPRFSingle(float(ps0));

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::ps_rsqrte(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const double ps0 = ppc_state.ps[inst.FB].PS0AsDouble();
  const double ps1 = ppc_state.ps[inst.FB].PS1AsDouble();

  if (ps0 == 0.0 || ps1 == 0.0)
  {
    SetFPException(ppc_state, FPSCR_ZX);
    ppc_state.fpscr.ClearFIFR();
  }

  if (ps0 < 0.0 || ps1 < 0.0)
  {
    SetFPException(ppc_state, FPSCR_VXSQRT);
    ppc_state.fpscr.ClearFIFR();
  }

  if (std::isnan(ps0) || std::isinf(ps0) || std::isnan(ps1) || std::isinf(ps1))
    ppc_state.fpscr.ClearFIFR();

  if (Common::IsSNAN(ps0) || Common::IsSNAN(ps1))
    SetFPException(ppc_state, FPSCR_VXSNAN);

  const float dst_ps0 = ForceSingle(ppc_state.fpscr, Common::ApproximateReciprocalSquareRoot(ps0));
  const float dst_ps1 = ForceSingle(ppc_state.fpscr, Common::ApproximateReciprocalSquareRoot(ps1));

  ppc_state.ps[inst.FD].SetBoth(dst_ps0, dst_ps1);
  ppc_state.UpdateFPRFSingle(dst_ps0);

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::ps_sub(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& a = ppc_state.ps[inst.FA];
  const auto& b = ppc_state.ps[inst.FB];

  const float ps0 =
      ForceSingle(ppc_state.fpscr, NI_sub(ppc_state, a.PS0AsDouble(), b.PS0AsDouble()).value);
  const float ps1 =
      ForceSingle(ppc_state.fpscr, NI_sub(ppc_state, a.PS1AsDouble(), b.PS1AsDouble()).value);

  ppc_state.ps[inst.FD].SetBoth(ps0, ps1);
  ppc_state.UpdateFPRFSingle(ps0);

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::ps_add(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& a = ppc_state.ps[inst.FA];
  const auto& b = ppc_state.ps[inst.FB];

  const float ps0 =
      ForceSingle(ppc_state.fpscr, NI_add(ppc_state, a.PS0AsDouble(), b.PS0AsDouble()).value);
  const float ps1 =
      ForceSingle(ppc_state.fpscr, NI_add(ppc_state, a.PS1AsDouble(), b.PS1AsDouble()).value);

  ppc_state.ps[inst.FD].SetBoth(ps0, ps1);
  ppc_state.UpdateFPRFSingle(ps0);

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::ps_mul(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& a = ppc_state.ps[inst.FA];
  const auto& c = ppc_state.ps[inst.FC];

  const double c0 = Force25Bit(c.PS0AsDouble());
  const double c1 = Force25Bit(c.PS1AsDouble());

  const float ps0 = ForceSingle(ppc_state.fpscr, NI_mul(ppc_state, a.PS0AsDouble(), c0).value);
  const float ps1 = ForceSingle(ppc_state.fpscr, NI_mul(ppc_state, a.PS1AsDouble(), c1).value);

  ppc_state.ps[inst.FD].SetBoth(ps0, ps1);
  ppc_state.UpdateFPRFSingle(ps0);

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::ps_msub(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& a = ppc_state.ps[inst.FA];
  const auto& b = ppc_state.ps[inst.FB];
  const auto& c = ppc_state.ps[inst.FC];

  const double c0 = Force25Bit(c.PS0AsDouble());
  const double c1 = Force25Bit(c.PS1AsDouble());

  const float ps0 =
      ForceSingle(ppc_state.fpscr, NI_msub(ppc_state, a.PS0AsDouble(), c0, b.PS0AsDouble()).value);
  const float ps1 =
      ForceSingle(ppc_state.fpscr, NI_msub(ppc_state, a.PS1AsDouble(), c1, b.PS1AsDouble()).value);

  ppc_state.ps[inst.FD].SetBoth(ps0, ps1);
  ppc_state.UpdateFPRFSingle(ps0);

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::ps_madd(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& a = ppc_state.ps[inst.FA];
  const auto& b = ppc_state.ps[inst.FB];
  const auto& c = ppc_state.ps[inst.FC];

  const double c0 = Force25Bit(c.PS0AsDouble());
  const double c1 = Force25Bit(c.PS1AsDouble());

  const float ps0 =
      ForceSingle(ppc_state.fpscr, NI_madd(ppc_state, a.PS0AsDouble(), c0, b.PS0AsDouble()).value);
  const float ps1 =
      ForceSingle(ppc_state.fpscr, NI_madd(ppc_state, a.PS1AsDouble(), c1, b.PS1AsDouble()).value);

  ppc_state.ps[inst.FD].SetBoth(ps0, ps1);
  ppc_state.UpdateFPRFSingle(ps0);

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::ps_nmsub(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& a = ppc_state.ps[inst.FA];
  const auto& b = ppc_state.ps[inst.FB];
  const auto& c = ppc_state.ps[inst.FC];

  const double c0 = Force25Bit(c.PS0AsDouble());
  const double c1 = Force25Bit(c.PS1AsDouble());

  const float tmp0 =
      ForceSingle(ppc_state.fpscr, NI_msub(ppc_state, a.PS0AsDouble(), c0, b.PS0AsDouble()).value);
  const float tmp1 =
      ForceSingle(ppc_state.fpscr, NI_msub(ppc_state, a.PS1AsDouble(), c1, b.PS1AsDouble()).value);

  const float ps0 = std::isnan(tmp0) ? tmp0 : -tmp0;
  const float ps1 = std::isnan(tmp1) ? tmp1 : -tmp1;

  ppc_state.ps[inst.FD].SetBoth(ps0, ps1);
  ppc_state.UpdateFPRFSingle(ps0);

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::ps_nmadd(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& a = ppc_state.ps[inst.FA];
  const auto& b = ppc_state.ps[inst.FB];
  const auto& c = ppc_state.ps[inst.FC];

  const double c0 = Force25Bit(c.PS0AsDouble());
  const double c1 = Force25Bit(c.PS1AsDouble());

  const float tmp0 =
      ForceSingle(ppc_state.fpscr, NI_madd(ppc_state, a.PS0AsDouble(), c0, b.PS0AsDouble()).value);
  const float tmp1 =
      ForceSingle(ppc_state.fpscr, NI_madd(ppc_state, a.PS1AsDouble(), c1, b.PS1AsDouble()).value);

  const float ps0 = std::isnan(tmp0) ? tmp0 : -tmp0;
  const float ps1 = std::isnan(tmp1) ? tmp1 : -tmp1;

  ppc_state.ps[inst.FD].SetBoth(ps0, ps1);
  ppc_state.UpdateFPRFSingle(ps0);

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::ps_sum0(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& a = ppc_state.ps[inst.FA];
  const auto& b = ppc_state.ps[inst.FB];
  const auto& c = ppc_state.ps[inst.FC];

  const float ps0 =
      ForceSingle(ppc_state.fpscr, NI_add(ppc_state, a.PS0AsDouble(), b.PS1AsDouble()).value);
  const float ps1 = ForceSingle(ppc_state.fpscr, c.PS1AsDouble());

  ppc_state.ps[inst.FD].SetBoth(ps0, ps1);
  ppc_state.UpdateFPRFSingle(ps0);

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::ps_sum1(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& a = ppc_state.ps[inst.FA];
  const auto& b = ppc_state.ps[inst.FB];
  const auto& c = ppc_state.ps[inst.FC];

  const float ps0 = ForceSingle(ppc_state.fpscr, c.PS0AsDouble());
  const float ps1 =
      ForceSingle(ppc_state.fpscr, NI_add(ppc_state, a.PS0AsDouble(), b.PS1AsDouble()).value);

  ppc_state.ps[inst.FD].SetBoth(ps0, ps1);
  ppc_state.UpdateFPRFSingle(ps1);

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::ps_muls0(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& a = ppc_state.ps[inst.FA];
  const auto& c = ppc_state.ps[inst.FC];

  const double c0 = Force25Bit(c.PS0AsDouble());
  const float ps0 = ForceSingle(ppc_state.fpscr, NI_mul(ppc_state, a.PS0AsDouble(), c0).value);
  const float ps1 = ForceSingle(ppc_state.fpscr, NI_mul(ppc_state, a.PS1AsDouble(), c0).value);

  ppc_state.ps[inst.FD].SetBoth(ps0, ps1);
  ppc_state.UpdateFPRFSingle(ps0);

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::ps_muls1(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& a = ppc_state.ps[inst.FA];
  const auto& c = ppc_state.ps[inst.FC];

  const double c1 = Force25Bit(c.PS1AsDouble());
  const float ps0 = ForceSingle(ppc_state.fpscr, NI_mul(ppc_state, a.PS0AsDouble(), c1).value);
  const float ps1 = ForceSingle(ppc_state.fpscr, NI_mul(ppc_state, a.PS1AsDouble(), c1).value);

  ppc_state.ps[inst.FD].SetBoth(ps0, ps1);
  ppc_state.UpdateFPRFSingle(ps0);

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::ps_madds0(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& a = ppc_state.ps[inst.FA];
  const auto& b = ppc_state.ps[inst.FB];
  const auto& c = ppc_state.ps[inst.FC];

  const double c0 = Force25Bit(c.PS0AsDouble());
  const float ps0 =
      ForceSingle(ppc_state.fpscr, NI_madd(ppc_state, a.PS0AsDouble(), c0, b.PS0AsDouble()).value);
  const float ps1 =
      ForceSingle(ppc_state.fpscr, NI_madd(ppc_state, a.PS1AsDouble(), c0, b.PS1AsDouble()).value);

  ppc_state.ps[inst.FD].SetBoth(ps0, ps1);
  ppc_state.UpdateFPRFSingle(ps0);

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::ps_madds1(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& a = ppc_state.ps[inst.FA];
  const auto& b = ppc_state.ps[inst.FB];
  const auto& c = ppc_state.ps[inst.FC];

  const double c1 = Force25Bit(c.PS1AsDouble());
  const float ps0 =
      ForceSingle(ppc_state.fpscr, NI_madd(ppc_state, a.PS0AsDouble(), c1, b.PS0AsDouble()).value);
  const float ps1 =
      ForceSingle(ppc_state.fpscr, NI_madd(ppc_state, a.PS1AsDouble(), c1, b.PS1AsDouble()).value);

  ppc_state.ps[inst.FD].SetBoth(ps0, ps1);
  ppc_state.UpdateFPRFSingle(ps0);

  if (inst.Rc)
    ppc_state.UpdateCR1();
}

void Interpreter::ps_cmpu0(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& a = ppc_state.ps[inst.FA];
  const auto& b = ppc_state.ps[inst.FB];

  Helper_FloatCompareUnordered(ppc_state, inst, a.PS0AsDouble(), b.PS0AsDouble());
}

void Interpreter::ps_cmpo0(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& a = ppc_state.ps[inst.FA];
  const auto& b = ppc_state.ps[inst.FB];

  Helper_FloatCompareOrdered(ppc_state, inst, a.PS0AsDouble(), b.PS0AsDouble());
}

void Interpreter::ps_cmpu1(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& a = ppc_state.ps[inst.FA];
  const auto& b = ppc_state.ps[inst.FB];

  Helper_FloatCompareUnordered(ppc_state, inst, a.PS1AsDouble(), b.PS1AsDouble());
}

void Interpreter::ps_cmpo1(Interpreter& interpreter, UGeckoInstruction inst)
{
  auto& ppc_state = interpreter.m_ppc_state;
  const auto& a = ppc_state.ps[inst.FA];
  const auto& b = ppc_state.ps[inst.FB];

  Helper_FloatCompareOrdered(ppc_state, inst, a.PS1AsDouble(), b.PS1AsDouble());
}
