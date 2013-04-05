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

template <typename F, typename T, int N, int frac, bool has_frac>
void LOADERDECL Pos_ReadDirect()
{
	static_assert(N <= 3, "N > 3 is not sane!");

	for (int i = 0; i < 3; ++i)
		DataWrite<F>(i<N ? DataRead<T>() : 0);
	
	if(has_frac)
		DataWrite<F>(frac+1);
	LOG_VTX();
}

template <typename F, typename I, typename T, int N, int frac, bool has_frac>
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

		if(has_frac)
			DataWrite<F>(frac+1);
		LOG_VTX();
	}
}

static TPipelineFunction tableReadPosition[3][5][2][32];
static int tableReadPositionVertexSize[3][5][2][32];
static VarType tableReadPositionGLType[3][5][2][32];
static int tableReadPositionGLSize[3][5][2][32];
static int tableReadPositionCount[3][5][2][32];

template <typename F, int formatnr, int formatsize, int N, int frac, int fracnr, VarType gltype, typename T, bool has_frac> void set_table()
{
	for(u32 i=0; i<3; i++)
	{
		tableReadPositionVertexSize[i][formatnr][N-2][fracnr] = i;
		tableReadPositionGLType[i][formatnr][N-2][fracnr] = gltype;
		tableReadPositionGLSize[i][formatnr][N-2][fracnr] = sizeof(F)*(3+has_frac);
		tableReadPositionCount[i][formatnr][N-2][fracnr] = 3+has_frac;
	}
	tableReadPosition[0][formatnr][N-2][fracnr] = Pos_ReadDirect<F, T, N, frac, has_frac>;
	tableReadPosition[1][formatnr][N-2][fracnr] = Pos_ReadIndex<F, u8, T, N, frac, has_frac>;
	tableReadPosition[2][formatnr][N-2][fracnr] = Pos_ReadIndex<F, u16, T, N, frac, has_frac>;
	
	tableReadPositionVertexSize[0][formatnr][N-2][fracnr] = formatsize*N;
}

// dx9 doesn't support signed bytes, so we have to convert them to shorts
template <int N, int frac> void set_table_formats()
{
	set_table<u8, 0, 1, N, frac, frac, VAR_UNSIGNED_BYTE, u8, 1>();
	if(g_ActiveConfig.backend_info.APIType & API_D3D9) {
		set_table<s8, 1, 1, N, frac, frac, VAR_SHORT, s16, 1>();
	} else {
		set_table<s8, 1, 1, N, frac, frac, VAR_BYTE, s8, 1>();
	}
	set_table<u16, 2, 2, N, frac, frac, VAR_UNSIGNED_SHORT, u16, 1>();
	set_table<s16, 3, 2, N, frac, frac, VAR_SHORT, s16, 1>();
	set_table<float, 4, 4, N, 0, frac, VAR_FLOAT, float, 0>();
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

void VertexLoader_Position::Init(void) {
	memset(tableReadPosition, 0, sizeof(tableReadPosition));
	memset(tableReadPositionVertexSize, 0, sizeof(tableReadPositionVertexSize));

	set_table_frac_16<0>();
}

unsigned int VertexLoader_Position::GetSize(unsigned int _type, unsigned int _format, unsigned int _elements, unsigned int has_frac) {
	return tableReadPositionVertexSize[_type-1][_format][_elements][has_frac];
}

TPipelineFunction VertexLoader_Position::GetFunction(unsigned int _type, unsigned int _format, unsigned int _elements, unsigned int has_frac) {
	return tableReadPosition[_type-1][_format][_elements][has_frac];
}

int VertexLoader_Position::GetGLType(unsigned int _type, unsigned int _format, unsigned int _elements, unsigned int has_frac) {
	return tableReadPositionGLType[_type-1][_format][_elements][has_frac];
}

int VertexLoader_Position::GetGLSize(unsigned int _type, unsigned int _format, unsigned int _elements, unsigned int has_frac) {
	return tableReadPositionGLSize[_type-1][_format][_elements][has_frac];
}

int VertexLoader_Position::GetCount(unsigned int _type, unsigned int _format, unsigned int _elements, unsigned int has_frac) {
	return tableReadPositionCount[_type-1][_format][_elements][has_frac];
}
