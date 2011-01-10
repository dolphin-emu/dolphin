// Copyright (C) 2003 Dolphin Project.

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



#ifndef _DATAREADER_H
#define _DATAREADER_H

extern u8* g_pVideoData;

#if _M_SSE >= 0x301 && !(defined __GNUC__ && !defined __SSSE3__)
#include <tmmintrin.h>
#endif

__forceinline void DataSkip(u32 skip)
{
	g_pVideoData += skip;
}

__forceinline u8 DataPeek8(int _uOffset)
{
	return g_pVideoData[_uOffset];
}

__forceinline u16 DataPeek16(int _uOffset)
{
	return Common::swap16(*(u16*)&g_pVideoData[_uOffset]);
}

__forceinline u32 DataPeek32(int _uOffset)	
{
	return Common::swap32(*(u32*)&g_pVideoData[_uOffset]);
}

__forceinline u8 DataReadU8()
{
	return *g_pVideoData++;
}

__forceinline s8 DataReadS8()
{
	return (s8)(*g_pVideoData++);
}

__forceinline u16 DataReadU16()
{
	u16 tmp = Common::swap16(*(u16*)g_pVideoData);
	g_pVideoData += 2;
	return tmp;
}

__forceinline u32 DataReadU32()
{
	u32 tmp = Common::swap32(*(u32*)g_pVideoData);
	g_pVideoData += 4;
	return tmp;
}

typedef void (*DataReadU32xNfunc)(u32 *buf);
extern DataReadU32xNfunc DataReadU32xFuncs[16];

#if _M_SSE >= 0x301

const __m128i mask1 = _mm_set_epi8(15,14,13,12,11,10,9,8,7,6,5,4,0,1,2,3);
const __m128i mask2 = _mm_set_epi8(15,14,13,12,11,10,9,8,4,5,6,7,0,1,2,3);
const __m128i mask3 = _mm_set_epi8(15,14,13,12,8,9,10,11,4,5,6,7,0,1,2,3);
const __m128i mask4 = _mm_set_epi8(12,13,14,15,8,9,10,11,4,5,6,7,0,1,2,3);

template<unsigned int N>
void DataReadU32xN_SSSE3(u32 *bufx16)
{
	__m128i* store = (__m128i *)bufx16;
	__m128i* load =  (__m128i *)g_pVideoData;
	switch(N)
	{
		case 13: _mm_storeu_si128(store++, _mm_shuffle_epi8(_mm_loadu_si128(load++), mask4));
		case 9:  _mm_storeu_si128(store++, _mm_shuffle_epi8(_mm_loadu_si128(load++), mask4));
		case 5:  _mm_storeu_si128(store++, _mm_shuffle_epi8(_mm_loadu_si128(load++), mask4));
		case 1:  _mm_storeu_si128(store,   _mm_shuffle_epi8(_mm_loadu_si128(load),   mask1));
			break;
		case 14: _mm_storeu_si128(store++, _mm_shuffle_epi8(_mm_loadu_si128(load++), mask4));
		case 10: _mm_storeu_si128(store++, _mm_shuffle_epi8(_mm_loadu_si128(load++), mask4));
		case 6:  _mm_storeu_si128(store++, _mm_shuffle_epi8(_mm_loadu_si128(load++), mask4));
		case 2:  _mm_storeu_si128(store,   _mm_shuffle_epi8(_mm_loadu_si128(load),   mask2));
			break;
		case 15: _mm_storeu_si128(store++, _mm_shuffle_epi8(_mm_loadu_si128(load++), mask4));
		case 11: _mm_storeu_si128(store++, _mm_shuffle_epi8(_mm_loadu_si128(load++), mask4));
		case 7:  _mm_storeu_si128(store++, _mm_shuffle_epi8(_mm_loadu_si128(load++), mask4));
		case 3:  _mm_storeu_si128(store,   _mm_shuffle_epi8(_mm_loadu_si128(load),   mask3));
			break;
		case 16: _mm_storeu_si128(store++, _mm_shuffle_epi8(_mm_loadu_si128(load++), mask4));
		case 12: _mm_storeu_si128(store++, _mm_shuffle_epi8(_mm_loadu_si128(load++), mask4));
		case 8:  _mm_storeu_si128(store++, _mm_shuffle_epi8(_mm_loadu_si128(load++), mask4));
		case 4:  _mm_storeu_si128(store,   _mm_shuffle_epi8(_mm_loadu_si128(load),   mask4));
			break;
	}
	g_pVideoData += (sizeof(u32) * N);
}

#endif

template<unsigned int N>
void DataReadU32xN(u32 *bufx16)
{
	memcpy(bufx16, g_pVideoData, sizeof(u32) * N);
	if (N >= 1) bufx16[0] = Common::swap32(bufx16[0]);
	if (N >= 2) bufx16[1] = Common::swap32(bufx16[1]);
	if (N >= 3) bufx16[2] = Common::swap32(bufx16[2]);
	if (N >= 4) bufx16[3] = Common::swap32(bufx16[3]);
	if (N >= 5) bufx16[4] = Common::swap32(bufx16[4]);
	if (N >= 6) bufx16[5] = Common::swap32(bufx16[5]);
	if (N >= 7) bufx16[6] = Common::swap32(bufx16[6]);
	if (N >= 8) bufx16[7] = Common::swap32(bufx16[7]);
	if (N >= 9) bufx16[8] = Common::swap32(bufx16[8]);
	if (N >= 10) bufx16[9] = Common::swap32(bufx16[9]);
	if (N >= 11) bufx16[10] = Common::swap32(bufx16[10]);
	if (N >= 12) bufx16[11] = Common::swap32(bufx16[11]);
	if (N >= 13) bufx16[12] = Common::swap32(bufx16[12]);
	if (N >= 14) bufx16[13] = Common::swap32(bufx16[13]);
	if (N >= 15) bufx16[14] = Common::swap32(bufx16[14]);
	if (N >= 16) bufx16[15] = Common::swap32(bufx16[15]);
	g_pVideoData += (sizeof(u32) * N);
}

__forceinline u32 DataReadU32Unswapped()
{
	u32 tmp = *(u32*)g_pVideoData;
	g_pVideoData += 4;
	return tmp;
}

template<class T>
__forceinline T DataRead()
{
	T tmp = *(T*)g_pVideoData;
	g_pVideoData += sizeof(T);
	return tmp;
}

template <>
__forceinline u16 DataRead()
{
	u16 tmp = Common::swap16(*(u16*)g_pVideoData);
	g_pVideoData += 2;
	return tmp;
}

template <>
__forceinline s16 DataRead()
{
	s16 tmp = (s16)Common::swap16(*(u16*)g_pVideoData);
	g_pVideoData += 2;
	return tmp;
}

template <>
__forceinline u32 DataRead()
{
	u32 tmp = (u32)Common::swap32(*(u32*)g_pVideoData);
	g_pVideoData += 4;
	return tmp;
}

template <>
__forceinline s32 DataRead()
{
	s32 tmp = (s32)Common::swap32(*(u32*)g_pVideoData);
	g_pVideoData += 4;
	return tmp;
}

__forceinline float DataReadF32()
{
	union {u32 i; float f;} temp;
	temp.i = Common::swap32(*(u32*)g_pVideoData);
	g_pVideoData += 4;
	float tmp = temp.f;
	return tmp;
}

__forceinline u8* DataGetPosition()
{
	return g_pVideoData;
}

#endif
