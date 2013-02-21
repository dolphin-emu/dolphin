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

#include "Common.h"
#include "VideoCommon.h"
#include "VertexLoader.h"
#include "VertexLoader_TextCoord.h"
#include "VertexManagerBase.h"
#include "CPUDetect.h"

#if _M_SSE >= 0x401
#include <smmintrin.h>
#elif _M_SSE >= 0x301 && !(defined __GNUC__ && !defined __SSSE3__)
#include <tmmintrin.h>
#endif

#define LOG_TEX1() // PRIM_LOG("tex: %f, ", ((float*)VertexManager::s_pCurBufferPointer)[0]);
#define LOG_TEX2() // PRIM_LOG("tex: %f %f, ", ((float*)VertexManager::s_pCurBufferPointer)[0], ((float*)VertexManager::s_pCurBufferPointer)[1]);

extern int tcIndex;
extern float tcScale[8];

void LOADERDECL TexCoord_Read_Dummy()
{
	tcIndex++;
}

template <typename T, int N>
void LOADERDECL TexCoord_ReadDirect()
{
	reinterpret_cast<float*>(VertexManager::s_pCurBufferPointer)[0] = DataRead<T>() * tcScale[tcIndex];
	if (N >= 1)
		reinterpret_cast<float*>(VertexManager::s_pCurBufferPointer)[1] = DataRead<T>() * tcScale[tcIndex];
	//LOG_TEX1();
	//LOG_TEX2();
	VertexManager::s_pCurBufferPointer += sizeof(float) * N;
	++tcIndex;
}

template <typename I, typename T, int N>
void LOADERDECL TexCoord_ReadIndex()
{
	auto const index = DataRead<I>();
	auto const data = reinterpret_cast<T*>(cached_arraybases[ARRAY_TEXCOORD0 + tcIndex] + (index * arraystrides[ARRAY_TEXCOORD0 + tcIndex]));
	reinterpret_cast<float*>(VertexManager::s_pCurBufferPointer)[0] = Common::FromBigEndian(data[0]) * tcScale[tcIndex];
	if (N >= 1)
		reinterpret_cast<float*>(VertexManager::s_pCurBufferPointer)[1] = Common::FromBigEndian(data[1]) * tcScale[tcIndex];
	//LOG_TEX1();
	//LOG_TEX2();
	VertexManager::s_pCurBufferPointer += sizeof(float) * N;
	++tcIndex;
}

#if _M_SSE >= 0x401
static const __m128i kMaskSwap16_2 = _mm_set_epi32(0xFFFFFFFFL, 0xFFFFFFFFL, 0xFFFFFFFFL, 0x02030001L);

void LOADERDECL TexCoord_ReadIndex16_Short2_SSE4()
{
	// Heavy in ZWW
	u16 Index = DataReadU16();
	const s32 *pData = (const s32*)(cached_arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]));
	const __m128i a = _mm_cvtsi32_si128(*pData);
	const __m128i b = _mm_shuffle_epi8(a, kMaskSwap16_2);
	const __m128i c = _mm_cvtepi16_epi32(b);
	const __m128 d = _mm_cvtepi32_ps(c);
	const __m128 e = _mm_load1_ps(&tcScale[tcIndex]);
	const __m128 f = _mm_mul_ps(d, e);
	_mm_storeu_ps((float*)VertexManager::s_pCurBufferPointer, f);
	LOG_TEX2();
	VertexManager::s_pCurBufferPointer += 8;
	tcIndex++;
}
#endif

#if _M_SSE >= 0x301
static const __m128i kMaskSwap32 = _mm_set_epi32(0xFFFFFFFFL, 0xFFFFFFFFL, 0x04050607L, 0x00010203L);

void LOADERDECL TexCoord_ReadIndex16_Float2_SSSE3()
{
	u16 Index = DataReadU16(); 
	const u32 *pData = (const u32 *)(cached_arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]));
	GC_ALIGNED128(const __m128i a = _mm_loadl_epi64((__m128i*)pData));
	GC_ALIGNED128(const __m128i b = _mm_shuffle_epi8(a, kMaskSwap32));
	u8* p = VertexManager::s_pCurBufferPointer;
	_mm_storel_epi64((__m128i*)p, b);
	LOG_TEX2();
	p += 8;
	VertexManager::s_pCurBufferPointer = p;
	tcIndex++;
}
#endif

static TPipelineFunction tableReadTexCoord[4][8][2] = {
	{
		{NULL, NULL,},
		{NULL, NULL,},
		{NULL, NULL,},
		{NULL, NULL,},
		{NULL, NULL,},
	},
	{
		{TexCoord_ReadDirect<u8, 1>,  TexCoord_ReadDirect<u8, 2>,},
		{TexCoord_ReadDirect<s8, 1>,   TexCoord_ReadDirect<s8, 2>,},
		{TexCoord_ReadDirect<u16, 1>, TexCoord_ReadDirect<u16, 2>,},
		{TexCoord_ReadDirect<s16, 1>,  TexCoord_ReadDirect<s16, 2>,},
		{TexCoord_ReadDirect<float, 1>,  TexCoord_ReadDirect<float, 2>,},
	},
	{
		{TexCoord_ReadIndex<u8, u8, 1>,  TexCoord_ReadIndex<u8, u8, 2>,},
		{TexCoord_ReadIndex<u8, s8, 1>,   TexCoord_ReadIndex<u8, s8, 2>,},
		{TexCoord_ReadIndex<u8, u16, 1>, TexCoord_ReadIndex<u8, u16, 2>,},
		{TexCoord_ReadIndex<u8, s16, 1>,  TexCoord_ReadIndex<u8, s16, 2>,},
		{TexCoord_ReadIndex<u8, float, 1>,  TexCoord_ReadIndex<u8, float, 2>,},
	},
	{
		{TexCoord_ReadIndex<u16, u8, 1>,  TexCoord_ReadIndex<u16, u8, 2>,},
		{TexCoord_ReadIndex<u16, s8, 1>,   TexCoord_ReadIndex<u16, s8, 2>,},
		{TexCoord_ReadIndex<u16, u16, 1>, TexCoord_ReadIndex<u16, u16, 2>,},
		{TexCoord_ReadIndex<u16, s16, 1>,  TexCoord_ReadIndex<u16, s16, 2>,},
		{TexCoord_ReadIndex<u16, float, 1>,  TexCoord_ReadIndex<u16, float, 2>,},
	},
};

static int tableReadTexCoordVertexSize[4][8][2] = {
	{
		{0, 0,}, {0, 0,}, {0, 0,}, {0, 0,}, {0, 0,},
	},
	{
		{1, 2,}, {1, 2,}, {2, 4,}, {2, 4,}, {4, 8,},
	},
	{
		{1, 1,}, {1, 1,}, {1, 1,}, {1, 1,}, {1, 1,},
	},
	{
		{2, 2,}, {2, 2,}, {2, 2,}, {2, 2,}, {2, 2,},
	},
};

void VertexLoader_TextCoord::Init(void) {

#if _M_SSE >= 0x301

	if (cpu_info.bSSSE3) {
		tableReadTexCoord[3][4][1] = TexCoord_ReadIndex16_Float2_SSSE3;
	}

#endif

#if _M_SSE >= 0x401

	if (cpu_info.bSSE4_1) {
		tableReadTexCoord[3][3][1] = TexCoord_ReadIndex16_Short2_SSE4;
	}

#endif

}

unsigned int VertexLoader_TextCoord::GetSize(unsigned int _type, unsigned int _format, unsigned int _elements) {
	return tableReadTexCoordVertexSize[_type][_format][_elements];
}

TPipelineFunction VertexLoader_TextCoord::GetFunction(unsigned int _type, unsigned int _format, unsigned int _elements) {
	return tableReadTexCoord[_type][_format][_elements];
}

TPipelineFunction VertexLoader_TextCoord::GetDummyFunction() {
	return TexCoord_Read_Dummy;
}
