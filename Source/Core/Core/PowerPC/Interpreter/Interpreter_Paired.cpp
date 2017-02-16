// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cmath>

#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"
#include "Core/PowerPC/Interpreter/Interpreter.h"
#include "Core/PowerPC/Interpreter/Interpreter_FPUtils.h"
#include "Core/PowerPC/PowerPC.h"

using namespace MathUtil;

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
  rPS0(inst.FD) = ForceSingle(NI_div(rPS0(inst.FA), rPS0(inst.FB)));
  rPS1(inst.FD) = ForceSingle(NI_div(rPS1(inst.FA), rPS1(inst.FB)));
  UpdateFPRF(rPS0(inst.FD));

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::ps_res(UGeckoInstruction inst)
{
  // this code is based on the real hardware tests
  double a = rPS0(inst.FB);
  double b = rPS1(inst.FB);

  if (a == 0.0 || b == 0.0)
  {
    SetFPException(FPSCR_ZX);
  }

  rPS0(inst.FD) = ApproximateReciprocal(a);
  rPS1(inst.FD) = ApproximateReciprocal(b);
  UpdateFPRF(rPS0(inst.FD));

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::ps_rsqrte(UGeckoInstruction inst)
{
  if (rPS0(inst.FB) == 0.0 || rPS1(inst.FB) == 0.0)
  {
    SetFPException(FPSCR_ZX);
  }

  if (rPS0(inst.FB) < 0.0 || rPS1(inst.FB) < 0.0)
  {
    SetFPException(FPSCR_VXSQRT);
  }

  rPS0(inst.FD) = ForceSingle(ApproximateReciprocalSquareRoot(rPS0(inst.FB)));
  rPS1(inst.FD) = ForceSingle(ApproximateReciprocalSquareRoot(rPS1(inst.FB)));

  UpdateFPRF(rPS0(inst.FD));

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::ps_sub(UGeckoInstruction inst)
{
  rPS0(inst.FD) = ForceSingle(NI_sub(rPS0(inst.FA), rPS0(inst.FB)));
  rPS1(inst.FD) = ForceSingle(NI_sub(rPS1(inst.FA), rPS1(inst.FB)));
  UpdateFPRF(rPS0(inst.FD));

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::ps_add(UGeckoInstruction inst)
{
  rPS0(inst.FD) = ForceSingle(NI_add(rPS0(inst.FA), rPS0(inst.FB)));
  rPS1(inst.FD) = ForceSingle(NI_add(rPS1(inst.FA), rPS1(inst.FB)));
  UpdateFPRF(rPS0(inst.FD));

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::ps_mul(UGeckoInstruction inst)
{
  double c0 = Force25Bit(rPS0(inst.FC));
  double c1 = Force25Bit(rPS1(inst.FC));
  rPS0(inst.FD) = ForceSingle(NI_mul(rPS0(inst.FA), c0));
  rPS1(inst.FD) = ForceSingle(NI_mul(rPS1(inst.FA), c1));
  UpdateFPRF(rPS0(inst.FD));

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::ps_msub(UGeckoInstruction inst)
{
  double c0 = Force25Bit(rPS0(inst.FC));
  double c1 = Force25Bit(rPS1(inst.FC));
  rPS0(inst.FD) = ForceSingle(NI_msub(rPS0(inst.FA), c0, rPS0(inst.FB)));
  rPS1(inst.FD) = ForceSingle(NI_msub(rPS1(inst.FA), c1, rPS1(inst.FB)));
  UpdateFPRF(rPS0(inst.FD));

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::ps_madd(UGeckoInstruction inst)
{
  double c0 = Force25Bit(rPS0(inst.FC));
  double c1 = Force25Bit(rPS1(inst.FC));
  rPS0(inst.FD) = ForceSingle(NI_madd(rPS0(inst.FA), c0, rPS0(inst.FB)));
  rPS1(inst.FD) = ForceSingle(NI_madd(rPS1(inst.FA), c1, rPS1(inst.FB)));
  UpdateFPRF(rPS0(inst.FD));

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::ps_nmsub(UGeckoInstruction inst)
{
  double c0 = Force25Bit(rPS0(inst.FC));
  double c1 = Force25Bit(rPS1(inst.FC));
  double result0 = ForceSingle(NI_msub(rPS0(inst.FA), c0, rPS0(inst.FB)));
  double result1 = ForceSingle(NI_msub(rPS1(inst.FA), c1, rPS1(inst.FB)));
  rPS0(inst.FD) = std::isnan(result0) ? result0 : -result0;
  rPS1(inst.FD) = std::isnan(result1) ? result1 : -result1;
  UpdateFPRF(rPS0(inst.FD));

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::ps_nmadd(UGeckoInstruction inst)
{
  double c0 = Force25Bit(rPS0(inst.FC));
  double c1 = Force25Bit(rPS1(inst.FC));
  double result0 = ForceSingle(NI_madd(rPS0(inst.FA), c0, rPS0(inst.FB)));
  double result1 = ForceSingle(NI_madd(rPS1(inst.FA), c1, rPS1(inst.FB)));
  rPS0(inst.FD) = std::isnan(result0) ? result0 : -result0;
  rPS1(inst.FD) = std::isnan(result1) ? result1 : -result1;
  UpdateFPRF(rPS0(inst.FD));

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::ps_sum0(UGeckoInstruction inst)
{
  double p0 = ForceSingle(NI_add(rPS0(inst.FA), rPS1(inst.FB)));
  double p1 = ForceSingle(rPS1(inst.FC));
  rPS0(inst.FD) = p0;
  rPS1(inst.FD) = p1;
  UpdateFPRF(rPS0(inst.FD));

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::ps_sum1(UGeckoInstruction inst)
{
  double p0 = ForceSingle(rPS0(inst.FC));
  double p1 = ForceSingle(NI_add(rPS0(inst.FA), rPS1(inst.FB)));
  rPS0(inst.FD) = p0;
  rPS1(inst.FD) = p1;
  UpdateFPRF(rPS1(inst.FD));

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::ps_muls0(UGeckoInstruction inst)
{
  double c0 = Force25Bit(rPS0(inst.FC));
  double p0 = ForceSingle(NI_mul(rPS0(inst.FA), c0));
  double p1 = ForceSingle(NI_mul(rPS1(inst.FA), c0));
  rPS0(inst.FD) = p0;
  rPS1(inst.FD) = p1;
  UpdateFPRF(rPS0(inst.FD));

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::ps_muls1(UGeckoInstruction inst)
{
  double c1 = Force25Bit(rPS1(inst.FC));
  double p0 = ForceSingle(NI_mul(rPS0(inst.FA), c1));
  double p1 = ForceSingle(NI_mul(rPS1(inst.FA), c1));
  rPS0(inst.FD) = p0;
  rPS1(inst.FD) = p1;
  UpdateFPRF(rPS0(inst.FD));

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::ps_madds0(UGeckoInstruction inst)
{
  double c0 = Force25Bit(rPS0(inst.FC));
  double p0 = ForceSingle(NI_madd(rPS0(inst.FA), c0, rPS0(inst.FB)));
  double p1 = ForceSingle(NI_madd(rPS1(inst.FA), c0, rPS1(inst.FB)));
  rPS0(inst.FD) = p0;
  rPS1(inst.FD) = p1;
  UpdateFPRF(rPS0(inst.FD));

  if (inst.Rc)
    Helper_UpdateCR1();
}

void Interpreter::ps_madds1(UGeckoInstruction inst)
{
  double c1 = Force25Bit(rPS1(inst.FC));
  double p0 = ForceSingle(NI_madd(rPS0(inst.FA), c1, rPS0(inst.FB)));
  double p1 = ForceSingle(NI_madd(rPS1(inst.FA), c1, rPS1(inst.FB)));
  rPS0(inst.FD) = p0;
  rPS1(inst.FD) = p1;
  UpdateFPRF(rPS0(inst.FD));

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

// __________________________________________________________________________________________________
// dcbz_l
// TODO(ector) check docs
void Interpreter::dcbz_l(UGeckoInstruction inst)
{
  // FAKE: clear memory instead of clearing the cache block
  PowerPC::ClearCacheLine(Helper_Get_EA_X(inst) & (~31));
}
