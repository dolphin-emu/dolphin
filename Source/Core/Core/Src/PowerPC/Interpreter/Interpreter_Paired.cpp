// Copyright (C) 2003-2008 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include <math.h>
#include "Interpreter.h"
#include "../../HW/Memmap.h"

// These "binary instructions" do not alter FPSCR.
void CInterpreter::ps_sel(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = static_cast<float>((rPS0(_inst.FA) >= -0.0) ? rPS0(_inst.FC) : rPS0(_inst.FB));
	rPS1(_inst.FD) = static_cast<float>((rPS1(_inst.FA) >= -0.0) ? rPS1(_inst.FC) : rPS1(_inst.FB));
}

void CInterpreter::ps_neg(UGeckoInstruction _inst)
{
	riPS0(_inst.FD) = riPS0(_inst.FB) ^ (1ULL << 63);
	riPS1(_inst.FD) = riPS1(_inst.FB) ^ (1ULL << 63);
}

void CInterpreter::ps_mr(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS0(_inst.FB);
	rPS1(_inst.FD) = rPS1(_inst.FB);
}

void CInterpreter::ps_nabs(UGeckoInstruction _inst)
{
	riPS0(_inst.FD) = riPS0(_inst.FB) | (1ULL << 63); 
	riPS1(_inst.FD) = riPS1(_inst.FB) | (1ULL << 63); 
}

void CInterpreter::ps_abs(UGeckoInstruction _inst)
{
	riPS0(_inst.FD) = riPS0(_inst.FB) &~ (1ULL << 63); 
	riPS1(_inst.FD) = riPS1(_inst.FB) &~ (1ULL << 63); 
}

// These are just moves, double is OK.
void CInterpreter::ps_merge00(UGeckoInstruction _inst)
{
	double p0 = rPS0(_inst.FA);
	double p1 = rPS0(_inst.FB);
	rPS0(_inst.FD) = p0;
	rPS1(_inst.FD) = p1;
}

void CInterpreter::ps_merge01(UGeckoInstruction _inst)
{
	double p0 = rPS0(_inst.FA);
	double p1 = rPS1(_inst.FB);
	rPS0(_inst.FD) = p0;
	rPS1(_inst.FD) = p1;
}

void CInterpreter::ps_merge10(UGeckoInstruction _inst)
{
	double p0 = rPS1(_inst.FA);
	double p1 = rPS0(_inst.FB);
	rPS0(_inst.FD) = p0;
	rPS1(_inst.FD) = p1;
}

void CInterpreter::ps_merge11(UGeckoInstruction _inst)
{
	double p0 = rPS1(_inst.FA);
	double p1 = rPS1(_inst.FB);
	rPS0(_inst.FD) = p0;
	rPS1(_inst.FD) = p1;
}


// From here on, the real deal.

void CInterpreter::ps_div(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = static_cast<float>(rPS0(_inst.FA) / rPS0(_inst.FB));
	rPS1(_inst.FD) = static_cast<float>(rPS1(_inst.FA) / rPS1(_inst.FB));
	FPSCR.FI = 0;
	if (fabs(rPS0(_inst.FB)) == 0.0) {
		FPSCR.ZX = 1;
	}
}

void CInterpreter::ps_sub(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = static_cast<float>(rPS0(_inst.FA) - rPS0(_inst.FB));
	rPS1(_inst.FD) = static_cast<float>(rPS1(_inst.FA) - rPS1(_inst.FB));
}

void CInterpreter::ps_add(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = static_cast<float>(rPS0(_inst.FA) + rPS0(_inst.FB));
	rPS1(_inst.FD) = static_cast<float>(rPS1(_inst.FA) + rPS1(_inst.FB));
}

void CInterpreter::ps_res(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = 1.0f / static_cast<float>(rPS0(_inst.FB));
	rPS1(_inst.FD) = 1.0f / static_cast<float>(rPS1(_inst.FB));
}

void CInterpreter::ps_mul(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = static_cast<float>(rPS0(_inst.FA) * rPS0(_inst.FC));
	rPS1(_inst.FD) = static_cast<float>(rPS1(_inst.FA) * rPS1(_inst.FC));
}

void CInterpreter::ps_rsqrte(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = static_cast<double>(1.0f / sqrtf((float)rPS0(_inst.FB)));
	rPS1(_inst.FD) = static_cast<double>(1.0f / sqrtf((float)rPS1(_inst.FB)));
	if (fabs(rPS0(_inst.FB)) == 0.0) {
		FPSCR.ZX = 1;
	}
}

void CInterpreter::ps_msub(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = static_cast<float>((rPS0(_inst.FA) * rPS0(_inst.FC)) - rPS0(_inst.FB));
	rPS1(_inst.FD) = static_cast<float>((rPS1(_inst.FA) * rPS1(_inst.FC)) - rPS1(_inst.FB));
}

void CInterpreter::ps_madd(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = static_cast<float>((rPS0(_inst.FA) * rPS0(_inst.FC)) + rPS0(_inst.FB));
	rPS1(_inst.FD) = static_cast<float>((rPS1(_inst.FA) * rPS1(_inst.FC)) + rPS1(_inst.FB));
}

void CInterpreter::ps_nmsub(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = static_cast<float>(-(rPS0(_inst.FA) * rPS0(_inst.FC) - rPS0(_inst.FB)));
	rPS1(_inst.FD) = static_cast<float>(-(rPS1(_inst.FA) * rPS1(_inst.FC) - rPS1(_inst.FB)));
}

void CInterpreter::ps_nmadd(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = static_cast<float>(-(rPS0(_inst.FA) * rPS0(_inst.FC) + rPS0(_inst.FB)));
	rPS1(_inst.FD) = static_cast<float>(-(rPS1(_inst.FA) * rPS1(_inst.FC) + rPS1(_inst.FB)));
}

void CInterpreter::ps_sum0(UGeckoInstruction _inst)
{
	double p0 = (float)(rPS0(_inst.FA) + rPS1(_inst.FB));
	double p1 = (float)(rPS1(_inst.FC));
	rPS0(_inst.FD) = p0;
	rPS1(_inst.FD) = p1;
}

void CInterpreter::ps_sum1(UGeckoInstruction _inst)
{
	double p0 = rPS0(_inst.FC);
	double p1 = rPS0(_inst.FA) + rPS1(_inst.FB);
	rPS0(_inst.FD) = p0;
	rPS1(_inst.FD) = p1;
}

void CInterpreter::ps_muls0(UGeckoInstruction _inst)
{
	double p0 = rPS0(_inst.FA) * rPS0(_inst.FC);
	double p1 = rPS1(_inst.FA) * rPS0(_inst.FC);
	rPS0(_inst.FD) = p0;
	rPS1(_inst.FD) = p1;
}

void CInterpreter::ps_muls1(UGeckoInstruction _inst)
{
	double p0 = rPS0(_inst.FA) * rPS1(_inst.FC);
	double p1 = rPS1(_inst.FA) * rPS1(_inst.FC);
	rPS0(_inst.FD) = p0;
	rPS1(_inst.FD) = p1;
}

void CInterpreter::ps_madds0(UGeckoInstruction _inst)
{
	double p0 = (rPS0(_inst.FA) * rPS0(_inst.FC)) + rPS0(_inst.FB);
	double p1 = (rPS1(_inst.FA) * rPS0(_inst.FC)) + rPS1(_inst.FB);
	rPS0(_inst.FD) = p0;
	rPS1(_inst.FD) = p1;
}

void CInterpreter::ps_madds1(UGeckoInstruction _inst)
{
	double p0 = (rPS0(_inst.FA) * rPS1(_inst.FC)) + rPS0(_inst.FB);
	double p1 = (rPS1(_inst.FA) * rPS1(_inst.FC)) + rPS1(_inst.FB);
	rPS0(_inst.FD) = p0;
	rPS1(_inst.FD) = p1;
}

void CInterpreter::ps_cmpu0(UGeckoInstruction _inst)
{
	double fa = rPS0(_inst.FA);
	double fb = rPS0(_inst.FB);
	int compareResult;
	if (fa < fb)		compareResult = 8; 
	else if (fa > fb) 	compareResult = 4; 
	else				compareResult = 2;
	SetCRField(_inst.CRFD, compareResult);
}

void CInterpreter::ps_cmpo0(UGeckoInstruction _inst)
{
	// for now HACK
	ps_cmpu0(_inst);
}

void CInterpreter::ps_cmpu1(UGeckoInstruction _inst)
{
	double fa = rPS1(_inst.FA);
	double fb = rPS1(_inst.FB);
	int compareResult;
	if (fa < fb)		compareResult = 8; 
	else if (fa > fb)	compareResult = 4; 
	else				compareResult = 2;

	SetCRField(_inst.CRFD, compareResult);
}

void CInterpreter::ps_cmpo1(UGeckoInstruction _inst)
{
	// for now HACK
	ps_cmpu1(_inst);
}

// __________________________________________________________________________________________________
// dcbz_l
// TODO(ector) check docs
void 
CInterpreter::dcbz_l(UGeckoInstruction _inst)
{
	// This is supposed to allocate a cache line in the locked cache. Not entirely sure how
	// this is visible to the rest of the world. For now, we ignore it.
	/*
	addr_t ea = Helper_Get_EA(_inst);

	u32 blockStart = ea & (~(CACHEBLOCKSIZE-1));
	u32 blockEnd = blockStart + CACHEBLOCKSIZE;

	//FAKE: clear memory instead of clearing the cache block
	for (int i=blockStart; i<blockEnd; i+=4)
	Memory::Write_U32(0,i);
	*/
}
