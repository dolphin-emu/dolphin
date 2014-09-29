// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <limits>

#include "Common/CommonTypes.h"
#include "Common/CPUDetect.h"

#include "VideoCommon/VertexLoader.h"
#include "VideoCommon/VertexLoader_Position.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VideoCommon.h"

// Thoughts on the implementation of a vertex loader compiler.
// s_pCurBufferPointer should definitely be in a register.
// Could load the position scale factor in XMM7, for example.

// The pointer inside DataReadU8 in another.
// Let's check out Pos_ReadDirect_UByte(). For Byte, replace MOVZX with MOVSX.

/*
MOVZX(32, R(EAX), MOffset(ESI, 0));
MOVZX(32, R(EBX), MOffset(ESI, 1));
MOVZX(32, R(ECX), MOffset(ESI, 2));
MOVD(XMM0, R(EAX));
MOVD(XMM1, R(EBX));
MOVD(XMM2, R(ECX));
CVTDQ2PS(XMM0, XMM0);
CVTDQ2PS(XMM1, XMM1);
CVTDQ2PS(XMM2, XMM2);
MULSS(XMM0, XMM7);
MULSS(XMM1, XMM7);
MULSS(XMM2, XMM7);
MOVSS(MOffset(EDI, 0), XMM0);
MOVSS(MOffset(EDI, 4), XMM1);
MOVSS(MOffset(EDI, 8), XMM2);

Alternatively, lookup table:
MOVZX(32, R(EAX), MOffset(ESI, 0));
MOVZX(32, R(EBX), MOffset(ESI, 1));
MOVZX(32, R(ECX), MOffset(ESI, 2));
MOV(32, R(EAX), MComplex(LUTREG, EAX, 4));
MOV(32, R(EBX), MComplex(LUTREG, EBX, 4));
MOV(32, R(ECX), MComplex(LUTREG, ECX, 4));
MOV(MOffset(EDI, 0), XMM0);
MOV(MOffset(EDI, 4), XMM1);
MOV(MOffset(EDI, 8), XMM2);

SSE4:
PINSRB(XMM0, MOffset(ESI, 0), 0);
PINSRB(XMM0, MOffset(ESI, 1), 4);
PINSRB(XMM0, MOffset(ESI, 2), 8);
CVTDQ2PS(XMM0, XMM0);
<two unpacks here to sign extend>
MULPS(XMM0, XMM7);
MOVUPS(MOffset(EDI, 0), XMM0);

									 */

template <typename T>
float PosScale(T val, float scale)
{
	return val * scale;
}

template <>
float PosScale(float val, float scale)
{
	return val;
}

template <typename T, int N>
void LOADERDECL Pos_ReadDirect()
{
	static_assert(N <= 3, "N > 3 is not sane!");
	auto const scale = posScale;
	DataWriter dst;
	DataReader src;

	for (int i = 0; i < 3; ++i)
		dst.Write(i<N ? PosScale(src.Read<T>(), scale) : 0.f);

	LOG_VTX();
}

template <typename I, typename T, int N>
void LOADERDECL Pos_ReadIndex()
{
	static_assert(!std::numeric_limits<I>::is_signed, "Only unsigned I is sane!");
	static_assert(N <= 3, "N > 3 is not sane!");

	auto const index = DataRead<I>();
	auto const data = reinterpret_cast<const T*>(cached_arraybases[ARRAY_POSITION] + (index * g_main_cp_state.array_strides[ARRAY_POSITION]));
	auto const scale = posScale;
	DataWriter dst;

	for (int i = 0; i < 3; ++i)
		dst.Write(i<N ? PosScale(Common::FromBigEndian(data[i]), scale) : 0.f);

	LOG_VTX();
}

#if _M_SSE >= 0x301
static const __m128i kMaskSwap32_3 = _mm_set_epi32(0xFFFFFFFFL, 0x08090A0BL, 0x04050607L, 0x00010203L);
static const __m128i kMaskSwap32_2 = _mm_set_epi32(0xFFFFFFFFL, 0xFFFFFFFFL, 0x04050607L, 0x00010203L);

template <typename I, bool three>
void LOADERDECL Pos_ReadIndex_Float_SSSE3()
{
	auto const index = DataRead<I>();
	const u32* pData = (const u32 *)(cached_arraybases[ARRAY_POSITION] + (index * g_main_cp_state.array_strides[ARRAY_POSITION]));
	GC_ALIGNED128(const __m128i a = _mm_loadu_si128((__m128i*)pData));
	GC_ALIGNED128(__m128i b = _mm_shuffle_epi8(a, three ? kMaskSwap32_3 : kMaskSwap32_2));
	_mm_storeu_si128((__m128i*)VertexManager::s_pCurBufferPointer, b);
	VertexManager::s_pCurBufferPointer += sizeof(float) * 3;
	LOG_VTX();
}
#endif

static TPipelineFunction tableReadPosition[4][8][2] = {
	{
		{nullptr, nullptr,},
		{nullptr, nullptr,},
		{nullptr, nullptr,},
		{nullptr, nullptr,},
		{nullptr, nullptr,},
	},
	{
		{Pos_ReadDirect<u8, 2>, Pos_ReadDirect<u8, 3>,},
		{Pos_ReadDirect<s8, 2>, Pos_ReadDirect<s8, 3>,},
		{Pos_ReadDirect<u16, 2>, Pos_ReadDirect<u16, 3>,},
		{Pos_ReadDirect<s16, 2>, Pos_ReadDirect<s16, 3>,},
		{Pos_ReadDirect<float, 2>, Pos_ReadDirect<float, 3>,},
	},
	{
		{Pos_ReadIndex<u8, u8, 2>, Pos_ReadIndex<u8, u8, 3>,},
		{Pos_ReadIndex<u8, s8, 2>, Pos_ReadIndex<u8, s8, 3>,},
		{Pos_ReadIndex<u8, u16, 2>, Pos_ReadIndex<u8, u16, 3>,},
		{Pos_ReadIndex<u8, s16, 2>, Pos_ReadIndex<u8, s16, 3>,},
		{Pos_ReadIndex<u8, float, 2>, Pos_ReadIndex<u8, float, 3>,},
	},
	{
		{Pos_ReadIndex<u16, u8, 2>, Pos_ReadIndex<u16, u8, 3>,},
		{Pos_ReadIndex<u16, s8, 2>, Pos_ReadIndex<u16, s8, 3>,},
		{Pos_ReadIndex<u16, u16, 2>, Pos_ReadIndex<u16, u16, 3>,},
		{Pos_ReadIndex<u16, s16, 2>, Pos_ReadIndex<u16, s16, 3>,},
		{Pos_ReadIndex<u16, float, 2>, Pos_ReadIndex<u16, float, 3>,},
	},
};

static int tableReadPositionVertexSize[4][8][2] = {
	{
		{0, 0,}, {0, 0,}, {0, 0,}, {0, 0,}, {0, 0,},
	},
	{
		{2, 3,}, {2, 3,}, {4, 6,}, {4, 6,}, {8, 12,},
	},
	{
		{1, 1,}, {1, 1,}, {1, 1,}, {1, 1,}, {1, 1,},
	},
	{
		{2, 2,}, {2, 2,}, {2, 2,}, {2, 2,}, {2, 2,},
	},
};


void VertexLoader_Position::Init()
{

#if _M_SSE >= 0x301

	if (cpu_info.bSSSE3)
	{
		tableReadPosition[2][4][0] = Pos_ReadIndex_Float_SSSE3<u8, false>;
		tableReadPosition[2][4][1] = Pos_ReadIndex_Float_SSSE3<u8, true>;
		tableReadPosition[3][4][0] = Pos_ReadIndex_Float_SSSE3<u16, false>;
		tableReadPosition[3][4][1] = Pos_ReadIndex_Float_SSSE3<u16, true>;
	}

#endif

}

unsigned int VertexLoader_Position::GetSize(u64 _type, unsigned int _format, unsigned int _elements)
{
	return tableReadPositionVertexSize[_type][_format][_elements];
}

TPipelineFunction VertexLoader_Position::GetFunction(u64 _type, unsigned int _format, unsigned int _elements)
{
	return tableReadPosition[_type][_format][_elements];
}
