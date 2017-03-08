// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstring>

#include "Common/CommonTypes.h"
#include "Common/Swap.h"

#include "VideoCommon/VertexLoader.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexLoaderUtils.h"
#include "VideoCommon/VertexLoader_Color.h"

#define AMASK 0xFF000000

static void SetCol(VertexLoader* loader, u32 val)
{
  DataWrite(val);
  loader->m_colIndex++;
}

// Color comes in format BARG in 16 bits
// BARG -> AABBGGRR
static void SetCol4444(VertexLoader* loader, u16 val_)
{
  u32 col, val = val_;
  col = val & 0x00F0;           // col  = 000000R0;
  col |= (val & 0x000F) << 12;  // col |= 0000G000;
  col |= (val & 0xF000) << 8;   // col |= 00B00000;
  col |= (val & 0x0F00) << 20;  // col |= A0000000;
  col |= col >> 4;              // col  = A0B0G0R0 | 0A0B0G0R;
  SetCol(loader, col);
}

// Color comes in format RGBA
// RRRRRRGG GGGGBBBB BBAAAAAA
static void SetCol6666(VertexLoader* loader, u32 val)
{
  u32 col = (val >> 16) & 0x000000FC;
  col |= (val >> 2) & 0x0000FC00;
  col |= (val << 12) & 0x00FC0000;
  col |= (val << 26) & 0xFC000000;
  col |= (col >> 6) & 0x03030303;
  SetCol(loader, col);
}

// Color comes in RGB
// RRRRRGGG GGGBBBBB
static void SetCol565(VertexLoader* loader, u16 val_)
{
  u32 col, val = val_;
  col = (val >> 8) & 0x0000F8;
  col |= (val << 5) & 0x00FC00;
  col |= (val << 19) & 0xF80000;
  col |= (col >> 5) & 0x070007;
  col |= (col >> 6) & 0x000300;
  SetCol(loader, col | AMASK);
}

static u32 Read32(const u8* addr)
{
  u32 value;
  std::memcpy(&value, addr, sizeof(u32));
  return value;
}

static u32 Read24(const u8* addr)
{
  return Read32(addr) | AMASK;
}

void Color_ReadDirect_24b_888(VertexLoader* loader)
{
  SetCol(loader, Read24(DataGetPosition()));
  DataSkip(3);
}

void Color_ReadDirect_32b_888x(VertexLoader* loader)
{
  SetCol(loader, Read24(DataGetPosition()));
  DataSkip(4);
}
void Color_ReadDirect_16b_565(VertexLoader* loader)
{
  SetCol565(loader, DataRead<u16>());
}
void Color_ReadDirect_16b_4444(VertexLoader* loader)
{
  u16 value;
  std::memcpy(&value, DataGetPosition(), sizeof(u16));

  SetCol4444(loader, value);
  DataSkip(2);
}
void Color_ReadDirect_24b_6666(VertexLoader* loader)
{
  SetCol6666(loader, Common::swap32(DataGetPosition() - 1));
  DataSkip(3);
}
void Color_ReadDirect_32b_8888(VertexLoader* loader)
{
  SetCol(loader, DataReadU32Unswapped());
}

template <typename I>
void Color_ReadIndex_16b_565(VertexLoader* loader)
{
  auto const Index = DataRead<I>();
  const u8* const address =
      VertexLoaderManager::cached_arraybases[ARRAY_COLOR + loader->m_colIndex] +
      (Index * g_main_cp_state.array_strides[ARRAY_COLOR + loader->m_colIndex]);

  u16 value;
  std::memcpy(&value, address, sizeof(u16));

  SetCol565(loader, Common::swap16(value));
}

template <typename I>
void Color_ReadIndex_24b_888(VertexLoader* loader)
{
  auto const Index = DataRead<I>();
  const u8* iAddress = VertexLoaderManager::cached_arraybases[ARRAY_COLOR + loader->m_colIndex] +
                       (Index * g_main_cp_state.array_strides[ARRAY_COLOR + loader->m_colIndex]);
  SetCol(loader, Read24(iAddress));
}

template <typename I>
void Color_ReadIndex_32b_888x(VertexLoader* loader)
{
  auto const Index = DataRead<I>();
  const u8* iAddress = VertexLoaderManager::cached_arraybases[ARRAY_COLOR + loader->m_colIndex] +
                       (Index * g_main_cp_state.array_strides[ARRAY_COLOR + loader->m_colIndex]);
  SetCol(loader, Read24(iAddress));
}

template <typename I>
void Color_ReadIndex_16b_4444(VertexLoader* loader)
{
  auto const Index = DataRead<I>();
  const u8* const address =
      VertexLoaderManager::cached_arraybases[ARRAY_COLOR + loader->m_colIndex] +
      (Index * g_main_cp_state.array_strides[ARRAY_COLOR + loader->m_colIndex]);

  u16 value;
  std::memcpy(&value, address, sizeof(u16));

  SetCol4444(loader, value);
}

template <typename I>
void Color_ReadIndex_24b_6666(VertexLoader* loader)
{
  auto const Index = DataRead<I>();
  const u8* pData = VertexLoaderManager::cached_arraybases[ARRAY_COLOR + loader->m_colIndex] +
                    (Index * g_main_cp_state.array_strides[ARRAY_COLOR + loader->m_colIndex]) - 1;
  u32 val = Common::swap32(pData);
  SetCol6666(loader, val);
}

template <typename I>
void Color_ReadIndex_32b_8888(VertexLoader* loader)
{
  auto const Index = DataRead<I>();
  const u8* iAddress = VertexLoaderManager::cached_arraybases[ARRAY_COLOR + loader->m_colIndex] +
                       (Index * g_main_cp_state.array_strides[ARRAY_COLOR + loader->m_colIndex]);
  SetCol(loader, Read32(iAddress));
}

void Color_ReadIndex8_16b_565(VertexLoader* loader)
{
  Color_ReadIndex_16b_565<u8>(loader);
}
void Color_ReadIndex8_24b_888(VertexLoader* loader)
{
  Color_ReadIndex_24b_888<u8>(loader);
}
void Color_ReadIndex8_32b_888x(VertexLoader* loader)
{
  Color_ReadIndex_32b_888x<u8>(loader);
}
void Color_ReadIndex8_16b_4444(VertexLoader* loader)
{
  Color_ReadIndex_16b_4444<u8>(loader);
}
void Color_ReadIndex8_24b_6666(VertexLoader* loader)
{
  Color_ReadIndex_24b_6666<u8>(loader);
}
void Color_ReadIndex8_32b_8888(VertexLoader* loader)
{
  Color_ReadIndex_32b_8888<u8>(loader);
}

void Color_ReadIndex16_16b_565(VertexLoader* loader)
{
  Color_ReadIndex_16b_565<u16>(loader);
}
void Color_ReadIndex16_24b_888(VertexLoader* loader)
{
  Color_ReadIndex_24b_888<u16>(loader);
}
void Color_ReadIndex16_32b_888x(VertexLoader* loader)
{
  Color_ReadIndex_32b_888x<u16>(loader);
}
void Color_ReadIndex16_16b_4444(VertexLoader* loader)
{
  Color_ReadIndex_16b_4444<u16>(loader);
}
void Color_ReadIndex16_24b_6666(VertexLoader* loader)
{
  Color_ReadIndex_24b_6666<u16>(loader);
}
void Color_ReadIndex16_32b_8888(VertexLoader* loader)
{
  Color_ReadIndex_32b_8888<u16>(loader);
}
