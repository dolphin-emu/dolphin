// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Core/PowerPC/Interpreter/Interpreter_FPUtils.h"

static u64 DoubleToU64(double d)
{
	union
	{
		double dval;
		u64 ival;
	};
	dval = d;
	return ival;
}

static double U64ToDouble(u64 d)
{
	union
	{
		double dval;
		u64 ival;
	};
	ival = d;
	return dval;
}

static double ForceSingle(double _x)
{
	// convert to float...
	float x = (float)_x;
	if (!cpu_info.bFlushToZero && FPSCR.NI)
	{
		x = FlushToZero(x);
	}
	// ...and back to double:
	return x;
}

static double ForceDouble(double d)
{
	if (!cpu_info.bFlushToZero && FPSCR.NI)
	{
		d = FlushToZero(d);
	}
	return d;
}

struct FPTemp
{
	u64 mantissa;
	u64 exponent;
	u64 sign;
};

static FPTemp DecomposeDouble(u64 double_a)
{
	FPTemp result;
	result.mantissa = ((double_a & DOUBLE_FRAC) | (1ULL << 52)) << 3;
	result.exponent = (double_a >> 52) & 0x7ff;
	result.sign = double_a >> 63;
	return result;
}

static u64 RoundFPTempToDouble(FPTemp a)
{
	u64 rounded_mantissa = a.mantissa >> 3;
	return (a.exponent << 52) | (a.sign << 63) | (rounded_mantissa & DOUBLE_FRAC);
}

static u64 RoundFPTempToSingle(FPTemp a)
{
	u64 rounded_mantissa = a.mantissa >> 3;
	return (a.exponent << 52) | (a.sign << 63) | (rounded_mantissa & DOUBLE_FRAC);
}

static u64 MultiplyMantissa(u64 a, u64 b)
{
	// 56 + 56 = 112.  Need 56 bits in top half, so tweak operands.
	a <<= 4;
	b <<= 5;
	u64 high;
	u64 low = _umul128(a, b, &high);
	high |= low != 0 ? 1 : 0;
	return high;
}

static u64 InputIsNan(u64 a)
{
	return ((a >> 52) & 0x7ff) == 0x7ff && (a & DOUBLE_FRAC) != 0;
}

static u64 HandleNaN(u64 a)
{
	return a | (1 << 51);
}

static FPTemp do_mul(u64 double_a, u64 double_b)
{
	//if (low & (1ULL << 57))
	//{
	//	high = (high >> 1) | (high & 1);
	//}
	return FPTemp();
}

static FPTemp AddFPTemp(FPTemp a, FPTemp b)
{
	// TODO: NaN/Infinity
	if (b.exponent > a.exponent ||
		(b.exponent == a.exponent && b.mantissa > a.mantissa))
	{
		std::swap(a, b);
	}

	u64 exponent_diff = a.exponent - b.exponent;
	u64 aligned_b = b.mantissa >> exponent_diff;
	if ((aligned_b << exponent_diff) != b.mantissa)
	{
		aligned_b |= 1;
	}
	if (a.sign != b.sign)
	{
		aligned_b = -aligned_b;
	}
	FPTemp result;
	result.mantissa = a.mantissa + aligned_b;
	result.exponent = a.exponent;
	if (result.mantissa & (1ULL << 56))
	{
		result.mantissa = (result.mantissa >> 1) | (result.mantissa & 1);
		++result.exponent;
	}
	return result;
}


u64 AddSinglePrecision(u64 double_a, u64 double_b)
{
	if (InputIsNan(double_a))
		return HandleNaN(double_a);
	if (InputIsNan(double_b))
		return HandleNaN(double_b);

	FPTemp a = DecomposeDouble(double_a);
	FPTemp b = DecomposeDouble(double_a);
	FPTemp result = AddFPTemp(a, b);
	u64 result_double = RoundFPTempToDouble(result);

	return result_double;
	//return DoubleToU64(ForceSingle(U64ToDouble(a) + U64ToDouble(b)));
}

u64 AddDoublePrecision(u64 double_a, u64 double_b)
{
	if (InputIsNan(double_a))
		return HandleNaN(double_a);
	if (InputIsNan(double_b))
		return HandleNaN(double_b);
	FPTemp a = DecomposeDouble(double_a);
	FPTemp b = DecomposeDouble(double_a);
	FPTemp result = AddFPTemp(a, b);
	u64 result_double = RoundFPTempToSingle(result);

	return result_double;
	//return DoubleToU64(ForceDouble(U64ToDouble(a) + U64ToDouble(b)));
}

u64 SubSinglePrecision(u64 a, u64 b)
{
	SetFI(0);
	FPSCR.FR = 0;
	return DoubleToU64(ForceSingle(U64ToDouble(a) - U64ToDouble(b)));
}

u64 SubDoublePrecision(u64 a, u64 b)
{
	SetFI(0);
	FPSCR.FR = 0;
	return DoubleToU64(ForceDouble(U64ToDouble(a) - U64ToDouble(b)));
}

u64 MultiplySinglePrecision(u64 a, u64 b)
{
	SetFI(0);
	FPSCR.FR = 0;
	return DoubleToU64(ForceSingle(U64ToDouble(a) * U64ToDouble(b)));
}

u64 MultiplyDoublePrecision(u64 a, u64 b)
{
	SetFI(0);
	FPSCR.FR = 0;
	return DoubleToU64(ForceDouble(U64ToDouble(a) * U64ToDouble(b)));
}

u64 MaddSinglePrecision(u64 a, u64 c, u64 b)
{
	SetFI(0);
	FPSCR.FR = 0;
	return DoubleToU64(ForceSingle(U64ToDouble(a) * U64ToDouble(b) + U64ToDouble(c)));
}

u64 MaddDoublePrecision(u64 a, u64 c, u64 b)
{
	SetFI(0);
	FPSCR.FR = 0;
	return DoubleToU64(ForceDouble(U64ToDouble(a) * U64ToDouble(b) + U64ToDouble(c)));
}

u64 MsubSinglePrecision(u64 a, u64 c, u64 b)
{
	SetFI(0);
	FPSCR.FR = 0;
	return DoubleToU64(ForceSingle(U64ToDouble(a) * U64ToDouble(b) - U64ToDouble(c)));
}

u64 MsubDoublePrecision(u64 a, u64 c, u64 b)
{
	SetFI(0);
	FPSCR.FR = 0;
	return DoubleToU64(ForceDouble(U64ToDouble(a) * U64ToDouble(b) - U64ToDouble(c)));
}

u64 NegMaddSinglePrecision(u64 a, u64 c, u64 b)
{
	SetFI(0);
	FPSCR.FR = 0;
	return DoubleToU64(ForceSingle(-(U64ToDouble(a) * U64ToDouble(b) + U64ToDouble(c))));
}

u64 NegMaddDoublePrecision(u64 a, u64 c, u64 b)
{
	SetFI(0);
	FPSCR.FR = 0;
	return DoubleToU64(ForceDouble(-(U64ToDouble(a) * U64ToDouble(b) + U64ToDouble(c))));
}

u64 NegMsubSinglePrecision(u64 a, u64 c, u64 b)
{
	SetFI(0);
	FPSCR.FR = 0;
	return DoubleToU64(ForceSingle(-(U64ToDouble(a) * U64ToDouble(b) - U64ToDouble(c))));
}

u64 NegMsubDoublePrecision(u64 a, u64 c, u64 b)
{
	SetFI(0);
	FPSCR.FR = 0;
	return DoubleToU64(ForceDouble(-(U64ToDouble(a) * U64ToDouble(b) - U64ToDouble(c))));
}

u64 DivSinglePrecision(u64 a, u64 b)
{
	SetFI(0);
	FPSCR.FR = 0;
	return DoubleToU64(ForceSingle(U64ToDouble(a) / U64ToDouble(b)));
}

u64 DivDoublePrecision(u64 a, u64 b)
{
	SetFI(0);
	FPSCR.FR = 0;
	return DoubleToU64(ForceDouble(U64ToDouble(a) / U64ToDouble(b)));
}

u64 RoundToSingle(u64 a)
{
	SetFI(0);
	FPSCR.FR = 0;
	return DoubleToU64(ForceSingle(U64ToDouble(a)));
}