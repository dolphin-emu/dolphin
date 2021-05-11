// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>

#include "Core/FifoPlayer/FifoAnalyzer.h"
#include "Core/FifoPlayer/FifoDataFile.h"

struct Object {
  u32 start;
  u32 end;
  FifoAnalyzer::CPMemory CPState;
};

struct DisplayList {
  u32 id;
  u32 address;
  u32 refCount = 0;
  std::vector<u8> cmdData;
  std::vector<Object> objects;
};

struct DisplayListCall {
  u32 offset;
  u32 displayListId; // Index into AnalyzedFrameInfo::DisplayList
};

struct AnalyzedFrameInfo
{
  std::vector<Object> objects;
  std::vector<MemoryUpdate> memoryUpdates;
  std::vector<DisplayList> displayLists;
  std::vector<DisplayListCall> displayListCalls;
};

namespace FifoPlaybackAnalyzer
{
void AnalyzeFrames(FifoDataFile* file, std::vector<AnalyzedFrameInfo>& frameInfo);
}  // namespace FifoPlaybackAnalyzer
