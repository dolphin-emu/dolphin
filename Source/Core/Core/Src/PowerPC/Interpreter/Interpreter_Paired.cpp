// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <math.h>
#include "Common.h"
#include "MathUtil.h"
#include "Interpreter.h"
#include "../../HW/Memmap.h"

#include "Interpreter_FPUtils.h"

using namespace MathUtil;

// These "binary instructions" do not alter FPSCR.
void Interpreter::ps_sel(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS0(_inst.FA) >= -0.0 ? rPS0(_inst.FC) : rPS0(_inst.FB);
	rPS1(_inst.FD) = rPS1(_inst.FA) >= -0.0 ? rPS1(_inst.FC) : rPS1(_inst.FB);
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void Interpreter::ps_neg(UGeckoInstruction _inst)
{
	riPS0(_inst.FD) = riPS0(_inst.FB) ^ (1ULL << 63);
	riPS1(_inst.FD) = riPS1(_inst.FB) ^ (1ULL << 63);
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void Interpreter::ps_mr(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS0(_inst.FB);
	rPS1(_inst.FD) = rPS1(_inst.FB);
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void Interpreter::ps_nabs(UGeckoInstruction _inst)
{
	riPS0(_inst.FD) = riPS0(_inst.FB) | (1ULL << 63); 
	riPS1(_inst.FD) = riPS1(_inst.FB) | (1ULL << 63); 
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void Interpreter::ps_abs(UGeckoInstruction _inst)
{
	riPS0(_inst.FD) = riPS0(_inst.FB) &~ (1ULL << 63); 
	riPS1(_inst.FD) = riPS1(_inst.FB) &~ (1ULL << 63); 
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

// These are just moves, double is OK.
void Interpreter::ps_merge00(UGeckoInstruction _inst)
{
	double p0 = rPS0(_inst.FA);
	double p1 = rPS0(_inst.FB);
	rPS0(_inst.FD) = p0;
	rPS1(_inst.FD) = p1;
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void Interpreter::ps_merge01(UGeckoInstruction _inst)
{
	double p0 = rPS0(_inst.FA);
	double p1 = rPS1(_inst.FB);
	rPS0(_inst.FD) = p0;
	rPS1(_inst.FD) = p1;
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void Interpreter::ps_merge10(UGeckoInstruction _inst)
{
	double p0 = rPS1(_inst.FA);
	double p1 = rPS0(_inst.FB);
	rPS0(_inst.FD) = p0;
	rPS1(_inst.FD) = p1;
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void Interpreter::ps_merge11(UGeckoInstruction _inst)
{
	double p0 = rPS1(_inst.FA);
	double p1 = rPS1(_inst.FB);
	rPS0(_inst.FD) = p0;
	rPS1(_inst.FD) = p1;
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

// From here on, the real deal.
void Interpreter::ps_div(UGeckoInstruction _inst)
{	
	u32 ex_mask = 0;

	// PS0
	{
		double a = rPS0(_inst.FA);
		double b = rPS0(_inst.FB);
		double &res = rPS0(_inst.FD);

		if (a != a) res = a;
		else if (b != b) res = b;
		else
		{
			if (b == 0.0)
			{
				ex_mask |= FPSCR_ZX;
				if (rPS0(_inst.FA) == 0.0)
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
		double a = rPS1(_inst.FA);
		double b = rPS1(_inst.FB);
		double &res = rPS1(_inst.FD);

		if (a != a) res = a;
		else if (b != b) res = b;
		else
		{
			if (b == 0.0)
			{
				ex_mask |= FPSCR_ZX;
				if (rPS0(_inst.FA) == 0.0)
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
	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
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
	rPS0(_inst.FD) = ForceSingle(1.0 / a);
	if (a != 0.0 && IsINF(rPS0(_inst.FD)))
	{
		if (rPS0(_inst.FD) > 0)
			riPS0(_inst.FD) = MAX_SINGLE; // largest finite single
		else
			riPS0(_inst.FD) = MIN_SINGLE; // most negative finite single
	}
	rPS1(_inst.FD) = ForceSingle(1.0 / b);
	if (b != 0.0 && IsINF(rPS1(_inst.FD)))
	{
		if (rPS1(_inst.FD) > 0)
			riPS1(_inst.FD) = MAX_SINGLE;
		else
			riPS1(_inst.FD) = MIN_SINGLE;
	}
	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void Interpreter::ps_rsqrte(UGeckoInstruction _inst)
{
	// this code is based on the real hardware tests
	if (rPS0(_inst.FB) == 0.0 || rPS1(_inst.FB) == 0.0)
	{
		SetFPException(FPSCR_ZX);
	}
	// PS0
	if (rPS0(_inst.FB) < 0.0)
	{
		SetFPException(FPSCR_VXSQRT);
		rPS0(_inst.FD) = PPC_NAN;
	}
	else
	{
		rPS0(_inst.FD) = 1.0 / sqrt(rPS0(_inst.FB));
		u32 t = ConvertToSingle(riPS0(_inst.FD));
		rPS0(_inst.FD) = *(float*)&t;
	}
	// PS1
	if (rPS1(_inst.FB) < 0.0)
	{
		SetFPException(FPSCR_VXSQRT);
		rPS1(_inst.FD) = PPC_NAN;
	}
	else
	{
		rPS1(_inst.FD) = 1.0 / sqrt(rPS1(_inst.FB));
		u32 t = ConvertToSingle(riPS1(_inst.FD));
		rPS1(_inst.FD) = *(float*)&t;
	}

	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}


void Interpreter::ps_sub(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = ForceSingle(NI_sub(rPS0(_inst.FA), rPS0(_inst.FB)));
	rPS1(_inst.FD) = ForceSingle(NI_sub(rPS1(_inst.FA), rPS1(_inst.FB)));
	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void Interpreter::ps_add(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = ForceSingle(NI_add(rPS0(_inst.FA), rPS0(_inst.FB)));
	rPS1(_inst.FD) = ForceSingle(NI_add(rPS1(_inst.FA), rPS1(_inst.FB)));
	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void Interpreter::ps_mul(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = ForceSingle(NI_mul(rPS0(_inst.FA), rPS0(_inst.FC)));
	rPS1(_inst.FD) = ForceSingle(NI_mul(rPS1(_inst.FA), rPS1(_inst.FC)));
	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}


void Interpreter::ps_msub(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = ForceSingle(NI_msub(rPS0(_inst.FA), rPS0(_inst.FC), rPS0(_inst.FB)));
	rPS1(_inst.FD) = ForceSingle(NI_msub(rPS1(_inst.FA), rPS1(_inst.FC), rPS1(_inst.FB)));
	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void Interpreter::ps_madd(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = ForceSingle(NI_madd(rPS0(_inst.FA), rPS0(_inst.FC), rPS0(_inst.FB)));
	rPS1(_inst.FD) = ForceSingle(NI_madd(rPS1(_inst.FA), rPS1(_inst.FC), rPS1(_inst.FB)));
	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void Interpreter::ps_nmsub(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = ForceSingle( -NI_msub( rPS0(_inst.FA), rPS0(_inst.FC), rPS0(_inst.FB) ) );
	rPS1(_inst.FD) = ForceSingle( -NI_msub( rPS1(_inst.FA), rPS1(_inst.FC), rPS1(_inst.FB) ) );
	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void Interpreter::ps_nmadd(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = ForceSingle( -NI_madd( rPS0(_inst.FA), rPS0(_inst.FC), rPS0(_inst.FB) ) );
	rPS1(_inst.FD) = ForceSingle( -NI_madd( rPS1(_inst.FA), rPS1(_inst.FC), rPS1(_inst.FB) ) );
	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void Interpreter::ps_sum0(UGeckoInstruction _inst)
{
	double p0 = ForceSingle(NI_add(rPS0(_inst.FA), rPS1(_inst.FB)));
	double p1 = ForceSingle(rPS1(_inst.FC));
	rPS0(_inst.FD) = p0;
	rPS1(_inst.FD) = p1;
	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void Interpreter::ps_sum1(UGeckoInstruction _inst)
{
	double p0 = ForceSingle(rPS0(_inst.FC));
	double p1 = ForceSingle(NI_add(rPS0(_inst.FA), rPS1(_inst.FB)));
	rPS0(_inst.FD) = p0;
	rPS1(_inst.FD) = p1;
	UpdateFPRF(rPS1(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void Interpreter::ps_muls0(UGeckoInstruction _inst)
{
	double p0 = ForceSingle(NI_mul(rPS0(_inst.FA), rPS0(_inst.FC)));
	double p1 = ForceSingle(NI_mul(rPS1(_inst.FA), rPS0(_inst.FC)));
	rPS0(_inst.FD) = p0;
	rPS1(_inst.FD) = p1;
	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void Interpreter::ps_muls1(UGeckoInstruction _inst)
{
	double p0 = ForceSingle(NI_mul(rPS0(_inst.FA), rPS1(_inst.FC)));
	double p1 = ForceSingle(NI_mul(rPS1(_inst.FA), rPS1(_inst.FC)));
	rPS0(_inst.FD) = p0;
	rPS1(_inst.FD) = p1;
	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void Interpreter::ps_madds0(UGeckoInstruction _inst)
{
	double p0 = ForceSingle( NI_madd( rPS0(_inst.FA), rPS0(_inst.FC), rPS0(_inst.FB)) );
	double p1 = ForceSingle( NI_madd( rPS1(_inst.FA), rPS0(_inst.FC), rPS1(_inst.FB)) );
	rPS0(_inst.FD) = p0;
	rPS1(_inst.FD) = p1;
	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void Interpreter::ps_madds1(UGeckoInstruction _inst)
{
	double p0 = ForceSingle( NI_madd( rPS0(_inst.FA), rPS1(_inst.FC), rPS0(_inst.FB)) );
	double p1 = ForceSingle( NI_madd( rPS1(_inst.FA), rPS1(_inst.FC), rPS1(_inst.FB)) );
	rPS0(_inst.FD) = p0;
	rPS1(_inst.FD) = p1;
	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void Interpreter::ps_cmpu0(UGeckoInstruction _inst)
{
	double fa = rPS0(_inst.FA);
	double fb = rPS0(_inst.FB);
	int compareResult;

	if (fa < fb)         		compareResult = 8; 
	else if (fa > fb)        	compareResult = 4; 
	else if (fa == fb)			compareResult = 2;
	else						
	{
		compareResult = 1;
		if (IsSNAN(fa) || IsSNAN(fb))
		{
			SetFPException(FPSCR_VXSNAN);
		}
	}
	FPSCR.FPRF = compareResult;
	SetCRField(_inst.CRFD, compareResult);	
}

void Interpreter::ps_cmpo0(UGeckoInstruction _inst)
{	
	double fa = rPS0(_inst.FA);
	double fb = rPS0(_inst.FB);
	int compareResult;

	if (fa < fb)         		compareResult = 8; 
	else if (fa > fb)        	compareResult = 4; 
	else if (fa == fb)			compareResult = 2;
	else
	{
		compareResult = 1;
		if (IsSNAN(fa) || IsSNAN(fb))
		{
			SetFPException(FPSCR_VXSNAN);
			if (!FPSCR.VE) 
				SetFPException(FPSCR_VXVC);
		}
		else 
		{
			//if (IsQNAN(fa) || IsQNAN(fb)) // this is always true
			SetFPException(FPSCR_VXVC);
		}
	}
	FPSCR.FPRF = compareResult;
	SetCRField(_inst.CRFD, compareResult);	
}

void Interpreter::ps_cmpu1(UGeckoInstruction _inst)
{
	double fa = rPS1(_inst.FA);
	double fb = rPS1(_inst.FB);
	int compareResult;

	if (fa < fb)         		compareResult = 8; 
	else if (fa > fb)        	compareResult = 4; 
	else if (fa == fb)			compareResult = 2;
	else						
	{
		compareResult = 1;
		if (IsSNAN(fa) || IsSNAN(fb))
		{
			SetFPException(FPSCR_VXSNAN);
		}
	}
	FPSCR.FPRF = compareResult;
	SetCRField(_inst.CRFD, compareResult);	
}

void Interpreter::ps_cmpo1(UGeckoInstruction _inst)
{	
	double fa = rPS1(_inst.FA);
	double fb = rPS1(_inst.FB);
	int compareResult;

	if (fa < fb)         		compareResult = 8; 
	else if (fa > fb)        	compareResult = 4; 
	else if (fa == fb)			compareResult = 2;
	else
	{
		compareResult = 1;
		if (IsSNAN(fa) || IsSNAN(fb))
		{
			SetFPException(FPSCR_VXSNAN);
			if (!FPSCR.VE) 
				SetFPException(FPSCR_VXVC);
		}
		else 
		{
			//if (IsQNAN(fa) || IsQNAN(fb)) // this is always true
			SetFPException(FPSCR_VXVC);
		}
	}
	FPSCR.FPRF = compareResult;
	SetCRField(_inst.CRFD, compareResult);	
}

// __________________________________________________________________________________________________
// dcbz_l
// TODO(ector) check docs
void Interpreter::dcbz_l(UGeckoInstruction _inst)
{
	//FAKE: clear memory instead of clearing the cache block
	Memory::Memset(Helper_Get_EA_X(_inst) & (~31), 0, 32);
}
