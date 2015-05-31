// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/CommonTypes.h"

#include "VideoCommon/VertexLoader.h"
#include "VideoCommon/VertexLoader_Color.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexManagerBase.h"
#include "VideoCommon/VideoCommon.h"

#define AMASK 0xFF000000

__forceinline void _SetCol(VertexLoader* loader, u32 val)
{
	DataWrite(val);
	loader->m_colIndex++;
}

//color comes in format BARG in 16 bits
//BARG -> AABBGGRR
__forceinline void _SetCol4444(VertexLoader* loader, u16 val_)
{
	u32 col, val = val_;
	col  =  val & 0x00F0;        // col  = 000000R0;
	col |= (val & 0x000F) << 12; // col |= 0000G000;
	col |= (val & 0xF000) << 8;  // col |= 00B00000;
	col |= (val & 0x0F00) << 20; // col |= A0000000;
	col |= col >> 4;             // col  = A0B0G0R0 | 0A0B0G0R;
	_SetCol(loader, col);
}

//color comes in format RGBA
//RRRRRRGG GGGGBBBB BBAAAAAA
__forceinline void _SetCol6666(VertexLoader* loader, u32 val)
{
	u32 col = (val >> 16) & 0x000000FC;
	col |=    (val >>  2) & 0x0000FC00;
	col |=    (val << 12) & 0x00FC0000;
	col |=    (val << 26) & 0xFC000000;
	col |=    (col >>  6) & 0x03030303;
	_SetCol(loader, col);
}

//color comes in RGB
//RRRRRGGG GGGBBBBB
__forceinline void _SetCol565(VertexLoader* loader, u16 val_)
{
	u32 col, val = val_;
	col  = (val >>  8) & 0x0000F8;
	col |= (val <<  5) & 0x00FC00;
	col |= (val << 19) & 0xF80000;
	col |= (col >>  5) & 0x070007;
	col |= (col >>  6) & 0x000300;
	_SetCol(loader, col | AMASK);
}

__forceinline u32 _Read24(const u8 *addr)
{
	return (*(const u32 *)addr) | AMASK;
}

__forceinline u32 _Read32(const u8 *addr)
{
	return *(const u32 *)addr;
}


void LOADERDECL Color_ReadDirect_24b_888(VertexLoader* loader)
{
	_SetCol(loader, _Read24(DataGetPosition()));
	DataSkip(3);
}

void LOADERDECL Color_ReadDirect_32b_888x(VertexLoader* loader)
{
	_SetCol(loader, _Read24(DataGetPosition()));
	DataSkip(4);
}
void LOADERDECL Color_ReadDirect_16b_565(VertexLoader* loader)
{
	_SetCol565(loader, DataReadU16());
}
void LOADERDECL Color_ReadDirect_16b_4444(VertexLoader* loader)
{
	_SetCol4444(loader, *(u16*)DataGetPosition());
	DataSkip(2);
}
void LOADERDECL Color_ReadDirect_24b_6666(VertexLoader* loader)
{
	_SetCol6666(loader, Common::swap32(DataGetPosition() - 1));
	DataSkip(3);
}
void LOADERDECL Color_ReadDirect_32b_8888(VertexLoader* loader)
{
	_SetCol(loader, DataReadU32Unswapped());
}

template <typename I>
void Color_ReadIndex_16b_565(VertexLoader* loader)
{
	auto const Index = DataRead<I>();
	u16 val = Common::swap16(*(const u16 *)(VertexLoaderManager::cached_arraybases[ARRAY_COLOR + loader->m_colIndex] + (Index * g_main_cp_state.array_strides[ARRAY_COLOR + loader->m_colIndex])));
	_SetCol565(loader, val);
}

template <typename I>
void Color_ReadIndex_24b_888(VertexLoader* loader)
{
	auto const Index = DataRead<I>();
	const u8 *iAddress = VertexLoaderManager::cached_arraybases[ARRAY_COLOR + loader->m_colIndex] + (Index * g_main_cp_state.array_strides[ARRAY_COLOR + loader->m_colIndex]);
	_SetCol(loader, _Read24(iAddress));
}

template <typename I>
void Color_ReadIndex_32b_888x(VertexLoader* loader)
{
	auto const Index = DataRead<I>();
	const u8 *iAddress = VertexLoaderManager::cached_arraybases[ARRAY_COLOR + loader->m_colIndex] + (Index * g_main_cp_state.array_strides[ARRAY_COLOR + loader->m_colIndex]);
	_SetCol(loader, _Read24(iAddress));
}

template <typename I>
void Color_ReadIndex_16b_4444(VertexLoader* loader)
{
	auto const Index = DataRead<I>();
	u16 val = *(const u16 *)(VertexLoaderManager::cached_arraybases[ARRAY_COLOR + loader->m_colIndex] + (Index * g_main_cp_state.array_strides[ARRAY_COLOR + loader->m_colIndex]));
	_SetCol4444(loader, val);
}

template <typename I>
void Color_ReadIndex_24b_6666(VertexLoader* loader)
{
	auto const Index = DataRead<I>();
	const u8* pData = VertexLoaderManager::cached_arraybases[ARRAY_COLOR + loader->m_colIndex] + (Index * g_main_cp_state.array_strides[ARRAY_COLOR + loader->m_colIndex]) - 1;
	u32 val = Common::swap32(pData);
	_SetCol6666(loader, val);
}

template <typename I>
void Color_ReadIndex_32b_8888(VertexLoader* loader)
{
	auto const Index = DataRead<I>();
	const u8 *iAddress = VertexLoaderManager::cached_arraybases[ARRAY_COLOR + loader->m_colIndex] + (Index * g_main_cp_state.array_strides[ARRAY_COLOR + loader->m_colIndex]);
	_SetCol(loader, _Read32(iAddress));
}

void LOADERDECL Color_ReadIndex8_16b_565(VertexLoader* loader) { Color_ReadIndex_16b_565<u8>(loader); }
void LOADERDECL Color_ReadIndex8_24b_888(VertexLoader* loader) { Color_ReadIndex_24b_888<u8>(loader); }
void LOADERDECL Color_ReadIndex8_32b_888x(VertexLoader* loader) { Color_ReadIndex_32b_888x<u8>(loader); }
void LOADERDECL Color_ReadIndex8_16b_4444(VertexLoader* loader) { Color_ReadIndex_16b_4444<u8>(loader); }
void LOADERDECL Color_ReadIndex8_24b_6666(VertexLoader* loader) { Color_ReadIndex_24b_6666<u8>(loader); }
void LOADERDECL Color_ReadIndex8_32b_8888(VertexLoader* loader) { Color_ReadIndex_32b_8888<u8>(loader); }

void LOADERDECL Color_ReadIndex16_16b_565(VertexLoader* loader) { Color_ReadIndex_16b_565<u16>(loader); }
void LOADERDECL Color_ReadIndex16_24b_888(VertexLoader* loader) { Color_ReadIndex_24b_888<u16>(loader); }
void LOADERDECL Color_ReadIndex16_32b_888x(VertexLoader* loader) { Color_ReadIndex_32b_888x<u16>(loader); }
void LOADERDECL Color_ReadIndex16_16b_4444(VertexLoader* loader) { Color_ReadIndex_16b_4444<u16>(loader); }
void LOADERDECL Color_ReadIndex16_24b_6666(VertexLoader* loader) { Color_ReadIndex_24b_6666<u16>(loader); }
void LOADERDECL Color_ReadIndex16_32b_8888(VertexLoader* loader) { Color_ReadIndex_32b_8888<u16>(loader); }
