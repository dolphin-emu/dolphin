// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <cstdlib>
#include <vector>

#include "Common/CommonTypes.h"

namespace MathUtil
{

template <typename T>
constexpr T SNANConstant()
{
	return std::numeric_limits<T>::signaling_NaN();
}

#ifdef _MSC_VER

// MSVC needs a workaround, because its std::numeric_limits<double>::signaling_NaN()
// will use __builtin_nans, which is improperly handled by the compiler and generates
// a bad constant. Here we go back to the version MSVC used before the builtin.
// TODO: Remove this and use numeric_limits directly whenever this bug is fixed.
template <>
constexpr double SNANConstant()
{
	return (_CSTD _Snan._Double);
}
template <>
constexpr float SNANConstant()
{
	return (_CSTD _Snan._Float);
}

#endif

template<class T>
constexpr T Clamp(const T val, const T& min, const T& max)
{
	return std::max(min, std::min(max, val));
}

constexpr bool IsPow2(u32 imm)
{
	return (imm & (imm - 1)) == 0;
}

// The most significant bit of the fraction is an is-quiet bit on all architectures we care about.

static const u64 DOUBLE_SIGN = 0x8000000000000000ULL,
                 DOUBLE_EXP  = 0x7FF0000000000000ULL,
                 DOUBLE_FRAC = 0x000FFFFFFFFFFFFFULL,
                 DOUBLE_ZERO = 0x0000000000000000ULL,
                 DOUBLE_QBIT = 0x0008000000000000ULL;

static const u32 FLOAT_SIGN = 0x80000000,
                 FLOAT_EXP  = 0x7F800000,
                 FLOAT_FRAC = 0x007FFFFF,
                 FLOAT_ZERO = 0x00000000;

union IntDouble {
	double d;
	u64 i;

	explicit IntDouble(u64 _i) : i(_i) {}
	explicit IntDouble(double _d) : d(_d) {}
};
union IntFloat {
	float f;
	u32 i;

	explicit IntFloat(u32 _i) : i(_i) {}
	explicit IntFloat(float _f) : f(_f) {}
};

inline bool IsQNAN(double d)
{
	IntDouble x(d);
	return ((x.i & DOUBLE_EXP) == DOUBLE_EXP) &&
	       ((x.i & DOUBLE_QBIT) == DOUBLE_QBIT);
}

inline bool IsSNAN(double d)
{
	IntDouble x(d);
	return ((x.i & DOUBLE_EXP) == DOUBLE_EXP) &&
	       ((x.i & DOUBLE_FRAC) != DOUBLE_ZERO) &&
	       ((x.i & DOUBLE_QBIT) == DOUBLE_ZERO);
}

inline float FlushToZero(float f)
{
	IntFloat x(f);
	if ((x.i & FLOAT_EXP) == 0)
	{
		x.i &= FLOAT_SIGN;  // turn into signed zero
	}
	return x.f;
}

inline double FlushToZero(double d)
{
	IntDouble x(d);
	if ((x.i & DOUBLE_EXP) == 0)
	{
		x.i &= DOUBLE_SIGN;  // turn into signed zero
	}
	return x.d;
}

enum PPCFpClass
{
	PPC_FPCLASS_QNAN = 0x11,
	PPC_FPCLASS_NINF = 0x9,
	PPC_FPCLASS_NN   = 0x8,
	PPC_FPCLASS_ND   = 0x18,
	PPC_FPCLASS_NZ   = 0x12,
	PPC_FPCLASS_PZ   = 0x2,
	PPC_FPCLASS_PD   = 0x14,
	PPC_FPCLASS_PN   = 0x4,
	PPC_FPCLASS_PINF = 0x5,
};

// Uses PowerPC conventions for the return value, so it can be easily
// used directly in CPU emulation.
u32 ClassifyDouble(double dvalue);
// More efficient float version.
u32 ClassifyFloat(float fvalue);

extern const int frsqrte_expected_base[];
extern const int frsqrte_expected_dec[];
extern const int fres_expected_base[];
extern const int fres_expected_dec[];

// PowerPC approximation algorithms
double ApproximateReciprocalSquareRoot(double val);
double ApproximateReciprocal(double val);

template<class T>
struct Rectangle
{
	T left{};
	T top{};
	T right{};
	T bottom{};

	constexpr Rectangle() = default;

	constexpr Rectangle(T theLeft, T theTop, T theRight, T theBottom)
		: left(theLeft), top(theTop), right(theRight), bottom(theBottom)
	{}

	constexpr bool operator==(const Rectangle& r) const
	{
		return left == r.left && top == r.top && right == r.right && bottom == r.bottom;
	}

	T GetWidth() const { return abs(right - left); }
	T GetHeight() const { return abs(bottom - top); }

	// If the rectangle is in a coordinate system with a lower-left origin, use
	// this Clamp.
	void ClampLL(T x1, T y1, T x2, T y2)
	{
		left   = Clamp(left, x1, x2);
		right  = Clamp(right, x1, x2);
		top    = Clamp(top, y2, y1);
		bottom = Clamp(bottom, y2, y1);
	}

	// If the rectangle is in a coordinate system with an upper-left origin,
	// use this Clamp.
	void ClampUL(T x1, T y1, T x2, T y2)
	{
		left   = Clamp(left, x1, x2);
		right  = Clamp(right, x1, x2);
		top    = Clamp(top, y1, y2);
		bottom = Clamp(bottom, y1, y2);
	}
};

}  // namespace MathUtil

float MathFloatVectorSum(const std::vector<float>&);

#define ROUND_UP(x, a)   (((x) + (a) - 1) & ~((a) - 1))
#define ROUND_DOWN(x, a) ((x) & ~((a) - 1))

// Rounds down. 0 -> undefined
inline int IntLog2(u64 val)
{
#if defined(__GNUC__)
	return 63 - __builtin_clzll(val);

#elif defined(_MSC_VER)
	unsigned long result = -1;
	_BitScanReverse64(&result, val);
	return result;

#else
	int result = -1;
	while (val != 0)
	{
		val >>= 1;
		++result;
	}
	return result;
#endif
}

// Tiny matrix/vector library.
// Used for things like Free-Look in the gfx backend.

class Matrix33
{
public:
	static void LoadIdentity(Matrix33& mtx);

	// set mtx to be a rotation matrix around the x axis
	static void RotateX(Matrix33& mtx, float rad);
	// set mtx to be a rotation matrix around the y axis
	static void RotateY(Matrix33& mtx, float rad);

	// set result = a x b
	static void Multiply(const Matrix33& a, const Matrix33& b, Matrix33& result);
	static void Multiply(const Matrix33& a, const float vec[3], float result[3]);

	float data[9];
};

class Matrix44
{
public:
	static void LoadIdentity(Matrix44& mtx);
	static void LoadMatrix33(Matrix44& mtx, const Matrix33& m33);
	static void Set(Matrix44& mtx, const float mtxArray[16]);

	static void Translate(Matrix44& mtx, const float vec[3]);
	static void Shear(Matrix44& mtx, const float a, const float b = 0);

	static void Multiply(const Matrix44& a, const Matrix44& b, Matrix44& result);

	float data[16];
};
