// Copyright 2011 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <vector>

#include "Core/FifoPlayer/FifoAnalyzer.h"
#include "Core/FifoPlayer/FifoDataFile.h"

struct AnalyzedFrameInfo
{
  // Start of the primitives for the object (after previous update commands)
  std::vector<u32> objectStarts;
  std::vector<FifoAnalyzer::CPMemory> objectCPStates;
  // End of the primitives for the object
  std::vector<u32> objectEnds;
  std::vector<MemoryUpdate> memoryUpdates;
};

namespace FifoPlaybackAnalyzer
{
void AnalyzeFrames(FifoDataFile* file, std::vector<AnalyzedFrameInfo>& frameInfo);
}  // namespace FifoPlaybackAnalyzer
