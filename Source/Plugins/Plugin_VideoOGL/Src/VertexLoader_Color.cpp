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

#ifndef _VERTEXLOADERCOLOR_H
#define _VERTEXLOADERCOLOR_H

#include "Globals.h"
#include "LookUpTables.h"
#include "VertexLoader.h"
#include "VertexManager.h"
#include "VertexLoader_Color.h"

#define RSHIFT 0
#define GSHIFT 8
#define BSHIFT 16
#define ASHIFT 24

extern int colIndex;
extern int colElements[2];

inline void _SetCol(u32 val)
{
	*(u32*)VertexManager::s_pCurBufferPointer = val;
	VertexManager::s_pCurBufferPointer += 4;
	colIndex++;
}

void _SetCol4444(u16 val)
{
	u32 col = lut4to8[(val>>0)&0xF]<<ASHIFT;
	col    |= lut4to8[(val>>12)&0xF]   <<RSHIFT;
	col    |= lut4to8[(val>>8)&0xF]    <<GSHIFT;
	col    |= lut4to8[(val>>4)&0xF]    <<BSHIFT;
	_SetCol(col);
}

void _SetCol6666(u32 val)
{
	u32 col = lut6to8[(val>>18)&0x3F] << RSHIFT;
	col    |= lut6to8[(val>>12)&0x3F] << GSHIFT;
	col    |= lut6to8[(val>>6)&0x3F]  << BSHIFT;
	col    |= lut6to8[(val>>0)&0x3F]  << ASHIFT;
	_SetCol(col);
}

void _SetCol565(u16 val)
{
	u32 col = lut5to8[(val>>11)&0x1f] << RSHIFT;
	col     |= lut6to8[(val>>5 )&0x3f] << GSHIFT;
	col     |= lut5to8[(val    )&0x1f] << BSHIFT;
	_SetCol(col | (0xFF<<ASHIFT));
}
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
inline u32 _Read24(u32 iAddress)
{
	u32 col = Memory_Read_U8(iAddress)   << RSHIFT;      //should just get a pointer to main memory instead of going thru slow memhandler
	col     |= Memory_Read_U8(iAddress+1) << GSHIFT;    //we can guarantee that it is reading from main memory
	col     |= Memory_Read_U8(iAddress+2) << BSHIFT;
	return col | (0xFF<<ASHIFT);
}

inline u32 _Read32(u32 iAddress)
{
	u32 col = Memory_Read_U8(iAddress)   << RSHIFT;      //should just get a pointer to main memory instead of going thru slow memhandler
	col     |= Memory_Read_U8(iAddress+1) << GSHIFT;    //we can guarantee that it is reading from main memory
	col     |= Memory_Read_U8(iAddress+2) << BSHIFT;
	col     |= Memory_Read_U8(iAddress+3) << ASHIFT;
	return col;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void LOADERDECL Color_ReadDirect_24b_888()
{
	u32 col = DataReadU8()<<RSHIFT;
	col     |= DataReadU8()<<GSHIFT;
	col     |= DataReadU8()<<BSHIFT;
	_SetCol(col | (0xFF<<ASHIFT));
}

void LOADERDECL Color_ReadDirect_32b_888x(){
	u32 col = DataReadU8()<<RSHIFT;
	col     |= DataReadU8()<<GSHIFT;
	col     |= DataReadU8()<<BSHIFT;
	_SetCol(col | (0xFF<<ASHIFT));
	DataReadU8();
}
void LOADERDECL Color_ReadDirect_16b_565()
{
	_SetCol565(DataReadU16());
}
void LOADERDECL Color_ReadDirect_16b_4444()
{
	_SetCol4444(DataReadU16());
}
void LOADERDECL Color_ReadDirect_24b_6666()
{
	u32 val = DataReadU8()<<16;
	val|=DataReadU8()<<8;
	val|=DataReadU8(); 
	_SetCol6666(val);
}

// F|RES: i am not 100 percent show, but the colElements seems to be important for rendering only
// at least it fixes mario party 4
//
//	if (colElements[colIndex])	
//	else
//		col |= 0xFF<<ASHIFT;
//
void LOADERDECL Color_ReadDirect_32b_8888()
{
	// TODO (mb2): check this
	u32 col = DataReadU8()<<RSHIFT;
	col     |= DataReadU8()<<GSHIFT;
	col     |= DataReadU8()<<BSHIFT;
	col		|= DataReadU8()<<ASHIFT;

	// "kill" the alpha
	if (!colElements[colIndex])	
		col |= 0xFF<<ASHIFT;

	_SetCol(col);
}
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void LOADERDECL Color_ReadIndex8_16b_565()
{
	u8 Index = DataReadU8();
	u32 iAddress = arraybases[ARRAY_COLOR+colIndex] + (Index * arraystrides[ARRAY_COLOR+colIndex]);
	u16 val = Memory_Read_U16(iAddress);
	_SetCol565(val);
}
void LOADERDECL Color_ReadIndex8_24b_888()
{
	u8 Index = DataReadU8();
	u32 iAddress = arraybases[ARRAY_COLOR+colIndex] + (Index * arraystrides[ARRAY_COLOR+colIndex]);
	_SetCol(_Read24(iAddress));
}
void LOADERDECL Color_ReadIndex8_32b_888x()
{
	u8 Index = DataReadU8();
	u32 iAddress = arraybases[ARRAY_COLOR+colIndex] + (Index * arraystrides[ARRAY_COLOR]+colIndex);
	_SetCol(_Read24(iAddress));
}
void LOADERDECL Color_ReadIndex8_16b_4444()
{
	u8 Index = DataReadU8();
	u32 iAddress = arraybases[ARRAY_COLOR+colIndex] + (Index * arraystrides[ARRAY_COLOR+colIndex]);
	u16 val = Memory_Read_U16(iAddress);
	_SetCol4444(val);
}
void LOADERDECL Color_ReadIndex8_24b_6666()
{
	u8 Index = DataReadU8();
	u32 iAddress = arraybases[ARRAY_COLOR+colIndex] + (Index * arraystrides[ARRAY_COLOR+colIndex]);
	u32 val = Memory_Read_U8(iAddress+2) | 
			  (Memory_Read_U8(iAddress+1)<<8) |
			  (Memory_Read_U8(iAddress)<<16); 
	
	_SetCol6666(val);
}
void LOADERDECL Color_ReadIndex8_32b_8888()
{
	u8 Index = DataReadU8();
	u32 iAddress = arraybases[ARRAY_COLOR+colIndex] + (Index * arraystrides[ARRAY_COLOR+colIndex]);
	_SetCol(_Read32(iAddress));
}
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void LOADERDECL Color_ReadIndex16_16b_565()
{
	u16 Index = DataReadU16(); 
	u32 iAddress = arraybases[ARRAY_COLOR+colIndex] + (Index * arraystrides[ARRAY_COLOR+colIndex]);
	u16 val = Memory_Read_U16(iAddress);
	_SetCol565(val);
}
void LOADERDECL Color_ReadIndex16_24b_888()
{
	u16 Index = DataReadU16(); 
	u32 iAddress = arraybases[ARRAY_COLOR+colIndex] + (Index * arraystrides[ARRAY_COLOR+colIndex]);
	_SetCol(_Read24(iAddress));
}
void LOADERDECL Color_ReadIndex16_32b_888x()
{
	u16 Index = DataReadU16(); 
	u32 iAddress = arraybases[ARRAY_COLOR+colIndex] + (Index * arraystrides[ARRAY_COLOR+colIndex]);
	_SetCol(_Read24(iAddress));
}
void LOADERDECL Color_ReadIndex16_16b_4444()
{
	u16 Index = DataReadU16();
	u32 iAddress = arraybases[ARRAY_COLOR+colIndex] + (Index * arraystrides[ARRAY_COLOR+colIndex]);
	u16 val = Memory_Read_U16(iAddress);
	_SetCol4444(val);
}
void LOADERDECL Color_ReadIndex16_24b_6666()
{
	u16 Index = DataReadU16();
	u32 iAddress = arraybases[ARRAY_COLOR+colIndex] + (Index * arraystrides[ARRAY_COLOR+colIndex]);
	u32 val = Memory_Read_U8(iAddress+2) | 
			   (Memory_Read_U8(iAddress+1)<<8) |
			   (Memory_Read_U8(iAddress)<<16); 
	_SetCol6666(val);
}
void LOADERDECL Color_ReadIndex16_32b_8888()
{
	u16 Index = DataReadU16(); 
	u32 iAddress = arraybases[ARRAY_COLOR+colIndex] + (Index * arraystrides[ARRAY_COLOR+colIndex]);
	_SetCol(_Read32(iAddress));
}

#endif
