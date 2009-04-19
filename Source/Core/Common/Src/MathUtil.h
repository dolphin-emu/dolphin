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

/*
   There are two different flavors of float to int conversion:
   _mm_cvtps_epi32() and _mm_cvttps_epi32(). The first rounds
   according to the MXCSR rounding bits. The second one always
   uses round towards zero.
 */

inline float pow2f(float x) {return x * x;}
inline double pow2(double x) {return x * x;}

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
