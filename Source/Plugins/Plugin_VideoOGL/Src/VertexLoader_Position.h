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

#define LOG_VTX() PRIM_LOG("vtx: %f %f %f, ", ((float*)VertexManager::s_pCurBufferPointer)[0], ((float*)VertexManager::s_pCurBufferPointer)[1], ((float*)VertexManager::s_pCurBufferPointer)[2]);

// ==============================================================================
// Direct
// ==============================================================================
void LOADERDECL Pos_ReadDirect_UByte(void* _p)
{
	TVtxAttr* pVtxAttr = (TVtxAttr*)_p;
	((float*)VertexManager::s_pCurBufferPointer)[0] = (float)ReadBuffer8() * posScale;
    ((float*)VertexManager::s_pCurBufferPointer)[1] = (float)ReadBuffer8() * posScale;
    if (pVtxAttr->PosElements)
		((float*)VertexManager::s_pCurBufferPointer)[2] = (float)ReadBuffer8() * posScale;
	else
		((float*)VertexManager::s_pCurBufferPointer)[2] = 1.0f;
    LOG_VTX();
    VertexManager::s_pCurBufferPointer += 12;
}

void LOADERDECL Pos_ReadDirect_Byte(void* _p)
{	
	TVtxAttr* pVtxAttr = (TVtxAttr*)_p;
	((float*)VertexManager::s_pCurBufferPointer)[0] = (float)(s8)ReadBuffer8() * posScale;
	((float*)VertexManager::s_pCurBufferPointer)[1] = (float)(s8)ReadBuffer8() * posScale;
	if (pVtxAttr->PosElements)
		((float*)VertexManager::s_pCurBufferPointer)[2] = (float)(s8)ReadBuffer8() * posScale;
	else
		((float*)VertexManager::s_pCurBufferPointer)[2] = 1.0;
    LOG_VTX();
    VertexManager::s_pCurBufferPointer += 12;
}

void LOADERDECL Pos_ReadDirect_UShort(void* _p)
{
	TVtxAttr* pVtxAttr = (TVtxAttr*)_p;
	((float*)VertexManager::s_pCurBufferPointer)[0] = (float)ReadBuffer16() * posScale;
	((float*)VertexManager::s_pCurBufferPointer)[1] = (float)ReadBuffer16() * posScale;
	if (pVtxAttr->PosElements)
		((float*)VertexManager::s_pCurBufferPointer)[2] = (float)ReadBuffer16() * posScale;
	else
		((float*)VertexManager::s_pCurBufferPointer)[2] = 1.0f;
    LOG_VTX();
    VertexManager::s_pCurBufferPointer += 12;
}

void LOADERDECL Pos_ReadDirect_Short(void* _p)
{
	TVtxAttr* pVtxAttr = (TVtxAttr*)_p;
	((float*)VertexManager::s_pCurBufferPointer)[0] = (float)(s16)ReadBuffer16() * posScale;
	((float*)VertexManager::s_pCurBufferPointer)[1] = (float)(s16)ReadBuffer16() * posScale;
	if (pVtxAttr->PosElements)
		((float*)VertexManager::s_pCurBufferPointer)[2] = (float)(s16)ReadBuffer16() * posScale;
	else
		((float*)VertexManager::s_pCurBufferPointer)[2] = 1.0f;
    LOG_VTX();
    VertexManager::s_pCurBufferPointer += 12;
}

void LOADERDECL Pos_ReadDirect_Float(void* _p)
{
	TVtxAttr* pVtxAttr = (TVtxAttr*)_p;
	((float*)VertexManager::s_pCurBufferPointer)[0] = ReadBuffer32F(); 
	((float*)VertexManager::s_pCurBufferPointer)[1] = ReadBuffer32F();
	if (pVtxAttr->PosElements)
		((float*)VertexManager::s_pCurBufferPointer)[2] = ReadBuffer32F();
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
void LOADERDECL Pos_ReadIndex8_UByte(void* _p) 
{ 
	TVtxAttr* pVtxAttr = (TVtxAttr*)_p;
	u8 Index = ReadBuffer8();
    Pos_ReadIndex_Byte(u8);
}

void LOADERDECL Pos_ReadIndex8_Byte(void* _p)
{
	TVtxAttr* pVtxAttr = (TVtxAttr*)_p;
	u8 Index = ReadBuffer8();
	Pos_ReadIndex_Byte(s8);
}

void LOADERDECL Pos_ReadIndex8_UShort(void* _p)
{
	TVtxAttr* pVtxAttr = (TVtxAttr*)_p;
	u8 Index = ReadBuffer8();
    Pos_ReadIndex_Short(u16);
}

void LOADERDECL Pos_ReadIndex8_Short(void* _p)
{
	TVtxAttr* pVtxAttr = (TVtxAttr*)_p;
	u8 Index = ReadBuffer8();
	Pos_ReadIndex_Short(s16);
}

void LOADERDECL Pos_ReadIndex8_Float(void* _p)
{
	TVtxAttr* pVtxAttr = (TVtxAttr*)_p;
	u8 Index = ReadBuffer8();
    Pos_ReadIndex_Float();
}

// ==============================================================================
// Index 16
// ==============================================================================

void LOADERDECL Pos_ReadIndex16_UByte(void* _p){
	TVtxAttr* pVtxAttr = (TVtxAttr*)_p;
	u16 Index = ReadBuffer16(); 
	Pos_ReadIndex_Byte(u8);
}

void LOADERDECL Pos_ReadIndex16_Byte(void* _p){
	TVtxAttr* pVtxAttr = (TVtxAttr*)_p;
	u16 Index = ReadBuffer16(); 
	Pos_ReadIndex_Byte(s8);
}

void LOADERDECL Pos_ReadIndex16_UShort(void* _p){
	TVtxAttr* pVtxAttr = (TVtxAttr*)_p;
	u16 Index = ReadBuffer16(); 
	Pos_ReadIndex_Short(u16);
}

void LOADERDECL Pos_ReadIndex16_Short(void* _p)
{
	TVtxAttr* pVtxAttr = (TVtxAttr*)_p;
	u16 Index = ReadBuffer16(); 
	Pos_ReadIndex_Short(s16);
}

void LOADERDECL Pos_ReadIndex16_Float(void* _p)
{
	TVtxAttr* pVtxAttr = (TVtxAttr*)_p;
	u16 Index = ReadBuffer16(); 
	Pos_ReadIndex_Float();
}

#endif
