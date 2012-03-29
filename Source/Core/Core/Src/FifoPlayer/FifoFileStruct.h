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

#ifndef _FIFOFILESTRUCT_H_
#define _FIFOFILESTRUCT_H_

#include "Common.h"

namespace FifoFileStruct
{

enum
{
	FILE_ID = 0x0d01f1f0,
	VERSION_NUMBER = 1,
	MIN_LOADER_VERSION = 1,
};

#pragma pack(push, 4)

union FileHeader
{
	struct 
	{
		u32 fileId;
		u32 file_version;		
		u32 min_loader_version;		
		u64 bpMemOffset;
		u32 bpMemSize;
		u64 cpMemOffset;
		u32 cpMemSize;
		u64 xfMemOffset;
		u32 xfMemSize;
		u64 xfRegsOffset;
		u32 xfRegsSize;
		u64 frameListOffset;
		u32 frameCount;
		u32 flags;
	};
	u32 rawData[32];
};

union FileFrameInfo
{
	struct
	{
		u64 fifoDataOffset;
		u32 fifoDataSize;
		u32 fifoStart;
		u32 fifoEnd;
		u64 memoryUpdatesOffset;
		u32 numMemoryUpdates;
	};
	u32 rawData[16];
};


struct FileMemoryUpdate
{
	u32 fifoPosition;
	u32 address;
	u64 dataOffset;
	u32 dataSize;
	u8 type;
};

#pragma pack(pop)

}

#endif
