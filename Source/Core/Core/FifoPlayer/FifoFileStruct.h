// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

namespace FifoFileStruct
{

enum
{
	FILE_ID            = 0x0d01f1f0,
	VERSION_NUMBER     = 3,
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
