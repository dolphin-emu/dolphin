// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/CommonTypes.h"
#include "Common/CPUDetect.h"

#include "VideoCommon/VertexLoader.h"
#include "VideoCommon/VertexLoader_TextCoord.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VideoCommon.h"


#if _M_SSE >= 0x401
#include <smmintrin.h>
#elif _M_SSE >= 0x301 && !(defined __GNUC__ && !defined __SSSE3__)
#include <tmmintrin.h>
#endif

template <int N>
void LOG_TEX();

template <>
__forceinline void LOG_TEX<1>()
{
	// warning: mapping buffer should be disabled to use this
	// PRIM_LOG("tex: %f, ", ((float*)VertexManager::s_pCurBufferPointer)[-1]);
}

template <>
__forceinline void LOG_TEX<2>()
{
	// warning: mapping buffer should be disabled to use this
	// PRIM_LOG("tex: %f %f, ", ((float*)VertexManager::s_pCurBufferPointer)[-2], ((float*)VertexManager::s_pCurBufferPointer)[-1]);
}

static void LOADERDECL TexCoord_Read_Dummy()
{
	tcIndex++;
}

template <typename T>
float TCScale(T val, float scale)
{
	return val * scale;
}

template <>
float TCScale(float val, float scale)
{
	return val;
}

template <typename T, int N>
void LOADERDECL TexCoord_ReadDirect()
{
	auto const scale = tcScale[tcIndex];
	DataWriter dst;
	DataReader src;

	for (int i = 0; i != N; ++i)
		dst.Write(TCScale(src.Read<T>(), scale));

	LOG_TEX<N>();

	++tcIndex;
}

template <typename I, typename T, int N>
void LOADERDECL TexCoord_ReadIndex()
{
	static_assert(!std::numeric_limits<I>::is_signed, "Only unsigned I is sane!");

	auto const index = DataRead<I>();
	auto const data = reinterpret_cast<const T*>(cached_arraybases[ARRAY_TEXCOORD0 + tcIndex]
		+ (index * g_main_cp_state.array_strides[ARRAY_TEXCOORD0 + tcIndex]));
	auto const scale = tcScale[tcIndex];
	DataWriter dst;

	for (int i = 0; i != N; ++i)
		dst.Write(TCScale(Common::FromBigEndian(data[i]), scale));

	LOG_TEX<N>();
	++tcIndex;
}

#if _M_SSE >= 0x401
static const __m128i kMaskSwap16_2 = _mm_set_epi32(0xFFFFFFFFL, 0xFFFFFFFFL, 0xFFFFFFFFL, 0x02030001L);

template <typename I>
void LOADERDECL TexCoord_ReadIndex_Short2_SSE4()
{
	static_assert(!std::numeric_limits<I>::is_signed, "Only unsigned I is sane!");

	// Heavy in ZWW
	auto const index = DataRead<I>();
	const s32 *pData = (const s32*)(cached_arraybases[ARRAY_TEXCOORD0+tcIndex] + (index * g_main_cp_state.array_strides[ARRAY_TEXCOORD0+tcIndex]));
	const __m128i a = _mm_cvtsi32_si128(*pData);
	const __m128i b = _mm_shuffle_epi8(a, kMaskSwap16_2);
	const __m128i c = _mm_cvtepi16_epi32(b);
	const __m128 d = _mm_cvtepi32_ps(c);
	const __m128 e = _mm_load1_ps(&tcScale[tcIndex]);
	const __m128 f = _mm_mul_ps(d, e);
	_mm_storeu_ps((float*)VertexManager::s_pCurBufferPointer, f);
	VertexManager::s_pCurBufferPointer += sizeof(float) * 2;
	LOG_TEX<2>();
	tcIndex++;
}
#endif

#if _M_SSE >= 0x301
static const __m128i kMaskSwap32 = _mm_set_epi32(0xFFFFFFFFL, 0xFFFFFFFFL, 0x04050607L, 0x00010203L);

template <typename I>
void LOADERDECL TexCoord_ReadIndex_Float2_SSSE3()
{
	static_assert(!std::numeric_limits<I>::is_signed, "Only unsigned I is sane!");

	auto const index = DataRead<I>();
	const u32 *pData = (const u32 *)(cached_arraybases[ARRAY_TEXCOORD0+tcIndex] + (index * g_main_cp_state.array_strides[ARRAY_TEXCOORD0+tcIndex]));
	GC_ALIGNED128(const __m128i a = _mm_loadl_epi64((__m128i*)pData));
	GC_ALIGNED128(const __m128i b = _mm_shuffle_epi8(a, kMaskSwap32));
	_mm_storel_epi64((__m128i*)VertexManager::s_pCurBufferPointer, b);
	VertexManager::s_pCurBufferPointer += sizeof(float) * 2;
	LOG_TEX<2>();
	tcIndex++;
}
#endif

static TPipelineFunction tableReadTexCoord[4][8][2] = {
	{
		{nullptr, nullptr,},
		{nullptr, nullptr,},
		{nullptr, nullptr,},
		{nullptr, nullptr,},
		{nullptr, nullptr,},
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

void VertexLoader_TextCoord::Init()
{

#if _M_SSE >= 0x301

	if (cpu_info.bSSSE3)
	{
		tableReadTexCoord[2][4][1] = TexCoord_ReadIndex_Float2_SSSE3<u8>;
		tableReadTexCoord[3][4][1] = TexCoord_ReadIndex_Float2_SSSE3<u16>;
	}

#endif

#if _M_SSE >= 0x401

	if (cpu_info.bSSE4_1)
	{
		tableReadTexCoord[2][3][1] = TexCoord_ReadIndex_Short2_SSE4<u8>;
		tableReadTexCoord[3][3][1] = TexCoord_ReadIndex_Short2_SSE4<u16>;
	}

#endif

}

unsigned int VertexLoader_TextCoord::GetSize(u64 _type, unsigned int _format, unsigned int _elements)
{
	return tableReadTexCoordVertexSize[_type][_format][_elements];
}

TPipelineFunction VertexLoader_TextCoord::GetFunction(u64 _type, unsigned int _format, unsigned int _elements)
{
	return tableReadTexCoord[_type][_format][_elements];
}

TPipelineFunction VertexLoader_TextCoord::GetDummyFunction()
{
	return TexCoord_Read_Dummy;
}
