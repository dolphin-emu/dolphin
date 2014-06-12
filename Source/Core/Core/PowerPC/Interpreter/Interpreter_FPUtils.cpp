// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Core/PowerPC/Interpreter/Interpreter_FPUtils.h"

#define VERY_ACCURATE_FP

#ifndef VERY_ACCURATE_FP
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
#endif

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
			int shift = 55 - Log2(result.mantissa);
			result.mantissa <<= shift;
			result.exponent -= shift - 1;
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
		round_up = false;
		break;
	case 2:
		round_up = !sign && round_bits;
		break;
	case 3:
		round_up = sign && round_bits;
		break;
	}
	SetFI(round_bits != 0);
	FPSCR.FR = round_up;
	if (round_up)
		rounded_mantissa += 1;
	return rounded_mantissa;
}

static u64 RoundFPTemp(FPTemp a, bool single)
{
	// IF there is no mantissa, just return early; the result is exactly zero.
	if (!a.mantissa)
		return (u64)a.sign << 63;

	int denormal_exponent = single ? 0x380 : 0;
	if (FPSCR.NI && a.exponent <= denormal_exponent)
	{
		// In non-IEEE mode, flush denormals to zero.
		SetFI(1);
		SetFPException(FPSCR_UX);
		return (u64)a.sign << 63;
	}

	int shift = 0;
	if (a.exponent <= denormal_exponent)
	{
		// Build denormal value
		shift = denormal_exponent - a.exponent + 1;
		if (shift < 64)
			a.mantissa = StickyShiftRight(a.mantissa, shift);
		else
			a.mantissa = 1;

		// Zero out the exponent if the result representation is denormal.
		if (!single)
			a.exponent = 0;
	}
	// Round singles to single precision.
	if (single)
		a.mantissa = StickyShiftRight(a.mantissa, 29);

	a.mantissa = RoundMantissa(a.mantissa, a.sign);

	// Underflow occurs when the result is denormal and inexact.
	// This is true even if the rounded result is not denormal.
	if (a.exponent <= denormal_exponent && FPSCR.FI)
		SetFPException(FPSCR_UX);

	// If we round to zero, output zero.
	if (a.mantissa == 0)
		a.exponent = 0;

	// Renormalize singles.
	if (single && 29 + shift < 64)
		a.mantissa <<= 29 + shift;

	// Renormalize after rounding
	if (a.mantissa == (1ULL << 53))
	{
		a.mantissa = 1ULL << 52;
		a.exponent += 1;
	}

	_dbg_assert_(POWERPC, a.mantissa < (1ULL << 53));
	_dbg_assert_(POWERPC, a.exponent == 0 || (a.mantissa & (1ULL << 52)));

	int max_exponent = single ? 0x47F : 0x7FF;
	if (a.exponent >= max_exponent)
	{
		// Overflow: infinity
		SetFPException(FPSCR_OX);
		SetFI(1);
		a.exponent = 0x7FF;
		a.mantissa = 0;
	}

	return ((u64)a.exponent << 52) | ((u64)a.sign << 63) | (a.mantissa & DOUBLE_FRAC);
}

static u64 RoundFPTempToDouble(FPTemp a)
{
	return RoundFPTemp(a, false);
}

static u64 RoundFPTempToSingle(FPTemp a)
{
	return RoundFPTemp(a, true);
}

static u64 InputIsNan(u64 a)
{
	return (a & DOUBLE_EXP) == DOUBLE_EXP && (a & DOUBLE_FRAC) != 0;
}

static u64 InputIsSNan(u64 a)
{
	return (a & DOUBLE_EXP) == DOUBLE_EXP && (a & DOUBLE_FRAC) != 0 && !(a & (1ULL << 51));
}

// Add two inputs.  a_low contains the additional temporary bits
// if this computation is part of a madd. Each input is either a
// normalized finite number or zero.
static void AddCore(FPTemp a, u64 a_low, FPTemp b, FPTemp &result)
{
	// Make sure the larger number is on the LHS; this simplifies aligning
	// the operands.
	// Note that the exponent doesn't mean anything if the mantissa is zero.
	u64 b_low = 0;
	if ((b.exponent > a.exponent && b.mantissa != 0) ||
		(b.exponent == a.exponent && b.mantissa > a.mantissa) ||
		(a.mantissa == 0 && b.mantissa != 0))
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
	else if (b.mantissa || b_low)
	{
		aligned_b_low = 1;
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
		// Result is zero.
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
		// If the result is less than 56 bits, shift left until it is 56 bits.
		// (e.g. 2.0 - 1.5 = 0.5)
		if (!result.mantissa)
		{
			result.mantissa = result_low >> (64 - 54);
			result_low <<= 54;
			result.exponent -= 54;
		}
		int shift = 55 - Log2(result.mantissa);
		result.mantissa = (result.mantissa << shift) | (result_low >> (64 - shift));
		result_low <<= shift;
		result.exponent -= shift;
	}

	// OR low bits into the sticky bit.
	if (result_low)
		result.mantissa |= 1;

	if (result.mantissa == 0)
		result.sign = FPSCR.RN == 3;
}

// Multiply two inputs.  Each input is either a normalized finite number
// or zero. The output low contains addtional bits from the result mantissa
// that don't fit into a 56-bit integer (necessary for madd).
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

	// Result sign is XOR of input signs.
	result.sign = a.sign ^ b.sign;
}

static FPTemp DivFPTemp(FPTemp a, FPTemp b)
{
	// Long division, one bit at a time, using 128-bit math.
	// TODO: Newton-Raphson or something like that would probably be
	// faster.
	u64 d_high = b.mantissa, d_low = 0;
	u64 r_high = a.mantissa, r_low = 0;
	u64 q = 0;
	for (unsigned i = 0; i < 57; ++i)
	{
		q <<= 1;
		if (r_high > d_high || (r_high == d_high && r_low >= d_low))
		{
			q |= 1;
			if (r_low < d_low)
				r_high -= 1;
			r_low -= d_low;
			r_high -= d_high;
		}
		d_low = (d_low >> 1) | (d_high << 63);
		d_high >>= 1;
	}
	if (r_high != 0 || r_low != 0)
		q |= 1;

	FPTemp result;
	result.mantissa = q;
	// The exponent of the result is the difference between the exponents.
	result.exponent = a.exponent - b.exponent + 0x3ff - 1;
	// The sign of the result is the XOR of the signs.
	result.sign = a.sign ^ b.sign;
	// The division can come up with either a 56-bit or 57-bit result;
	// normalize to 56.
	if (result.mantissa & (1ULL << 56))
	{
		result.mantissa = StickyShiftRight(result.mantissa, 1);
		++result.exponent;
	}
	return result;
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

enum PreprocessOperation
{
	PreprocessAddition,
	PreprocessSubtraction,
	PreprocessMultiplication,
	PreprocessDivision,
	PreprocessMadd,
	PreprocessMsub,
	PreprocessNMadd,
	PreprocessNMsub,
	PreprocessRoundToSingle,
};

static u64 HandleNaN(u64 a)
{
	return a | (1ULL << 51);
}

static bool PreprocessAdd(u64 &double_a, u64 &double_b, u64 &early_result, bool subtraction)
{
	if (InputIsNan(double_a) || InputIsNan(double_b))
	{
		if (InputIsSNan(double_a) || InputIsSNan(double_b))
			SetFPException(FPSCR_VXSNAN);
		early_result = HandleNaN(InputIsNan(double_a) ? double_a : double_b);
		return true;
	}

	if (subtraction)
		double_b ^= DOUBLE_SIGN;

	if ((double_a & DOUBLE_EXP) == DOUBLE_EXP)
	{
		if ((double_b & DOUBLE_EXP) == DOUBLE_EXP && double_a != double_b)
		{
			// infinity + -infinity = NaN
			early_result = PPC_NAN_U64;
			SetFPException(FPSCR_VXISI);
			return true;
		}
		// infinity + n = infinity
		early_result = double_a;
		return true;
	}
	if ((double_b & DOUBLE_EXP) == DOUBLE_EXP)
	{
		// infinity + n = infinity
		early_result = double_b;
		return true;
	}
	return false;
}

static bool PreprocessMultiply(u64 &double_a, u64 &double_b, u64 &early_result)
{
	if (InputIsNan(double_a) || InputIsNan(double_b))
	{
		if (InputIsSNan(double_a) || InputIsSNan(double_b))
			SetFPException(FPSCR_VXSNAN);
		early_result = HandleNaN(InputIsNan(double_a) ? double_a : double_b);
		return true;
	}

	if ((double_a & DOUBLE_EXP) == DOUBLE_EXP || (double_b & DOUBLE_EXP) == DOUBLE_EXP)
	{
		if ((double_a & ~DOUBLE_SIGN) == 0 || (double_b & ~DOUBLE_SIGN) == 0)
		{
			// infinity * 0 = NaN
			SetFPException(FPSCR_VXIMZ);
			early_result = PPC_NAN_U64;
			return true;
		}
		// infinity * n = infinity
		early_result = DOUBLE_EXP | ((double_a ^ double_b) & DOUBLE_SIGN);
		return true;
	}
	return false;
}

// Preprocess inputs for unary ops
bool PreprocessInput(u64 &double_a, u64 &early_result, PreprocessOperation op)
{
	SetFI(0);
	FPSCR.FR = 0;

	if (InputIsNan(double_a))
	{
		if (InputIsSNan(double_a))
			SetFPException(FPSCR_VXSNAN);
		early_result = HandleNaN(double_a);
		return true;
	}

	if (op == PreprocessRoundToSingle)
	{
		if ((double_a & DOUBLE_EXP) == DOUBLE_EXP)
		{
			// Infinity rounds to infinity.
			early_result = double_a;
			return true;
		}
	}

	return false;
}


// Preprocess inputs for binary ops
bool PreprocessInput(u64 &double_a, u64 &double_b, u64 &early_result, PreprocessOperation op)
{
	SetFI(0);
	FPSCR.FR = 0;

	if (op == PreprocessSubtraction || op == PreprocessAddition)
	{
		return PreprocessAdd(double_a, double_b, early_result, op == PreprocessSubtraction);
	}
	else if (op == PreprocessMultiplication)
	{
		return PreprocessMultiply(double_a, double_b, early_result);
	}
	else if (op == PreprocessDivision)
	{
		if ((double_b & ~DOUBLE_SIGN) == 0)
		{
			if ((double_a & ~DOUBLE_SIGN) == 0)
			{
				// 0 / 0 = NaN
				SetFPException(FPSCR_VXZDZ);
				early_result = PPC_NAN_U64;
				return true;
			}
			// n / 0 = infinity
			SetFPException(FPSCR_ZX);
			early_result = DOUBLE_EXP | ((double_a ^ double_b) & DOUBLE_SIGN);
			return true;
		}
		if ((double_a & DOUBLE_EXP) == DOUBLE_EXP)
		{
			if ((double_b & DOUBLE_EXP) == DOUBLE_EXP)
			{
				// infinity / infinity = NaN
				SetFPException(FPSCR_VXIDI);
				early_result = PPC_NAN_U64;
				return true;
			}
			// infinity / n = infinity
			early_result = DOUBLE_EXP | ((double_a ^ double_b) & DOUBLE_SIGN);
			return true;
		}
		if ((double_b & DOUBLE_EXP) == DOUBLE_EXP)
		{
			// n / infinity = 0
			early_result = (double_a ^ double_b) & DOUBLE_SIGN;
			return true;
		}
	}

	return false;
}

// Preprocess inputs for madd
bool PreprocessInput(u64 &double_a, u64 &double_b, u64 &double_c, u64 &early_result, PreprocessOperation op)
{
	SetFI(0);
	FPSCR.FR = 0;

	// We have to be very careful here here to handle NaNs correctly.
	// For multiple NaNs, the priority order for madd is mul LHS,
	// add RHS, mul RHS.  0 * inf + SNaN sets two invalid operation flags.
	// We also have to preserve the sign of any NaN.

	early_result = 0;
	if (PreprocessMultiply(double_a, double_b, early_result))
	{
		if (InputIsNan(early_result))
		{
			if (InputIsSNan(double_c))
				SetFPException(FPSCR_VXSNAN);
			if (!InputIsNan(double_a) && InputIsNan(double_c))
				early_result = HandleNaN(double_c);
			return true;
		}
	}

	bool subtraction = op == PreprocessMsub || op == PreprocessNMsub;
	if (PreprocessAdd(early_result, double_c, early_result, subtraction))
	{
		if (InputIsNan(early_result))
			return true;
	}

	if (op == PreprocessNMadd || op == PreprocessNMsub)
		early_result ^= DOUBLE_SIGN;

	return (early_result & DOUBLE_EXP) == DOUBLE_EXP;
}

// Round a double to single-precision; assumes the input is zero, inf, or nan.
static u64 RoundSpecialToSingle(u64 a)
{
	if (InputIsNan(a))
	{
		// Single-precision operations truncate NaNs to single-precision.
		a &= ~((1 << 29) - 1);
	}
	return a;
}

u64 AddSinglePrecision(u64 double_a, u64 double_b)
{
#ifdef VERY_ACCURATE_FP
	u64 early_result;
	if (PreprocessInput(double_a, double_b, early_result, PreprocessAddition))
		return RoundSpecialToSingle(early_result);

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
	u64 early_result;
	if (PreprocessInput(double_a, double_b, early_result, PreprocessAddition))
		return early_result;

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
	u64 early_result;
	if (PreprocessInput(double_a, double_b, early_result, PreprocessSubtraction))
		return RoundSpecialToSingle(early_result);

	FPTemp a = DecomposeDouble(double_a);
	FPTemp b = DecomposeDouble(double_b);
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
	u64 early_result;
	if (PreprocessInput(double_a, double_b, early_result, PreprocessSubtraction))
		return early_result;

	FPTemp a = DecomposeDouble(double_a);
	FPTemp b = DecomposeDouble(double_b);
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
	u64 early_result;
	if (PreprocessInput(double_a, double_b, early_result, PreprocessMultiplication))
		return RoundSpecialToSingle(early_result);

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
	u64 early_result;
	if (PreprocessInput(double_a, double_b, early_result, PreprocessMultiplication))
		return early_result;

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
	PreprocessOperation op;
	if (negate_c)
		op = negate_result ? PreprocessNMsub : PreprocessMsub;
	else
		op = negate_result ? PreprocessNMadd : PreprocessMadd;
	u64 early_result;
	if (PreprocessInput(double_a, double_b, double_c, early_result, op))
		return RoundSpecialToSingle(early_result);

	FPTemp a = DecomposeDouble(double_a);
	FPTemp b = DecomposeDouble(double_b);
	FPTemp c = DecomposeDouble(double_c);
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
	PreprocessOperation op;
	if (negate_c)
		op = negate_result ? PreprocessNMsub : PreprocessMsub;
	else
		op = negate_result ? PreprocessNMadd : PreprocessMadd;
	u64 early_result;
	if (PreprocessInput(double_a, double_b, double_c, early_result, op))
		return early_result;

	FPTemp a = DecomposeDouble(double_a);
	FPTemp b = DecomposeDouble(double_b);
	FPTemp c = DecomposeDouble(double_c);
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
#ifdef VERY_ACCURATE_FP
	u64 early_result;
	if (PreprocessInput(double_a, double_b, early_result, PreprocessDivision))
		return RoundSpecialToSingle(early_result);

	FPTemp a = DecomposeDouble(double_a);
	FPTemp b = DecomposeDouble(double_b);
	FPTemp result = DivFPTemp(a, b);
	u64 result_double = RoundFPTempToSingle(result);

	return result_double;
#else
	SetFI(0);
	FPSCR.FR = 0;
	return DoubleToU64(ForceSingle(U64ToDouble(double_a) / U64ToDouble(double_b)));
#endif
}

u64 DivDoublePrecision(u64 double_a, u64 double_b)
{
#ifdef VERY_ACCURATE_FP
	u64 early_result;
	if (PreprocessInput(double_a, double_b, early_result, PreprocessDivision))
		return early_result;

	FPTemp a = DecomposeDouble(double_a);
	FPTemp b = DecomposeDouble(double_b);
	FPTemp result = DivFPTemp(a, b);
	u64 result_double = RoundFPTempToDouble(result);

	return result_double;
#else
	SetFI(0);
	FPSCR.FR = 0;
	return DoubleToU64(ForceDouble(U64ToDouble(double_a) / U64ToDouble(double_b)));
#endif
}

u64 RoundToSingle(u64 double_a)
{
#ifdef VERY_ACCURATE_FP
	u64 early_result;
	if (PreprocessInput(double_a, early_result, PreprocessRoundToSingle))
		return RoundSpecialToSingle(early_result);

	FPTemp a = DecomposeDouble(double_a);
	u64 result_double = RoundFPTempToSingle(a);

	return result_double;
#else
	SetFI(0);
	FPSCR.FR = 0;
	return DoubleToU64(ForceSingle(U64ToDouble(double_a)));
#endif
}

void UpdateFPRFDouble(u64 dvalue)
{
	// TODO: Move impl here
	MathUtil::IntDouble value(dvalue);
	FPSCR.FPRF = MathUtil::ClassifyDouble(value.d);
}

void UpdateFPRFSingle(u64 dvalue)
{
	// TODO: Move impl here
	MathUtil::IntDouble value(dvalue);
	FPSCR.FPRF = MathUtil::ClassifyFloat((float)value.d);
}
