// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cmath>
#include <limits>

#ifdef _WIN32
#define interlockedbittestandset workaround_ms_header_bug_platform_sdk6_set
#define interlockedbittestandreset workaround_ms_header_bug_platform_sdk6_reset
#define interlockedbittestandset64 workaround_ms_header_bug_platform_sdk6_set64
#define interlockedbittestandreset64 workaround_ms_header_bug_platform_sdk6_reset64
#include <intrin.h>
#undef interlockedbittestandset
#undef interlockedbittestandreset
#undef interlockedbittestandset64
#undef interlockedbittestandreset64
#endif

#include "Common/MathUtil.h"
#include "Core/PowerPC/LUT_frsqrtex.h"
#include "Core/PowerPC/Interpreter/Interpreter.h"
#include "Core/PowerPC/Interpreter/Interpreter_FPUtils.h"

using namespace MathUtil;

void UpdateSSEState();

// Extremely rare - actually, never seen.
// Star Wars : Rogue Leader spams that at some point :|
void Interpreter::Helper_UpdateCR1(double fValue)
{
	SetCRField(1, (FPSCR.FX << 4) | (FPSCR.FEX << 3) | (FPSCR.VX << 2) | FPSCR.OX);
}

void Interpreter::Helper_FloatCompareOrdered(UGeckoInstruction inst, double fa, double fb)
{
	int compareResult;

	if (fa < fb)
	{
		compareResult = FPCC::FL;
	}
	else if (fa > fb)
	{
		compareResult = FPCC::FG;
	}
	else if (fa == fb)
	{
		compareResult = FPCC::FE;
	}
	else // NaN
	{
		FPSCR.FX = 1;
		compareResult = FPCC::FU;
		if (IsSNAN(fa) || IsSNAN(fb))
		{
			SetFPException(FPSCR_VXSNAN);
			if (FPSCR.VE == 0)
			{
				SetFPException(FPSCR_VXVC);
			}
		}
		else // QNaN
		{
			SetFPException(FPSCR_VXVC);
		}
	}

	FPSCR.FPRF = compareResult;
	SetCRField(inst.CRFD, compareResult);
}

void Interpreter::Helper_FloatCompareUnordered(UGeckoInstruction inst, double fa, double fb)
{
	int compareResult;

	if (fa < fb)
	{
		compareResult = FPCC::FL;
	}
	else if (fa > fb)
	{
		compareResult = FPCC::FG;
	}
	else if (fa == fb)
	{
		compareResult = FPCC::FE;
	}
	else
	{
		compareResult = FPCC::FU;
		if (IsSNAN(fa) || IsSNAN(fb))
		{
			FPSCR.FX = 1;
			SetFPException(FPSCR_VXSNAN);
		}
	}
	FPSCR.FPRF = compareResult;
	SetCRField(inst.CRFD, compareResult);
}

void Interpreter::fcmpo(UGeckoInstruction inst)
{
	Helper_FloatCompareOrdered(inst, rPS0(inst.FA), rPS0(inst.FB));
}

void Interpreter::fcmpu(UGeckoInstruction inst)
{
	Helper_FloatCompareUnordered(inst, rPS0(inst.FA), rPS0(inst.FB));
}

// Apply current rounding mode
void Interpreter::fctiwx(UGeckoInstruction inst)
{
	const double b = rPS0(inst.FB);
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
	riPS0(inst.FD) = 0xfff8000000000000ull | value;
	if (value == 0 && ( (*(u64*)&b) & DOUBLE_SIGN ))
		riPS0(inst.FD) |= 0x100000000ull;
	if (inst.Rc)
		Helper_UpdateCR1(rPS0(inst.FD));
}

// Always round toward zero
void Interpreter::fctiwzx(UGeckoInstruction inst)
{
	const double b = rPS0(inst.FB);
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
	riPS0(inst.FD) = 0xfff8000000000000ull | value;
	if (value == 0 && ( (*(u64*)&b) & DOUBLE_SIGN ))
		riPS0(inst.FD) |= 0x100000000ull;
	if (inst.Rc)
		Helper_UpdateCR1(rPS0(inst.FD));
}

void Interpreter::fmrx(UGeckoInstruction inst)
{
	riPS0(inst.FD) = riPS0(inst.FB);
	// This is a binary instruction. Does not alter FPSCR
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
}

void Interpreter::fabsx(UGeckoInstruction inst)
{
	rPS0(inst.FD) = fabs(rPS0(inst.FB));
	// This is a binary instruction. Does not alter FPSCR
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
}

void Interpreter::fnabsx(UGeckoInstruction inst)
{
	riPS0(inst.FD) = riPS0(inst.FB) | (1ULL << 63);
	// This is a binary instruction. Does not alter FPSCR
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
}

void Interpreter::fnegx(UGeckoInstruction inst)
{
	riPS0(inst.FD) = riPS0(inst.FB) ^ (1ULL << 63);
	// This is a binary instruction. Does not alter FPSCR
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
}

void Interpreter::fselx(UGeckoInstruction inst)
{
	rPS0(inst.FD) = (rPS0(inst.FA) >= -0.0) ? rPS0(inst.FC) : rPS0(inst.FB);
	// This is a binary instruction. Does not alter FPSCR
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
}

// !!! warning !!!
// PS1 must be set to the value of PS0 or DragonballZ will be f**ked up
// PS1 is said to be undefined
void Interpreter::frspx(UGeckoInstruction inst)  // round to single
{
	double b = rPS0(inst.FB);
	double rounded = ForceSingle(b);
	SetFI(b != rounded);
	FPSCR.FR = fabs(rounded) > fabs(b);
	UpdateFPRF(rounded);
	rPS0(inst.FD) = rPS1(inst.FD) = rounded;
	return;
}


void Interpreter::fmulx(UGeckoInstruction inst)
{
	rPS0(inst.FD) = ForceDouble(NI_mul(rPS0(inst.FA), rPS0(inst.FC)));
	FPSCR.FI = 0; // are these flags important?
	FPSCR.FR = 0;
	UpdateFPRF(rPS0(inst.FD));
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
}
void Interpreter::fmulsx(UGeckoInstruction inst)
{
	double d_value = NI_mul(rPS0(inst.FA), rPS0(inst.FC));
	rPS0(inst.FD) = rPS1(inst.FD) = ForceSingle(d_value);
	//FPSCR.FI = d_value != rPS0(inst.FD);
	FPSCR.FI = 0;
	FPSCR.FR = 0;
	UpdateFPRF(rPS0(inst.FD));
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
}

void Interpreter::fmaddx(UGeckoInstruction inst)
{
	double result = ForceDouble(NI_madd( rPS0(inst.FA), rPS0(inst.FC), rPS0(inst.FB) ));
	rPS0(inst.FD) = result;
	UpdateFPRF(result);
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
}

void Interpreter::fmaddsx(UGeckoInstruction inst)
{
	double d_value = NI_madd( rPS0(inst.FA), rPS0(inst.FC), rPS0(inst.FB) );
	rPS0(inst.FD) = rPS1(inst.FD) = ForceSingle(d_value);
	FPSCR.FI = d_value != rPS0(inst.FD);
	FPSCR.FR = 0;
	UpdateFPRF(rPS0(inst.FD));
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
}


void Interpreter::faddx(UGeckoInstruction inst)
{
	rPS0(inst.FD) = ForceDouble(NI_add(rPS0(inst.FA), rPS0(inst.FB)));
	UpdateFPRF(rPS0(inst.FD));
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
}
void Interpreter::faddsx(UGeckoInstruction inst)
{
	rPS0(inst.FD) = rPS1(inst.FD) = ForceSingle(NI_add(rPS0(inst.FA), rPS0(inst.FB)));
	UpdateFPRF(rPS0(inst.FD));
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
}

void Interpreter::fdivx(UGeckoInstruction inst)
{
	double a = rPS0(inst.FA);
	double b = rPS0(inst.FB);
	if (a != a) rPS0(inst.FD) = a;
	else if (b != b) rPS0(inst.FD) = b;
	else
	{
		rPS0(inst.FD) = ForceDouble(a / b);
		if (b == 0.0)
		{
			if (a == 0.0)
			{
				SetFPException(FPSCR_VXZDZ);
				rPS0(inst.FD) = PPC_NAN;
			}
			SetFPException(FPSCR_ZX);
		}
		else
		{
			if (IsINF(a) && IsINF(b))
			{
				SetFPException(FPSCR_VXIDI);
				rPS0(inst.FD) = PPC_NAN;
			}
		}
	}
	UpdateFPRF(rPS0(inst.FD));
	// FR,FI,OX,UX???
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
}
void Interpreter::fdivsx(UGeckoInstruction inst)
{
	double a = rPS0(inst.FA);
	double b = rPS0(inst.FB);
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
	rPS0(inst.FD) = rPS1(inst.FD) = res;
	UpdateFPRF(rPS0(inst.FD));
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
}

// Single precision only.
void Interpreter::fresx(UGeckoInstruction inst)
{
	double b = rPS0(inst.FB);
	double one_over = ForceSingle(1.0 / b);
	// this is based on the real hardware tests
	if (b != 0.0 && IsINF(one_over))
	{
		if (one_over > 0)
			riPS0(inst.FD) = riPS1(inst.FD) = MAX_SINGLE;
		else
			riPS0(inst.FD) = riPS1(inst.FD) = MIN_SINGLE;
	}
	else
	{
		rPS0(inst.FD) = rPS1(inst.FD) = one_over;
	}
	if (b == 0.0)
	{
		SetFPException(FPSCR_ZX);
	}
	UpdateFPRF(rPS0(inst.FD));
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
}

void Interpreter::frsqrtex(UGeckoInstruction inst)
{
	double b = rPS0(inst.FB);
	if (b < 0.0)
	{
		SetFPException(FPSCR_VXSQRT);
		rPS0(inst.FD) = PPC_NAN;
	}
	else
	{
#ifdef VERY_ACCURATE_FP
		if (b == 0.0)
		{
			SetFPException(FPSCR_ZX);
			riPS0(inst.FD) = 0x7ff0000000000000;
		}
		else
		{
			u32 fsa = Common::swap32(Common::swap64(riPS0(inst.FB)));
			u32 fsb = Common::swap32(Common::swap64(riPS0(inst.FB)) >> 32);
			u32 idx=(fsa >> 5) % (sizeof(frsqrtex_lut) / sizeof(frsqrtex_lut[0]));

			s32 e = fsa >> (32-12);
			e &= 2047;
			e -= 1023;
			s32 oe =- ((e + 1) / 2);
			oe -= ((e + 1) & 1);

			u32 outb = frsqrtex_lut[idx] << 20;
			u32 outa = ((oe + 1023) & 2047) << 20;
			outa |= frsqrtex_lut[idx] >> 12;
			riPS0(inst.FD) = ((u64)outa << 32) + (u64)outb;
		}
#else
		if (b == 0.0)
			SetFPException(FPSCR_ZX);
		rPS0(inst.FD) = ForceDouble(1.0 / sqrt(b));
#endif
	}
	UpdateFPRF(rPS0(inst.FD));
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
}

void Interpreter::fmsubx(UGeckoInstruction inst)
{
	rPS0(inst.FD) = ForceDouble(NI_msub( rPS0(inst.FA), rPS0(inst.FC), rPS0(inst.FB) ));
	UpdateFPRF(rPS0(inst.FD));
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
}

void Interpreter::fmsubsx(UGeckoInstruction inst)
{
	rPS0(inst.FD) = rPS1(inst.FD) =
		ForceSingle( NI_msub(rPS0(inst.FA), rPS0(inst.FC), rPS0(inst.FB) ));
	UpdateFPRF(rPS0(inst.FD));
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
}

void Interpreter::fnmaddx(UGeckoInstruction inst)
{
	rPS0(inst.FD) = ForceDouble(-NI_madd(rPS0(inst.FA), rPS0(inst.FC), rPS0(inst.FB)));
	UpdateFPRF(rPS0(inst.FD));
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
}
void Interpreter::fnmaddsx(UGeckoInstruction inst)
{
	rPS0(inst.FD) = rPS1(inst.FD) =
		ForceSingle(-NI_madd(rPS0(inst.FA), rPS0(inst.FC), rPS0(inst.FB)));
	UpdateFPRF(rPS0(inst.FD));
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
}

void Interpreter::fnmsubx(UGeckoInstruction inst)
{
	rPS0(inst.FD) = ForceDouble(-NI_msub(rPS0(inst.FA), rPS0(inst.FC), rPS0(inst.FB)));
	UpdateFPRF(rPS0(inst.FD));
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
}

// fnmsubsx does not handle QNAN properly - see NI_msub
void Interpreter::fnmsubsx(UGeckoInstruction inst)
{
	rPS0(inst.FD) = rPS1(inst.FD) =
		ForceSingle(-NI_msub(rPS0(inst.FA), rPS0(inst.FC), rPS0(inst.FB)));
	UpdateFPRF(rPS0(inst.FD));
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
}

void Interpreter::fsubx(UGeckoInstruction inst)
{
	rPS0(inst.FD) = ForceDouble(NI_sub(rPS0(inst.FA), rPS0(inst.FB)));
	UpdateFPRF(rPS0(inst.FD));
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
}

void Interpreter::fsubsx(UGeckoInstruction inst)
{
	rPS0(inst.FD) = rPS1(inst.FD) = ForceSingle(NI_sub(rPS0(inst.FA), rPS0(inst.FB)));
	UpdateFPRF(rPS0(inst.FD));
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
}

void Interpreter::fsqrtx(UGeckoInstruction inst)
{
	// GEKKO is not supposed to support this instruction.
	// PanicAlert("fsqrtx");
	double b = rPS0(inst.FB);
	if (b < 0.0) {
		FPSCR.VXSQRT = 1;
	}
	rPS0(inst.FD) = sqrt(b);
	UpdateFPRF(rPS0(inst.FD));
	if (inst.Rc) Helper_UpdateCR1(rPS0(inst.FD));
}
