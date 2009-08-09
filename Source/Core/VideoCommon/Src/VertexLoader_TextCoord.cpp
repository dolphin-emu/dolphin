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

#ifndef VERTEXLOADER_TEXCOORD_H
#define VERTEXLOADER_TEXCOORD_H

#include "Common.h"
#include "VideoCommon.h"
#include "VertexLoader.h"
#include "VertexLoader_Position.h"
#include "NativeVertexWriter.h"

#define LOG_TEX1() PRIM_LOG("tex: %f, ", ((float*)VertexManager::s_pCurBufferPointer)[0]);
#define LOG_TEX2() PRIM_LOG("tex: %f %f, ", ((float*)VertexManager::s_pCurBufferPointer)[0], ((float*)VertexManager::s_pCurBufferPointer)[1]);

extern int tcIndex;
extern float tcScale[8];

void LOADERDECL TexCoord_Read_Dummy()
{
	tcIndex++;
}

void LOADERDECL TexCoord_ReadDirect_UByte1()
{
	((float*)VertexManager::s_pCurBufferPointer)[0] = (float)DataReadU8() * tcScale[tcIndex];
	LOG_TEX1();
	VertexManager::s_pCurBufferPointer += 4;
	tcIndex++;
}
void LOADERDECL TexCoord_ReadDirect_UByte2()
{
	((float*)VertexManager::s_pCurBufferPointer)[0] = (float)DataReadU8() * tcScale[tcIndex];
	((float*)VertexManager::s_pCurBufferPointer)[1] = (float)DataReadU8() * tcScale[tcIndex];
	LOG_TEX2();
	VertexManager::s_pCurBufferPointer += 8;
	tcIndex++;
}

void LOADERDECL TexCoord_ReadDirect_Byte1()
{
	((float*)VertexManager::s_pCurBufferPointer)[0] = (float)(s8)DataReadU8() * tcScale[tcIndex];
	LOG_TEX1();
	VertexManager::s_pCurBufferPointer += 4;
	tcIndex++;
}
void LOADERDECL TexCoord_ReadDirect_Byte2()
{
	((float*)VertexManager::s_pCurBufferPointer)[0] = (float)(s8)DataReadU8() * tcScale[tcIndex];
	((float*)VertexManager::s_pCurBufferPointer)[1] = (float)(s8)DataReadU8() * tcScale[tcIndex];
	LOG_TEX2();
	VertexManager::s_pCurBufferPointer += 8;
	tcIndex++;
}

void LOADERDECL TexCoord_ReadDirect_UShort1()
{
	((float*)VertexManager::s_pCurBufferPointer)[0] = (float)DataReadU16() * tcScale[tcIndex];
	LOG_TEX1();
	VertexManager::s_pCurBufferPointer += 4;
	tcIndex++;
}
void LOADERDECL TexCoord_ReadDirect_UShort2()
{
	((float*)VertexManager::s_pCurBufferPointer)[0] = (float)DataReadU16() * tcScale[tcIndex];
	((float*)VertexManager::s_pCurBufferPointer)[1] = (float)DataReadU16() * tcScale[tcIndex];
	LOG_TEX2();
	VertexManager::s_pCurBufferPointer += 8;
	tcIndex++;
}

void LOADERDECL TexCoord_ReadDirect_Short1()
{
	((float*)VertexManager::s_pCurBufferPointer)[0] = (float)(s16)DataReadU16() * tcScale[tcIndex];
	LOG_TEX1();
	VertexManager::s_pCurBufferPointer += 4;
	tcIndex++;
}
void LOADERDECL TexCoord_ReadDirect_Short2()
{
	((float*)VertexManager::s_pCurBufferPointer)[0] = (float)(s16)DataReadU16() * tcScale[tcIndex];
	((float*)VertexManager::s_pCurBufferPointer)[1] = (float)(s16)DataReadU16() * tcScale[tcIndex];
	LOG_TEX2();
	VertexManager::s_pCurBufferPointer += 8;
	tcIndex++;
}

void LOADERDECL TexCoord_ReadDirect_Float1()
{
	((u32*)VertexManager::s_pCurBufferPointer)[0] = DataReadU32();
	LOG_TEX1();
	VertexManager::s_pCurBufferPointer += 4;
	tcIndex++;
}
void LOADERDECL TexCoord_ReadDirect_Float2()
{
	((u32*)VertexManager::s_pCurBufferPointer)[0] = DataReadU32();
	((u32*)VertexManager::s_pCurBufferPointer)[1] = DataReadU32();
	LOG_TEX2();
	VertexManager::s_pCurBufferPointer += 8;
	tcIndex++;
}

// ==================================================================================
void LOADERDECL TexCoord_ReadIndex8_UByte1()	
{
	u8 Index = DataReadU8();
	const u8 *pData = cached_arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]);
	((float*)VertexManager::s_pCurBufferPointer)[0] = (float)(*pData) * tcScale[tcIndex];
	LOG_TEX1();
	VertexManager::s_pCurBufferPointer += 4;
	tcIndex++;
}
void LOADERDECL TexCoord_ReadIndex8_UByte2()	
{
	u8 Index = DataReadU8();
	const u8 *pData = cached_arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]);
	((float*)VertexManager::s_pCurBufferPointer)[0] = (float)(u8)(pData[0]) * tcScale[tcIndex];
	((float*)VertexManager::s_pCurBufferPointer)[1] = (float)(u8)(pData[1]) * tcScale[tcIndex];
	LOG_TEX2();
	VertexManager::s_pCurBufferPointer += 8;
	tcIndex++;
}

void LOADERDECL TexCoord_ReadIndex8_Byte1()		
{
	u8 Index = DataReadU8();
	const u8 *pData = cached_arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]);

	((float*)VertexManager::s_pCurBufferPointer)[0] = (float)(s8)(*pData) * tcScale[tcIndex];
	LOG_TEX1();
	VertexManager::s_pCurBufferPointer += 4;
	tcIndex++;
}
void LOADERDECL TexCoord_ReadIndex8_Byte2()		
{
	u8 Index = DataReadU8();
	const u8 *pData = cached_arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]);
	((float*)VertexManager::s_pCurBufferPointer)[0] = (float)(s8)(pData[0]) * tcScale[tcIndex];
	((float*)VertexManager::s_pCurBufferPointer)[1] = (float)(s8)(pData[1]) * tcScale[tcIndex];
	LOG_TEX2();
	VertexManager::s_pCurBufferPointer += 8;
	tcIndex++;
}

void LOADERDECL TexCoord_ReadIndex8_UShort1()	
{
	u8 Index = DataReadU8();
	const u16 *pData = (const u16 *)(cached_arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]));
	((float*)VertexManager::s_pCurBufferPointer)[0] = (float)(u16)Common::swap16(*pData) * tcScale[tcIndex];
	LOG_TEX1();
	VertexManager::s_pCurBufferPointer += 4;
	tcIndex++;
}
void LOADERDECL TexCoord_ReadIndex8_UShort2()	
{
	u8 Index = DataReadU8();
	const u16 *pData = (const u16 *)(cached_arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]));
	((float*)VertexManager::s_pCurBufferPointer)[0] = (float)(u16)Common::swap16(pData[0]) * tcScale[tcIndex];
	((float*)VertexManager::s_pCurBufferPointer)[1] = (float)(u16)Common::swap16(pData[1]) * tcScale[tcIndex];
	LOG_TEX2();
	VertexManager::s_pCurBufferPointer += 8;
	tcIndex++;
}

void LOADERDECL TexCoord_ReadIndex8_Short1()	
{
	u8 Index = DataReadU8();
	const u16 *pData = (const u16 *)(cached_arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]));
	((float*)VertexManager::s_pCurBufferPointer)[0] = (float)(s16)Common::swap16(pData[0]) * tcScale[tcIndex];
	LOG_TEX1();
	VertexManager::s_pCurBufferPointer += 4;
	tcIndex++;
}
void LOADERDECL TexCoord_ReadIndex8_Short2()	
{
	u8 Index = DataReadU8();
	const u16 *pData = (const u16 *)(cached_arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]));
	((float*)VertexManager::s_pCurBufferPointer)[0] = (float)(s16)Common::swap16(pData[0]) * tcScale[tcIndex];
	((float*)VertexManager::s_pCurBufferPointer)[1] = (float)(s16)Common::swap16(pData[1]) * tcScale[tcIndex];
	LOG_TEX2();
	VertexManager::s_pCurBufferPointer += 8;
	tcIndex++;
}

void LOADERDECL TexCoord_ReadIndex8_Float1()	
{
	u16 Index = DataReadU8(); 
	const u32 *pData = (const u32 *)(cached_arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]));
	((u32*)VertexManager::s_pCurBufferPointer)[0] = Common::swap32(pData[0]);
	LOG_TEX1();
	VertexManager::s_pCurBufferPointer += 4;
	tcIndex++;
}
void LOADERDECL TexCoord_ReadIndex8_Float2()	
{
	u16 Index = DataReadU8(); 
	const u32 *pData = (const u32 *)(cached_arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]));
	((u32*)VertexManager::s_pCurBufferPointer)[0] = Common::swap32(pData[0]);
	((u32*)VertexManager::s_pCurBufferPointer)[1] = Common::swap32(pData[1]);
	LOG_TEX2();
	VertexManager::s_pCurBufferPointer += 8;
	tcIndex++;
}

// ==================================================================================
void LOADERDECL TexCoord_ReadIndex16_UByte1()	
{
	u16 Index = DataReadU16(); 
	const u8 *pData = cached_arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]);
	((float*)VertexManager::s_pCurBufferPointer)[0] =  (float)(u8)(pData[0]) * tcScale[tcIndex];
	LOG_TEX1();
	VertexManager::s_pCurBufferPointer += 4;
	tcIndex++;
}
void LOADERDECL TexCoord_ReadIndex16_UByte2()	
{
	u16 Index = DataReadU16(); 
	const u8 *pData = cached_arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]);
	((float*)VertexManager::s_pCurBufferPointer)[0] =  (float)(u8)(pData[0]) * tcScale[tcIndex];
	((float*)VertexManager::s_pCurBufferPointer)[1] =  (float)(u8)(pData[1]) * tcScale[tcIndex];
	LOG_TEX2();
	VertexManager::s_pCurBufferPointer += 8;
	tcIndex++;
}

void LOADERDECL TexCoord_ReadIndex16_Byte1()	
{
	u16 Index = DataReadU16(); 
	const u8 *pData = cached_arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]);
	((float*)VertexManager::s_pCurBufferPointer)[0] =  (float)(s8)(pData[0]) * tcScale[tcIndex];
	LOG_TEX1();
	VertexManager::s_pCurBufferPointer += 4;
	tcIndex++;
}
void LOADERDECL TexCoord_ReadIndex16_Byte2()	
{
	u16 Index = DataReadU16(); 
	const u8 *pData = cached_arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]);
	((float*)VertexManager::s_pCurBufferPointer)[0] =  (float)(s8)(pData[0]) * tcScale[tcIndex];
	((float*)VertexManager::s_pCurBufferPointer)[1] =  (float)(s8)(pData[1]) * tcScale[tcIndex];
	LOG_TEX2();
	VertexManager::s_pCurBufferPointer += 8;
	tcIndex++;
}

void LOADERDECL TexCoord_ReadIndex16_UShort1()	
{
	u16 Index = DataReadU16(); 
	const u16* pData = (const u16 *)(cached_arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]));
	((float*)VertexManager::s_pCurBufferPointer)[0] = (float)(u16)Common::swap16(pData[0]) * tcScale[tcIndex];
	LOG_TEX1();
	VertexManager::s_pCurBufferPointer += 4;
	tcIndex++;
}
void LOADERDECL TexCoord_ReadIndex16_UShort2()	
{
	u16 Index = DataReadU16(); 
	const u16* pData = (const u16 *)(cached_arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]));
	((float*)VertexManager::s_pCurBufferPointer)[0] = (float)(u16)Common::swap16(pData[0]) * tcScale[tcIndex];
	((float*)VertexManager::s_pCurBufferPointer)[1] = (float)(u16)Common::swap16(pData[1]) * tcScale[tcIndex];
	LOG_TEX2();
	VertexManager::s_pCurBufferPointer += 8;
	tcIndex++;
}

void LOADERDECL TexCoord_ReadIndex16_Short1()	
{
	u16 Index = DataReadU16(); 
	const u16 *pData = (const u16 *)(cached_arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]));
	((float*)VertexManager::s_pCurBufferPointer)[0] = (float)(s16)Common::swap16(*pData) * tcScale[tcIndex];
	LOG_TEX1();
	VertexManager::s_pCurBufferPointer += 4;
	tcIndex++;
}
void LOADERDECL TexCoord_ReadIndex16_Short2()	
{
	// Heavy in ZWW
	u16 Index = DataReadU16(); 
	const u16 *pData = (const u16 *)(cached_arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]));
	((float*)VertexManager::s_pCurBufferPointer)[0] = (float)(s16)Common::swap16(pData[0]) * tcScale[tcIndex];
	((float*)VertexManager::s_pCurBufferPointer)[1] = (float)(s16)Common::swap16(pData[1]) * tcScale[tcIndex];
	LOG_TEX2();
	VertexManager::s_pCurBufferPointer += 8;
	tcIndex++;
}

void LOADERDECL TexCoord_ReadIndex16_Float1()	
{
	u16 Index = DataReadU16(); 
	const u32 *pData = (const u32 *)(cached_arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]));
	((u32*)VertexManager::s_pCurBufferPointer)[0] = Common::swap32(pData[0]);
	LOG_TEX1();
	VertexManager::s_pCurBufferPointer += 4;
	tcIndex++;
}
void LOADERDECL TexCoord_ReadIndex16_Float2()	
{
	u16 Index = DataReadU16(); 
	const u32 *pData = (const u32 *)(cached_arraybases[ARRAY_TEXCOORD0+tcIndex] + (Index * arraystrides[ARRAY_TEXCOORD0+tcIndex]));
	((u32*)VertexManager::s_pCurBufferPointer)[0] = Common::swap32(pData[0]);
	((u32*)VertexManager::s_pCurBufferPointer)[1] = Common::swap32(pData[1]);
	LOG_TEX2();
	VertexManager::s_pCurBufferPointer += 8;
	tcIndex++;
}

#endif
