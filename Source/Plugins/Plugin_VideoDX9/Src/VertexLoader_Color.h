#pragma once 

#include "Utils.h"

#define RSHIFT 16
#define GSHIFT 8
#define BSHIFT 0
#define ASHIFT 24

extern DecodedVArray *varray;
extern int colIndex;

inline void _SetCol(u32 val)
{
	varray->SetColor(colIndex, val);
	colIndex++;
}

void _SetCol4444(u16 val)
{
	u32 col = lut4to8[(val>>0)&0xF]<<ASHIFT;
	col |= lut4to8[(val>>12)&0xF]   <<RSHIFT;
	col |= lut4to8[(val>>8)&0xF]    <<GSHIFT;
    col |= lut4to8[(val>>4)&0xF]    <<BSHIFT;
	_SetCol(col);
}

void _SetCol6666(u32 val)
{
	u32 col = lut6to8[(val>>18)&0x3F] << RSHIFT;
	col     |= lut6to8[(val>>12)&0x3F] << GSHIFT;
	col     |= lut6to8[(val>>6)&0x3F]  << BSHIFT;
	col     |= lut6to8[(val>>0)&0x3F]  << ASHIFT;
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

void LOADERDECL Color_ReadDirect_24b_888(void* _p)
{
	u32 col = ReadBuffer8()<<RSHIFT;
	col     |= ReadBuffer8()<<GSHIFT;
	col     |= ReadBuffer8()<<BSHIFT;
	_SetCol(col | (0xFF<<ASHIFT));
}

void LOADERDECL Color_ReadDirect_32b_888x(void* _p){
	u32 col = ReadBuffer8()<<RSHIFT;
	col     |= ReadBuffer8()<<GSHIFT;
	col     |= ReadBuffer8()<<BSHIFT;
	_SetCol(col | (0xFF<<ASHIFT));
	ReadBuffer8();
}
void LOADERDECL Color_ReadDirect_16b_565(void* _p)
{
	_SetCol565(ReadBuffer16());
}
void LOADERDECL Color_ReadDirect_16b_4444(void *_p)
{
	_SetCol4444(ReadBuffer16());
}
void LOADERDECL Color_ReadDirect_24b_6666(void* _p)
{
	u32 val = ReadBuffer8()<<16;
	val|=ReadBuffer8()<<8;
	val|=ReadBuffer8(); 
	_SetCol6666(val);
}

// F|RES: i am not 100 percent show, but the colElements seems to be important for rendering only
// at least it fixes mario party 4
//
//	if (colElements[colIndex])	
//	else
//		col |= 0xFF<<ASHIFT;
//
void LOADERDECL Color_ReadDirect_32b_8888(void* _p)
{
	u32 col = ReadBuffer8()<<RSHIFT;
	col     |= ReadBuffer8()<<GSHIFT;
	col     |= ReadBuffer8()<<BSHIFT;
	col		|= ReadBuffer8()<<ASHIFT;

	// "kill" the alpha
	if (!colElements[colIndex])	
		col |= 0xFF<<ASHIFT;

	_SetCol(col);
}
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void LOADERDECL Color_ReadIndex8_16b_565(void* _p)
{
	u8 Index = ReadBuffer8();
	u32 iAddress = arraybases[ARRAY_COLOR+colIndex] + (Index * arraystrides[ARRAY_COLOR+colIndex]);
	u16 val = Memory_Read_U16(iAddress);
	_SetCol565(val);
}
void LOADERDECL Color_ReadIndex8_24b_888(void* _p)
{
	u8 Index = ReadBuffer8();
	u32 iAddress = arraybases[ARRAY_COLOR+colIndex] + (Index * arraystrides[ARRAY_COLOR+colIndex]);
	_SetCol(_Read24(iAddress));
}
void LOADERDECL Color_ReadIndex8_32b_888x(void* _p)
{
	u8 Index = ReadBuffer8();
	u32 iAddress = arraybases[ARRAY_COLOR+colIndex] + (Index * arraystrides[ARRAY_COLOR]+colIndex);
	_SetCol(_Read24(iAddress));
}
void LOADERDECL Color_ReadIndex8_16b_4444(void* _p)
{
	u8 Index = ReadBuffer8();
	u32 iAddress = arraybases[ARRAY_COLOR+colIndex] + (Index * arraystrides[ARRAY_COLOR+colIndex]);
	u16 val = Memory_Read_U16(iAddress);
	_SetCol4444(val);
}
void LOADERDECL Color_ReadIndex8_24b_6666(void* _p)
{
	u8 Index = ReadBuffer8();
	u32 iAddress = arraybases[ARRAY_COLOR+colIndex] + (Index * arraystrides[ARRAY_COLOR+colIndex]);
	u32 val = Memory_Read_U8(iAddress+2) | 
	          (Memory_Read_U8(iAddress+1)<<8) |
	     	  (Memory_Read_U8(iAddress)<<16); 
	
	_SetCol6666(val);
}
void LOADERDECL Color_ReadIndex8_32b_8888(void* _p)
{
	u8 Index = ReadBuffer8();
	u32 iAddress = arraybases[ARRAY_COLOR+colIndex] + (Index * arraystrides[ARRAY_COLOR+colIndex]);
	_SetCol(_Read32(iAddress));
}
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void LOADERDECL Color_ReadIndex16_16b_565(void* _p)
{
	u16 Index = ReadBuffer16(); 
	u32 iAddress = arraybases[ARRAY_COLOR+colIndex] + (Index * arraystrides[ARRAY_COLOR+colIndex]);
	u16 val = Memory_Read_U16(iAddress);
	_SetCol565(val);
}
void LOADERDECL Color_ReadIndex16_24b_888(void* _p)
{
	u16 Index = ReadBuffer16(); 
	u32 iAddress = arraybases[ARRAY_COLOR+colIndex] + (Index * arraystrides[ARRAY_COLOR+colIndex]);
	_SetCol(_Read24(iAddress));
}
void LOADERDECL Color_ReadIndex16_32b_888x(void* _p)
{
	u16 Index = ReadBuffer16(); 
	u32 iAddress = arraybases[ARRAY_COLOR+colIndex] + (Index * arraystrides[ARRAY_COLOR+colIndex]);
	_SetCol(_Read24(iAddress));
}
void LOADERDECL Color_ReadIndex16_16b_4444(void* _p)
{
	u16 Index = ReadBuffer16();
	u32 iAddress = arraybases[ARRAY_COLOR+colIndex] + (Index * arraystrides[ARRAY_COLOR+colIndex]);
	u16 val = Memory_Read_U16(iAddress);
	_SetCol4444(val);
}
void LOADERDECL Color_ReadIndex16_24b_6666(void* _p)
{
	u16 Index = ReadBuffer16();
	u32 iAddress = arraybases[ARRAY_COLOR+colIndex] + (Index * arraystrides[ARRAY_COLOR+colIndex]);
	u32 val = Memory_Read_U8(iAddress+2) | 
		       (Memory_Read_U8(iAddress+1)<<8) |
			   (Memory_Read_U8(iAddress)<<16); 
	_SetCol6666(val);
}
void LOADERDECL Color_ReadIndex16_32b_8888(void* _p)
{
	u16 Index = ReadBuffer16(); 
	u32 iAddress = arraybases[ARRAY_COLOR+colIndex] + (Index * arraystrides[ARRAY_COLOR+colIndex]);
	_SetCol(_Read32(iAddress));
}

