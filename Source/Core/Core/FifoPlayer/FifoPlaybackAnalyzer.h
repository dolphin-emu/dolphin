// Copyright 2011 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <vector>

#include "Core/FifoPlayer/FifoAnalyzer.h"
#include "Core/FifoPlayer/FifoDataFile.h"

struct ObjectInfo
{
  constexpr ObjectInfo(u32 start, u32 primitive_offset, u32 size,
                       const FifoAnalyzer::CPMemory& cpmem)
      : m_start(start), m_primitive_offset(primitive_offset), m_size(size), m_cpmem(cpmem)
  {
  }

  const u32 m_start;
  // Offset from the start of the object to primitive data.
  // May be equal to m_size, if there is no primitive data.
  const u32 m_primitive_offset;
  const u32 m_size;
  const FifoAnalyzer::CPMemory m_cpmem;
};

struct AnalyzedFrameInfo
{
  std::vector<ObjectInfo> objects;
  std::vector<MemoryUpdate> memoryUpdates;
};

namespace FifoPlaybackAnalyzer
{
void AnalyzeFrames(FifoDataFile* file, std::vector<AnalyzedFrameInfo>& frameInfo);
}  // namespace FifoPlaybackAnalyzer
