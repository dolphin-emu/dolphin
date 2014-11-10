// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Core/HW/GCMemcard.h"
#include "DiscIO/Volume.h"

// Uncomment this to write the system data of the memorycard from directory to disc
//#define _WRITE_MC_HEADER 1
void MigrateFromMemcardFile(const std::string& strDirectoryName, int card_index);

class GCMemcardDirectory : public MemoryCardBase, NonCopyable
{
public:
	GCMemcardDirectory(const std::string& directory, int slot = 0, u16 sizeMb = MemCard2043Mb, bool ascii = true,
		DiscIO::IVolume::ECountry  card_region = DiscIO::IVolume::COUNTRY_EUROPE, int gameId = 0);
	~GCMemcardDirectory();
	void FlushToFile();

	s32 Read(u32 address, s32 length, u8 *destaddress) override;
	s32 Write(u32 destaddress, s32 length, u8 *srcaddress) override;
	void ClearBlock(u32 address) override;
	void ClearAll() override {}
	void DoState(PointerWrap &p) override;

private:
	int LoadGCI(const std::string& fileName, DiscIO::IVolume::ECountry card_region, bool currentGameOnly);
	inline s32 SaveAreaRW(u32 block, bool writing = false);
	// s32 DirectoryRead(u32 offset, u32 length, u8* destaddress);
	s32 DirectoryWrite(u32 destaddress, u32 length, u8 *srcaddress);
	inline void SyncSaves();
	bool SetUsedBlocks(int saveIndex);

	u32 m_GameId;
	s32 m_LastBlock;
	u8 *m_LastBlockAddress;

	Header m_hdr;
	Directory m_dir1, m_dir2;
	BlockAlloc m_bat1, m_bat2;
	std::vector<GCIFile> m_saves;

	std::vector<std::string> m_loaded_saves;
	std::string m_SaveDirectory;
};
