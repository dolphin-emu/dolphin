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

#if _WIN32
#include <intrin.h>
#endif

#include <xmmintrin.h>

#include "XFBConvert.h"
#include "Common.h"

namespace {

const __m128i _bias1 = _mm_set_epi32(128/2 << 16,        0, 128/2 << 16, 16 << 16);
const __m128i _bias2 = _mm_set_epi32(128/2 << 16, 16 << 16, 128/2 << 16,        0);

__m128i _y[256];
__m128i _u[256];
__m128i _v[256];

__m128i _r1[256];
__m128i _r2[256];
__m128i _g1[256];
__m128i _g2[256];
__m128i _b1[256];
__m128i _b2[256];

}  // namespace

void InitXFBConvTables() 
{
	for (int i = 0; i < 256; i++)
	{
		_y[i] = _mm_set_epi32(0xFFFFFFF,     76283*(i - 16),      76283*(i - 16),     76283*(i - 16));
		_u[i] = _mm_set_epi32(        0,                  0,  -25624 * (i - 128), 132252 * (i - 128));
		_v[i] = _mm_set_epi32(        0, 104595 * (i - 128),  -53281 * (i - 128),                  0);

		_r1[i] = _mm_set_epi32( 28770 * i / 2,          0,  -9699 * i / 2, 16843 * i);
		_g1[i] = _mm_set_epi32(-24117 * i / 2,          0, -19071 * i / 2, 33030 * i);
		_b1[i] = _mm_set_epi32( -4653 * i / 2,          0,  28770 * i / 2,  6423 * i);
										   									
		_r2[i] = _mm_set_epi32( 28770 * i / 2,  16843 * i,  -9699 * i / 2,         0);
		_g2[i] = _mm_set_epi32(-24117 * i / 2,  33030 * i, -19071 * i / 2,         0);
		_b2[i] = _mm_set_epi32( -4653 * i / 2,   6423 * i,  28770 * i / 2,         0);
	}
}

void ConvertFromXFB(u32 *dst, const u8* _pXFB, int width, int height)
{
	const unsigned char *src = _pXFB;
	u32 numBlocks = ((width * height) / 2) / 2;
	for (u32 i = 0; i < numBlocks; i++)
	{
		__m128i y1 = _y[src[0]];
		__m128i u  = _u[src[1]];
		__m128i y2 = _y[src[2]];
		__m128i v  = _v[src[3]];
		__m128i y1_2 = _y[src[4+0]];
		__m128i u_2  = _u[src[4+1]];
		__m128i y2_2 = _y[src[4+2]];
		__m128i v_2  = _v[src[4+3]];

		__m128i c1 = _mm_srai_epi32(_mm_add_epi32(y1, _mm_add_epi32(u, v)), 16);
		__m128i c2 = _mm_srai_epi32(_mm_add_epi32(y2, _mm_add_epi32(u, v)), 16);
		__m128i c3 = _mm_srai_epi32(_mm_add_epi32(y1_2, _mm_add_epi32(u_2, v_2)), 16);
		__m128i c4 = _mm_srai_epi32(_mm_add_epi32(y2_2, _mm_add_epi32(u_2, v_2)), 16);

		__m128i four_dest = _mm_packus_epi16(_mm_packs_epi32(c1, c2), _mm_packs_epi32(c3, c4));
		_mm_storeu_si128((__m128i *)dst, four_dest);
		dst += 4;
		src += 8;
	}
}

void ConvertToXFB(u32 *dst, const u8* _pEFB, int width, int height)
{
	const unsigned char *src = _pEFB;

	u32 numBlocks = ((width * height) / 2) / 4;
	if (((size_t)dst & 0xF) != 0) {
		PanicAlert("Weird - unaligned XFB");
	}
	__m128i zero = _mm_setzero_si128();
	for (int i = 0; i < numBlocks; i++)
	{
		__m128i yuyv0 = _mm_srai_epi32(
			_mm_add_epi32(
				_mm_add_epi32(_mm_add_epi32(_r1[src[0]], _mm_add_epi32(_g1[src[1]], _b1[src[2]])), _bias1),
				_mm_add_epi32(_mm_add_epi32(_r2[src[4]], _mm_add_epi32(_g2[src[5]], _b2[src[6]])), _bias2)), 16);
		src += 8;
		__m128i yuyv1 = _mm_srai_epi32(
			_mm_add_epi32(
				_mm_add_epi32(_mm_add_epi32(_r1[src[0]], _mm_add_epi32(_g1[src[1]], _b1[src[2]])), _bias1),
				_mm_add_epi32(_mm_add_epi32(_r2[src[4]], _mm_add_epi32(_g2[src[5]], _b2[src[6]])), _bias2)), 16);
		src += 8;
		__m128i yuyv2 = _mm_srai_epi32(
			_mm_add_epi32(
				_mm_add_epi32(_mm_add_epi32(_r1[src[0]], _mm_add_epi32(_g1[src[1]], _b1[src[2]])), _bias1),
				_mm_add_epi32(_mm_add_epi32(_r2[src[4]], _mm_add_epi32(_g2[src[5]], _b2[src[6]])), _bias2)), 16);
		src += 8;
		__m128i yuyv3 = _mm_srai_epi32(
			_mm_add_epi32(
				_mm_add_epi32(_mm_add_epi32(_r1[src[0]], _mm_add_epi32(_g1[src[1]], _b1[src[2]])), _bias1),
				_mm_add_epi32(_mm_add_epi32(_r2[src[4]], _mm_add_epi32(_g2[src[5]], _b2[src[6]])), _bias2)), 16);
		src += 8;
		__m128i four_dest = _mm_packus_epi16(_mm_packs_epi32(yuyv0, yuyv1), _mm_packs_epi32(yuyv2, yuyv3));
		_mm_store_si128((__m128i *)dst, four_dest);
		dst += 4;
	}
}

