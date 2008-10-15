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

u8 bound_table[3*256];
int y[256];
int v1[256];
int v2[256];
int u1[256];
int u2[256];
u8 *bound_lut = bound_table + 256;

inline int bound(int i)
{
	return bound_lut[i];
}

inline void yuv2rgb(int y, int u, int v, int &r, int &g, int &b)
{
	int gray = 76283*(y - 16);
	b = bound((gray + 132252*(u - 128))>>16);
	g = bound((gray - 53281 *(v - 128) - 25624*(u - 128))>>16);
	r = bound((gray + 104595*(v - 128))>>16);
}

inline void rgb2yuv(int r, int g, int b, int &y, int &u, int &v)
{
	y = (((16843 * r) + (33030 * g) + (6423 * b)) >> 16) + 16;
	v = (((28770 * r) - (24117 * g) - (4653 * b)) >> 16) + 128;
	u = ((-(9699 * r) - (19071 * g) + (28770 * b)) >> 16) + 128;
}

}  // namespace


//const __m128i _bias1 = _mm_set_epi32(16 << 16, 128/2 << 16,        0, 128/2 << 16);
//const __m128i _bias2 = _mm_set_epi32(0,        128/2 << 16, 16 << 16, 128/2 << 16);

const __m128i _bias1 = _mm_set_epi32(128/2 << 16,        0, 128/2 << 16, 16 << 16);
const __m128i _bias2 = _mm_set_epi32(128/2 << 16, 16 << 16, 128/2 << 16,        0);
__m128i _r1[256];
__m128i _r2[256];
__m128i _g1[256];
__m128i _g2[256];
__m128i _b1[256];
__m128i _b2[256];

void InitXFBConvTables() 
{
	for (int i = 0; i < 256; i++)
	{
		bound_table[i] = 0;
		bound_table[256 + i] = i;
		bound_table[512 + i] = 255;

		y[i] = 76283*(i - 16);
		u1[i] = 132252 * (i - 128);
		u2[i] = -25624 * (i - 128);
		v1[i] = -53281 * (i - 128);
		v2[i] = 104595 * (i - 128);

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
	u32 numBlocks = (width * height) / 2;
	for (u32 i = 0; i < numBlocks; i++, src += 4)
	{
		int Y1 = y[src[0]];
		int Y2 = y[src[2]];
		int U  = src[1];
		int V  = src[3];
		int b1 = bound((Y1 + u1[U]) >> 16);
		int g1 = bound((Y1 + v1[V] + u2[U])>>16);
		int r1 = bound((Y1 + v2[V]) >> 16);
		int b2 = bound((Y2 + u1[U]) >> 16);
		int g2 = bound((Y2 + v1[V] + u2[U]) >> 16);
		int r2 = bound((Y2 + v2[V]) >> 16);
		*dst++ = 0xFF000000 | (r1<<16) | (g1<<8) | (b1);
		*dst++ = 0xFF000000 | (r2<<16) | (g2<<8) | (b2);
	}	
}

void ConvertToXFB(u32 *dst, const u8* _pEFB, int width, int height)
{
	const unsigned char *src = _pEFB;

	u32 numBlocks = (width * height) / 2;
#if 1
	__m128i zero = _mm_setzero_si128();
	for (int i = 0; i < numBlocks / 4; i++)
	{
		__m128i yuyv[4];
		for (int j = 0; j < 4; j++) {
			yuyv[j] = _mm_srai_epi32(
				_mm_add_epi32(
					_mm_add_epi32(
						_mm_add_epi32(_r1[src[0]],	_mm_add_epi32(_g1[src[1]], _b1[src[2]])), _bias1),
					_mm_add_epi32(
						_mm_add_epi32(_r2[src[4]], _mm_add_epi32(_g2[src[5]], _b2[src[6]])), _bias2)
					), 16);
			src += 8;
		}
		__m128i four_dest = _mm_packus_epi16(_mm_packs_epi32(yuyv[0], yuyv[1]), _mm_packs_epi32(yuyv[2], yuyv[3]));
		_mm_storeu_si128((__m128i *)dst, four_dest);
		dst += 4;
	}
#else
	for (int i = 0; i < numBlocks; i++)
	{
		int y1 = (((16843 * src[0]) + (33030 * src[1]) + (6423 * src[2])) >> 16) + 16;		
		int u1 = ((-(9699 * src[0]) - (19071 * src[1]) + (28770 * src[2])) >> 16) + 128;

		int v1 = (((28770 * src[0]) - (24117 * src[1]) - (4653 * src[2])) >> 16) + 128;
		src += 4;

		int u2 = ((-(9699 * src[0]) - (19071 * src[1]) + (28770 * src[2])) >> 16) + 128;
		int y2 = (((16843 * src[0]) + (33030 * src[1]) + (6423 * src[2])) >> 16) + 16;
		int v2 = (((28770 * src[0]) - (24117 * src[1]) - (4653 * src[2])) >> 16) + 128;
		src += 4;

		int u = bound_lut[(u1 + u2) / 2];
		int v = bound_lut[(v1 + v2) / 2];
		*dst++ = (v << 24) | (y2 << 16) | (u << 8) | (y1);
	}
#endif
}

