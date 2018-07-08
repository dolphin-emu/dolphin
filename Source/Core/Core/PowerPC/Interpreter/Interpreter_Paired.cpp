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
  rPS0(inst.FD) = rPS0(inst.FA) >= -0.0 ? rPS0(inst.FC) : rPS0(inst.FB);
  rPS1(inst.FD) = rPS1(inst.FA) >= -0.0 ? rPS1(inst.FC) : rPS1(inst.FB);

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::ps_neg(UGeckoInstruction inst)
{
  riPS0(inst.FD) = riPS0(inst.FB) ^ (1ULL << 63);
  riPS1(inst.FD) = riPS1(inst.FB) ^ (1ULL << 63);

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::ps_mr(UGeckoInstruction inst)
{
  rPS0(inst.FD) = rPS0(inst.FB);
  rPS1(inst.FD) = rPS1(inst.FB);

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::ps_nabs(UGeckoInstruction inst)
{
  riPS0(inst.FD) = riPS0(inst.FB) | (1ULL << 63);
  riPS1(inst.FD) = riPS1(inst.FB) | (1ULL << 63);

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::ps_abs(UGeckoInstruction inst)
{
  riPS0(inst.FD) = riPS0(inst.FB) & ~(1ULL << 63);
  riPS1(inst.FD) = riPS1(inst.FB) & ~(1ULL << 63);

  if (inst.Rc)
    Helper_UpdateCR1();
}

// These are just moves, double is OK.
void Interpreter::ps_merge00(UGeckoInstruction inst)
{
  double p0 = rPS0(inst.FA);
  double p1 = rPS0(inst.FB);
  rPS0(inst.FD) = p0;
  rPS1(inst.FD) = p1;

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::ps_merge01(UGeckoInstruction inst)
{
  double p0 = rPS0(inst.FA);
  double p1 = rPS1(inst.FB);
  rPS0(inst.FD) = p0;
  rPS1(inst.FD) = p1;

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::ps_merge10(UGeckoInstruction inst)
{
  double p0 = rPS1(inst.FA);
  double p1 = rPS0(inst.FB);
  rPS0(inst.FD) = p0;
  rPS1(inst.FD) = p1;

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::ps_merge11(UGeckoInstruction inst)
{
  double p0 = rPS1(inst.FA);
  double p1 = rPS1(inst.FB);
  rPS0(inst.FD) = p0;
  rPS1(inst.FD) = p1;

  if (inst.Rc)
    Helper_UpdateCR1();
}

// From here on, the real deal.
void Interpreter::ps_div(UGeckoInstruction inst)
{
  rPS0(inst.FD) = ForceSingle(NI_div(rPS0(inst.FA), rPS0(inst.FB)).value);
  rPS1(inst.FD) = ForceSingle(NI_div(rPS1(inst.FA), rPS1(inst.FB)).value);
  PowerPC::UpdateFPRF(rPS0(inst.FD));

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::ps_res(UGeckoInstruction inst)
{
  // this code is based on the real hardware tests
  const double a = rPS0(inst.FB);
  const double b = rPS1(inst.FB);

  if (a == 0.0 || b == 0.0)
  {
    SetFPException(FPSCR_ZX);
    FPSCR.ClearFIFR();
  }

  if (std::isnan(a) || std::isinf(a) || std::isnan(b) || std::isinf(b))
    FPSCR.ClearFIFR();

  if (Common::IsSNAN(a) || Common::IsSNAN(b))
    SetFPException(FPSCR_VXSNAN);

  rPS0(inst.FD) = Common::ApproximateReciprocal(a);
  rPS1(inst.FD) = Common::ApproximateReciprocal(b);
  PowerPC::UpdateFPRF(rPS0(inst.FD));

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::ps_rsqrte(UGeckoInstruction inst)
{
  const double ps0 = rPS0(inst.FB);
  const double ps1 = rPS1(inst.FB);

  if (ps0 == 0.0 || ps1 == 0.0)
  {
    SetFPException(FPSCR_ZX);
    FPSCR.ClearFIFR();
  }

  if (ps0 < 0.0 || ps1 < 0.0)
  {
    SetFPException(FPSCR_VXSQRT);
    FPSCR.ClearFIFR();
  }

  if (std::isnan(ps0) || std::isinf(ps0) || std::isnan(ps1) || std::isinf(ps1))
    FPSCR.ClearFIFR();

  if (Common::IsSNAN(ps0) || Common::IsSNAN(ps1))
    SetFPException(FPSCR_VXSNAN);

  rPS0(inst.FD) = ForceSingle(Common::ApproximateReciprocalSquareRoot(ps0));
  rPS1(inst.FD) = ForceSingle(Common::ApproximateReciprocalSquareRoot(ps1));

  PowerPC::UpdateFPRF(rPS0(inst.FD));

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::ps_sub(UGeckoInstruction inst)
{
  rPS0(inst.FD) = ForceSingle(NI_sub(rPS0(inst.FA), rPS0(inst.FB)).value);
  rPS1(inst.FD) = ForceSingle(NI_sub(rPS1(inst.FA), rPS1(inst.FB)).value);
  PowerPC::UpdateFPRF(rPS0(inst.FD));

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::ps_add(UGeckoInstruction inst)
{
  rPS0(inst.FD) = ForceSingle(NI_add(rPS0(inst.FA), rPS0(inst.FB)).value);
  rPS1(inst.FD) = ForceSingle(NI_add(rPS1(inst.FA), rPS1(inst.FB)).value);
  PowerPC::UpdateFPRF(rPS0(inst.FD));

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::ps_mul(UGeckoInstruction inst)
{
  const double c0 = Force25Bit(rPS0(inst.FC));
  const double c1 = Force25Bit(rPS1(inst.FC));
  rPS0(inst.FD) = ForceSingle(NI_mul(rPS0(inst.FA), c0).value);
  rPS1(inst.FD) = ForceSingle(NI_mul(rPS1(inst.FA), c1).value);
  PowerPC::UpdateFPRF(rPS0(inst.FD));

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::ps_msub(UGeckoInstruction inst)
{
  const double c0 = Force25Bit(rPS0(inst.FC));
  const double c1 = Force25Bit(rPS1(inst.FC));
  rPS0(inst.FD) = ForceSingle(NI_msub(rPS0(inst.FA), c0, rPS0(inst.FB)).value);
  rPS1(inst.FD) = ForceSingle(NI_msub(rPS1(inst.FA), c1, rPS1(inst.FB)).value);
  PowerPC::UpdateFPRF(rPS0(inst.FD));

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::ps_madd(UGeckoInstruction inst)
{
  const double c0 = Force25Bit(rPS0(inst.FC));
  const double c1 = Force25Bit(rPS1(inst.FC));
  rPS0(inst.FD) = ForceSingle(NI_madd(rPS0(inst.FA), c0, rPS0(inst.FB)).value);
  rPS1(inst.FD) = ForceSingle(NI_madd(rPS1(inst.FA), c1, rPS1(inst.FB)).value);
  PowerPC::UpdateFPRF(rPS0(inst.FD));

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::ps_nmsub(UGeckoInstruction inst)
{
  const double c0 = Force25Bit(rPS0(inst.FC));
  const double c1 = Force25Bit(rPS1(inst.FC));
  const double result0 = ForceSingle(NI_msub(rPS0(inst.FA), c0, rPS0(inst.FB)).value);
  const double result1 = ForceSingle(NI_msub(rPS1(inst.FA), c1, rPS1(inst.FB)).value);
  rPS0(inst.FD) = std::isnan(result0) ? result0 : -result0;
  rPS1(inst.FD) = std::isnan(result1) ? result1 : -result1;
  PowerPC::UpdateFPRF(rPS0(inst.FD));

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::ps_nmadd(UGeckoInstruction inst)
{
  const double c0 = Force25Bit(rPS0(inst.FC));
  const double c1 = Force25Bit(rPS1(inst.FC));
  const double result0 = ForceSingle(NI_madd(rPS0(inst.FA), c0, rPS0(inst.FB)).value);
  const double result1 = ForceSingle(NI_madd(rPS1(inst.FA), c1, rPS1(inst.FB)).value);
  rPS0(inst.FD) = std::isnan(result0) ? result0 : -result0;
  rPS1(inst.FD) = std::isnan(result1) ? result1 : -result1;
  PowerPC::UpdateFPRF(rPS0(inst.FD));

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::ps_sum0(UGeckoInstruction inst)
{
  const double p0 = ForceSingle(NI_add(rPS0(inst.FA), rPS1(inst.FB)).value);
  const double p1 = ForceSingle(rPS1(inst.FC));
  rPS0(inst.FD) = p0;
  rPS1(inst.FD) = p1;
  PowerPC::UpdateFPRF(rPS0(inst.FD));

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::ps_sum1(UGeckoInstruction inst)
{
  const double p0 = ForceSingle(rPS0(inst.FC));
  const double p1 = ForceSingle(NI_add(rPS0(inst.FA), rPS1(inst.FB)).value);
  rPS0(inst.FD) = p0;
  rPS1(inst.FD) = p1;
  PowerPC::UpdateFPRF(rPS1(inst.FD));

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::ps_muls0(UGeckoInstruction inst)
{
  const double c0 = Force25Bit(rPS0(inst.FC));
  const double p0 = ForceSingle(NI_mul(rPS0(inst.FA), c0).value);
  const double p1 = ForceSingle(NI_mul(rPS1(inst.FA), c0).value);
  rPS0(inst.FD) = p0;
  rPS1(inst.FD) = p1;
  PowerPC::UpdateFPRF(rPS0(inst.FD));

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::ps_muls1(UGeckoInstruction inst)
{
  const double c1 = Force25Bit(rPS1(inst.FC));
  const double p0 = ForceSingle(NI_mul(rPS0(inst.FA), c1).value);
  const double p1 = ForceSingle(NI_mul(rPS1(inst.FA), c1).value);
  rPS0(inst.FD) = p0;
  rPS1(inst.FD) = p1;
  PowerPC::UpdateFPRF(rPS0(inst.FD));

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::ps_madds0(UGeckoInstruction inst)
{
  const double c0 = Force25Bit(rPS0(inst.FC));
  const double p0 = ForceSingle(NI_madd(rPS0(inst.FA), c0, rPS0(inst.FB)).value);
  const double p1 = ForceSingle(NI_madd(rPS1(inst.FA), c0, rPS1(inst.FB)).value);
  rPS0(inst.FD) = p0;
  rPS1(inst.FD) = p1;
  PowerPC::UpdateFPRF(rPS0(inst.FD));

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::ps_madds1(UGeckoInstruction inst)
{
  const double c1 = Force25Bit(rPS1(inst.FC));
  const double p0 = ForceSingle(NI_madd(rPS0(inst.FA), c1, rPS0(inst.FB)).value);
  const double p1 = ForceSingle(NI_madd(rPS1(inst.FA), c1, rPS1(inst.FB)).value);
  rPS0(inst.FD) = p0;
  rPS1(inst.FD) = p1;
  PowerPC::UpdateFPRF(rPS0(inst.FD));

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::ps_cmpu0(UGeckoInstruction inst)
{
  Helper_FloatCompareUnordered(inst, rPS0(inst.FA), rPS0(inst.FB));
}

void Interpreter::ps_cmpo0(UGeckoInstruction inst)
{
  Helper_FloatCompareOrdered(inst, rPS0(inst.FA), rPS0(inst.FB));
}

void Interpreter::ps_cmpu1(UGeckoInstruction inst)
{
  Helper_FloatCompareUnordered(inst, rPS1(inst.FA), rPS1(inst.FB));
}

void Interpreter::ps_cmpo1(UGeckoInstruction inst)
{
  Helper_FloatCompareOrdered(inst, rPS1(inst.FA), rPS1(inst.FB));
}
