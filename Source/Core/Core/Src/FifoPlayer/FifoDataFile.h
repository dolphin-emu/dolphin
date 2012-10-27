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

#ifndef _FIFODATAFILE_H_
#define _FIFODATAFILE_H_

#include "Common.h"
#include <vector>

namespace File
{
	class IOFile;
}

struct MemoryUpdate
{
	enum Type
	{
		TEXTURE_MAP = 0x01,
		XF_DATA = 0x02,
		VERTEX_STREAM = 0x04,
		TLUT = 0x08
	};

	u32 fifoPosition;
	u32 address;
	u32 size;
	u8 *data;
	Type type;
};

struct FifoFrameInfo
{
	u8 *fifoData;
	u32 fifoDataSize;

	u32 fifoStart;
	u32 fifoEnd;

	// Must be sorted by fifoPosition
	std::vector<MemoryUpdate> memoryUpdates;
};

class FifoDataFile
{
public:
	enum
	{
		BP_MEM_SIZE = 256,
		CP_MEM_SIZE = 256,
		XF_MEM_SIZE = 4096,
		XF_REGS_SIZE = 96,
	};

	FifoDataFile();
	~FifoDataFile();	

	void SetIsWii(bool isWii);
	bool GetIsWii() const;

	u32 *GetBPMem() { return m_BPMem; }
	u32 *GetCPMem() { return m_CPMem; }
	u32 *GetXFMem() { return m_XFMem; }
	u32 *GetXFRegs() { return m_XFRegs; }

	void AddFrame(const FifoFrameInfo &frameInfo);
	const FifoFrameInfo &GetFrame(int frame) const { return m_Frames[frame]; }
	int GetFrameCount() { return (int)m_Frames.size(); }

	bool Save(const char *filename);

	static FifoDataFile *Load(const std::string &filename, bool flagsOnly);

private:
	enum
	{
		FLAG_IS_WII = 1
	};
	
	void PadFile(u32 numBytes, File::IOFile &file);

	void SetFlag(u32 flag, bool set);
	bool GetFlag(u32 flag) const;

	u64 WriteMemoryUpdates(const std::vector<MemoryUpdate> &memUpdates, File::IOFile &file);
	static void ReadMemoryUpdates(u64 fileOffset, u32 numUpdates, std::vector<MemoryUpdate> &memUpdates, File::IOFile &file);

	u32 m_BPMem[BP_MEM_SIZE];
	u32 m_CPMem[CP_MEM_SIZE];
	u32 m_XFMem[XF_MEM_SIZE];
	u32 m_XFRegs[XF_REGS_SIZE];

	u32 m_Flags;

	std::vector<FifoFrameInfo> m_Frames;
};

#endif
