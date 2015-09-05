// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>

#include "Core/FifoPlayer/FifoAnalyzer.h"
#include "Core/FifoPlayer/FifoDataFile.h"

struct AnalyzedFrameInfo
{
	std::vector<u32> objectStarts;
	std::vector<u32> objectEnds;
	std::vector<MemoryUpdate> memoryUpdates;
};

class FifoPlaybackAnalyzer
{
public:
	FifoPlaybackAnalyzer();

	void AnalyzeFrames(FifoDataFile *file, std::vector<AnalyzedFrameInfo> &frameInfo);

private:
	struct MemoryRange
	{
		u32 begin;
		u32 end;
	};

	void AddMemoryUpdate(MemoryUpdate memUpdate, AnalyzedFrameInfo &frameInfo);

	u32 DecodeCommand(u8 *data);

	void StoreEfbCopyRegion();
	void StoreWrittenRegion(u32 address, u32 size);

	bool m_DrawingObject;

	std::vector<MemoryRange> m_WrittenMemory;

	BPMemory m_BpMem;
	FifoAnalyzer::CPMemory m_CpMem;
};
