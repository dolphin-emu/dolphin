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
#include <limits>

#ifdef _WIN32
#include <intrin.h>
#else
#include <xmmintrin.h>
#endif

#include "../../Core.h"
#include "Interpreter.h"

// SUPER MONKEY BALL IS BEING A ROYAL PAIN
// We are missing the caller of 800070ec
// POSSIBLE APPROACHES:
// * Full SW FPU. Urgh.
// * Partial SW FPU, emulate just as much as necessary for monkey ball. Feasible but a lot of work.
// * HLE hacking. Figure out what all the evil functions really do and fake them.

// Interesting places in Super Monkey Ball:
// 80036654: fctwixz stuff
// 80007e08:
//	-98: Various entry points that loads various odd fp values into f1
// 800070b0: Estimate inverse square root.
// 800070ec: Examine f1. Reads a value out of locked cache into f2 (fixed address). Some cases causes us to call the above thing.
//           If all goes well, jump to 70b0, which estimates the inverse square root. 
//           Then multiply the loaded variable with the original value of f1. Result should be the square root. (1 / sqrt(x)) * x  = x / sqrt(x) = sqrt(x)
// 8000712c: Similar, but does not do the multiply at the end, just an frspx.
// 8000716c: Sort of similar, but has extra junk at the end.
//
// 
// 800072a4 - nightmare of nightmares
// Fun stuff used:
// bso+
// mcrfs (ARGH pulls stuff out of .. FPSCR). it uses this to check the result of frsp mostly (!!!!)
// crclr
// crset
// crxor
// fnabs
// Super Monkey Ball reads FPRF & friends after fmadds, fmuls, frspx
// WHY do the FR & FI flags affect it so much?

void UpdateFPSCR(UReg_FPSCR fp);
void UpdateSSEState();

void UpdateFPRF(double value)
{
	u64 ivalue = *((u64*)&value);
	// 5 bits (C, <, >, =, ?)
	// top: class descriptor
	FPSCR.FPRF = 4;
	// easy cases first
	if (ivalue == 0) {
		// positive zero
		FPSCR.FPRF = 0x2;
	} else if (ivalue == 0x8000000000000000ULL) {
		// negative zero
		FPSCR.FPRF = 0x12;
	} else if (ivalue == 0x7FF0000000000000ULL) {
		// positive inf
		FPSCR.FPRF = 0x5;
	} else if (ivalue == 0xFFF0000000000000ULL) {
		// negative inf
		FPSCR.FPRF = 0x9;
	} else {
		// OK let's dissect this thing.
		int sign = (int)(ivalue >> 63);
		int exp = (int)((ivalue >> 52) & 0x7FF);
		if (exp >= 1 && exp <= 2046) {
			// Nice normalized number.
			if (sign) {
				FPSCR.FPRF = 0x8; // negative
			} else {
				FPSCR.FPRF = 0x4; // positive
			}
			return;
		}
		u64 mantissa = ivalue & 0x000FFFFFFFFFFFFFULL;
		int mantissa_top = (int)(mantissa >> 51);
		if (exp == 0 && mantissa) {
			// Denormalized number.
			if (sign) {
				FPSCR.FPRF = 0x18;
			} else {
				FPSCR.FPRF = 0x14;
			}
		} else if (exp == 0x7FF && mantissa /* && mantissa_top*/) {
			FPSCR.FPRF = 0x11; // Quiet NAN
			return;
		}
	}
}


// extremely rare
void CInterpreter::Helper_UpdateCR1(double _fValue)
{
	FPSCR.FPRF = 0;
	if (_fValue == 0.0 || _fValue == -0.0)
		FPSCR.FPRF |= 2;
	if (_fValue > 0.0)
		FPSCR.FPRF |= 4;
	if (_fValue < 0.0)
		FPSCR.FPRF |= 8;
	SetCRField(1, (FPSCR.Hex & 0x0000F000) >> 12);

	PanicAlert("CR1");
}

bool CInterpreter::IsNAN(double _dValue) 
{ 
	return _dValue != _dValue; 
}

void CInterpreter::fcmpo(UGeckoInstruction _inst)
{
	double fa =	rPS0(_inst.FA);
	double fb =	rPS0(_inst.FB);
	u32 compareResult;
	if (IsNAN(fa) || IsNAN(fb))  compareResult = 1;
	else if (fa < fb)            compareResult = 8; 
	else if (fa > fb)            compareResult = 4; 
	else                         compareResult = 2;

	FPSCR.FPRF = compareResult;
	SetCRField(_inst.CRFD, compareResult);

/* missing part
	if ((frA) is an SNaN or (frB) is an SNaN )
		then VXSNAN ¬ 1
		if VE = 0
			then VXVC ¬ 1
		else if ((frA) is a QNaN or (frB) is a QNaN )
		then VXVC ¬ 1 */
}

void CInterpreter::fcmpu(UGeckoInstruction _inst)
{
	double fa =	rPS0(_inst.FA);
	double fb =	rPS0(_inst.FB);

	u32 compareResult;
	if (IsNAN(fa) || IsNAN(fb))  compareResult = 1; 
	else if (fa < fb)            compareResult = 8; 
	else if (fa > fb)            compareResult = 4; 
	else                         compareResult = 2;

	FPSCR.FPRF = compareResult;
	SetCRField(_inst.CRFD, compareResult);

/* missing part
	if ((frA) is an SNaN or (frB) is an SNaN)
		then VXSNAN ¬ 1 */
}

// Apply current rounding mode
void CInterpreter::fctiwx(UGeckoInstruction _inst)
{
	UpdateSSEState();
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
		double d_value = (double)value;
		bool inexact = (d_value != b);
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
void CInterpreter::fctiwzx(UGeckoInstruction _inst)
{
	//UpdateFPSCR(FPSCR);
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
		double d_value = (double)value;
		bool inexact = (d_value != b);
//		FPSCR.FI = inexact ? 1 : 0;
//		FPSCR.XX |= FPSCR.FI;
//		FPSCR.FR = 1; //fabs(d_value) > fabs(b);
	}
	//FPRF undefined

	riPS0(_inst.FD) = (u64)value;
	if (_inst.Rc) 
		Helper_UpdateCR1(rPS0(_inst.FD));
}

void CInterpreter::fmrx(UGeckoInstruction _inst)
{
	riPS0(_inst.FD) = riPS0(_inst.FB);
	// This is a binary instruction. Does not alter FPSCR
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}

void CInterpreter::fabsx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = fabs(rPS0(_inst.FB));
	// This is a binary instruction. Does not alter FPSCR
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void CInterpreter::fnabsx(UGeckoInstruction _inst)
{
	riPS0(_inst.FD) = riPS0(_inst.FB) | (1ULL << 63);
	// This is a binary instruction. Does not alter FPSCR
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void CInterpreter::fnegx(UGeckoInstruction _inst)
{
	riPS0(_inst.FD) = riPS0(_inst.FB) ^ (1ULL << 63);
	// This is a binary instruction. Does not alter FPSCR
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void CInterpreter::fselx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = (rPS0(_inst.FA) >= -0.0) ? rPS0(_inst.FC) : rPS0(_inst.FB);
	// This is a binary instruction. Does not alter FPSCR
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}


// !!! warning !!!
// PS1 must be set to the value of PS0 or DragonballZ will be f**ked up
// PS1 is said to be undefined
// Super Monkey Ball is using this to do wacky tricks so we need 100% correct emulation.
void CInterpreter::frspx(UGeckoInstruction _inst)  // round to single
{
	if (true || FPSCR.RN != 0)
	{
		// Not used in Super Monkey Ball
		UpdateSSEState();
		double b = rPS0(_inst.FB);
		double rounded = (double)(float)b;
		FPSCR.FI = b != rounded;  // changing both of these affect Super Monkey Ball behaviour greatly.
		FPSCR.FR = 1;  // WHY? fabs(rounded) > fabs(b);
		rPS0(_inst.FD) = rPS1(_inst.FD) = rounded;
		return;
		// PanicAlert("frspx: FPSCR.RN=%i", FPSCR.RN);
	}

	// OK, let's try it in 100% software! Not yet working right.
	union {
		double d;
		u64 i;
	} in, out;
	in.d = rPS0(_inst.FB);
	out = in;
	int sign = (int)(in.i >> 63);
	int exp = (int)((in.i >> 52) & 0x7FF);
	u64 mantissa = in.i & 0x000FFFFFFFFFFFFFULL;
	u64 mantissa_single = mantissa & 0x000FFFFFE0000000ULL;
	u64 leftover_single = mantissa & 0x000000001FFFFFFFULL;

	// OK. First make sure that we have a "normal" number.
	if (exp >= 1 && exp <= 2046) {
		// OK. Check for overflow. TODO

		FPSCR.FI = leftover_single != 0; // Inexact
		if (leftover_single >= 0x10000000ULL) {
			//PanicAlert("rounding up");
			FPSCR.FR = 1;
			mantissa_single += 0x20000000;
			if (mantissa_single & 0x0010000000000000ULL) {
				// PanicAlert("renormalizing");
				mantissa_single >>= 1;
				exp += 1;
				// if (exp > 2046) { OVERFLOW }
			}
		}
		out.i = ((u64)sign << 63) | ((u64)exp << 52) | mantissa_single;
	} else {
		if (!exp && !mantissa) {
			// Positive or negative Zero. All is well.
			FPSCR.FI = 0;
			FPSCR.FR = 0;
		} else if (exp == 0 && mantissa) {
			// Denormalized number.
			PanicAlert("denorm");
		} else if (exp == 2047 && !mantissa) {
			// Infinite.
			//PanicAlert("infinite");
			FPSCR.FI = 1;
			FPSCR.FR = 1;
//			FPSCR.OX = 1;
		} else {
			//PanicAlert("NAN %08x %08x", in.i >> 32, in.i);
		}
	}
	UpdateFPRF(out.d);
	FPSCR.FR = 1; // SUPER MONKEY BALL HACK
	rPS0(_inst.FD) = rPS1(_inst.FD) = out.d;

	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}


void CInterpreter::fmulx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS0(_inst.FA) * rPS0(_inst.FC);
	FPSCR.FI = 0;
	FPSCR.FR = 1;
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}
void CInterpreter::fmulsx(UGeckoInstruction _inst)
{
	double d_value = rPS0(_inst.FA) * rPS0(_inst.FC);
	rPS0(_inst.FD) = rPS1(_inst.FD) = static_cast<float>(d_value);
	FPSCR.FI = d_value != rPS0(_inst.FD);
	FPSCR.FR = rand()&1;
	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));  
}


void CInterpreter::fmaddx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = (rPS0(_inst.FA) * rPS0(_inst.FC)) + rPS0(_inst.FB);
	FPSCR.FI = 0;
	FPSCR.FR = 0;
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}
void CInterpreter::fmaddsx(UGeckoInstruction _inst)
{
	double d_value = (rPS0(_inst.FA) * rPS0(_inst.FC)) + rPS0(_inst.FB);
	rPS0(_inst.FD) = rPS1(_inst.FD) = 
		static_cast<float>(d_value);
	FPSCR.FI = d_value != rPS0(_inst.FD);
	FPSCR.FR = 0;
	UpdateFPRF(rPS0(_inst.FD));
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}


void CInterpreter::faddx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS0(_inst.FA) + rPS0(_inst.FB);
//	FPSCR.FI = 0;
//	FPSCR.FR = 1;
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}
void CInterpreter::faddsx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS1(_inst.FD) = static_cast<float>(rPS0(_inst.FA) + rPS0(_inst.FB));
//	FPSCR.FI = 0;
//	FPSCR.FR = 1;
//	FPSCR.Hex = (rand() ^ (rand() << 8) ^ (rand() << 16)) & ~(0x000000F8);
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}


void CInterpreter::fdivx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS0(_inst.FA) / rPS0(_inst.FB);
//	FPSCR.FI = 0;
//	FPSCR.FR = 1;
	if (fabs(rPS0(_inst.FB)) == 0.0) {
		FPSCR.ZX = 1;
	}
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}
void CInterpreter::fdivsx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS1(_inst.FD) = static_cast<float>(rPS0(_inst.FA) / rPS0(_inst.FB));
//	FPSCR.FI = 0;
//	FPSCR.FR = 1;
	if (fabs(rPS0(_inst.FB)) == 0.0) {
		FPSCR.ZX = 1;
	}
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));  
}
void CInterpreter::fresx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS1(_inst.FD) = static_cast<float>(1.0f / rPS0(_inst.FB));
//	FPSCR.FI = 0;
//	FPSCR.FR = 1;
	if (fabs(rPS0(_inst.FB)) == 0.0) {
		FPSCR.ZX = 1;
	}
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}


void CInterpreter::fmsubx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = (rPS0(_inst.FA) * rPS0(_inst.FC)) - rPS0(_inst.FB);
//	FPSCR.FI = 0;
//	FPSCR.FR = 0;
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}

void CInterpreter::fmsubsx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS1(_inst.FD) =
		static_cast<float>((rPS0(_inst.FA) * rPS0(_inst.FC)) - rPS0(_inst.FB));
//	FPSCR.FI = 0;
//	FPSCR.FR = 0;
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}


void CInterpreter::fnmaddx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = -((rPS0(_inst.FA) * rPS0(_inst.FC)) + rPS0(_inst.FB));
//	FPSCR.FI = 0;
//	FPSCR.FR = 0;
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}
void CInterpreter::fnmaddsx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS1(_inst.FD) = 
		static_cast<float>(-((rPS0(_inst.FA) * rPS0(_inst.FC)) + rPS0(_inst.FB)));
//	FPSCR.FI = 0;
//	FPSCR.FR = 0;
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}


void CInterpreter::fnmsubx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = -((rPS0(_inst.FA) * rPS0(_inst.FC)) - rPS0(_inst.FB));
//	FPSCR.FI = 0;
//	FPSCR.FR = 0;
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}
void CInterpreter::fnmsubsx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS1(_inst.FD) = 
		static_cast<float>(-((rPS0(_inst.FA) * rPS0(_inst.FC)) - rPS0(_inst.FB)));
//	FPSCR.FI = 0;
//	FPSCR.FR = 0;
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD)); 
}


void CInterpreter::fsubx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS0(_inst.FA) - rPS0(_inst.FB);
//	FPSCR.FI = 0;
//	FPSCR.FR = 0;
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}
void CInterpreter::fsubsx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = rPS1(_inst.FD) = static_cast<float>(rPS0(_inst.FA) - rPS0(_inst.FB));
//	FPSCR.FI = 0;
//	FPSCR.FR = 0;
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}


void CInterpreter::frsqrtex(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = 1.0f / (sqrt(rPS0(_inst.FB)));
//	FPSCR.FI = 0;
//	FPSCR.FR = 0;
	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}

void CInterpreter::fsqrtx(UGeckoInstruction _inst)
{
	rPS0(_inst.FD) = sqrt(rPS0(_inst.FB));
//	FPSCR.FI = 0;
//	FPSCR.FR = 0;

	if (_inst.Rc) Helper_UpdateCR1(rPS0(_inst.FD));
}
