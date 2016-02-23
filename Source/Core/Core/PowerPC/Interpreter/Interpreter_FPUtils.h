// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cmath>
#include <limits>

#include "Common/CommonTypes.h"
#include "Common/CPUDetect.h"
#include "Common/MathUtil.h"
#include "Core/PowerPC/Gekko.h"
#include "Core/PowerPC/PowerPC.h"

const u64 PPC_NAN_U64 = 0x7ff8000000000000ull;
const double PPC_NAN  = *(double* const)&PPC_NAN_U64;

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
	MathUtil::IntDouble x(d);
	x.i = (x.i & 0xFFFFFFFFF8000000ULL) + (x.i & 0x8000000);
	return x.d;
}

inline double MakeQuiet(double d)
{
	MathUtil::IntDouble x(d);
	x.i |= MathUtil::DOUBLE_QBIT;
	return x.d;
}

// these functions allow globally modify operations behaviour
// also, these may be used to set flags like FR, FI, OX, UX

inline double NI_mul(double a, double b)
{
	double t = a * b;
	if (std::isnan(t))
	{
		if (std::isnan(a)) return MakeQuiet(a);
		if (std::isnan(b)) return MakeQuiet(b);
		SetFPException(FPSCR_VXIMZ);
		return PPC_NAN;
	}
	return t;
}

inline double NI_div(double a, double b)
{
	double t = a / b;
	if (std::isnan(t))
	{
		if (std::isnan(a)) return MakeQuiet(a);
		if (std::isnan(b)) return MakeQuiet(b);
		if (b == 0.0)
		{
			SetFPException(FPSCR_ZX);
			if (a == 0.0)
				SetFPException(FPSCR_VXZDZ);
		}
		else if (std::isinf(a) && std::isinf(b))
		{
			SetFPException(FPSCR_VXIDI);
		}
		return PPC_NAN;
	}
	return t;
}

inline double NI_add(double a, double b)
{
	double t = a + b;
	if (std::isnan(t))
	{
		if (std::isnan(a)) return MakeQuiet(a);
		if (std::isnan(b)) return MakeQuiet(b);
		SetFPException(FPSCR_VXISI);
		return PPC_NAN;
	}
	return t;
}

inline double NI_sub(double a, double b)
{
	double t = a - b;
	if (std::isnan(t))
	{
		if (std::isnan(a)) return MakeQuiet(a);
		if (std::isnan(b)) return MakeQuiet(b);
		SetFPException(FPSCR_VXISI);
		return PPC_NAN;
	}
	return t;
}

// FMA instructions on PowerPC are weird:
// They calculate (a * c) + b, but the order in which
// inputs are checked for NaN is still a, b, c.
inline double NI_madd(double a, double c, double b)
{
	double t = a * c;
	if (std::isnan(t))
	{
		if (std::isnan(a)) return MakeQuiet(a);
		if (std::isnan(b)) return MakeQuiet(b); // !
		if (std::isnan(c)) return MakeQuiet(c);
		SetFPException(FPSCR_VXIMZ);
		return PPC_NAN;
	}
	t += b;
	if (std::isnan(t))
	{
		if (std::isnan(b)) return MakeQuiet(b);
		SetFPException(FPSCR_VXISI);
		return PPC_NAN;
	}
	return t;
}

inline double NI_msub(double a, double c, double b)
{
	double t = a * c;
	if (std::isnan(t))
	{
		if (std::isnan(a)) return MakeQuiet(a);
		if (std::isnan(b)) return MakeQuiet(b); // !
		if (std::isnan(c)) return MakeQuiet(c);
		SetFPException(FPSCR_VXIMZ);
		return PPC_NAN;
	}

	t -= b;
	if (std::isnan(t))
	{
		if (std::isnan(b)) return MakeQuiet(b);
		SetFPException(FPSCR_VXISI);
		return PPC_NAN;
	}
	return t;
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

