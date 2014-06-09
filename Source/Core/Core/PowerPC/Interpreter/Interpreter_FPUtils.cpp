// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Core/PowerPC/Interpreter/Interpreter_FPUtils.h"

#define VERY_ACCURATE_FP

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
	int exponent;
	bool sign;
};

static FPTemp DecomposeDouble(u64 double_a)
{
	FPTemp result;
	result.mantissa = ((double_a & DOUBLE_FRAC) | (1ULL << 52)) << 3;
	result.exponent = (double_a >> 52) & 0x7ff;
	result.sign = (double_a >> 63) != 0;
	return result;
}

static u64 RoundMantissa(u64 mantissa, bool sign)
{
	u64 rounded_mantissa = mantissa >> 3;
	u64 round_bits = mantissa & 7;
	bool round_up = false;
	switch (FPSCR.RN)
	{
	case 0:
		round_up = round_bits > ((rounded_mantissa & 1) ? 3 : 4);
		break;
	case 1:
		round_up = true;
		break;
	case 2:
		round_up = sign && round_bits > 3;
		break;
	case 3:
		round_up = !sign && round_bits > 3;
		break;
	}
	SetFI(round_bits != 0);
	FPSCR.FR = round_up;
	if (round_up)
		rounded_mantissa += 1;
	return rounded_mantissa;
}

static u64 ConvertResultToDouble(u64 rounded_mantissa, int exponent, bool sign)
{
	if (rounded_mantissa & (1ULL << 56))
	{
		if (rounded_mantissa != (1ULL << 56))
			PanicAlert("Unexpected mantissa");
		rounded_mantissa >>= 1;
		exponent += 1;
	}
	if (rounded_mantissa >= (1ULL << 56))
		PanicAlert("Unexpected mantissa");
	if (exponent >= 0x7FF)
	{
		// Overflow: infinity
		exponent = 0x7FF;
		rounded_mantissa = 0;
	}
	if (exponent <= 0)
	{
		// Underflow: denormal
		if (FPSCR.NI)
		{
			// In non-IEEE mode, flush denormals to zero.
			rounded_mantissa = 0;
		}
		else
		{
			// Build denormal value
			int shift = -exponent + 1;
			if (shift < 64)
				rounded_mantissa >>= shift;
			else
				rounded_mantissa = 0;
		}
		exponent = 0;
	}
	return ((u64)exponent << 52) | ((u64)sign << 63) | (rounded_mantissa & DOUBLE_FRAC);
}

static u64 RoundFPTempToDouble(FPTemp a)
{
	u64 rounded_mantissa = RoundMantissa(a.mantissa, a.sign);
	return ConvertResultToDouble(rounded_mantissa, a.exponent, a.sign);
}

static u64 RoundFPTempToSingle(FPTemp a)
{
	u64 rounded_mantissa = (a.mantissa >> 29) | ((a.mantissa & ((1 << 29) - 1)) != 0);
	rounded_mantissa = RoundMantissa(rounded_mantissa, a.sign);
	rounded_mantissa <<= 29;
	return ConvertResultToDouble(rounded_mantissa, a.exponent, a.sign);
}

static u64 MultiplyMantissa(u64 a, u64 b)
{
	// Need exactly 57 bits of product in "high", so tweak operands.
	// 57 == 53 bits of double mantissa + 3 guard bits +
	//     1 extra in case the top result bit isn't 1.
	// 56 bits of input mantissa * 2 inputs + (4 + 5) extra bits -
	//     64 bits in lower half == 57 bits.
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
	return a | (1ULL << 51);
}

static FPTemp MulFPTemp(FPTemp a, FPTemp b)
{
	FPTemp result;
	result.mantissa = MultiplyMantissa(a.mantissa, b.mantissa);
	result.exponent = a.exponent + b.exponent - 0x3ff;
	if (result.mantissa & (1ULL << 56))
	{
		result.mantissa = (result.mantissa >> 1) | (result.mantissa & 1);
		++result.exponent;
	}
	result.sign = a.sign ^ b.sign;
	return result;
}

static FPTemp AddFPTemp(FPTemp a, FPTemp b)
{
	if (b.exponent > a.exponent ||
		(b.exponent == a.exponent && b.mantissa > a.mantissa))
	{
		std::swap(a, b);
	}

	int exponent_diff = a.exponent - b.exponent;
	u64 aligned_b = b.mantissa >> exponent_diff;
	if ((aligned_b << exponent_diff) != b.mantissa)
	{
		aligned_b |= 1;
	}
	if (a.sign != b.sign)
	{
		aligned_b = 0 - aligned_b;
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
#ifdef VERY_ACCURATE_FP
	if (InputIsNan(double_a))
		return HandleNaN(double_a);
	if (InputIsNan(double_b))
		return HandleNaN(double_b);

	FPTemp a = DecomposeDouble(double_a);
	FPTemp b = DecomposeDouble(double_b);
	FPTemp result = AddFPTemp(a, b);
	u64 result_double = RoundFPTempToSingle(result);

	return result_double;
#else
	SetFI(0);
	FPSCR.FR = 0;
	return DoubleToU64(ForceSingle(U64ToDouble(double_a) + U64ToDouble(double_b)));
#endif
}

u64 AddDoublePrecision(u64 double_a, u64 double_b)
{
#ifdef VERY_ACCURATE_FP
	if (InputIsNan(double_a))
		return HandleNaN(double_a);
	if (InputIsNan(double_b))
		return HandleNaN(double_b);

	FPTemp a = DecomposeDouble(double_a);
	FPTemp b = DecomposeDouble(double_b);
	FPTemp result = AddFPTemp(a, b);
	u64 result_double = RoundFPTempToDouble(result);

	return result_double;
#else
	SetFI(0);
	FPSCR.FR = 0;
	return DoubleToU64(ForceDouble(U64ToDouble(double_a) + U64ToDouble(double_b)));
#endif
}

u64 SubSinglePrecision(u64 double_a, u64 double_b)
{
#ifdef VERY_ACCURATE_FP
	if (InputIsNan(double_a))
		return HandleNaN(double_a);
	if (InputIsNan(double_b))
		return HandleNaN(double_b);

	FPTemp a = DecomposeDouble(double_a);
	FPTemp b = DecomposeDouble(double_b);
	b.sign = !b.sign;
	FPTemp result = AddFPTemp(a, b);
	u64 result_double = RoundFPTempToSingle(result);

	return result_double;
#else
	SetFI(0);
	FPSCR.FR = 0;
	return DoubleToU64(ForceSingle(U64ToDouble(double_a) - U64ToDouble(double_b)));
#endif
}

u64 SubDoublePrecision(u64 double_a, u64 double_b)
{
#ifdef VERY_ACCURATE_FP
	if (InputIsNan(double_a))
		return HandleNaN(double_a);
	if (InputIsNan(double_b))
		return HandleNaN(double_b);

	FPTemp a = DecomposeDouble(double_a);
	FPTemp b = DecomposeDouble(double_b);
	b.sign = !b.sign;
	FPTemp result = AddFPTemp(a, b);
	u64 result_double = RoundFPTempToDouble(result);

	return result_double;
#else
	SetFI(0);
	FPSCR.FR = 0;
	return DoubleToU64(ForceDouble(U64ToDouble(double_a) - U64ToDouble(double_b)));
#endif
}

u64 MultiplySinglePrecision(u64 double_a, u64 double_b)
{
#ifdef VERY_ACCURATE_FP
	if (InputIsNan(double_a))
		return HandleNaN(double_a);
	if (InputIsNan(double_b))
		return HandleNaN(double_b);

	FPTemp a = DecomposeDouble(double_a);
	FPTemp b = DecomposeDouble(double_b);
	FPTemp result = MulFPTemp(a, b);
	u64 result_double = RoundFPTempToSingle(result);

	return result_double;
#else
	SetFI(0);
	FPSCR.FR = 0;
	return DoubleToU64(ForceSingle(U64ToDouble(double_a) * U64ToDouble(double_b)));
#endif
}

u64 MultiplyDoublePrecision(u64 double_a, u64 double_b)
{
#ifdef VERY_ACCURATE_FP
	if (InputIsNan(double_a))
		return HandleNaN(double_a);
	if (InputIsNan(double_b))
		return HandleNaN(double_b);

	FPTemp a = DecomposeDouble(double_a);
	FPTemp b = DecomposeDouble(double_b);
	FPTemp result = MulFPTemp(a, b);
	u64 result_double = RoundFPTempToSingle(result);

	return result_double;
#else
	SetFI(0);
	FPSCR.FR = 0;
	return DoubleToU64(ForceDouble(U64ToDouble(double_a) * U64ToDouble(double_b)));
#endif
}

u64 MaddSinglePrecision(u64 double_a, u64 double_b, u64 double_c, bool negate_c, bool negate_result)
{
#ifdef VERY_ACCURATE_FP
	if (InputIsNan(double_a))
		return HandleNaN(double_a);
	if (InputIsNan(double_c))
		return HandleNaN(double_c);
	if (InputIsNan(double_b))
		return HandleNaN(double_b);

	FPTemp a = DecomposeDouble(double_a);
	FPTemp b = DecomposeDouble(double_b);
	FPTemp c = DecomposeDouble(double_c);
	if (negate_c)
		c.sign = !c.sign;
	FPTemp result = MulFPTemp(a, b);
	result = AddFPTemp(result, c);
	if (negate_result)
		result.sign = !result.sign;
	u64 result_double = RoundFPTempToSingle(result);

	return result_double;
#else
	SetFI(0);
	FPSCR.FR = 0;
	double result = U64ToDouble(double_a) * U64ToDouble(double_b);
	result = result + (negate_c ? -U64ToDouble(double_c) : U64ToDouble(double_c));
	result = negate_result ? -result : result;
	return DoubleToU64(ForceSingle(result));
#endif
}

u64 MaddDoublePrecision(u64 double_a, u64 double_b, u64 double_c, bool negate_c, bool negate_result)
{
#ifdef VERY_ACCURATE_FP
	if (InputIsNan(double_a))
		return HandleNaN(double_a);
	if (InputIsNan(double_c))
		return HandleNaN(double_c);
	if (InputIsNan(double_b))
		return HandleNaN(double_b);

	FPTemp a = DecomposeDouble(double_a);
	FPTemp b = DecomposeDouble(double_b);
	FPTemp c = DecomposeDouble(double_c);
	if (negate_c)
		c.sign = !c.sign;
	FPTemp result = MulFPTemp(a, b);
	result = AddFPTemp(result, c);
	if (negate_result)
		result.sign = !result.sign;
	u64 result_double = RoundFPTempToSingle(result);

	return result_double;
#else
	SetFI(0);
	FPSCR.FR = 0;
	double result = U64ToDouble(double_a) * U64ToDouble(double_b);
	result = result + (negate_c ? -U64ToDouble(double_c) : U64ToDouble(double_c));
	result = negate_result ? -result : result;
	return DoubleToU64(ForceDouble(result));
#endif
}

u64 DivSinglePrecision(u64 double_a, u64 double_b)
{
	if (InputIsNan(double_a))
		return HandleNaN(double_a);
	if (InputIsNan(double_b))
		return HandleNaN(double_b);

	SetFI(0);
	FPSCR.FR = 0;
	return DoubleToU64(ForceSingle(U64ToDouble(double_a) / U64ToDouble(double_b)));
}

u64 DivDoublePrecision(u64 double_a, u64 double_b)
{
	if (InputIsNan(double_a))
		return HandleNaN(double_a);
	if (InputIsNan(double_b))
		return HandleNaN(double_b);

	SetFI(0);
	FPSCR.FR = 0;
	return DoubleToU64(ForceDouble(U64ToDouble(double_a) / U64ToDouble(double_b)));
}

u64 RoundToSingle(u64 double_a)
{
#ifdef VERY_ACCURATE_FP
	if (InputIsNan(double_a))
		return HandleNaN(double_a);

	FPTemp a = DecomposeDouble(double_a);
	u64 result_double = RoundFPTempToSingle(a);

	return result_double;
#else
	SetFI(0);
	FPSCR.FR = 0;
	return DoubleToU64(ForceSingle(U64ToDouble(double_a)));
#endif
}