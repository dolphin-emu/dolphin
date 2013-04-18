// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _INTERPRETER_FPUTILS_H
#define _INTERPRETER_FPUTILS_H

#include "../../Core.h"
#include "Interpreter.h"
#include "MathUtil.h"

using namespace MathUtil;

// warning! very slow! This setting fixes NAN
//#define VERY_ACCURATE_FP

#define MIN_SINGLE 0xc7efffffe0000000ull
#define MAX_SINGLE 0x47efffffe0000000ull

// FPSCR exception flags
const u32 FPSCR_OX         = (u32)1 << (31 - 3);
const u32 FPSCR_UX         = (u32)1 << (31 - 4);
const u32 FPSCR_ZX         = (u32)1 << (31 - 5);
// ! XX shouldn't be accessed directly to set 1. Use SetFI() instead !
const u32 FPSCR_XX         = (u32)1 << (31 - 6);
const u32 FPSCR_VXSNAN     = (u32)1 << (31 - 7);
const u32 FPSCR_VXISI      = (u32)1 << (31 - 8);
const u32 FPSCR_VXIDI      = (u32)1 << (31 - 9);
const u32 FPSCR_VXZDZ      = (u32)1 << (31 - 10);
const u32 FPSCR_VXIMZ      = (u32)1 << (31 - 11);
const u32 FPSCR_VXVC       = (u32)1 << (31 - 12);
const u32 FPSCR_VXSOFT     = (u32)1 << (31 - 21);
const u32 FPSCR_VXSQRT     = (u32)1 << (31 - 22);
const u32 FPSCR_VXCVI      = (u32)1 << (31 - 23);

const u32 FPSCR_VX_ANY     = FPSCR_VXSNAN | FPSCR_VXISI | FPSCR_VXIDI | FPSCR_VXZDZ |
							 FPSCR_VXIMZ | FPSCR_VXVC | FPSCR_VXSOFT | FPSCR_VXSQRT | FPSCR_VXCVI;

const u32 FPSCR_ANY_X      = FPSCR_OX | FPSCR_UX | FPSCR_ZX | FPSCR_XX | FPSCR_VX_ANY;

const u64 PPC_NAN_U64      = 0x7ff8000000000000ull;
const double PPC_NAN       = *(double* const)&PPC_NAN_U64;

inline bool IsINF(double x)
{
	return ((*(u64*)&x) & ~DOUBLE_SIGN) == DOUBLE_EXP;
}

inline void SetFPException(u32 mask)
{
	if ((FPSCR.Hex & mask) != mask)
		FPSCR.FX = 1;
	FPSCR.Hex |= mask;
}

inline void SetFI(int FI)
{
	if (FI)
	{
		SetFPException(FPSCR_XX);
	}
	FPSCR.FI = FI;
}

inline void UpdateFPSCR()
{
	FPSCR.VX = (FPSCR.Hex & FPSCR_VX_ANY) != 0;
	FPSCR.FEX = 0; // we assume that "?E" bits are always 0
}

inline double ForceSingle(double _x)
{
	//if (FPSCR.RN != 0)
	//	PanicAlert("RN = %d at %x", (int)FPSCR.RN, PC);
	if (FPSCR.NI)
		_x = FlushToZeroAsFloat(_x);

	double x = static_cast<float>(_x);

	return x;
}

inline double ForceDouble(double d)
{
	//if (FPSCR.RN != 0)
	//	PanicAlert("RN = %d at %x", (int)FPSCR.RN, PC);

	//if (FPSCR.NI)
	//{
	//	IntDouble x; x.d = d;
		//if ((x.i & DOUBLE_EXP) == 0)
		//	x.i &= DOUBLE_SIGN;  // turn into signed zero
	//	return x.d;
	//}
	return d;
}

// these functions allow globally modify operations behaviour
// also, these may be used to set flags like FR, FI, OX, UX

inline double NI_mul(const double a, const double b)
{
#ifdef VERY_ACCURATE_FP
	if (a != a) return a;
	if (b != b) return b;
	double t = a * b;
	if (t != t)
	{
		SetFPException(FPSCR_VXIMZ);
		return PPC_NAN;
	}
	return t;
#else
	return a * b;
#endif
}

inline double NI_add(const double a, const double b)
{	
#ifdef VERY_ACCURATE_FP
	if (a != a) return a;
	if (b != b) return b;
	double t = a + b;
	if (t != t)
	{
		SetFPException(FPSCR_VXISI);
		return PPC_NAN;
	}
	return t;
#else
	return a + b;
#endif
}

inline double NI_sub(const double a, const double b)
{	
#ifdef VERY_ACCURATE_FP
	if (a != a) return a;
	if (b != b) return b;
	double t = a - b;
	if (t != t)
	{
		SetFPException(FPSCR_VXISI);
		return PPC_NAN;
	}
	return t;
#else
	return a - b;
#endif
}

inline double NI_madd(const double a, const double b, const double c)
{
#ifdef VERY_ACCURATE_FP
	if (a != a) return a;
	if (c != c) return c;
	if (b != b) return b;
	double t = a * b;
	if (t != t)
	{
		SetFPException(FPSCR_VXIMZ);
		return PPC_NAN;
	}
	t = t + c;
	if (t != t)
	{
		SetFPException(FPSCR_VXISI);
		return PPC_NAN;
	}
	return t;
#else
	return NI_add(NI_mul(a, b), c);
#endif
}

inline double NI_msub(const double a, const double b, const double c)
{
//#ifdef VERY_ACCURATE_FP
//  This code does not produce accurate fp!  NAN's are not calculated correctly, nor negative zero.
//	The code is kept here for reference.
//
//	if (a != a) return a;
//	if (c != c) return c;
//	if (b != b) return b;
//	double t = a * b;
//	if (t != t)
//	{
//		SetFPException(FPSCR_VXIMZ);
//		return PPC_NAN;
//	}
//
//	t = t - c;
//	if (t != t)
//	{
//		SetFPException(FPSCR_VXISI);
//		return PPC_NAN;
//	}
//	return t;
//#else
//	This code does not calculate QNAN's correctly but calculates negative zero correctly.
	return NI_sub(NI_mul(a, b), c);
// #endif
}

// used by stfsXX instructions and ps_rsqrte
inline u32 ConvertToSingle(u64 x)
{
	u32 exp = (x >> 52) & 0x7ff;
	if (exp > 896 || (x & ~DOUBLE_SIGN) == 0)
	{
		return ((x >> 32) & 0xc0000000) | ((x >> 29) & 0x3fffffff);
	}
	else if (exp >= 874)
	{
		u32 t = (u32)(0x80000000 | ((x & DOUBLE_FRAC) >> 21));
		t = t >> (905 - exp);
		t |= (x >> 32) & 0x80000000;
		return t;
	}
	else
	{
		// This is said to be undefined.
		// The code is based on hardware tests.
		return ((x >> 32) & 0xc0000000) | ((x >> 29) & 0x3fffffff);
	}
}

// used by psq_stXX operations. 
inline u32 ConvertToSingleFTZ(u64 x)
{
	u32 exp = (x >> 52) & 0x7ff;
	if (exp > 896 || (x & ~DOUBLE_SIGN) == 0)
	{
		return ((x >> 32) & 0xc0000000) | ((x >> 29) & 0x3fffffff);
	}
	else
	{
		return (x >> 32) & 0x80000000;
	}
}

#endif
