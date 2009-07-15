// Copyright (C) 2003-2009 Dolphin Project.

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
#ifndef _mm_cvttsd_si32 // No SSE2 support
#define _mm_set_sd
#define _mm_cvttsd_si32 truncl
#define _mm_cvtsd_si32 lrint
#endif
// F-ZERO IS BEING A ROYAL PAIN
// POSSIBLE APPROACHES:
// * Full SW FPU. Urgh.
// * Partial SW FPU, emulate just as much as necessary for f-zero. Feasible, I guess.
// * HLE hacking. Figure out what all the evil functions really do and fake them.
//   This worked well for Monkey Ball, not so much for F-Zero.

using namespace MathUtil;

namespace Interpreter
{

void UpdateFPSCR(UReg_FPSCR fp);
void UpdateSSEState();

// Extremely rare - actually, never seen.
void Helper_UpdateCR1(double _fValue)
{
	// Should just update exception flags, not do any compares.
	PanicAlert("CR1");
}

void fcmpo(UGeckoInstruction _inst)
{
	// Use FlushToZeroAsFloat() to fix a couple of games - but seriously,
	// the real problem should be fixed instead.
	double fa = rPS0(_inst.FA);
	double fb = rPS0(_inst.FB);

	int compareResult;
	if (IsNAN(fa) || IsNAN(fb)) 
	{
		FPSCR.FX = 1;
		compareResult = 1;
		if (IsSNAN(fa) || IsSNAN(fb))
		{
			FPSCR.VXSNAN = 1;
			if (!FPSCR.FEX || IsQNAN(fa) || IsQNAN(fb))
				FPSCR.VXVC = 1;
		}
	}
	else if (fa < fb)           compareResult = 8; 
	else if (fa > fb)           compareResult = 4; 
	else                        compareResult = 2;

	FPSCR.FPRF = compareResult;
	SetCRField(_inst.CRFD, compareResult);
}

void fcmpu(UGeckoInstruction _inst)
{
	// Use FlushToZeroAsFloat() to fix a couple of games - but seriously,
	// the real problem should be fixed instead.
	double fa = rPS0(_inst.FA);
	double fb = rPS0(_inst.FB);

	int compareResult;
	if (IsNAN(fa) || IsNAN(fb))
	{
		FPSCR.FX = 1;
		compareResult = 1; 
		if (IsSNAN(fa) || IsSNAN(fb))
		{
			FPSCR.VXSNAN = 1;
		}
	}
	else if (fa < fb)            compareResult = 8; 
	else if (fa > fb)            compareResult = 4; 
	else                         compareResult = 2;

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
		FPSCR.VXCVI = 1;
	}
	else if (b < -(double)0x7fffffff) 
	{
		value = 0x80000000; 
		FPSCR.VXCVI = 1;
	}
	else
	{
		value = (u32)(s32)_mm_cvtsd_si32(_mm_set_sd(b));  // obey current rounding mode
//		double d_value = (double)value;
//		bool inexact = (d_value != b);
//		FPSCR.FI = inexact ? 1 : 0;
//		FPSCR.XX |= FPSCR.FI;
//		FPSCR.FR = fabs(d_value) > fabs(b);
	}

	//TODO: FR
	//FPRF undefined

	riPS0(_inst.FD) = (u64)value; // zero extend
	if (_inst.Rc) 
		Helper_UpdateCR1(rPS0(_inst.FD));
}

/*
In the float -> int direction, floating point input values larger than the largest 
representable int result in 0x80000000 (a very negative number) rather than the 
largest representable int on PowerPC. */

// Always round toward zero
void fctiwzx(UGeckoInstruction _inst)
{
	const double b = rPS0(_inst.FB);
	u32 value;
	if (b > (double)0x7fffffff)
	{
		value = 0x7fffffff;
		FPSCR.VXCVI = 1;
	}
	else if (b < -(double)0x7fffffff)
	{
		value = 0x80000000;
		FPSCR.VXCVI = 1;
	}
	else
	{
		value = (u32)(s32)_mm_cvttsd_si32(_mm_set_sd(b)); // truncate
//		double d_value = (double)value;
//		bool inexact = (d_value != b);
//		FPSCR.FI = inexact ? 1 : 0;
//		FPSCR.XX |= FPSCR.FI;
//		FPSCR.FR = 1; //fabs(d_value) > fabs(b);
	}

	riPS0(_inst.FD) = (u64)value;
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
	double rounded = (double)(float)b;
	//FPSCR.FI = b != rounded;
	UpdateFPRF(rounded);
	rPS0(_inst.FD) = rPS1(_inst.FD) = rounded;
	return;
}


void fmulx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS0(_inst.FA) * rPS0(_inst.FC);
	FPSCR.FI = 0;
	FPSCR.FR = 1;
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}
void fmulsx(UGeckoInstruction _inst)
{
	double d_value = rPS0(_inst.FA) * rPS0(_inst.FC);
	rPS0(_inst.FD) = rPS1(_inst.FD) = static_cast<float>(d_value);
	FPSCR.FI = d_value != rPS0(_inst.FD);
	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));  
}


void fmaddx(UGeckoInstruction _inst)
{
	double result = (rPS0(_inst.FA) * rPS0(_inst.FC)) + rPS0(_inst.FB);
	rPS0(_inst.FD) = result;
	UpdateFPRF(result);
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void fmaddsx(UGeckoInstruction _inst)
{
	double d_value = (rPS0(_inst.FA) * rPS0(_inst.FC)) + rPS0(_inst.FB);
	rPS0(_inst.FD) = rPS1(_inst.FD) = static_cast<float>(d_value);
	FPSCR.FI = d_value != rPS0(_inst.FD);
	FPSCR.FR = 0;
 	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}


void faddx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS0(_inst.FA) + rPS0(_inst.FB);
 	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}
void faddsx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS1(_inst.FD) = static_cast<float>(rPS0(_inst.FA) + rPS0(_inst.FB));
 	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}


void fdivx(UGeckoInstruction _inst)
{
	double a = rPS0(_inst.FA);
	double b = rPS0(_inst.FB);
	rPS0(_inst.FD) = a / b;
	if (b == 0.0) {
		if (!FPSCR.ZX)
			FPSCR.FX = 1;
		FPSCR.ZX = 1;
		FPSCR.XX = 1;
	}
 	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}
void fdivsx(UGeckoInstruction _inst)
{
	float a = (float)rPS0(_inst.FA);
	float b = (float)rPS0(_inst.FB);
	rPS0(_inst.FD) = rPS1(_inst.FD) = a / b;
	if (b == 0.0)
	{
		if (!FPSCR.ZX)
			FPSCR.FX = 1;
		FPSCR.ZX = 1;
		FPSCR.XX = 1;
	}
 	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));  
}

// Single precision only.
void fresx(UGeckoInstruction _inst)
{
	float b = (float)rPS0(_inst.FB);
	float one_over = 1.0f / b;
	rPS0(_inst.FD) = rPS1(_inst.FD) = one_over;
	if (b == 0.0)
	{
		if (!FPSCR.ZX)
			FPSCR.FX = 1;
		FPSCR.ZX = 1;
		FPSCR.XX = 1;
	}
 	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}

void frsqrtex(UGeckoInstruction _inst)
{
	float b = (float)rPS0(_inst.FB);
	if (b < 0.0) {
		FPSCR.VXSQRT = 1;
	} else if (b == 0) {
		FPSCR.ZX = 1;
	}
	rPS0(_inst.FD) = 1.0f / sqrtf(b);	
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

void fmsubx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = (rPS0(_inst.FA) * rPS0(_inst.FC)) - rPS0(_inst.FB);
 	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}

void fmsubsx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS1(_inst.FD) =
		static_cast<float>((rPS0(_inst.FA) * rPS0(_inst.FC)) - rPS0(_inst.FB));
 	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}


void fnmaddx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = -((rPS0(_inst.FA) * rPS0(_inst.FC)) + rPS0(_inst.FB));
 	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}
void fnmaddsx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS1(_inst.FD) = 
		static_cast<float>(-((rPS0(_inst.FA) * rPS0(_inst.FC)) + rPS0(_inst.FB)));
 	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}


void fnmsubx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = -((rPS0(_inst.FA) * rPS0(_inst.FC)) - rPS0(_inst.FB));
 	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}
void fnmsubsx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS1(_inst.FD) = 
		static_cast<float>(-((rPS0(_inst.FA) * rPS0(_inst.FC)) - rPS0(_inst.FB)));
 	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}


void fsubx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS0(_inst.FA) - rPS0(_inst.FB);
 	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}
void fsubsx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS1(_inst.FD) = static_cast<float>(rPS0(_inst.FA) - rPS0(_inst.FB));
 	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

}  // namespace
