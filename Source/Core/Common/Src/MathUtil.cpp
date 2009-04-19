// Copyright (C) 2003-2009 Dolphin Project.

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

#include <xmmintrin.h>

#include "Common.h"
#include "MathUtil.h"

#include <cmath>

static u32 saved_sse_state = _mm_getcsr();
static const u32 default_sse_state = _mm_getcsr();


void LoadDefaultSSEState()
{
	_mm_setcsr(default_sse_state);
}


void LoadSSEState()
{
	_mm_setcsr(saved_sse_state);
}


void SaveSSEState()
{
	saved_sse_state = _mm_getcsr();
}

void MatrixMul(int n, const float *a, const float *b, float *result)
{    
    for(int i = 0; i < n; ++i) {        
        for(int j= 0; j < n; ++j) {
            float temp = 0;
            for(int k = 0; k < n; ++k) {
                temp += a[i * n + k] * b[k * n + j];
            }
            result[i * n + j] = temp;
        }
    }
}

void Matrix33::LoadIdentity(Matrix33 &mtx)
{
    memset(mtx.data, 0, sizeof(mtx.data));
    mtx.data[0] = 1.0f;
    mtx.data[4] = 1.0f;
    mtx.data[8] = 1.0f;
}

void Matrix33::RotateX(Matrix33 &mtx, float rad)
{
    float s = sin(rad);
    float c = cos(rad);
    memset(mtx.data, 0, sizeof(mtx.data));
    mtx.data[0] = 1;
    mtx.data[4] = c;
    mtx.data[5] = -s;
    mtx.data[7] = s;
    mtx.data[8] = c;
}
void Matrix33::RotateY(Matrix33 &mtx, float rad)
{
    float s = sin(rad);
    float c = cos(rad);
    memset(mtx.data, 0, sizeof(mtx.data));
    mtx.data[0] = c;
    mtx.data[2] = s;
    mtx.data[4] = 1;
    mtx.data[6] = -s;    
    mtx.data[8] = c;
}

void Matrix33::Multiply(const Matrix33 &a, const Matrix33 &b, Matrix33 &result)
{
    MatrixMul(3, a.data, b.data, result.data);
}

void Matrix33::Multiply(const Matrix33 &a, const float vec[3], float result[3])
{
    for(int i = 0; i < 3; ++i) {
        result[i] = 0;
        for(int k = 0; k < 3; ++k) {
            result[i] += a.data[i * 3 + k] * vec[k];
        }
    }
}

void Matrix44::LoadIdentity(Matrix44 &mtx)
{
    memset(mtx.data, 0, sizeof(mtx.data));
    mtx.data[0] = 1.0f;
    mtx.data[5] = 1.0f;
    mtx.data[10] = 1.0f;
    mtx.data[15] = 1.0f;
}

void Matrix44::LoadMatrix33(Matrix44 &mtx, const Matrix33 &m33)
{
    for(int i = 0; i < 3; ++i) {
        for(int j = 0; j < 3; ++j) {
            mtx.data[i * 4 + j] = m33.data[i * 3 + j];
        }
    }

    for(int i = 0; i < 3; ++i) {
        mtx.data[i * 4 + 3] = 0;
        mtx.data[i + 12] = 0;
    }
    mtx.data[15] = 1.0f;
}

void Matrix44::Set(Matrix44 &mtx, const float mtxArray[16])
{
    for(int i = 0; i < 16; ++i) {
        mtx.data[i] = mtxArray[i];
    }
}

void Matrix44::Translate(Matrix44 &mtx, const float vec[3])
{
    LoadIdentity(mtx);
    mtx.data[3] = vec[0];
    mtx.data[7] = vec[1];
    mtx.data[11] = vec[2];
}

void Matrix44::Multiply(const Matrix44 &a, const Matrix44 &b, Matrix44 &result)
{
    MatrixMul(4, a.data, b.data, result.data);
}
