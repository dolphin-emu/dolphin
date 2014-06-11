// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cmath>
#include "Common/Common.h"
#include "Common/MathUtil.h"
#include "Core/PowerPC/Interpreter/Interpreter.h"
#include "Core/PowerPC/Interpreter/Interpreter_FPUtils.h"

using namespace MathUtil;

// These "binary instructions" do not alter FPSCR.
void Interpreter::ps_sel(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS0(_inst.FA) >= -0.0 ? rPS0(_inst.FC) : rPS0(_inst.FB);
	rPS1(_inst.FD) = rPS1(_inst.FA) >= -0.0 ? rPS1(_inst.FC) : rPS1(_inst.FB);
	if (_inst.Rc) Helper_UpdateCR1();
}

void Interpreter::ps_neg(UGeckoInstruction _inst)
{
	riPS0(_inst.FD) = riPS0(_inst.FB) ^ (1ULL << 63);
	riPS1(_inst.FD) = riPS1(_inst.FB) ^ (1ULL << 63);
	if (_inst.Rc) Helper_UpdateCR1();
}

void Interpreter::ps_mr(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS0(_inst.FB);
	rPS1(_inst.FD) = rPS1(_inst.FB);
	if (_inst.Rc) Helper_UpdateCR1();
}

void Interpreter::ps_nabs(UGeckoInstruction _inst)
{
	riPS0(_inst.FD) = riPS0(_inst.FB) | (1ULL << 63);
	riPS1(_inst.FD) = riPS1(_inst.FB) | (1ULL << 63);
	if (_inst.Rc) Helper_UpdateCR1();
}

void Interpreter::ps_abs(UGeckoInstruction _inst)
{
	riPS0(_inst.FD) = riPS0(_inst.FB) &~ (1ULL << 63);
	riPS1(_inst.FD) = riPS1(_inst.FB) &~ (1ULL << 63);
	if (_inst.Rc) Helper_UpdateCR1();
}

// These are just moves, double is OK.
void Interpreter::ps_merge00(UGeckoInstruction _inst)
{
	double p0 = rPS0(_inst.FA);
	double p1 = rPS0(_inst.FB);
	rPS0(_inst.FD) = p0;
	rPS1(_inst.FD) = p1;
	if (_inst.Rc) Helper_UpdateCR1();
}

void Interpreter::ps_merge01(UGeckoInstruction _inst)
{
	double p0 = rPS0(_inst.FA);
	double p1 = rPS1(_inst.FB);
	rPS0(_inst.FD) = p0;
	rPS1(_inst.FD) = p1;
	if (_inst.Rc) Helper_UpdateCR1();
}

void Interpreter::ps_merge10(UGeckoInstruction _inst)
{
	double p0 = rPS1(_inst.FA);
	double p1 = rPS0(_inst.FB);
	rPS0(_inst.FD) = p0;
	rPS1(_inst.FD) = p1;
	if (_inst.Rc) Helper_UpdateCR1();
}

void Interpreter::ps_merge11(UGeckoInstruction _inst)
{
	double p0 = rPS1(_inst.FA);
	double p1 = rPS1(_inst.FB);
	rPS0(_inst.FD) = p0;
	rPS1(_inst.FD) = p1;
	if (_inst.Rc) Helper_UpdateCR1();
}

// From here on, the real deal.
void Interpreter::ps_div(UGeckoInstruction _inst)
{
	riPS1(_inst.FD) = DivSinglePrecision(riPS1(_inst.FA), riPS1(_inst.FB));
	riPS0(_inst.FD) = DivSinglePrecision(riPS0(_inst.FA), riPS0(_inst.FB));
	UpdateFPRFSingle(riPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1();
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
	UpdateFPRFSingle(riPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1();
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

	rPS0(_inst.FD) = ApproximateReciprocalSquareRoot(rPS0(_inst.FB));
	rPS1(_inst.FD) = ApproximateReciprocalSquareRoot(rPS1(_inst.FB));

	UpdateFPRFSingle(riPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1();
}


void Interpreter::ps_sub(UGeckoInstruction _inst)
{
	riPS1(_inst.FD) = SubSinglePrecision(riPS1(_inst.FA), riPS1(_inst.FB));
	riPS0(_inst.FD) = SubSinglePrecision(riPS0(_inst.FA), riPS0(_inst.FB));
	UpdateFPRFSingle(riPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1();
}

void Interpreter::ps_add(UGeckoInstruction _inst)
{
	riPS1(_inst.FD) = AddSinglePrecision(riPS1(_inst.FA), riPS1(_inst.FB));
	riPS0(_inst.FD) = AddSinglePrecision(riPS0(_inst.FA), riPS0(_inst.FB));
	UpdateFPRFSingle(riPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1();
}

void Interpreter::ps_mul(UGeckoInstruction _inst)
{
	riPS1(_inst.FD) = MultiplySinglePrecision(riPS1(_inst.FA), riPS1(_inst.FC));
	riPS0(_inst.FD) = MultiplySinglePrecision(riPS0(_inst.FA), riPS0(_inst.FC));
	UpdateFPRFSingle(riPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1();
}


void Interpreter::ps_msub(UGeckoInstruction _inst)
{
	riPS1(_inst.FD) = MaddSinglePrecision(riPS1(_inst.FA), riPS1(_inst.FC), riPS1(_inst.FB), true, false);
	riPS0(_inst.FD) = MaddSinglePrecision(riPS0(_inst.FA), riPS0(_inst.FC), riPS0(_inst.FB), true, false);
	UpdateFPRFSingle(riPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1();
}

void Interpreter::ps_madd(UGeckoInstruction _inst)
{
	riPS1(_inst.FD) = MaddSinglePrecision(riPS1(_inst.FA), riPS1(_inst.FC), riPS1(_inst.FB), false, false);
	riPS0(_inst.FD) = MaddSinglePrecision(riPS0(_inst.FA), riPS0(_inst.FC), riPS0(_inst.FB), false, false);
	UpdateFPRFSingle(riPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1();
}

void Interpreter::ps_nmsub(UGeckoInstruction _inst)
{
	riPS1(_inst.FD) = MaddSinglePrecision(riPS1(_inst.FA), riPS1(_inst.FC), riPS1(_inst.FB), true, true);
	riPS0(_inst.FD) = MaddSinglePrecision(riPS0(_inst.FA), riPS0(_inst.FC), riPS0(_inst.FB), true, true);
	UpdateFPRFSingle(riPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1();
}

void Interpreter::ps_nmadd(UGeckoInstruction _inst)
{
	riPS1(_inst.FD) = MaddSinglePrecision(riPS1(_inst.FA), riPS1(_inst.FC), riPS1(_inst.FB), false, true);
	riPS0(_inst.FD) = MaddSinglePrecision(riPS0(_inst.FA), riPS0(_inst.FC), riPS0(_inst.FB), false, true);
	UpdateFPRFSingle(riPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1();
}

void Interpreter::ps_sum0(UGeckoInstruction _inst)
{
	riPS0(_inst.FD) = AddSinglePrecision(riPS0(_inst.FA), riPS1(_inst.FB));
	riPS1(_inst.FD) = riPS1(_inst.FC);
	UpdateFPRFSingle(riPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1();
}

void Interpreter::ps_sum1(UGeckoInstruction _inst)
{
	riPS1(_inst.FD) = AddSinglePrecision(riPS0(_inst.FA), riPS1(_inst.FB));
	riPS0(_inst.FD) = riPS0(_inst.FC);
	UpdateFPRFSingle(riPS1(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1();
}

void Interpreter::ps_muls0(UGeckoInstruction _inst)
{
	u64 p0 = MultiplySinglePrecision(riPS0(_inst.FA), riPS0(_inst.FC));
	u64 p1 = MultiplySinglePrecision(riPS1(_inst.FA), riPS0(_inst.FC));
	riPS0(_inst.FD) = p0;
	riPS1(_inst.FD) = p1;
	UpdateFPRFSingle(riPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1();
}

void Interpreter::ps_muls1(UGeckoInstruction _inst)
{
	u64 p0 = MultiplySinglePrecision(riPS0(_inst.FA), riPS1(_inst.FC));
	u64 p1 = MultiplySinglePrecision(riPS1(_inst.FA), riPS1(_inst.FC));
	riPS0(_inst.FD) = p0;
	riPS1(_inst.FD) = p1;
	UpdateFPRFSingle(riPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1();
}

void Interpreter::ps_madds0(UGeckoInstruction _inst)
{
	u64 p0 = MaddSinglePrecision(riPS0(_inst.FA), riPS0(_inst.FC), riPS0(_inst.FB), false, false);
	u64 p1 = MaddSinglePrecision(riPS1(_inst.FA), riPS0(_inst.FC), riPS1(_inst.FB), false, false);
	riPS0(_inst.FD) = p0;
	riPS1(_inst.FD) = p1;
	UpdateFPRFSingle(riPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1();
}

void Interpreter::ps_madds1(UGeckoInstruction _inst)
{
	u64 p0 = MaddSinglePrecision(riPS0(_inst.FA), riPS1(_inst.FC), riPS0(_inst.FB), false, false);
	u64 p1 = MaddSinglePrecision(riPS1(_inst.FA), riPS1(_inst.FC), riPS1(_inst.FB), false, false);
	riPS0(_inst.FD) = p0;
	riPS1(_inst.FD) = p1;
	UpdateFPRFSingle(riPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1();
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
	Memory::Memset(Helper_Get_EA_X(_inst) & (~31), 0, 32);
}
