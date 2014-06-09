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

static u64 StickyShiftRight(u64 a, u64 shift)
{
	u64 result = a >> shift;
	if ((result << shift) != a)
		result |= 1;
	return result;
}

// 64x64->128 unsigned multiply.
static u64 UMul128(u64 a, u64 b, u64 *high)
{
#if defined _WIN32
	return _umul128(a, b, high);
#else
	// Generic, very inefficient implementation: use four 32x32->64 multiplies,
	// and sum the result.
	u64 low = (a & 0xFFFFFFFF) * (b & 0xFFFFFFFF);
	*high = (a >> 32) * (b >> 32);
	u64 mid1 = (a >> 32) * (b & 0xFFFFFFFF);
	u64 mid2 = (a & 0xFFFFFFFF) * (b >> 32);
	u64 mid1_low = mid1 << 32;
	u64 mid1_high = mid1 >> 32;
	u64 mid2_low = mid2 << 32;
	u64 mid2_high = mid2 >> 32;
	low += mid1_low;
	if (low < mid1_low)
		*high += 1;
	low += mid2_low;
	if (low < mid2_low)
		*high += 1;
	*high += mid1_high + mid2_high;
	return low;
#endif
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
	result.exponent = (double_a >> 52) & 0x7ff;
	if (result.exponent == 0)
	{
		result.mantissa = (double_a & DOUBLE_FRAC) << 3;
		if (result.mantissa != 0)
		{
			while (!(result.mantissa & (1ULL << 55)))
			{
				result.mantissa <<= 1;
				result.exponent -= 1;
			}
			result.exponent += 1;
		}
	}
	else
	{
		result.mantissa = ((double_a & DOUBLE_FRAC) | (1ULL << 52)) << 3;
	}
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
	if (rounded_mantissa & (1ULL << 53))
	{
		_dbg_assert_(POWERPC, rounded_mantissa == (1ULL << 53));
		rounded_mantissa >>= 1;
		exponent += 1;
	}
	_dbg_assert_(POWERPC, rounded_mantissa < (1ULL << 53));
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
	u64 rounded_mantissa = StickyShiftRight(a.mantissa, 29);
	rounded_mantissa = RoundMantissa(rounded_mantissa, a.sign);
	rounded_mantissa <<= 29;
	return ConvertResultToDouble(rounded_mantissa, a.exponent, a.sign);
}

static u64 InputIsNan(u64 a)
{
	return ((a >> 52) & 0x7ff) == 0x7ff && (a & DOUBLE_FRAC) != 0;
}

static u64 HandleNaN(u64 a)
{
	return a | (1ULL << 51);
}

// Core addition routine.  a_low contains the additional temporary bits\
// if this computation is part of a madd.
static void AddCore(FPTemp a, u64 a_low, FPTemp b, FPTemp &result)
{
	// Make sure the larger number is on the LHS; this simplifies aligning
	// the operands.
	u64 b_low = 0;
	if (b.exponent > a.exponent ||
		(b.exponent == a.exponent && b.mantissa > a.mantissa))
	{
		std::swap(a, b);
		std::swap(a_low, b_low);
	}

	// Align the addition.
	int exponent_diff = a.exponent - b.exponent;
	u64 aligned_b = 0;
	u64 aligned_b_low = 0;
	if (exponent_diff == 0)
	{
		aligned_b = b.mantissa;
	}
	else if (exponent_diff < 64)
	{
		aligned_b = b.mantissa >> exponent_diff;
		aligned_b_low = StickyShiftRight(b_low, exponent_diff) | (b.mantissa << (64 - exponent_diff));
	}
	else if (exponent_diff < 128)
	{
		aligned_b_low = StickyShiftRight(b.mantissa, (exponent_diff - 64));
	}

	if (a.sign != b.sign)
	{
		// If the signs are different, negate b.
		// (This is an 128-bit negate expanded over 64-bit operations.)
		aligned_b_low = 0 - aligned_b_low;
		aligned_b = 0 - aligned_b;
		if (aligned_b_low != 0)
		{
			aligned_b -= 1;
		}
	}

	// Copy exponent and sign from LHS.
	result.exponent = a.exponent;
	result.sign = a.sign;

	// Sum the mantissas.
	// (This is an 128-bit addition expanded over 64-bit operations.)
	result.mantissa = a.mantissa;
	u64 result_low = a_low + aligned_b_low;
	if (result_low < aligned_b_low)
	{
		result.mantissa += 1;
	}
	result.mantissa += aligned_b;

	if (result.mantissa == 0 && result_low == 0)
	{
		result.exponent = 0;
	}
	else if (result.mantissa & (1ULL << 56))
	{
		// If the upper part of the result is greater than 56 bits, shift right
		// one bit.  (e.g. 1.5 + 1.5 = 3)
		result.mantissa = (result.mantissa >> 1) | (result.mantissa & 1);
		++result.exponent;
	}
	else if (!(result.mantissa & (1ULL << 55)))
	{
		// If the result is less than 56 bits, shift left until it is.
		// (e.g. 2.0 - 1.5 = 0.5)
		do
		{
			result.mantissa <<= 1;
			result.mantissa |= result_low >> 63;
			result_low <<= 1;
			--result.exponent;
		} while (!(result.mantissa & (1ULL << 55)));
	}

	// OR low bits into the sticky bit.
	if (result_low)
		result.mantissa |= 1;
}

static void MulCore(FPTemp a, FPTemp b, FPTemp &result, u64 &low)
{
	// Need exactly 57 bits of product in "high", so tweak operands.
	// 57 == 53 bits of double mantissa + 3 guard bits +
	//     1 extra in case the top result bit isn't 1.
	// 56 bits of input mantissa * 2 inputs + (4 + 5) extra bits -
	//     64 bits in lower half == 57 bits.
	low = UMul128(a.mantissa << 4, b.mantissa << 5, &result.mantissa);

	// Result exponent is just the sum of the input exponents
	result.exponent = a.exponent + b.exponent - 0x3ff;

	// The upper part of the result is either 56 or 57 bits long; if it's 57 bits,
	// shift right one bit.
	if (result.mantissa & (1ULL << 56))
	{
		low = (low >> 1) | (result.mantissa << 63);
		result.mantissa >>= 1;
		++result.exponent;
	}

	// Result sign is exclusive or of input signs.
	result.sign = a.sign ^ b.sign;
}

static FPTemp MulFPTemp(FPTemp a, FPTemp b)
{
	FPTemp result;
	u64 low;
	MulCore(a, b, result, low);

	// OR the low bits into the sticky bit
	result.mantissa |= (low != 0 ? 1 : 0);

	return result;
}

static FPTemp AddFPTemp(FPTemp a, FPTemp b)
{
	FPTemp result;
	AddCore(a, 0, b, result);
	return result;
}

static FPTemp MaddFPTemp(FPTemp a, FPTemp b, FPTemp c)
{
	FPTemp result;
	u64 result_low;
	MulCore(a, b, result, result_low);
	AddCore(result, result_low, c, result);
	return result;
}

bool InputIsSpecial(u64 a)
{
	return (a & ~DOUBLE_SIGN) == 0 || (a & DOUBLE_EXP) == DOUBLE_EXP;
}

u64 AddSinglePrecision(u64 double_a, u64 double_b)
{
#ifdef VERY_ACCURATE_FP
	if (InputIsNan(double_a))
		return HandleNaN(double_a);
	if (InputIsNan(double_b))
		return HandleNaN(double_b);

	if (InputIsSpecial(double_a) || InputIsSpecial(double_b))
	{
		// TODO: Real impl
		return DoubleToU64(ForceSingle(U64ToDouble(double_a) + U64ToDouble(double_b)));
	}
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

	if (InputIsSpecial(double_a) || InputIsSpecial(double_b))
	{
		// TODO: Real impl
		return DoubleToU64(ForceDouble(U64ToDouble(double_a) + U64ToDouble(double_b)));
	}
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

	if (InputIsSpecial(double_a) || InputIsSpecial(double_b))
	{
		// TODO: Real impl
		return DoubleToU64(ForceSingle(U64ToDouble(double_a) - U64ToDouble(double_b)));
	}
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

	if (InputIsSpecial(double_a) || InputIsSpecial(double_b))
	{
		// TODO: Real impl
		return DoubleToU64(ForceDouble(U64ToDouble(double_a) - U64ToDouble(double_b)));
	}

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

	if (InputIsSpecial(double_a) || InputIsSpecial(double_b))
	{
		// TODO: Real impl
		return DoubleToU64(ForceSingle(U64ToDouble(double_a) * U64ToDouble(double_b)));
	}

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

	if (InputIsSpecial(double_a) || InputIsSpecial(double_b))
	{
		// TODO: Real impl
		return DoubleToU64(ForceDouble(U64ToDouble(double_a) * U64ToDouble(double_b)));
	}

	FPTemp a = DecomposeDouble(double_a);
	FPTemp b = DecomposeDouble(double_b);
	FPTemp result = MulFPTemp(a, b);
	u64 result_double = RoundFPTempToDouble(result);

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

	if (InputIsSpecial(double_a) || InputIsSpecial(double_b) || InputIsSpecial(double_c))
	{
		// TODO: Real impl
		double result = U64ToDouble(double_a) * U64ToDouble(double_b);
		result = result + (negate_c ? -U64ToDouble(double_c) : U64ToDouble(double_c));
		result = negate_result ? -result : result;
		return DoubleToU64(ForceSingle(result));
	}

	FPTemp a = DecomposeDouble(double_a);
	FPTemp b = DecomposeDouble(double_b);
	FPTemp c = DecomposeDouble(double_c);
	if (negate_c)
		c.sign = !c.sign;
	FPTemp result = MaddFPTemp(a, b, c);
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

	if (InputIsSpecial(double_a) || InputIsSpecial(double_b) || InputIsSpecial(double_c))
	{
		// TODO: Real impl
		double result = U64ToDouble(double_a) * U64ToDouble(double_b);
		result = result + (negate_c ? -U64ToDouble(double_c) : U64ToDouble(double_c));
		result = negate_result ? -result : result;
		return DoubleToU64(ForceDouble(result));
	}

	FPTemp a = DecomposeDouble(double_a);
	FPTemp b = DecomposeDouble(double_b);
	FPTemp c = DecomposeDouble(double_c);
	if (negate_c)
		c.sign = !c.sign;
	FPTemp result = MaddFPTemp(a, b, c);
	if (negate_result)
		result.sign = !result.sign;
	u64 result_double = RoundFPTempToDouble(result);

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

	if (InputIsSpecial(double_a))
	{
		return DoubleToU64(ForceSingle(U64ToDouble(double_a)));
	}

	FPTemp a = DecomposeDouble(double_a);
	u64 result_double = RoundFPTempToSingle(a);

	return result_double;
#else
	SetFI(0);
	FPSCR.FR = 0;
	return DoubleToU64(ForceSingle(U64ToDouble(double_a)));
#endif
}