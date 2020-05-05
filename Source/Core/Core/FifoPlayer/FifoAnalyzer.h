// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>

#include "Common/CommonTypes.h"
#include "VideoCommon/CPMemory.h"

namespace FifoAnalyzer
{
enum class DecodeMode
{
  Record,
  Playback,
};

u32 AnalyzeCommand(const u8* data, DecodeMode mode);

struct CPMemory
{
  TVtxDesc vtxDesc;
  std::array<VAT, 8> vtxAttr;
  std::array<u32, 16> arrayBases;
  std::array<u32, 16> arrayStrides;
};

void LoadCPReg(u32 subCmd, u32 value, CPMemory& cpMem);


struct FifoDrawCacheEntry
{
  bool just_written;
  u8 original_command;
  std::size_t data_offset;
  u32 data_size;
};

void ClearDrawCache();
FifoDrawCacheEntry* GetDrawCacheEntry(const u8* data);

extern bool s_DrawingObject;
extern FifoAnalyzer::CPMemory s_CpMem;
}  // namespace FifoAnalyzer
