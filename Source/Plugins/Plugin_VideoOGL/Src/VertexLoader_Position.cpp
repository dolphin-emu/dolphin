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

#ifndef VERTEXLOADER_POSITION_H
#define VERTEXLOADER_POSITION_H

#include "Globals.h"
#include "VertexLoader.h"
#include "VertexManager.h"
#include "VertexLoader_Position.h"

extern float posScale;
extern TVtxAttr *pVtxAttr;

#define LOG_VTX() PRIM_LOG("vtx: %f %f %f, ", ((float*)VertexManager::s_pCurBufferPointer)[0], ((float*)VertexManager::s_pCurBufferPointer)[1], ((float*)VertexManager::s_pCurBufferPointer)[2]);

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
void LOADERDECL Pos_ReadDirect_UByte()
{
	((float*)VertexManager::s_pCurBufferPointer)[0] = (float)DataReadU8() * posScale;
	((float*)VertexManager::s_pCurBufferPointer)[1] = (float)DataReadU8() * posScale;
	if (pVtxAttr->PosElements)
		((float*)VertexManager::s_pCurBufferPointer)[2] = (float)DataReadU8() * posScale;
	else
		((float*)VertexManager::s_pCurBufferPointer)[2] = 1.0f;
	LOG_VTX();
	VertexManager::s_pCurBufferPointer += 12;
}

void LOADERDECL Pos_ReadDirect_Byte()
{	
	((float*)VertexManager::s_pCurBufferPointer)[0] = (float)(s8)DataReadU8() * posScale;
	((float*)VertexManager::s_pCurBufferPointer)[1] = (float)(s8)DataReadU8() * posScale;
	if (pVtxAttr->PosElements)
		((float*)VertexManager::s_pCurBufferPointer)[2] = (float)(s8)DataReadU8() * posScale;
	else
		((float*)VertexManager::s_pCurBufferPointer)[2] = 1.0;
	LOG_VTX();
	VertexManager::s_pCurBufferPointer += 12;
}

void LOADERDECL Pos_ReadDirect_UShort()
{
	((float*)VertexManager::s_pCurBufferPointer)[0] = (float)DataReadU16() * posScale;
	((float*)VertexManager::s_pCurBufferPointer)[1] = (float)DataReadU16() * posScale;
	if (pVtxAttr->PosElements)
		((float*)VertexManager::s_pCurBufferPointer)[2] = (float)DataReadU16() * posScale;
	else
		((float*)VertexManager::s_pCurBufferPointer)[2] = 1.0f;
	LOG_VTX();
	VertexManager::s_pCurBufferPointer += 12;
}

void LOADERDECL Pos_ReadDirect_Short()
{
	((float*)VertexManager::s_pCurBufferPointer)[0] = (float)(s16)DataReadU16() * posScale;
	((float*)VertexManager::s_pCurBufferPointer)[1] = (float)(s16)DataReadU16() * posScale;
	if (pVtxAttr->PosElements)
		((float*)VertexManager::s_pCurBufferPointer)[2] = (float)(s16)DataReadU16() * posScale;
	else
		((float*)VertexManager::s_pCurBufferPointer)[2] = 1.0f;
	LOG_VTX();
	VertexManager::s_pCurBufferPointer += 12;
}

void LOADERDECL Pos_ReadDirect_Float()
{
	// No need to use floating point here.
	((u32 *)VertexManager::s_pCurBufferPointer)[0] = DataReadU32(); 
	((u32 *)VertexManager::s_pCurBufferPointer)[1] = DataReadU32();
	if (pVtxAttr->PosElements)
		((u32 *)VertexManager::s_pCurBufferPointer)[2] = DataReadU32();
	else
		((float*)VertexManager::s_pCurBufferPointer)[2] = 1.0f;
	LOG_VTX();
	VertexManager::s_pCurBufferPointer += 12;
}

#define Pos_ReadIndex_Byte(T) { \
	u32 iAddress = arraybases[ARRAY_POSITION] + ((u32)Index * arraystrides[ARRAY_POSITION]); \
	((float*)VertexManager::s_pCurBufferPointer)[0] = ((float)(T)Memory_Read_U8(iAddress)) * posScale; \
	((float*)VertexManager::s_pCurBufferPointer)[1] = ((float)(T)Memory_Read_U8(iAddress+1)) * posScale; \
	if (pVtxAttr->PosElements) \
		((float*)VertexManager::s_pCurBufferPointer)[2] = ((float)(T)Memory_Read_U8(iAddress+2)) * posScale; \
	else \
		((float*)VertexManager::s_pCurBufferPointer)[2] = 1.0f; \
	LOG_VTX(); \
	VertexManager::s_pCurBufferPointer += 12; \
}

#define Pos_ReadIndex_Short(T) { \
	u32 iAddress = arraybases[ARRAY_POSITION] + ((u32)Index * arraystrides[ARRAY_POSITION]); \
	((float*)VertexManager::s_pCurBufferPointer)[0] = ((float)(T)Memory_Read_U16(iAddress)) * posScale; \
	((float*)VertexManager::s_pCurBufferPointer)[1] = ((float)(T)Memory_Read_U16(iAddress+2)) * posScale; \
	if (pVtxAttr->PosElements) \
		((float*)VertexManager::s_pCurBufferPointer)[2] = ((float)(T)Memory_Read_U16(iAddress+4)) * posScale; \
	else \
		((float*)VertexManager::s_pCurBufferPointer)[2] = 1.0f; \
	LOG_VTX(); \
	VertexManager::s_pCurBufferPointer += 12; \
}

#define Pos_ReadIndex_Float() { \
	u32 iAddress = arraybases[ARRAY_POSITION] + (Index * arraystrides[ARRAY_POSITION]); \
	((u32*)VertexManager::s_pCurBufferPointer)[0] = Memory_Read_U32(iAddress); \
	((u32*)VertexManager::s_pCurBufferPointer)[1] = Memory_Read_U32(iAddress+4); \
	if (pVtxAttr->PosElements) \
		((u32*)VertexManager::s_pCurBufferPointer)[2] = Memory_Read_U32(iAddress+8); \
	else \
		((float*)VertexManager::s_pCurBufferPointer)[2] = 1.0f; \
	LOG_VTX(); \
	VertexManager::s_pCurBufferPointer += 12; \
}

// ==============================================================================
// Index 8
// ==============================================================================
void LOADERDECL Pos_ReadIndex8_UByte() 
{ 
	u8 Index = DataReadU8();
	Pos_ReadIndex_Byte(u8);
}

void LOADERDECL Pos_ReadIndex8_Byte()
{
	u8 Index = DataReadU8();
	Pos_ReadIndex_Byte(s8);
}

void LOADERDECL Pos_ReadIndex8_UShort()
{
	u8 Index = DataReadU8();
	Pos_ReadIndex_Short(u16);
}

void LOADERDECL Pos_ReadIndex8_Short()
{
	u8 Index = DataReadU8();
	Pos_ReadIndex_Short(s16);
}

void LOADERDECL Pos_ReadIndex8_Float()
{
	u8 Index = DataReadU8();
	Pos_ReadIndex_Float();
}

// ==============================================================================
// Index 16
// ==============================================================================

void LOADERDECL Pos_ReadIndex16_UByte(){
	u16 Index = DataReadU16(); 
	Pos_ReadIndex_Byte(u8);
}

void LOADERDECL Pos_ReadIndex16_Byte(){
	u16 Index = DataReadU16(); 
	Pos_ReadIndex_Byte(s8);
}

void LOADERDECL Pos_ReadIndex16_UShort(){
	u16 Index = DataReadU16(); 
	Pos_ReadIndex_Short(u16);
}

void LOADERDECL Pos_ReadIndex16_Short()
{
	u16 Index = DataReadU16(); 
	Pos_ReadIndex_Short(s16);
}

void LOADERDECL Pos_ReadIndex16_Float()
{
	u16 Index = DataReadU16(); 
	Pos_ReadIndex_Float();
}

#endif
