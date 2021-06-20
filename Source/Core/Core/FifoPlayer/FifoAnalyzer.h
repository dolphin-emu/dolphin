// Copyright 2011 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>

#include "Common/CommonTypes.h"
#include "Common/EnumMap.h"

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
  Common::EnumMap<u32, CPArray::XF_D> arrayBases{};
  Common::EnumMap<u32, CPArray::XF_D> arrayStrides{};
};

void LoadCPReg(u32 subCmd, u32 value, CPMemory& cpMem);

extern bool s_DrawingObject;
extern FifoAnalyzer::CPMemory s_CpMem;
}  // namespace FifoAnalyzer
