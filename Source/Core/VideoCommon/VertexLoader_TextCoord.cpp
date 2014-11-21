// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <type_traits>

#include "Common/CommonTypes.h"
#include "Common/CPUDetect.h"

#include "VideoCommon/VertexLoader.h"
#include "VideoCommon/VertexLoader_TextCoord.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VideoCommon.h"

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
	auto const scale = tcScale[tcIndex][0];
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
	static_assert(std::is_unsigned<I>::value, "Only unsigned I is sane!");

	auto const index = DataRead<I>();
	auto const data = reinterpret_cast<const T*>(cached_arraybases[ARRAY_TEXCOORD0 + tcIndex]
	                + (index * g_main_cp_state.array_strides[ARRAY_TEXCOORD0 + tcIndex]));
	auto const scale = tcScale[tcIndex][0];
	DataWriter dst;

	for (int i = 0; i != N; ++i)
		dst.Write(TCScale(Common::FromBigEndian(data[i]), scale));

	LOG_TEX<N>();
	++tcIndex;
}

#if _M_SSE >= 0x301
template <typename T>
void LOADERDECL TexCoord_ReadDirect2_SSSE3()
{
	const T* pData = reinterpret_cast<const T*>(DataGetPosition());
	__m128 scale = _mm_castsi128_ps(_mm_loadl_epi64((__m128i*)tcScale[tcIndex]));
	Vertex_Read_SSSE3<T, false, false>(pData, scale);
	DataSkip<2 * sizeof(T)>();
	LOG_TEX<2>();
	tcIndex++;
}

template <typename I, typename T>
void LOADERDECL TexCoord_ReadIndex2_SSSE3()
{
	static_assert(std::is_unsigned<I>::value, "Only unsigned I is sane!");

	auto const index = DataRead<I>();
	const T* pData = (const T*)(cached_arraybases[ARRAY_TEXCOORD0 + tcIndex] + (index * g_main_cp_state.array_strides[ARRAY_TEXCOORD0 + tcIndex]));
	__m128 scale = _mm_castsi128_ps(_mm_loadl_epi64((__m128i*)tcScale[tcIndex]));
	Vertex_Read_SSSE3<T, false, false>(pData, scale);
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
		tableReadTexCoord[1][0][1] = TexCoord_ReadDirect2_SSSE3<u8>;
		tableReadTexCoord[1][1][1] = TexCoord_ReadDirect2_SSSE3<s8>;
		tableReadTexCoord[1][2][1] = TexCoord_ReadDirect2_SSSE3<u16>;
		tableReadTexCoord[1][3][1] = TexCoord_ReadDirect2_SSSE3<s16>;
		tableReadTexCoord[1][4][1] = TexCoord_ReadDirect2_SSSE3<float>;
		tableReadTexCoord[2][0][1] = TexCoord_ReadIndex2_SSSE3<u8, u8>;
		tableReadTexCoord[3][0][1] = TexCoord_ReadIndex2_SSSE3<u16, u8>;
		tableReadTexCoord[2][1][1] = TexCoord_ReadIndex2_SSSE3<u8, s8>;
		tableReadTexCoord[3][1][1] = TexCoord_ReadIndex2_SSSE3<u16, s8>;
		tableReadTexCoord[2][2][1] = TexCoord_ReadIndex2_SSSE3<u8, u16>;
		tableReadTexCoord[3][2][1] = TexCoord_ReadIndex2_SSSE3<u16, u16>;
		tableReadTexCoord[2][3][1] = TexCoord_ReadIndex2_SSSE3<u8, s16>;
		tableReadTexCoord[3][3][1] = TexCoord_ReadIndex2_SSSE3<u16, s16>;
		tableReadTexCoord[2][4][1] = TexCoord_ReadIndex2_SSSE3<u8, float>;
		tableReadTexCoord[3][4][1] = TexCoord_ReadIndex2_SSSE3<u16, float>;
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
