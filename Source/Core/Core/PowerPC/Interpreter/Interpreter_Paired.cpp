// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cmath>
#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"
#include "Core/PowerPC/Interpreter/Interpreter.h"
#include "Core/PowerPC/Interpreter/Interpreter_FPUtils.h"

using namespace MathUtil;

// These "binary instructions" do not alter FPSCR.
void Interpreter::ps_sel(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS0(_inst.FA) >= -0.0 ? rPS0(_inst.FC) : rPS0(_inst.FB);
	rPS1(_inst.FD) = rPS1(_inst.FA) >= -0.0 ? rPS1(_inst.FC) : rPS1(_inst.FB);

	if (_inst.Rc)
		Helper_UpdateCR1();
}

void Interpreter::ps_neg(UGeckoInstruction _inst)
{
	riPS0(_inst.FD) = riPS0(_inst.FB) ^ (1ULL << 63);
	riPS1(_inst.FD) = riPS1(_inst.FB) ^ (1ULL << 63);

	if (_inst.Rc)
		Helper_UpdateCR1();
}

void Interpreter::ps_mr(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS0(_inst.FB);
	rPS1(_inst.FD) = rPS1(_inst.FB);

	if (_inst.Rc)
		Helper_UpdateCR1();
}

void Interpreter::ps_nabs(UGeckoInstruction _inst)
{
	riPS0(_inst.FD) = riPS0(_inst.FB) | (1ULL << 63);
	riPS1(_inst.FD) = riPS1(_inst.FB) | (1ULL << 63);

	if (_inst.Rc)
		Helper_UpdateCR1();
}

void Interpreter::ps_abs(UGeckoInstruction _inst)
{
	riPS0(_inst.FD) = riPS0(_inst.FB) & ~(1ULL << 63);
	riPS1(_inst.FD) = riPS1(_inst.FB) & ~(1ULL << 63);

	if (_inst.Rc)
		Helper_UpdateCR1();
}

// These are just moves, double is OK.
void Interpreter::ps_merge00(UGeckoInstruction _inst)
{
	double p0 = rPS0(_inst.FA);
	double p1 = rPS0(_inst.FB);
	rPS0(_inst.FD) = p0;
	rPS1(_inst.FD) = p1;

	if (_inst.Rc)
		Helper_UpdateCR1();
}

void Interpreter::ps_merge01(UGeckoInstruction _inst)
{
	double p0 = rPS0(_inst.FA);
	double p1 = rPS1(_inst.FB);
	rPS0(_inst.FD) = p0;
	rPS1(_inst.FD) = p1;

	if (_inst.Rc)
		Helper_UpdateCR1();
}

void Interpreter::ps_merge10(UGeckoInstruction _inst)
{
	double p0 = rPS1(_inst.FA);
	double p1 = rPS0(_inst.FB);
	rPS0(_inst.FD) = p0;
	rPS1(_inst.FD) = p1;

	if (_inst.Rc)
		Helper_UpdateCR1();
}

void Interpreter::ps_merge11(UGeckoInstruction _inst)
{
	double p0 = rPS1(_inst.FA);
	double p1 = rPS1(_inst.FB);
	rPS0(_inst.FD) = p0;
	rPS1(_inst.FD) = p1;

	if (_inst.Rc)
		Helper_UpdateCR1();
}

// From here on, the real deal.
void Interpreter::ps_div(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = ForceSingle(NI_div(rPS0(_inst.FA), rPS0(_inst.FB)));
	rPS1(_inst.FD) = ForceSingle(NI_div(rPS1(_inst.FA), rPS1(_inst.FB)));
	UpdateFPRF(rPS0(_inst.FD));

	if (_inst.Rc)
		Helper_UpdateCR1();
}

void Interpreter::ps_res(UGeckoInstruction _inst)
{
	// this code is based on the real hardware tests
	double a = rPS0(_inst.FB);
	double b = rPS1(_inst.FB);

	if (a == 0.0 || b == 0.0)
	{
		SetFPException(FPSCR_ZX);
	}

	rPS0(_inst.FD) = ApproximateReciprocal(a);
	rPS1(_inst.FD) = ApproximateReciprocal(b);
	UpdateFPRF(rPS0(_inst.FD));

	if (_inst.Rc)
		Helper_UpdateCR1();
}

void Interpreter::ps_rsqrte(UGeckoInstruction _inst)
{
	if (rPS0(_inst.FB) == 0.0 || rPS1(_inst.FB) == 0.0)
	{
		SetFPException(FPSCR_ZX);
	}

	if (rPS0(_inst.FB) < 0.0 || rPS1(_inst.FB) < 0.0)
	{
		SetFPException(FPSCR_VXSQRT);
	}

	rPS0(_inst.FD) = ForceSingle(ApproximateReciprocalSquareRoot(rPS0(_inst.FB)));
	rPS1(_inst.FD) = ForceSingle(ApproximateReciprocalSquareRoot(rPS1(_inst.FB)));

	UpdateFPRF(rPS0(_inst.FD));

	if (_inst.Rc)
		Helper_UpdateCR1();
}


void Interpreter::ps_sub(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = ForceSingle(NI_sub(rPS0(_inst.FA), rPS0(_inst.FB)));
	rPS1(_inst.FD) = ForceSingle(NI_sub(rPS1(_inst.FA), rPS1(_inst.FB)));
	UpdateFPRF(rPS0(_inst.FD));

	if (_inst.Rc)
		Helper_UpdateCR1();
}

void Interpreter::ps_add(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = ForceSingle(NI_add(rPS0(_inst.FA), rPS0(_inst.FB)));
	rPS1(_inst.FD) = ForceSingle(NI_add(rPS1(_inst.FA), rPS1(_inst.FB)));
	UpdateFPRF(rPS0(_inst.FD));

	if (_inst.Rc)
		Helper_UpdateCR1();
}

void Interpreter::ps_mul(UGeckoInstruction _inst)
{
	double c0 = Force25Bit(rPS0(_inst.FC));
	double c1 = Force25Bit(rPS1(_inst.FC));
	rPS0(_inst.FD) = ForceSingle(NI_mul(rPS0(_inst.FA), c0));
	rPS1(_inst.FD) = ForceSingle(NI_mul(rPS1(_inst.FA), c1));
	UpdateFPRF(rPS0(_inst.FD));

	if (_inst.Rc)
		Helper_UpdateCR1();
}


void Interpreter::ps_msub(UGeckoInstruction _inst)
{
	double c0 = Force25Bit(rPS0(_inst.FC));
	double c1 = Force25Bit(rPS1(_inst.FC));
	rPS0(_inst.FD) = ForceSingle(NI_msub(rPS0(_inst.FA), c0, rPS0(_inst.FB)));
	rPS1(_inst.FD) = ForceSingle(NI_msub(rPS1(_inst.FA), c1, rPS1(_inst.FB)));
	UpdateFPRF(rPS0(_inst.FD));

	if (_inst.Rc)
		Helper_UpdateCR1();
}

void Interpreter::ps_madd(UGeckoInstruction _inst)
{
	double c0 = Force25Bit(rPS0(_inst.FC));
	double c1 = Force25Bit(rPS1(_inst.FC));
	rPS0(_inst.FD) = ForceSingle(NI_madd(rPS0(_inst.FA), c0, rPS0(_inst.FB)));
	rPS1(_inst.FD) = ForceSingle(NI_madd(rPS1(_inst.FA), c1, rPS1(_inst.FB)));
	UpdateFPRF(rPS0(_inst.FD));

	if (_inst.Rc)
		Helper_UpdateCR1();
}

void Interpreter::ps_nmsub(UGeckoInstruction _inst)
{
	double c0 = Force25Bit(rPS0(_inst.FC));
	double c1 = Force25Bit(rPS1(_inst.FC));
	rPS0(_inst.FD) = ForceSingle(NI_msub(rPS0(_inst.FA), c0, rPS0(_inst.FB), true));
	rPS1(_inst.FD) = ForceSingle(NI_msub(rPS1(_inst.FA), c1, rPS1(_inst.FB), true));
	UpdateFPRF(rPS0(_inst.FD));

	if (_inst.Rc)
		Helper_UpdateCR1();
}

void Interpreter::ps_nmadd(UGeckoInstruction _inst)
{
	double c0 = Force25Bit(rPS0(_inst.FC));
	double c1 = Force25Bit(rPS1(_inst.FC));
	rPS0(_inst.FD) = ForceSingle(NI_madd(rPS0(_inst.FA), c0, rPS0(_inst.FB), true));
	rPS1(_inst.FD) = ForceSingle(NI_madd(rPS1(_inst.FA), c1, rPS1(_inst.FB), true));
	UpdateFPRF(rPS0(_inst.FD));

	if (_inst.Rc)
		Helper_UpdateCR1();
}

void Interpreter::ps_sum0(UGeckoInstruction _inst)
{
	double p0 = ForceSingle(NI_add(rPS0(_inst.FA), rPS1(_inst.FB)));
	double p1 = ForceSingle(rPS1(_inst.FC));
	rPS0(_inst.FD) = p0;
	rPS1(_inst.FD) = p1;
	UpdateFPRF(rPS0(_inst.FD));

	if (_inst.Rc)
		Helper_UpdateCR1();
}

void Interpreter::ps_sum1(UGeckoInstruction _inst)
{
	double p0 = ForceSingle(rPS0(_inst.FC));
	double p1 = ForceSingle(NI_add(rPS0(_inst.FA), rPS1(_inst.FB)));
	rPS0(_inst.FD) = p0;
	rPS1(_inst.FD) = p1;
	UpdateFPRF(rPS1(_inst.FD));

	if (_inst.Rc)
		Helper_UpdateCR1();
}

void Interpreter::ps_muls0(UGeckoInstruction _inst)
{
	double c0 = Force25Bit(rPS0(_inst.FC));
	double p0 = ForceSingle(NI_mul(rPS0(_inst.FA), c0));
	double p1 = ForceSingle(NI_mul(rPS1(_inst.FA), c0));
	rPS0(_inst.FD) = p0;
	rPS1(_inst.FD) = p1;
	UpdateFPRF(rPS0(_inst.FD));

	if (_inst.Rc)
		Helper_UpdateCR1();
}

void Interpreter::ps_muls1(UGeckoInstruction _inst)
{
	double c1 = Force25Bit(rPS1(_inst.FC));
	double p0 = ForceSingle(NI_mul(rPS0(_inst.FA), c1));
	double p1 = ForceSingle(NI_mul(rPS1(_inst.FA), c1));
	rPS0(_inst.FD) = p0;
	rPS1(_inst.FD) = p1;
	UpdateFPRF(rPS0(_inst.FD));

	if (_inst.Rc)
		Helper_UpdateCR1();
}

void Interpreter::ps_madds0(UGeckoInstruction _inst)
{
	double c0 = Force25Bit(rPS0(_inst.FC));
	double p0 = ForceSingle(NI_madd(rPS0(_inst.FA), c0, rPS0(_inst.FB)));
	double p1 = ForceSingle(NI_madd(rPS1(_inst.FA), c0, rPS1(_inst.FB)));
	rPS0(_inst.FD) = p0;
	rPS1(_inst.FD) = p1;
	UpdateFPRF(rPS0(_inst.FD));

	if (_inst.Rc)
		Helper_UpdateCR1();
}

void Interpreter::ps_madds1(UGeckoInstruction _inst)
{
	double c1 = Force25Bit(rPS1(_inst.FC));
	double p0 = ForceSingle(NI_madd(rPS0(_inst.FA), c1, rPS0(_inst.FB)));
	double p1 = ForceSingle(NI_madd(rPS1(_inst.FA), c1, rPS1(_inst.FB)));
	rPS0(_inst.FD) = p0;
	rPS1(_inst.FD) = p1;
	UpdateFPRF(rPS0(_inst.FD));

	if (_inst.Rc)
		Helper_UpdateCR1();
}

void Interpreter::ps_cmpu0(UGeckoInstruction _inst)
{
	Helper_FloatCompareUnordered(_inst, rPS0(_inst.FA), rPS0(_inst.FB));
}

void Interpreter::ps_cmpo0(UGeckoInstruction _inst)
{
	Helper_FloatCompareOrdered(_inst, rPS0(_inst.FA), rPS0(_inst.FB));
}

void Interpreter::ps_cmpu1(UGeckoInstruction _inst)
{
	Helper_FloatCompareUnordered(_inst, rPS1(_inst.FA), rPS1(_inst.FB));
}

void Interpreter::ps_cmpo1(UGeckoInstruction _inst)
{
	Helper_FloatCompareOrdered(_inst, rPS1(_inst.FA), rPS1(_inst.FB));
}

// __________________________________________________________________________________________________
// dcbz_l
// TODO(ector) check docs
void Interpreter::dcbz_l(UGeckoInstruction _inst)
{
	//FAKE: clear memory instead of clearing the cache block
	PowerPC::ClearCacheLine(Helper_Get_EA_X(_inst) & (~31));
}
