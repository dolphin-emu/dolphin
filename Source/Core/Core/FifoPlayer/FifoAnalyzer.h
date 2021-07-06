// Copyright 2011 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

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
  std::array<VAT, CP_NUM_VAT_REG> vtxAttr;
  std::array<u32, CP_NUM_ARRAYS> arrayBases;
  std::array<u32, CP_NUM_ARRAYS> arrayStrides;
};

void LoadCPReg(u32 subCmd, u32 value, CPMemory& cpMem);

extern bool s_DrawingObject;
extern FifoAnalyzer::CPMemory s_CpMem;
}  // namespace FifoAnalyzer
