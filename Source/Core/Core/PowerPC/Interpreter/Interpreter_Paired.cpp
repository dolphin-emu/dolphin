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
void Interpreter::ps_sel(UGeckoInstruction inst)
{
	rPS0(inst.FD) = rPS0(inst.FA) >= -0.0 ? rPS0(inst.FC) : rPS0(inst.FB);
	rPS1(inst.FD) = rPS1(inst.FA) >= -0.0 ? rPS1(inst.FC) : rPS1(inst.FB);
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
}

void Interpreter::ps_neg(UGeckoInstruction inst)
{
	riPS0(inst.FD) = riPS0(inst.FB) ^ (1ULL << 63);
	riPS1(inst.FD) = riPS1(inst.FB) ^ (1ULL << 63);
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
}

void Interpreter::ps_mr(UGeckoInstruction inst)
{
	rPS0(inst.FD) = rPS0(inst.FB);
	rPS1(inst.FD) = rPS1(inst.FB);
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
}

void Interpreter::ps_nabs(UGeckoInstruction inst)
{
	riPS0(inst.FD) = riPS0(inst.FB) | (1ULL << 63);
	riPS1(inst.FD) = riPS1(inst.FB) | (1ULL << 63);
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
}

void Interpreter::ps_abs(UGeckoInstruction inst)
{
	riPS0(inst.FD) = riPS0(inst.FB) &~ (1ULL << 63);
	riPS1(inst.FD) = riPS1(inst.FB) &~ (1ULL << 63);
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
}

// These are just moves, double is OK.
void Interpreter::ps_merge00(UGeckoInstruction inst)
{
	double p0 = rPS0(inst.FA);
	double p1 = rPS0(inst.FB);
	rPS0(inst.FD) = p0;
	rPS1(inst.FD) = p1;
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
}

void Interpreter::ps_merge01(UGeckoInstruction inst)
{
	double p0 = rPS0(inst.FA);
	double p1 = rPS1(inst.FB);
	rPS0(inst.FD) = p0;
	rPS1(inst.FD) = p1;
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
}

void Interpreter::ps_merge10(UGeckoInstruction inst)
{
	double p0 = rPS1(inst.FA);
	double p1 = rPS0(inst.FB);
	rPS0(inst.FD) = p0;
	rPS1(inst.FD) = p1;
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
}

void Interpreter::ps_merge11(UGeckoInstruction inst)
{
	double p0 = rPS1(inst.FA);
	double p1 = rPS1(inst.FB);
	rPS0(inst.FD) = p0;
	rPS1(inst.FD) = p1;
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
}

// From here on, the real deal.
void Interpreter::ps_div(UGeckoInstruction inst)
{
	u32 ex_mask = 0;

	// PS0
	{
		double a = rPS0(inst.FA);
		double b = rPS0(inst.FB);
		double &res = rPS0(inst.FD);

		if (a != a) res = a;
		else if (b != b) res = b;
		else
		{
			if (b == 0.0)
			{
				ex_mask |= FPSCR_ZX;
				if (rPS0(inst.FA) == 0.0)
				{
					ex_mask |= FPSCR_VXZDZ;
					res = PPC_NAN;
				}
				else
				{
					res = ForceSingle(a / b);
				}
			}
			else
			{
				if (IsINF(a) && IsINF(b))
				{
					ex_mask |= FPSCR_VXIDI;
					res = PPC_NAN;
				}
				else
				{
					res = ForceSingle(a / b);
				}
			}
		}
	}

	// PS1
	{
		double a = rPS1(inst.FA);
		double b = rPS1(inst.FB);
		double &res = rPS1(inst.FD);

		if (a != a) res = a;
		else if (b != b) res = b;
		else
		{
			if (b == 0.0)
			{
				ex_mask |= FPSCR_ZX;
				if (rPS0(inst.FA) == 0.0)
				{
					ex_mask |= FPSCR_VXZDZ;
					res = PPC_NAN;
				}
				else
				{
					res = ForceSingle(a / b);
				}
			}
			else
			{
				if (IsINF(a) && IsINF(b))
				{
					ex_mask |= FPSCR_VXIDI;
					res = PPC_NAN;
				}
				else
				{
					res = ForceSingle(a / b);
				}
			}
		}
	}

	SetFPException(ex_mask);
	UpdateFPRF(rPS0(inst.FD));
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
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
	rPS0(inst.FD) = ForceSingle(1.0 / a);
	if (a != 0.0 && IsINF(rPS0(inst.FD)))
	{
		if (rPS0(inst.FD) > 0)
			riPS0(inst.FD) = MAX_SINGLE; // largest finite single
		else
			riPS0(inst.FD) = MIN_SINGLE; // most negative finite single
	}
	rPS1(inst.FD) = ForceSingle(1.0 / b);
	if (b != 0.0 && IsINF(rPS1(inst.FD)))
	{
		if (rPS1(inst.FD) > 0)
			riPS1(inst.FD) = MAX_SINGLE;
		else
			riPS1(inst.FD) = MIN_SINGLE;
	}
	UpdateFPRF(rPS0(inst.FD));
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
}

void Interpreter::ps_rsqrte(UGeckoInstruction inst)
{
	// this code is based on the real hardware tests
	if (rPS0(inst.FB) == 0.0 || rPS1(inst.FB) == 0.0)
	{
		SetFPException(FPSCR_ZX);
	}
	// PS0
	if (rPS0(inst.FB) < 0.0)
	{
		SetFPException(FPSCR_VXSQRT);
		rPS0(inst.FD) = PPC_NAN;
	}
	else
	{
		rPS0(inst.FD) = 1.0 / sqrt(rPS0(inst.FB));
		u32 t = ConvertToSingle(riPS0(inst.FD));
		rPS0(inst.FD) = *(float*)&t;
	}
	// PS1
	if (rPS1(inst.FB) < 0.0)
	{
		SetFPException(FPSCR_VXSQRT);
		rPS1(inst.FD) = PPC_NAN;
	}
	else
	{
		rPS1(inst.FD) = 1.0 / sqrt(rPS1(inst.FB));
		u32 t = ConvertToSingle(riPS1(inst.FD));
		rPS1(inst.FD) = *(float*)&t;
	}

	UpdateFPRF(rPS0(inst.FD));
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
}


void Interpreter::ps_sub(UGeckoInstruction inst)
{
	rPS0(inst.FD) = ForceSingle(NI_sub(rPS0(inst.FA), rPS0(inst.FB)));
	rPS1(inst.FD) = ForceSingle(NI_sub(rPS1(inst.FA), rPS1(inst.FB)));
	UpdateFPRF(rPS0(inst.FD));
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
}

void Interpreter::ps_add(UGeckoInstruction inst)
{
	rPS0(inst.FD) = ForceSingle(NI_add(rPS0(inst.FA), rPS0(inst.FB)));
	rPS1(inst.FD) = ForceSingle(NI_add(rPS1(inst.FA), rPS1(inst.FB)));
	UpdateFPRF(rPS0(inst.FD));
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
}

void Interpreter::ps_mul(UGeckoInstruction inst)
{
	rPS0(inst.FD) = ForceSingle(NI_mul(rPS0(inst.FA), rPS0(inst.FC)));
	rPS1(inst.FD) = ForceSingle(NI_mul(rPS1(inst.FA), rPS1(inst.FC)));
	UpdateFPRF(rPS0(inst.FD));
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
}


void Interpreter::ps_msub(UGeckoInstruction inst)
{
	rPS0(inst.FD) = ForceSingle(NI_msub(rPS0(inst.FA), rPS0(inst.FC), rPS0(inst.FB)));
	rPS1(inst.FD) = ForceSingle(NI_msub(rPS1(inst.FA), rPS1(inst.FC), rPS1(inst.FB)));
	UpdateFPRF(rPS0(inst.FD));
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
}

void Interpreter::ps_madd(UGeckoInstruction inst)
{
	rPS0(inst.FD) = ForceSingle(NI_madd(rPS0(inst.FA), rPS0(inst.FC), rPS0(inst.FB)));
	rPS1(inst.FD) = ForceSingle(NI_madd(rPS1(inst.FA), rPS1(inst.FC), rPS1(inst.FB)));
	UpdateFPRF(rPS0(inst.FD));
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
}

void Interpreter::ps_nmsub(UGeckoInstruction inst)
{
	rPS0(inst.FD) = ForceSingle( -NI_msub( rPS0(inst.FA), rPS0(inst.FC), rPS0(inst.FB) ) );
	rPS1(inst.FD) = ForceSingle( -NI_msub( rPS1(inst.FA), rPS1(inst.FC), rPS1(inst.FB) ) );
	UpdateFPRF(rPS0(inst.FD));
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
}

void Interpreter::ps_nmadd(UGeckoInstruction inst)
{
	rPS0(inst.FD) = ForceSingle( -NI_madd( rPS0(inst.FA), rPS0(inst.FC), rPS0(inst.FB) ) );
	rPS1(inst.FD) = ForceSingle( -NI_madd( rPS1(inst.FA), rPS1(inst.FC), rPS1(inst.FB) ) );
	UpdateFPRF(rPS0(inst.FD));
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
}

void Interpreter::ps_sum0(UGeckoInstruction inst)
{
	double p0 = ForceSingle(NI_add(rPS0(inst.FA), rPS1(inst.FB)));
	double p1 = ForceSingle(rPS1(inst.FC));
	rPS0(inst.FD) = p0;
	rPS1(inst.FD) = p1;
	UpdateFPRF(rPS0(inst.FD));
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
}

void Interpreter::ps_sum1(UGeckoInstruction inst)
{
	double p0 = ForceSingle(rPS0(inst.FC));
	double p1 = ForceSingle(NI_add(rPS0(inst.FA), rPS1(inst.FB)));
	rPS0(inst.FD) = p0;
	rPS1(inst.FD) = p1;
	UpdateFPRF(rPS1(inst.FD));
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
}

void Interpreter::ps_muls0(UGeckoInstruction inst)
{
	double p0 = ForceSingle(NI_mul(rPS0(inst.FA), rPS0(inst.FC)));
	double p1 = ForceSingle(NI_mul(rPS1(inst.FA), rPS0(inst.FC)));
	rPS0(inst.FD) = p0;
	rPS1(inst.FD) = p1;
	UpdateFPRF(rPS0(inst.FD));
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
}

void Interpreter::ps_muls1(UGeckoInstruction inst)
{
	double p0 = ForceSingle(NI_mul(rPS0(inst.FA), rPS1(inst.FC)));
	double p1 = ForceSingle(NI_mul(rPS1(inst.FA), rPS1(inst.FC)));
	rPS0(inst.FD) = p0;
	rPS1(inst.FD) = p1;
	UpdateFPRF(rPS0(inst.FD));
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
}

void Interpreter::ps_madds0(UGeckoInstruction inst)
{
	double p0 = ForceSingle( NI_madd( rPS0(inst.FA), rPS0(inst.FC), rPS0(inst.FB)) );
	double p1 = ForceSingle( NI_madd( rPS1(inst.FA), rPS0(inst.FC), rPS1(inst.FB)) );
	rPS0(inst.FD) = p0;
	rPS1(inst.FD) = p1;
	UpdateFPRF(rPS0(inst.FD));
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
}

void Interpreter::ps_madds1(UGeckoInstruction inst)
{
	double p0 = ForceSingle( NI_madd( rPS0(inst.FA), rPS1(inst.FC), rPS0(inst.FB)) );
	double p1 = ForceSingle( NI_madd( rPS1(inst.FA), rPS1(inst.FC), rPS1(inst.FB)) );
	rPS0(inst.FD) = p0;
	rPS1(inst.FD) = p1;
	UpdateFPRF(rPS0(inst.FD));
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
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
	//FAKE: clear memory instead of clearing the cache block
	Memory::Memset(Helper_Get_EA_X(inst) & (~31), 0, 32);
}
