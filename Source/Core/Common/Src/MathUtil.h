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

#ifndef _MATH_UTIL_H_
#define _MATH_UTIL_H_

#include <xmmintrin.h>

#include "Common.h"

namespace MathUtil
{

static const u64 DOUBLE_SIGN = 0x8000000000000000ULL,
	DOUBLE_EXP  = 0x7FF0000000000000ULL,
	DOUBLE_FRAC = 0x000FFFFFFFFFFFFFULL,
	DOUBLE_ZERO = 0x0000000000000000ULL,

	FLOAT_SIGN = 0x80000000,
	FLOAT_EXP  = 0x7F800000,
	FLOAT_FRAC = 0x007FFFFF,
	FLOAT_ZERO = 0x00000000ULL;

union IntDouble {
	double d;
	u64 i;
};
union IntFloat {
	float f;
	u32 i;
};

inline bool IsNAN(double d)
{
	IntDouble x; x.d = d;
	return ( ((x.i & DOUBLE_EXP) == DOUBLE_EXP) &&
			 ((x.i & DOUBLE_FRAC) != DOUBLE_ZERO) );
}

inline bool IsQNAN(double d)
{
	IntDouble x; x.d = d;
	return ( ((x.i & DOUBLE_EXP) == DOUBLE_EXP) &&
		     ((x.i & 0x0007fffffffffffULL) == 0x000000000000000ULL) &&
		     ((x.i & 0x000800000000000ULL) == 0x000800000000000ULL) );
}

inline bool IsSNAN(double d)
{
	IntDouble x; x.d = d;
	return( ((x.i & DOUBLE_EXP) == DOUBLE_EXP) &&
			((x.i & DOUBLE_FRAC) != DOUBLE_ZERO) &&
			((x.i & 0x0008000000000000ULL) == DOUBLE_ZERO) );
}

inline float FlushToZero(float f)
{
	IntFloat x; x.f = f;
	if ((x.i & FLOAT_EXP) == 0)
		x.i &= FLOAT_SIGN;  // turn into signed zero
	return x.f;
}

inline double FlushToZeroAsFloat(double d)
{
	IntDouble x; x.d = d;
	if ((x.i & DOUBLE_EXP) < 0x3800000000000000ULL)
		x.i &= DOUBLE_SIGN;  // turn into signed zero
	return x.d;
}

}  // namespace MathUtil


inline float pow2f(float x) {return x * x;}
inline double pow2(double x) {return x * x;}


/*
   There are two different flavors of float to int conversion:
   _mm_cvtps_epi32() and _mm_cvttps_epi32(). The first rounds
   according to the MXCSR rounding bits. The second one always
   uses round towards zero.
 */

void SaveSSEState();
void LoadSSEState();
void LoadDefaultSSEState();

#define ROUND_UP(x, a)		(((x) + (a) - 1) & ~((a) - 1))

// ugly matrix implementation
class Matrix33
{
public:
    static void LoadIdentity(Matrix33 &mtx);

    // set mtx to be a rotation matrix around the x axis
    static void RotateX(Matrix33 &mtx, float rad);
    // set mtx to be a rotation matrix around the y axis
    static void RotateY(Matrix33 &mtx, float rad);

    // set result = a x b
    static void Multiply(const Matrix33 &a, const Matrix33 &b, Matrix33 &result);
    static void Multiply(const Matrix33 &a, const float vec[3], float result[3]);

    float data[9];
};

class Matrix44
{
public:
    static void LoadIdentity(Matrix44 &mtx);
    static void LoadMatrix33(Matrix44 &mtx, const Matrix33 &m33);
    static void Set(Matrix44 &mtx, const float mtxArray[16]);

    static void Translate(Matrix44 &mtx, const float vec[3]);

    static void Multiply(const Matrix44 &a, const Matrix44 &b, Matrix44 &result);

    float data[16];
};

#endif // _MATH_UTIL_H_
