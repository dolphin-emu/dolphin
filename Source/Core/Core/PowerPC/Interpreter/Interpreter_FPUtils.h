// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <limits>

#include "Common/CPUDetect.h"
#include "Common/MathUtil.h"
#include "Core/PowerPC/Interpreter/Interpreter.h"

// warning! very slow! This setting fixes NAN
//#define VERY_ACCURATE_FP

#define MIN_SINGLE 0xc7efffffe0000000ull
#define MAX_SINGLE 0x47efffffe0000000ull

const u64 PPC_NAN_U64      = 0x7ff8000000000000ull;
const double PPC_NAN       = *(double* const)&PPC_NAN_U64;

// the 4 less-significand bits in FPSCR[FPRF]
enum FPCC
{
	FL = 8, // <
	FG = 4, // >
	FE = 2, // =
	FU = 1, // ?
};

inline void SetFPException(u32 mask)
{
	if ((FPSCR.Hex & mask) != mask)
	{
		FPSCR.FX = 1;
	}
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
	// convert to float...
	float x = (float) _x;
	if (!cpu_info.bFlushToZero && FPSCR.NI)
	{
		x = MathUtil::FlushToZero(x);
	}
	// ...and back to double:
	return x;
}

inline double ForceDouble(double d)
{
	if (!cpu_info.bFlushToZero && FPSCR.NI)
	{
		d = MathUtil::FlushToZero(d);
	}
	return d;
}

inline double Force25Bit(double d)
{
	u64 di = *(u64*)&d;
	di = (di & 0xFFFFFFFFF8000000ULL) + (di & 0x8000000);
	return *(double*)&di;
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
	if (exp > 896 || (x & ~MathUtil::DOUBLE_SIGN) == 0)
	{
		return ((x >> 32) & 0xc0000000) | ((x >> 29) & 0x3fffffff);
	}
	else if (exp >= 874)
	{
		u32 t = (u32)(0x80000000 | ((x & MathUtil::DOUBLE_FRAC) >> 21));
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
	if (exp > 896 || (x & ~MathUtil::DOUBLE_SIGN) == 0)
	{
		return ((x >> 32) & 0xc0000000) | ((x >> 29) & 0x3fffffff);
	}
	else
	{
		return (x >> 32) & 0x80000000;
	}
}

inline u64 ConvertToDouble(u32 _x)
{
	// This is a little-endian re-implementation of the algorithm described in
	// the PowerPC Programming Environments Manual for loading single
	// precision floating point numbers.
	// See page 566 of http://www.freescale.com/files/product/doc/MPCFPE32B.pdf

	u64 x = _x;
	u64 exp = (x >> 23) & 0xff;
	u64 frac = x & 0x007fffff;

	if (exp > 0 && exp < 255) // Normal number
	{
		u64 y = !(exp >> 7);
		u64 z = y << 61 | y << 60 | y << 59;
		return ((x & 0xc0000000) << 32) | z | ((x & 0x3fffffff) << 29);
	}
	else if (exp == 0 && frac != 0) // Subnormal number
	{
		exp = 1023 - 126;
		do
		{
			frac <<= 1;
			exp -= 1;
		} while ((frac & 0x00800000) == 0);

		return ((x & 0x80000000) << 32) | (exp << 52) | ((frac & 0x007fffff) << 29);
	}
	else // QNaN, SNaN or Zero
	{
		u64 y = exp >> 7;
		u64 z = y << 61 | y << 60 | y << 59;
		return ((x & 0xc0000000) << 32) | z | ((x & 0x3fffffff) << 29);
	}
}

// Used by fres and ps_res.
inline double ApproximateReciprocal(double val)
{
	static const int expected_base[] = {
		0x7ff800, 0x783800, 0x70ea00, 0x6a0800,
		0x638800, 0x5d6200, 0x579000, 0x520800,
		0x4cc800, 0x47ca00, 0x430800, 0x3e8000,
		0x3a2c00, 0x360800, 0x321400, 0x2e4a00,
		0x2aa800, 0x272c00, 0x23d600, 0x209e00,
		0x1d8800, 0x1a9000, 0x17ae00, 0x14f800,
		0x124400, 0x0fbe00, 0x0d3800, 0x0ade00,
		0x088400, 0x065000, 0x041c00, 0x020c00,
	};
	static const int expected_dec[] = {
		0x3e1, 0x3a7, 0x371, 0x340,
		0x313, 0x2ea, 0x2c4, 0x2a0,
		0x27f, 0x261, 0x245, 0x22a,
		0x212, 0x1fb, 0x1e5, 0x1d1,
		0x1be, 0x1ac, 0x19b, 0x18b,
		0x17c, 0x16e, 0x15b, 0x15b,
		0x143, 0x143, 0x12d, 0x12d,
		0x11a, 0x11a, 0x108, 0x106,
	};

	union
	{
		double valf;
		s64 vali;
	};

	valf = val;
	s64 mantissa = vali & ((1LL << 52) - 1);
	s64 sign = vali & (1ULL << 63);
	s64 exponent = vali & (0x7FFLL << 52);

	// Special case 0
	if (mantissa == 0 && exponent == 0)
		return sign ? -std::numeric_limits<double>::infinity() :
		               std::numeric_limits<double>::infinity();

	// Special case NaN-ish numbers
	if (exponent == (0x7FFLL << 52))
	{
		if (mantissa == 0)
			return sign ? -0.0 : 0.0;
		return 0.0 + valf;
	}

	// Special case small inputs
	if (exponent < (895LL << 52))
		return sign ? -std::numeric_limits<float>::max() :
		               std::numeric_limits<float>::max();

	// Special case large inputs
	if (exponent >= (1149LL << 52))
		return sign ? -0.0f : 0.0f;

	exponent = (0x7FDLL << 52) - exponent;

	int i = (int)(mantissa >> 37);
	vali = sign | exponent;
	vali |= (s64)(expected_base[i / 1024] - (expected_dec[i / 1024] * (i % 1024) + 1) / 2) << 29;
	return valf;
}

