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
#include "VertexLoader_Position.h"
#include "VertexManagerBase.h"
#include "CPUDetect.h"

extern float posScale;
extern TVtxAttr *pVtxAttr;

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

// ==============================================================================
// Direct
// ==============================================================================

template <class T, bool three>
void Pos_ReadDirect()
{
	((float*)VertexManager::s_pCurBufferPointer)[0] = (float)(T)DataRead<T>() * posScale;
	((float*)VertexManager::s_pCurBufferPointer)[1] = (float)(T)DataRead<T>() * posScale;
	if (three)
		((float*)VertexManager::s_pCurBufferPointer)[2] = (float)(T)DataRead<T>() * posScale;
	else
		((float*)VertexManager::s_pCurBufferPointer)[2] = 0.0f;
	LOG_VTX();
	VertexManager::s_pCurBufferPointer += 12;
}

void LOADERDECL Pos_ReadDirect_UByte3()  { Pos_ReadDirect<u8,  true>(); }
void LOADERDECL Pos_ReadDirect_Byte3()   { Pos_ReadDirect<s8,  true>(); }
void LOADERDECL Pos_ReadDirect_UShort3() { Pos_ReadDirect<u16, true>(); }
void LOADERDECL Pos_ReadDirect_Short3()  { Pos_ReadDirect<s16, true>(); }
void LOADERDECL Pos_ReadDirect_UByte2()  { Pos_ReadDirect<u8,  false>(); }
void LOADERDECL Pos_ReadDirect_Byte2()   { Pos_ReadDirect<s8,  false>(); }
void LOADERDECL Pos_ReadDirect_UShort2() { Pos_ReadDirect<u16, false>(); }
void LOADERDECL Pos_ReadDirect_Short2()  { Pos_ReadDirect<s16, false>(); }

void LOADERDECL Pos_ReadDirect_Float3()
{
	// No need to use floating point here.
	((u32 *)VertexManager::s_pCurBufferPointer)[0] = DataReadU32(); 
	((u32 *)VertexManager::s_pCurBufferPointer)[1] = DataReadU32();
	((u32 *)VertexManager::s_pCurBufferPointer)[2] = DataReadU32();
	LOG_VTX();
	VertexManager::s_pCurBufferPointer += 12;
}

void LOADERDECL Pos_ReadDirect_Float2()
{
	// No need to use floating point here.
	((u32 *)VertexManager::s_pCurBufferPointer)[0] = DataReadU32(); 
	((u32 *)VertexManager::s_pCurBufferPointer)[1] = DataReadU32();
	((u32 *)VertexManager::s_pCurBufferPointer)[2] = 0;
	LOG_VTX();
	VertexManager::s_pCurBufferPointer += 12;
}


template<class T, bool three,int MaxSize>
inline void Pos_ReadIndex_Byte(int Index)
{
	if(Index < MaxSize)
	{
		const u8* pData = cached_arraybases[ARRAY_POSITION] + ((u32)Index * arraystrides[ARRAY_POSITION]);
		((float*)VertexManager::s_pCurBufferPointer)[0] = ((float)(T)(pData[0])) * posScale;
		((float*)VertexManager::s_pCurBufferPointer)[1] = ((float)(T)(pData[1])) * posScale;
		if (three)
			((float*)VertexManager::s_pCurBufferPointer)[2] = ((float)(T)(pData[2])) * posScale;
		else
			((float*)VertexManager::s_pCurBufferPointer)[2] = 0.0f;
		LOG_VTX();
		VertexManager::s_pCurBufferPointer += 12;
	}
}

template<class T, bool three,int MaxSize>
inline void Pos_ReadIndex_Short(int Index)
{
	if(Index < MaxSize)
	{
		const u16* pData = (const u16 *)(cached_arraybases[ARRAY_POSITION] + ((u32)Index * arraystrides[ARRAY_POSITION]));
		((float*)VertexManager::s_pCurBufferPointer)[0] = ((float)(T)Common::swap16(pData[0])) * posScale;
		((float*)VertexManager::s_pCurBufferPointer)[1] = ((float)(T)Common::swap16(pData[1])) * posScale;
		if (three)
			((float*)VertexManager::s_pCurBufferPointer)[2] = ((float)(T)Common::swap16(pData[2])) * posScale;
		else
			((float*)VertexManager::s_pCurBufferPointer)[2] = 0.0f;
		LOG_VTX();
		VertexManager::s_pCurBufferPointer += 12;
	}
}

template<bool three,int MaxSize>
void Pos_ReadIndex_Float(int Index)
{
	if(Index < MaxSize)
	{
		const u32* pData = (const u32 *)(cached_arraybases[ARRAY_POSITION] + (Index * arraystrides[ARRAY_POSITION]));
		((u32*)VertexManager::s_pCurBufferPointer)[0] = Common::swap32(pData[0]);
		((u32*)VertexManager::s_pCurBufferPointer)[1] = Common::swap32(pData[1]);
		if (three)
			((u32*)VertexManager::s_pCurBufferPointer)[2] = Common::swap32(pData[2]);
		else
			((float*)VertexManager::s_pCurBufferPointer)[2] = 0.0f;
		LOG_VTX();
		VertexManager::s_pCurBufferPointer += 12;
	}
}

#if _M_SSE >= 0x301
static const __m128i kMaskSwap32_3 = _mm_set_epi32(0xFFFFFFFFL, 0x08090A0BL, 0x04050607L, 0x00010203L);
static const __m128i kMaskSwap32_2 = _mm_set_epi32(0xFFFFFFFFL, 0xFFFFFFFFL, 0x04050607L, 0x00010203L);

template<bool three,int MaxSize>
void Pos_ReadIndex_Float_SSSE3(int Index)
{
	if(Index < MaxSize)
	{
		const u32* pData = (const u32 *)(cached_arraybases[ARRAY_POSITION] + (Index * arraystrides[ARRAY_POSITION]));
		GC_ALIGNED128(const __m128i a = _mm_loadu_si128((__m128i*)pData));
		GC_ALIGNED128(__m128i b = _mm_shuffle_epi8(a, three ? kMaskSwap32_3 : kMaskSwap32_2));
		_mm_storeu_si128((__m128i*)VertexManager::s_pCurBufferPointer, b);
		LOG_VTX();
		VertexManager::s_pCurBufferPointer += 12;
	}
}
#endif

// Explicitly instantiate these functions to decrease the possibility of
// symbol binding problems when (only) calling them from JIT compiled code.
template void Pos_ReadDirect<u8,  true>();
template void Pos_ReadDirect<s8,  true>();
template void Pos_ReadDirect<u16, true>();
template void Pos_ReadDirect<s16, true>();
template void Pos_ReadDirect<u8,  false>();
template void Pos_ReadDirect<s8,  false>();
template void Pos_ReadDirect<u16, false>();
template void Pos_ReadDirect<s16, false>();
template void Pos_ReadIndex_Byte<u8, true, 255>(int Index);
template void Pos_ReadIndex_Byte<s8, true, 255>(int Index);
template void Pos_ReadIndex_Short<u16, true, 255>(int Index);
template void Pos_ReadIndex_Short<s16, true, 255>(int Index);
template void Pos_ReadIndex_Float<true, 255>(int Index);
template void Pos_ReadIndex_Byte<u8, false, 255>(int Index);
template void Pos_ReadIndex_Byte<s8, false, 255>(int Index);
template void Pos_ReadIndex_Short<u16, false, 255>(int Index);
template void Pos_ReadIndex_Short<s16, false, 255>(int Index);
template void Pos_ReadIndex_Float<false, 255>(int Index);
template void Pos_ReadIndex_Byte<u8, true, 65535>(int Index);
template void Pos_ReadIndex_Byte<s8, true, 65535>(int Index);
template void Pos_ReadIndex_Short<u16, true, 65535>(int Index);
template void Pos_ReadIndex_Short<s16, true, 65535>(int Index);
template void Pos_ReadIndex_Float<true, 65535>(int Index);
template void Pos_ReadIndex_Byte<u8, false, 65535>(int Index);
template void Pos_ReadIndex_Byte<s8, false, 65535>(int Index);
template void Pos_ReadIndex_Short<u16, false, 65535>(int Index);
template void Pos_ReadIndex_Short<s16, false, 65535>(int Index);
template void Pos_ReadIndex_Float<false, 65535>(int Index);

// ==============================================================================
// Index 8
// ==============================================================================
void LOADERDECL Pos_ReadIndex8_UByte3()  {Pos_ReadIndex_Byte<u8,   true, 255> (DataReadU8());}
void LOADERDECL Pos_ReadIndex8_Byte3()   {Pos_ReadIndex_Byte<s8,   true, 255> (DataReadU8());}
void LOADERDECL Pos_ReadIndex8_UShort3() {Pos_ReadIndex_Short<u16, true, 255> (DataReadU8());}
void LOADERDECL Pos_ReadIndex8_Short3()  {Pos_ReadIndex_Short<s16, true, 255> (DataReadU8());}
void LOADERDECL Pos_ReadIndex8_Float3()  {Pos_ReadIndex_Float<true, 255>      (DataReadU8());}
void LOADERDECL Pos_ReadIndex8_UByte2()  {Pos_ReadIndex_Byte<u8,   false, 255>(DataReadU8());}
void LOADERDECL Pos_ReadIndex8_Byte2()   {Pos_ReadIndex_Byte<s8,   false, 255>(DataReadU8());}
void LOADERDECL Pos_ReadIndex8_UShort2() {Pos_ReadIndex_Short<u16, false, 255>(DataReadU8());}
void LOADERDECL Pos_ReadIndex8_Short2()  {Pos_ReadIndex_Short<s16, false, 255>(DataReadU8());}
void LOADERDECL Pos_ReadIndex8_Float2()  {Pos_ReadIndex_Float<false, 255>     (DataReadU8());}

// ==============================================================================
// Index 16
// ==============================================================================
void LOADERDECL Pos_ReadIndex16_UByte3()  {Pos_ReadIndex_Byte<u8,   true, 65535> (DataReadU16());}
void LOADERDECL Pos_ReadIndex16_Byte3()   {Pos_ReadIndex_Byte<s8,   true, 65535> (DataReadU16());}
void LOADERDECL Pos_ReadIndex16_UShort3() {Pos_ReadIndex_Short<u16, true, 65535> (DataReadU16());}
void LOADERDECL Pos_ReadIndex16_Short3()  {Pos_ReadIndex_Short<s16, true, 65535> (DataReadU16());}
void LOADERDECL Pos_ReadIndex16_Float3()  {Pos_ReadIndex_Float<true, 65535>      (DataReadU16());}
void LOADERDECL Pos_ReadIndex16_UByte2()  {Pos_ReadIndex_Byte<u8,   false, 65535>(DataReadU16());}
void LOADERDECL Pos_ReadIndex16_Byte2()   {Pos_ReadIndex_Byte<s8,   false, 65535>(DataReadU16());}
void LOADERDECL Pos_ReadIndex16_UShort2() {Pos_ReadIndex_Short<u16, false, 65535>(DataReadU16());}
void LOADERDECL Pos_ReadIndex16_Short2()  {Pos_ReadIndex_Short<s16, false, 65535>(DataReadU16());}
void LOADERDECL Pos_ReadIndex16_Float2()  {Pos_ReadIndex_Float<false, 65535>     (DataReadU16());}

#if _M_SSE >= 0x301
void LOADERDECL Pos_ReadIndex8_Float3_SSSE3()  {Pos_ReadIndex_Float_SSSE3<true, 255>      (DataReadU8());}
void LOADERDECL Pos_ReadIndex8_Float2_SSSE3()  {Pos_ReadIndex_Float_SSSE3<false, 255>     (DataReadU8());}
void LOADERDECL Pos_ReadIndex16_Float3_SSSE3()  {Pos_ReadIndex_Float_SSSE3<true, 65535>      (DataReadU16());}
void LOADERDECL Pos_ReadIndex16_Float2_SSSE3()  {Pos_ReadIndex_Float_SSSE3<false, 65535>     (DataReadU16());}
#endif

static TPipelineFunction tableReadPosition[4][8][2] = {
	{
		{NULL, NULL,},
		{NULL, NULL,},
		{NULL, NULL,},
		{NULL, NULL,},
		{NULL, NULL,},
	},
	{
		{Pos_ReadDirect_UByte2, Pos_ReadDirect_UByte3,},
		{Pos_ReadDirect_Byte2, Pos_ReadDirect_Byte3,},
		{Pos_ReadDirect_UShort2, Pos_ReadDirect_UShort3,},
		{Pos_ReadDirect_Short2, Pos_ReadDirect_Short3,},
		{Pos_ReadDirect_Float2, Pos_ReadDirect_Float3,},
	},
	{
		{Pos_ReadIndex8_UByte2, Pos_ReadIndex8_UByte3,},
		{Pos_ReadIndex8_Byte2, Pos_ReadIndex8_Byte3,},
		{Pos_ReadIndex8_UShort2, Pos_ReadIndex8_UShort3,},
		{Pos_ReadIndex8_Short2, Pos_ReadIndex8_Short3,},
		{Pos_ReadIndex8_Float2, Pos_ReadIndex8_Float3,},
	},
	{
		{Pos_ReadIndex16_UByte2, Pos_ReadIndex16_UByte3,},
		{Pos_ReadIndex16_Byte2, Pos_ReadIndex16_Byte3,},
		{Pos_ReadIndex16_UShort2, Pos_ReadIndex16_UShort3,},
		{Pos_ReadIndex16_Short2, Pos_ReadIndex16_Short3,},
		{Pos_ReadIndex16_Float2, Pos_ReadIndex16_Float3,},
	},
};

static int tableReadPositionVertexSize[4][8][2] = {
	{
		{0, 0,},
		{0, 0,},
		{0, 0,},
		{0, 0,},
		{0, 0,},
	},
	{
		{2, 3,},
		{2, 3,},
		{4, 6,},
		{4, 6,},
		{8, 12,},
	},
	{
		{1, 1,},
		{1, 1,},
		{1, 1,},
		{1, 1,},
		{1, 1,},
	},
	{
		{2, 2,},
		{2, 2,},
		{2, 2,},
		{2, 2,},
		{2, 2,},
	},
};


void VertexLoader_Position::Init(void) {

#if _M_SSE >= 0x301

	if (cpu_info.bSSSE3) {
		tableReadPosition[2][4][0] = Pos_ReadIndex8_Float2_SSSE3;
		tableReadPosition[2][4][1] = Pos_ReadIndex8_Float3_SSSE3;
		tableReadPosition[3][4][0] = Pos_ReadIndex16_Float2_SSSE3;
		tableReadPosition[3][4][1] = Pos_ReadIndex16_Float3_SSSE3;
	}

#endif

}

unsigned int VertexLoader_Position::GetSize(unsigned int _type, unsigned int _format, unsigned int _elements) {
	return tableReadPositionVertexSize[_type][_format][_elements];
}

TPipelineFunction VertexLoader_Position::GetFunction(unsigned int _type, unsigned int _format, unsigned int _elements) {
	return tableReadPosition[_type][_format][_elements];
}
