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

#include <limits>

#include "Common.h"
#include "VideoCommon.h"
#include "VertexLoader.h"
#include "VertexLoader_Position.h"
#include "VertexManagerBase.h"
#include "CPUDetect.h"

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

template <typename F, typename T, int N, int frac>
void LOADERDECL Pos_ReadDirect()
{
	static_assert(N <= 3, "N > 3 is not sane!");

	for (int i = 0; i < 3; ++i)
		DataWrite<F>(i<N ? DataRead<T>() : 0);

	DataWrite<F>(frac);
	LOG_VTX();
}

template <typename F, typename I, typename T, int N, int frac>
void LOADERDECL Pos_ReadIndex()
{
	static_assert(!std::numeric_limits<I>::is_signed, "Only unsigned I is sane!");
	static_assert(N <= 3, "N > 3 is not sane!");

	auto const index = DataRead<I>();
	if (index < std::numeric_limits<I>::max())
	{
		auto const data = reinterpret_cast<const T*>(cached_arraybases[ARRAY_POSITION] + (index * arraystrides[ARRAY_POSITION]));

		for (int i = 0; i < 3; ++i)
			DataWrite<F>(i<N ? Common::FromBigEndian(data[i]) : 0);

		DataWrite<F>(frac);
		LOG_VTX();
	}
}

#if _M_SSE >= 0x301
static const __m128i kMaskSwap32_3 = _mm_set_epi32(0xFFFFFFFFL, 0x08090A0BL, 0x04050607L, 0x00010203L);
static const __m128i kMaskSwap32_2 = _mm_set_epi32(0xFFFFFFFFL, 0xFFFFFFFFL, 0x04050607L, 0x00010203L);

template <typename I, bool three>
void LOADERDECL Pos_ReadIndex_Float_SSSE3()
{
	auto const index = DataRead<I>();
	if (index < std::numeric_limits<I>::max())
	{
		const u32* pData = (const u32 *)(cached_arraybases[ARRAY_POSITION] + (index * arraystrides[ARRAY_POSITION]));
		GC_ALIGNED128(const __m128i a = _mm_loadu_si128((__m128i*)pData));
		GC_ALIGNED128(__m128i b = _mm_shuffle_epi8(a, three ? kMaskSwap32_3 : kMaskSwap32_2));
		_mm_storeu_si128((__m128i*)VertexManager::s_pCurBufferPointer, b);
		VertexManager::s_pCurBufferPointer += sizeof(float) * 3;
		DataWrite<float>(0);
		LOG_VTX();
	}
}
#endif

static TPipelineFunction tableReadPosition[4][5][2][32];
static int tableReadPositionVertexSize[4][5][2][32];

void VertexLoader_Position::Init(void) {
	memset(tableReadPosition, 0, sizeof(tableReadPosition));
	memset(tableReadPositionVertexSize, 0, sizeof(tableReadPositionVertexSize));

	// c++ templates can't be created in usual c++ loops, so we have to this per preprocessor
	// ugly as hell, but all other ways are even uglier
	
	#define set_table(format, formatnr, formatsize, elements, frac, fracnr) \
		tableReadPosition[1][formatnr][elements-2][fracnr] = Pos_ReadDirect<float, format, elements, frac>; \
		tableReadPosition[2][formatnr][elements-2][fracnr] = Pos_ReadIndex<float, u8, format, elements, frac>; \
		tableReadPosition[3][formatnr][elements-2][fracnr] = Pos_ReadIndex<float, u16, format, elements, frac>; \
		tableReadPositionVertexSize[1][formatnr][elements-2][fracnr] = formatsize*elements; \
		tableReadPositionVertexSize[2][formatnr][elements-2][fracnr] = 1; \
		tableReadPositionVertexSize[3][formatnr][elements-2][fracnr] = 2;

	#define set_table_formats(elements, frac) \
		set_table(u8, 0, 1, elements, frac, frac); \
		set_table(s8, 1, 1, elements, frac, frac); \
		set_table(u16, 2, 2, elements, frac, frac); \
		set_table(s16, 3, 2, elements, frac, frac); \
		set_table(float, 4, 4, elements, 0, frac);
		
	#define set_table_elements(frac) \
		set_table_formats(2, frac); \
		set_table_formats(3, frac);
	
	// preprocessor don't support looping, so do it by binary recursion
	// next lines are the same as: set_table_frac_16(n) := {for(int i=n; i<n+32; i++) set_table_elements(i);}
	#define set_table_frac_1(frac) set_table_elements(frac); set_table_elements(frac+1);
	#define set_table_frac_2(frac) set_table_frac_1(frac); set_table_frac_1(frac+2);
	#define set_table_frac_4(frac) set_table_frac_2(frac); set_table_frac_2(frac+4);
	#define set_table_frac_8(frac) set_table_frac_4(frac); set_table_frac_4(frac+8);
	#define set_table_frac_16(frac) set_table_frac_8(frac); set_table_frac_8(frac+16);

	set_table_frac_16(0);

#if _M_SSE >= 0x301

	if (cpu_info.bSSSE3) {
		for(int frac=0; frac<32; frac++)
		{
			tableReadPosition[2][4][0][frac] = Pos_ReadIndex_Float_SSSE3<u8, false>;
			tableReadPosition[2][4][1][frac] = Pos_ReadIndex_Float_SSSE3<u8, true>;
			tableReadPosition[3][4][0][frac] = Pos_ReadIndex_Float_SSSE3<u16, false>;
			tableReadPosition[3][4][1][frac] = Pos_ReadIndex_Float_SSSE3<u16, true>;
		}
	}

#endif

}

unsigned int VertexLoader_Position::GetSize(unsigned int _type, unsigned int _format, unsigned int _elements, unsigned int _frac) {
	return tableReadPositionVertexSize[_type][_format][_elements][_frac];
}

TPipelineFunction VertexLoader_Position::GetFunction(unsigned int _type, unsigned int _format, unsigned int _elements, unsigned int _frac) {
	return tableReadPosition[_type][_format][_elements][_frac];
}
