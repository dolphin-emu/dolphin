// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _FIFOPLAYBACKANALYZER_H_
#define _FIFOPLAYBACKANALYZER_H_

#include "FifoAnalyzer.h"
#include "FifoDataFile.h"

#include <string>
#include <vector>

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
	void LoadBP(u32 value0);

	void StoreEfbCopyRegion();
	void StoreWrittenRegion(u32 address, u32 size);

	bool m_DrawingObject;

	std::vector<MemoryRange> m_WrittenMemory;

	BPMemory m_BpMem;
	FifoAnalyzer::CPMemory m_CpMem;
};

#endif
