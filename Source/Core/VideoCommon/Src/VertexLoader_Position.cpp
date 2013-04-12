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
#include "VideoConfig.h"
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

// Reads indirect data
template <typename index_format, typename src_format>
class DataReader
{
public:
	DataReader()
	{
		static_assert(!std::numeric_limits<index_format>::is_signed, "Only unsigned index_format is sane!");
		
		auto const index = DataRead<index_format>();
		m_data = reinterpret_cast<const src_format*>(cached_arraybases[ARRAY_POSITION] + (index * arraystrides[ARRAY_POSITION]));
	}
	
	__forceinline src_format Read()
	{
		return Common::FromBigEndian(*(m_data++));
	}

private:
	const src_format* m_data;
};

// Reads direct data
template <typename src_format>
struct DataReader<void, src_format>
{
public:
	__forceinline static src_format Read()
	{
		return DataRead<src_format>();
	}
};

// void index_format == Read Direct
template <typename dest_format, typename index_format, typename src_format, int N, int frac, bool has_frac, bool apply_frac>
void LOADERDECL Pos_Read()
{
	static_assert(N <= 3, "N > 3 is not sane!");
	static_assert(!has_frac || !apply_frac, "calculating frac on cpu and gpu doesn't make sense");

	DataReader<index_format, src_format> reader;

	for (int i = 0; i < 3; ++i)
	{
		dest_format raw_value = dest_format(i < N ? reader.Read() : 0);
		dest_format divisor = dest_format(1u << frac);
		if(apply_frac && divisor > 1)
		{
			raw_value /= divisor;
		}
		DataWrite<dest_format>(raw_value);
	}

	if (has_frac)
		DataWrite<dest_format>(frac+1);
	LOG_VTX();
}

#if _M_SSE >= 0x301
static const __m128i kMaskSwap32_3 = _mm_set_epi32(0xFFFFFFFFL, 0x08090A0BL, 0x04050607L, 0x00010203L);
static const __m128i kMaskSwap32_2 = _mm_set_epi32(0xFFFFFFFFL, 0xFFFFFFFFL, 0x04050607L, 0x00010203L);

template <typename index_format, bool three>
void LOADERDECL Pos_ReadIndex_Float_SSSE3()
{
	auto const index = DataRead<index_format>();
	if (index < std::numeric_limits<index_format>::max())
	{
		const u32* pData = (const u32 *)(cached_arraybases[ARRAY_POSITION] + (index * arraystrides[ARRAY_POSITION]));
		GC_ALIGNED128(const __m128i a = _mm_loadu_si128((__m128i*)pData));
		GC_ALIGNED128(__m128i b = _mm_shuffle_epi8(a, three ? kMaskSwap32_3 : kMaskSwap32_2));
		_mm_storeu_si128((__m128i*)VertexManager::s_pCurBufferPointer, b);
		VertexManager::s_pCurBufferPointer += sizeof(float) * 3;
		LOG_VTX();
	}
}
#endif

static AttributeLoaderDeclaration table[3][5][2][32];

template <typename dest_format, int formatnr, int N, int frac, typename src_format, bool has_frac, bool apply_frac> void set_table()
{
	// if frac isn't used (float->float), we don't have to generate all template function
	const int frac_factor = (has_frac || apply_frac) ? frac : 0;
	
	table[0][formatnr][N-2][frac].func = Pos_Read<dest_format, void, src_format, N, frac_factor, has_frac, apply_frac>;
	table[1][formatnr][N-2][frac].func = Pos_Read<dest_format, u8, src_format, N, frac_factor, has_frac, apply_frac>;
	table[2][formatnr][N-2][frac].func = Pos_Read<dest_format, u16, src_format, N, frac_factor, has_frac, apply_frac>;
	
	for(u32 i=0; i<3; i++)
	{
		table[i][formatnr][N-2][frac].attr.count = 3+has_frac;
		table[i][formatnr][N-2][frac].attr.type = getVarType<dest_format>();
		table[i][formatnr][N-2][frac].attr.normalized = false;
		table[i][formatnr][N-2][frac].native_size = i;
		table[i][formatnr][N-2][frac].gl_size = sizeof(dest_format)*(3+has_frac);
	}
	
	table[0][formatnr][N-2][frac].native_size = sizeof(src_format)*N;
}

// dx9 doesn't support signed bytes, so we have to convert them to shorts
template <int N, int frac> void set_table_formats()
{
	if(!g_ActiveConfig.backend_info.APIType) // video software
	{
		// videosoftware only supports floats, so convert ...
		set_table<float, 0, N, frac, u8, 0, 1>();
		set_table<float, 1, N, frac, s8, 0, 1>();
		set_table<float, 2, N, frac, u16, 0, 1>();
		set_table<float, 3, N, frac, s16, 0, 1>();
	}
	else
	{
		set_table<u8, 0, N, frac, u8, 1, 0>();
		if(g_ActiveConfig.backend_info.APIType & API_D3D9) {
			// dx9 doesn't support s8, so convert it to s16
			set_table<s16, 1, N, frac, s8, 1, 0>();
		} else {
			set_table<s8, 1, N, frac, s8, 1, 0>();
		}
		set_table<u16, 2, N, frac, u16, 1, 0>();
		set_table<s16, 3, N, frac, s16, 1, 0>();
	}
	set_table<float, 4, N, frac, float, 0, 0>();
	#if _M_SSE >= 0x301
	if (cpu_info.bSSSE3) {
		table[1][4][0][frac].func = Pos_ReadIndex_Float_SSSE3<u8, false>;
		table[1][4][1][frac].func = Pos_ReadIndex_Float_SSSE3<u8, true>;
		table[2][4][0][frac].func = Pos_ReadIndex_Float_SSSE3<u16, false>;
		table[2][4][1][frac].func = Pos_ReadIndex_Float_SSSE3<u16, true>;
	}
	#endif
}

template <int frac> void set_table_elements()
{
	set_table_formats<2, frac>();
	set_table_formats<3, frac>();
}

// template don't support looping, so do it by binary recursion
// next lines are the same as: set_tablehas_frac_16<n>() := {for(int i=n; i<n+32; i++) set_table_elements(i);}
template <int frac> void set_table_frac_1() {set_table_elements<frac>(); set_table_elements<frac+1>();}
template <int frac> void set_table_frac_2() {set_table_frac_1<frac>(); set_table_frac_1<frac+2>();}
template <int frac> void set_table_frac_4() {set_table_frac_2<frac>(); set_table_frac_2<frac+4>();}
template <int frac> void set_table_frac_8() {set_table_frac_4<frac>(); set_table_frac_4<frac+8>();}
template <int frac> void set_table_frac_16() {set_table_frac_8<frac>(); set_table_frac_8<frac+16>();}

void VertexLoader_Position::Init(void)
{
	set_table_frac_16<0>();
}
	
// GetLoader
AttributeLoaderDeclaration VertexLoader_Position::GetLoader(unsigned int _type, unsigned int _format, unsigned int _elements, unsigned int has_frac)
{
	return table[_type-1][_format][_elements][has_frac];
}
