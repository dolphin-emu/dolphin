// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

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
  VAT vtxAttr[8];
  u32 arrayBases[16];
  u32 arrayStrides[16];
};

void LoadCPReg(u32 subCmd, u32 value, CPMemory& cpMem);

extern bool s_DrawingObject;
extern FifoAnalyzer::CPMemory s_CpMem;
}  // namespace FifoAnalyzer
