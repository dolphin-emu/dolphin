// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

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
#endif

#include "../../Core.h"
#include "Interpreter.h"
#include "MathUtil.h"
#include "Interpreter_FPUtils.h"
#include "../LUT_frsqrtex.h"

using namespace MathUtil;

void UpdateSSEState();

// Extremely rare - actually, never seen.
// Star Wars : Rogue Leader spams that at some point :|
void Interpreter::Helper_UpdateCR1(double _fValue)
{
	// Should just update exception flags, not do any compares.
	PanicAlert("CR1");
}

void Interpreter::fcmpo(UGeckoInstruction _inst)
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

void Interpreter::fcmpu(UGeckoInstruction _inst)
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
void Interpreter::fctiwx(UGeckoInstruction _inst)
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
		s32 i = 0;
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
void Interpreter::fctiwzx(UGeckoInstruction _inst)
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

void Interpreter::fmrx(UGeckoInstruction _inst)
{
	riPS0(_inst.FD) = riPS0(_inst.FB);
	// This is a binary instruction. Does not alter FPSCR
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}

void Interpreter::fabsx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = fabs(rPS0(_inst.FB));
	// This is a binary instruction. Does not alter FPSCR
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void Interpreter::fnabsx(UGeckoInstruction _inst)
{
	riPS0(_inst.FD) = riPS0(_inst.FB) | (1ULL << 63);
	// This is a binary instruction. Does not alter FPSCR
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));	
}

void Interpreter::fnegx(UGeckoInstruction _inst)
{
	riPS0(_inst.FD) = riPS0(_inst.FB) ^ (1ULL << 63);
	// This is a binary instruction. Does not alter FPSCR
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void Interpreter::fselx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = (rPS0(_inst.FA) >= -0.0) ? rPS0(_inst.FC) : rPS0(_inst.FB);
	// This is a binary instruction. Does not alter FPSCR
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

// !!! warning !!!
// PS1 must be set to the value of PS0 or DragonballZ will be f**ked up
// PS1 is said to be undefined
void Interpreter::frspx(UGeckoInstruction _inst)  // round to single
{
	double b = rPS0(_inst.FB);
	double rounded = ForceSingle(b);
	SetFI(b != rounded);
	FPSCR.FR = fabs(rounded) > fabs(b);
	UpdateFPRF(rounded);
	rPS0(_inst.FD) = rPS1(_inst.FD) = rounded;
	return;
}


void Interpreter::fmulx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = ForceDouble(NI_mul(rPS0(_inst.FA), rPS0(_inst.FC)));
	FPSCR.FI = 0; // are these flags important?
	FPSCR.FR = 0;
	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}
void Interpreter::fmulsx(UGeckoInstruction _inst)
{
	double d_value = NI_mul(rPS0(_inst.FA), rPS0(_inst.FC));
	rPS0(_inst.FD) = rPS1(_inst.FD) = ForceSingle(d_value);
	//FPSCR.FI = d_value != rPS0(_inst.FD);
	FPSCR.FI = 0;
	FPSCR.FR = 0;
	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void Interpreter::fmaddx(UGeckoInstruction _inst)
{
	double result = ForceDouble(NI_madd( rPS0(_inst.FA), rPS0(_inst.FC), rPS0(_inst.FB) ));
	rPS0(_inst.FD) = result;
	UpdateFPRF(result);
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void Interpreter::fmaddsx(UGeckoInstruction _inst)
{
	double d_value = NI_madd( rPS0(_inst.FA), rPS0(_inst.FC), rPS0(_inst.FB) );
	rPS0(_inst.FD) = rPS1(_inst.FD) = ForceSingle(d_value);
	FPSCR.FI = d_value != rPS0(_inst.FD);
	FPSCR.FR = 0;
	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}


void Interpreter::faddx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = ForceDouble(NI_add(rPS0(_inst.FA), rPS0(_inst.FB)));
	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}
void Interpreter::faddsx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS1(_inst.FD) = ForceSingle(NI_add(rPS0(_inst.FA), rPS0(_inst.FB)));
	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}

void Interpreter::fdivx(UGeckoInstruction _inst)
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
void Interpreter::fdivsx(UGeckoInstruction _inst)
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
void Interpreter::fresx(UGeckoInstruction _inst)
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

void Interpreter::frsqrtex(UGeckoInstruction _inst)
{
	double b = rPS0(_inst.FB);
	if (b < 0.0) 
	{
		SetFPException(FPSCR_VXSQRT);
		rPS0(_inst.FD) = PPC_NAN;
	}
	else
	{
#ifdef VERY_ACCURATE_FP
		if (b == 0.0)
		{
			SetFPException(FPSCR_ZX);
			riPS0(_inst.FD) = 0x7ff0000000000000;
		}
		else
		{
			u32 fsa = Common::swap32(Common::swap64(riPS0(_inst.FB)));
			u32 fsb = Common::swap32(Common::swap64(riPS0(_inst.FB)) >> 32);
			u32 idx=(fsa >> 5) % (sizeof(frsqrtex_lut) / sizeof(frsqrtex_lut[0]));

			s32 e = fsa >> (32-12);
			e &= 2047;
			e -= 1023;
			s32 oe =- ((e + 1) / 2);
			oe -= ((e + 1) & 1);

			u32 outb = frsqrtex_lut[idx] << 20;
			u32 outa = ((oe + 1023) & 2047) << 20;
			outa |= frsqrtex_lut[idx] >> 12;
			riPS0(_inst.FD) = ((u64)outa << 32) + (u64)outb;
		}
#else
		if (b == 0.0)
			SetFPException(FPSCR_ZX);
		rPS0(_inst.FD) = ForceDouble(1.0 / sqrt(b));
#endif
	}
	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void Interpreter::fmsubx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = ForceDouble(NI_msub( rPS0(_inst.FA), rPS0(_inst.FC), rPS0(_inst.FB) ));
	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}

void Interpreter::fmsubsx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS1(_inst.FD) =
		ForceSingle( NI_msub(rPS0(_inst.FA), rPS0(_inst.FC), rPS0(_inst.FB) ));
	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}

void Interpreter::fnmaddx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = ForceDouble(-NI_madd(rPS0(_inst.FA), rPS0(_inst.FC), rPS0(_inst.FB)));
	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}
void Interpreter::fnmaddsx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS1(_inst.FD) = 
		ForceSingle(-NI_madd(rPS0(_inst.FA), rPS0(_inst.FC), rPS0(_inst.FB)));
	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}

void Interpreter::fnmsubx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = ForceDouble(-NI_msub(rPS0(_inst.FA), rPS0(_inst.FC), rPS0(_inst.FB)));
	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

// fnmsubsx does not handle QNAN properly - see NI_msub
void Interpreter::fnmsubsx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS1(_inst.FD) =
		ForceSingle(-NI_msub(rPS0(_inst.FA), rPS0(_inst.FC), rPS0(_inst.FB)));
	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}

void Interpreter::fsubx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = ForceDouble(NI_sub(rPS0(_inst.FA), rPS0(_inst.FB)));
	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void Interpreter::fsubsx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS1(_inst.FD) = ForceSingle(NI_sub(rPS0(_inst.FA), rPS0(_inst.FB)));
 	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void Interpreter::fsqrtx(UGeckoInstruction _inst)
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
