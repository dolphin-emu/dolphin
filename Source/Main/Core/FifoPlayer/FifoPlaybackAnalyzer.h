// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>

#include "Core/FifoPlayer/FifoDataFile.h"

struct AnalyzedFrameInfo
{
  std::vector<u32> objectStarts;
  std::vector<u32> objectEnds;
  std::vector<MemoryUpdate> memoryUpdates;
};

namespace FifoPlaybackAnalyzer
{
void AnalyzeFrames(FifoDataFile* file, std::vector<AnalyzedFrameInfo>& frameInfo);
}  // namespace FifoPlaybackAnalyzer
