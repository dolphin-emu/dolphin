// Copyright 2011 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <vector>

#include "Core/FifoPlayer/FifoAnalyzer.h"
#include "Core/FifoPlayer/FifoDataFile.h"

enum class FramePartType
{
  Commands,
  PrimitiveData,
};

struct FramePart
{
  constexpr FramePart(FramePartType type, u32 start, u32 end, const FifoAnalyzer::CPMemory& cpmem)
      : m_type(type), m_start(start), m_end(end), m_cpmem(cpmem)
  {
  }

  const FramePartType m_type;
  const u32 m_start;
  const u32 m_end;
  const FifoAnalyzer::CPMemory m_cpmem;
};

struct AnalyzedFrameInfo
{
  std::vector<FramePart> parts;
  Common::EnumMap<u32, FramePartType::PrimitiveData> part_type_counts;

  void AddPart(FramePartType type, u32 start, u32 end, const FifoAnalyzer::CPMemory& cpmem)
  {
    parts.emplace_back(type, start, end, cpmem);
    part_type_counts[type]++;
  }
};

namespace FifoPlaybackAnalyzer
{
void AnalyzeFrames(FifoDataFile* file, std::vector<AnalyzedFrameInfo>& frameInfo);
}  // namespace FifoPlaybackAnalyzer
