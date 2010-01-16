// Copyright (C) 2003 Dolphin Project.

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
#include <limits>

#ifdef _WIN32
#define _interlockedbittestandset workaround_ms_header_bug_platform_sdk6_set
#define _interlockedbittestandreset workaround_ms_header_bug_platform_sdk6_reset
#define _interlockedbittestandset64 workaround_ms_header_bug_platform_sdk6_set64
#define _interlockedbittestandreset64 workaround_ms_header_bug_platform_sdk6_reset64
#include <intrin.h>
#undef _interlockedbittestandset
#undef _interlockedbittestandreset
#undef _interlockedbittestandset64
#undef _interlockedbittestandreset64
#else
#include <xmmintrin.h>
#endif

#include "../../Core.h"
#include "Interpreter.h"
#include "MathUtil.h"
#include "Interpreter_FPUtils.h"

using namespace MathUtil;

namespace Interpreter
{

void UpdateSSEState();

// Extremely rare - actually, never seen.
// Star Wars : Rogue Leader spams that at some point :|
void Helper_UpdateCR1(double _fValue)
{
	// Should just update exception flags, not do any compares.
	PanicAlert("CR1");
}

void fcmpo(UGeckoInstruction _inst)
{	
	double fa = rPS0(_inst.FA);
	double fb = rPS0(_inst.FB);

	int compareResult;

	if (fa < fb)				compareResult = 8; 
	else if (fa > fb)           compareResult = 4; 
	else if (fa == fb)          compareResult = 2;
	else
	{
		FPSCR.FX = 1;
		compareResult = 1;
		if (IsSNAN(fa) || IsSNAN(fb))
		{
			SetFPException(FPSCR_VXSNAN);			
			if (FPSCR.VE == 0)
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

void fcmpu(UGeckoInstruction _inst)
{	
	double fa = rPS0(_inst.FA);
	double fb = rPS0(_inst.FB);

	int compareResult;

	if (fa < fb)            compareResult = 8; 
	else if (fa > fb)       compareResult = 4; 
	else if (fa == fb)		compareResult = 2;
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

// Apply current rounding mode
void fctiwx(UGeckoInstruction _inst)
{
	const double b = rPS0(_inst.FB);
	u32 value;
	if (b > (double)0x7fffffff)
	{
		value = 0x7fffffff;		
		SetFPException(FPSCR_VXCVI);
		FPSCR.FI = 0;
		FPSCR.FR = 0;
	}
	else if (b < -(double)0x80000000) 
	{
		value = 0x80000000;
		SetFPException(FPSCR_VXCVI);
		FPSCR.FI = 0;
		FPSCR.FR = 0;
	}
	else
	{
		s32 i;
		switch (FPSCR.RN)
		{
		case 0: // nearest
			{
				double t = b + 0.5;
				i = (s32)t;
				if (t - i < 0 || (t - i == 0 && b > 0)) i--;
				break;
			}
		case 1: // zero
			i = (s32)b;
			break;
		case 2: // +inf			
			i = (s32)b;
			if (b - i > 0) i++;
			break;
		case 3: // -inf			
			i = (s32)b;
			if (b - i < 0) i--;
			break;
		}
		value = (u32)i;
		double di = i;
		if (di == b)
		{
			FPSCR.FI = 0;
			FPSCR.FR = 0;
		}
		else
		{
			SetFI(1);			
			FPSCR.FR = fabs(di) > fabs(b);
		}
	}	
	// based on HW tests
	// FPRF is not affected
	riPS0(_inst.FD) = 0xfff8000000000000ull | value;
	if (value == 0 && ( (*(u64*)&b) & DOUBLE_SIGN ))
		riPS0(_inst.FD) |= 0x100000000ull;
	if (_inst.Rc)
		Helper_UpdateCR1(rPS0(_inst.FD));
}

// Always round toward zero
void fctiwzx(UGeckoInstruction _inst)
{
	const double b = rPS0(_inst.FB);
	u32 value;
	if (b > (double)0x7fffffff)
	{
		value = 0x7fffffff;
		SetFPException(FPSCR_VXCVI);
		FPSCR.FI = 0;
		FPSCR.FR = 0;
	}
	else if (b < -(double)0x80000000)
	{
		value = 0x80000000;
		SetFPException(FPSCR_VXCVI);
		FPSCR.FI = 0;
		FPSCR.FR = 0;
	}
	else
	{
		s32 i = (s32)b;
		double di = i;
		if (di == b)
		{
			FPSCR.FI = 0;
			FPSCR.FR = 0;
		}
		else
		{
			SetFI(1);
			FPSCR.FR = fabs(di) > fabs(b);
		}
		value = (u32)i;
	}
	// based on HW tests
	// FPRF is not affected
	riPS0(_inst.FD) = 0xfff8000000000000ull | value;
	if (value == 0 && ( (*(u64*)&b) & DOUBLE_SIGN ))
		riPS0(_inst.FD) |= 0x100000000ull;	
	if (_inst.Rc)
		Helper_UpdateCR1(rPS0(_inst.FD));
}

void fmrx(UGeckoInstruction _inst)
{
	riPS0(_inst.FD) = riPS0(_inst.FB);
	// This is a binary instruction. Does not alter FPSCR
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}

void fabsx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = fabs(rPS0(_inst.FB));
	// This is a binary instruction. Does not alter FPSCR
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void fnabsx(UGeckoInstruction _inst)
{
	riPS0(_inst.FD) = riPS0(_inst.FB) | (1ULL << 63);
	// This is a binary instruction. Does not alter FPSCR
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));	
}	

void fnegx(UGeckoInstruction _inst)
{
	riPS0(_inst.FD) = riPS0(_inst.FB) ^ (1ULL << 63);
	// This is a binary instruction. Does not alter FPSCR
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void fselx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = (rPS0(_inst.FA) >= -0.0) ? rPS0(_inst.FC) : rPS0(_inst.FB);
	// This is a binary instruction. Does not alter FPSCR
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

// !!! warning !!!
// PS1 must be set to the value of PS0 or DragonballZ will be f**ked up
// PS1 is said to be undefined
void frspx(UGeckoInstruction _inst)  // round to single
{
	double b = rPS0(_inst.FB);
	double rounded = ForceSingle(b);
	SetFI(b != rounded);
	FPSCR.FR = fabs(rounded) > fabs(b);
	UpdateFPRF(rounded);
	rPS0(_inst.FD) = rPS1(_inst.FD) = rounded;
	return;
}


void fmulx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = ForceDouble(NI_mul(rPS0(_inst.FA), rPS0(_inst.FC)));
	FPSCR.FI = 0; // are these flags important?
	FPSCR.FR = 0;
	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}
void fmulsx(UGeckoInstruction _inst)
{
	double d_value = NI_mul(rPS0(_inst.FA), rPS0(_inst.FC));
	rPS0(_inst.FD) = rPS1(_inst.FD) = ForceSingle(d_value);
	//FPSCR.FI = d_value != rPS0(_inst.FD);
	FPSCR.FI = 0;
	FPSCR.FR = 0;
	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void fmaddx(UGeckoInstruction _inst)
{
	double result = ForceDouble(NI_madd( rPS0(_inst.FA), rPS0(_inst.FC), rPS0(_inst.FB) ));
	rPS0(_inst.FD) = result;
	UpdateFPRF(result);
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void fmaddsx(UGeckoInstruction _inst)
{
	double d_value = NI_madd( rPS0(_inst.FA), rPS0(_inst.FC), rPS0(_inst.FB) );
	rPS0(_inst.FD) = rPS1(_inst.FD) = ForceSingle(d_value);
	FPSCR.FI = d_value != rPS0(_inst.FD);
	FPSCR.FR = 0;
 	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}


void faddx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = ForceDouble(NI_add(rPS0(_inst.FA), rPS0(_inst.FB)));
 	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}
void faddsx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS1(_inst.FD) = ForceSingle(NI_add(rPS0(_inst.FA), rPS0(_inst.FB)));
 	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}

void fdivx(UGeckoInstruction _inst)
{
	double a = rPS0(_inst.FA);
	double b = rPS0(_inst.FB);
	if (a != a) rPS0(_inst.FD) = a;
	else if (b != b) rPS0(_inst.FD) = b;
	else
	{
		rPS0(_inst.FD) = ForceDouble(a / b);
		if (b == 0.0) 
		{		
			if (a == 0.0)
			{	
				SetFPException(FPSCR_VXZDZ);
				rPS0(_inst.FD) = PPC_NAN;
			}
			SetFPException(FPSCR_ZX);
		}
		else
		{
			if (IsINF(a) && IsINF(b))
			{
				SetFPException(FPSCR_VXIDI);
				rPS0(_inst.FD) = PPC_NAN;
			}
		}
	}
 	UpdateFPRF(rPS0(_inst.FD));
	// FR,FI,OX,UX???
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}
void fdivsx(UGeckoInstruction _inst)
{
	double a = rPS0(_inst.FA);
	double b = rPS0(_inst.FB);
	double res;
	if (a != a) res = a;
	else if (b != b) res = b;
	else
	{
		res = ForceSingle(a / b);
		if (b == 0.0)
		{					
			if (a == 0.0)
			{					
				SetFPException(FPSCR_VXZDZ);
				res = PPC_NAN;				
			}			
			SetFPException(FPSCR_ZX);
		}
		else
		{
			if (IsINF(a) && IsINF(b))
			{
				SetFPException(FPSCR_VXIDI);
				res = PPC_NAN;
			}
		}
	}
	rPS0(_inst.FD) = rPS1(_inst.FD) = res;
 	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

// Single precision only.
void fresx(UGeckoInstruction _inst)
{
	double b = rPS0(_inst.FB);
	double one_over = ForceSingle(1.0 / b);
	// this is based on the real hardware tests
	if (b != 0.0 && IsINF(one_over)) 
	{
		if (one_over > 0)
			riPS0(_inst.FD) = riPS1(_inst.FD) = MAX_SINGLE;
		else
			riPS0(_inst.FD) = riPS1(_inst.FD) = MIN_SINGLE;
	}
	else
	{
		rPS0(_inst.FD) = rPS1(_inst.FD) = one_over;
	}
	if (b == 0.0)
	{
		SetFPException(FPSCR_ZX);
	}
 	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void frsqrtex(UGeckoInstruction _inst)
{
	double b = rPS0(_inst.FB);
	if (b < 0.0) 
	{
		SetFPException(FPSCR_VXSQRT);
		rPS0(_inst.FD) = PPC_NAN;
	} 
	else 
	{
		if (b == 0.0) SetFPException(FPSCR_ZX);
		rPS0(_inst.FD) = ForceDouble(1.0 / sqrt(b));
	}
 	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void fmsubx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = ForceDouble(NI_msub( rPS0(_inst.FA), rPS0(_inst.FC), rPS0(_inst.FB) ));
 	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}

void fmsubsx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS1(_inst.FD) =
		ForceSingle( NI_msub(rPS0(_inst.FA), rPS0(_inst.FC), rPS0(_inst.FB) ));
 	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}

void fnmaddx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = ForceDouble(-NI_madd(rPS0(_inst.FA), rPS0(_inst.FC), rPS0(_inst.FB)));
 	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}
void fnmaddsx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS1(_inst.FD) = 
		ForceSingle(-NI_madd(rPS0(_inst.FA), rPS0(_inst.FC), rPS0(_inst.FB)));
 	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}

void fnmsubx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = ForceDouble(-NI_msub(rPS0(_inst.FA), rPS0(_inst.FC), rPS0(_inst.FB)));
 	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

// fnmsubsx does not handle QNAN properly - see NI_msub
void fnmsubsx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS1(_inst.FD) =
		ForceSingle(-NI_msub(rPS0(_inst.FA), rPS0(_inst.FC), rPS0(_inst.FB)));
 	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}

void fsubx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = ForceDouble(NI_sub(rPS0(_inst.FA), rPS0(_inst.FB)));
 	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void fsubsx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS1(_inst.FD) = ForceSingle(NI_sub(rPS0(_inst.FA), rPS0(_inst.FB)));
 	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void fsqrtx(UGeckoInstruction _inst)
{
	// GEKKO is not supposed to support this instruction.
	// PanicAlert("fsqrtx");
	double b = rPS0(_inst.FB);
	if (b < 0.0) {
		FPSCR.VXSQRT = 1;
	}
	rPS0(_inst.FD) = sqrt(b);
 	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

}  // namespace
