// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/ChunkFile.h"
#include "Common/Thread.h"
#include "Core/HW/GCMemcard.h"

// Data structure to be passed to the flushing thread.
struct FlushData
{
	bool bExiting;
	std::string filename;
	u8 *memcardContent;
	int memcardSize, memcardIndex;
};

class MemoryCard : public MemoryCardBase
{
public:
	MemoryCard(std::string filename, int _card_index, u16 sizeMb = MemCard2043Mb);
	~MemoryCard();
	void Flush(bool exiting = false) override;

	s32 Read(u32 address, s32 length, u8 *destaddress) override;
	s32 Write(u32 destaddress, s32 length, u8 *srcaddress) override;
	void ClearBlock(u32 address) override;
	void ClearAll() override;
	void DoState(PointerWrap &p) override;
	void JoinThread() override;

private:
	u8 *memory_card_content;
	bool m_bDirty;
	std::string m_strFilename;

	FlushData flushData;
	std::thread flushThread;
};
