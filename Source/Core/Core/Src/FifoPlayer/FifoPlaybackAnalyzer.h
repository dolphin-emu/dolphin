// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

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