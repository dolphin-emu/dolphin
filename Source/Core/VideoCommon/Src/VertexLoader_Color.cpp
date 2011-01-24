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

#ifndef _VERTEXLOADERCOLOR_H
#define _VERTEXLOADERCOLOR_H

#include "Common.h"
#include "VideoCommon.h"
#include "LookUpTables.h"
#include "VertexLoader.h"
#include "VertexLoader_Color.h"
#include "VertexManagerBase.h"

#define RSHIFT 0
#define GSHIFT 8
#define BSHIFT 16
#define ASHIFT 24
#define AMASK (0xFF << ASHIFT)

extern int colIndex;
extern int colElements[2];

__forceinline void _SetCol(u32 val)
{
	*(u32*)VertexManager::s_pCurBufferPointer = val;
	VertexManager::s_pCurBufferPointer += 4;
	colIndex++;
}

//Read 4444 directly from unswaped val so intead or RGBA yo get BARG
__forceinline void _SetCol4444(u16 val)
{
	u32 col = Convert4To8(val & 0xF) << GSHIFT;
	col |= Convert4To8((val >> 12) & 0xF) << BSHIFT;
	col |= Convert4To8((val >> 8) & 0xF) << ASHIFT;
	col |= Convert4To8((val >> 4) & 0xF) << RSHIFT;
	_SetCol(col);
}

__forceinline void _SetCol6666(u32 val)
{
	u32 col = Convert6To8((val >> 18) & 0x3F) << RSHIFT;
	col |= Convert6To8((val >> 12) & 0x3F) << GSHIFT;
	col |= Convert6To8((val >> 6) & 0x3F) << BSHIFT;
	col |= Convert6To8(val & 0x3F) << ASHIFT;
	_SetCol(col);
}

__forceinline void _SetCol565(u16 val)
{
	u32 col = Convert5To8((val >> 11) & 0x1F) << RSHIFT;
	col |= Convert6To8((val >> 5) & 0x3F) << GSHIFT;
	col |= Convert5To8(val & 0x1F) << BSHIFT;
	_SetCol(col | AMASK);
}

__forceinline u32 _Read24(const u8 *addr)
{
	return Common::swap32(addr) | AMASK;
}

__forceinline u32 _Read32(const u8 *addr)
{
	return *(const u32 *)addr;
}

void LOADERDECL Color_ReadDirect_24b_888()
{
	_SetCol(_Read24(DataGetPosition()));
	DataSkip(3);
}

void LOADERDECL Color_ReadDirect_32b_888x()
{
	_SetCol(_Read24(DataGetPosition()));
	DataSkip(4);
}

void LOADERDECL Color_ReadDirect_16b_565()
{
	_SetCol565(DataReadU16());
}

void LOADERDECL Color_ReadDirect_16b_4444()
{
	_SetCol4444(((const u16*)DataGetPosition())[0]);
	DataSkip(2);
}

void LOADERDECL Color_ReadDirect_24b_6666()
{
	_SetCol6666(((const u32*)(DataGetPosition() - 1))[0]);
	DataSkip(3);
}

// F|RES: i am not 100 percent sure, but the colElements seems to be important for rendering only
// at least it fixes mario party 4
//
//	if (colElements[colIndex])	
//	else
//		col |= 0xFF<<ASHIFT;
//
void LOADERDECL Color_ReadDirect_32b_8888()
{
	// TODO (mb2): check this
	u32 col = DataReadU32Unswapped();

	// "kill" the alpha
	if (!colElements[colIndex])	
		col |= AMASK;

	_SetCol(col);
}



void LOADERDECL Color_ReadIndex8_16b_565()
{
	u8 Index = DataReadU8();
	u16 val = Common::swap16(*(const u16 *)(cached_arraybases[ARRAY_COLOR+colIndex] + (Index * arraystrides[ARRAY_COLOR+colIndex])));
	_SetCol565(val);
}
void LOADERDECL Color_ReadIndex8_24b_888()
{
	u8 Index = DataReadU8();
	const u8 *iAddress = cached_arraybases[ARRAY_COLOR+colIndex] + (Index * arraystrides[ARRAY_COLOR+colIndex]);
	_SetCol(_Read24(iAddress));
}
void LOADERDECL Color_ReadIndex8_32b_888x()
{
	u8 Index = DataReadU8();
	const u8 *iAddress = cached_arraybases[ARRAY_COLOR+colIndex] + (Index * arraystrides[ARRAY_COLOR]+colIndex);
	_SetCol(_Read24(iAddress));
}
void LOADERDECL Color_ReadIndex8_16b_4444()
{
	u8 Index = DataReadU8();
	u16 val = ((const u16 *)(cached_arraybases[ARRAY_COLOR+colIndex] + (Index * arraystrides[ARRAY_COLOR+colIndex])))[0];
	_SetCol4444(val);
}
void LOADERDECL Color_ReadIndex8_24b_6666()
{
	u8 Index = DataReadU8();
	const u32* pData = (const u32*)(cached_arraybases[ARRAY_COLOR+colIndex] + (Index * arraystrides[ARRAY_COLOR+colIndex]) - 1);
	_SetCol6666(pData[0]);
}
void LOADERDECL Color_ReadIndex8_32b_8888()
{
	u8 Index = DataReadU8();
	const u8 *iAddress = cached_arraybases[ARRAY_COLOR+colIndex] + (Index * arraystrides[ARRAY_COLOR+colIndex]);
	_SetCol(_Read32(iAddress));
}

void LOADERDECL Color_ReadIndex16_16b_565()
{
	u16 Index = DataReadU16(); 
	u16 val = Common::swap16(*(const u16 *)(cached_arraybases[ARRAY_COLOR+colIndex] + (Index * arraystrides[ARRAY_COLOR+colIndex])));
	_SetCol565(val);
}
void LOADERDECL Color_ReadIndex16_24b_888()
{
	u16 Index = DataReadU16(); 
	const u8 *iAddress = cached_arraybases[ARRAY_COLOR+colIndex] + (Index * arraystrides[ARRAY_COLOR+colIndex]);
	_SetCol(_Read24(iAddress));
}
void LOADERDECL Color_ReadIndex16_32b_888x()
{
	u16 Index = DataReadU16(); 
	const u8 *iAddress = cached_arraybases[ARRAY_COLOR+colIndex] + (Index * arraystrides[ARRAY_COLOR+colIndex]);
	_SetCol(_Read24(iAddress));
}
void LOADERDECL Color_ReadIndex16_16b_4444()
{
	u16 Index = DataReadU16();
	u16 val = ((const u16 *)(cached_arraybases[ARRAY_COLOR+colIndex] + (Index * arraystrides[ARRAY_COLOR+colIndex])))[0];
	_SetCol4444(val);
}
void LOADERDECL Color_ReadIndex16_24b_6666()
{
	u16 Index = DataReadU16();
	const u32* pData = (const u32*)(cached_arraybases[ARRAY_COLOR+colIndex] + (Index * arraystrides[ARRAY_COLOR+colIndex]) - 1);
	_SetCol6666(pData[0]);
}
void LOADERDECL Color_ReadIndex16_32b_8888()
{
	u16 Index = DataReadU16(); 
	const u8 *iAddress = cached_arraybases[ARRAY_COLOR+colIndex] + (Index * arraystrides[ARRAY_COLOR+colIndex]);
	_SetCol(_Read32(iAddress));
}

#endif
