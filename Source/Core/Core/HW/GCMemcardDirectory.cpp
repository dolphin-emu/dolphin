// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.
#include <cinttypes>

#include "Common/CommonTypes.h"
#include "Common/FileSearch.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/GCMemcardDirectory.h"
#include "DiscIO/Volume.h"

const int NO_INDEX = -1;
static const char *MC_HDR = "MC_SYSTEM_AREA";

int GCMemcardDirectory::LoadGCI(const std::string& fileName, DiscIO::IVolume::ECountry card_region, bool currentGameOnly)
{
	File::IOFile gcifile(fileName, "rb");
	if (gcifile)
	{
		GCIFile gci;
		gci.m_filename = fileName;
		gci.m_dirty = false;
		if (!gcifile.ReadBytes(&(gci.m_gci_header), DENTRY_SIZE))
		{
			ERROR_LOG(EXPANSIONINTERFACE, "%s failed to read header", fileName.c_str());
			return NO_INDEX;
		}

		DiscIO::IVolume::ECountry gci_region;
		// check region
		switch (gci.m_gci_header.Gamecode[3])
		{
		case 'J':
			gci_region = DiscIO::IVolume::COUNTRY_JAPAN;
			break;
		case 'E':
			gci_region = DiscIO::IVolume::COUNTRY_USA;
			break;
		case 'C':
			// Used by Datel Action Replay Save
			// can be on any regions card
			gci_region = card_region;
			break;
		default:
			gci_region = DiscIO::IVolume::COUNTRY_EUROPE;
			break;
		}

		if (gci_region != card_region)
		{
			PanicAlertT("GCI save file was not loaded because it is the wrong region for this memory card:\n%s",
				fileName.c_str());
			return NO_INDEX;
		}

		std::string gci_filename = gci.m_gci_header.GCI_FileName();
		for (u16 i = 0; i < m_loaded_saves.size(); ++i)
		{
			if (m_loaded_saves[i] == gci_filename)
			{
				PanicAlertT(
					"%s\nwas not loaded because it has the same internal filename as previously loaded save\n%s",
					gci.m_filename.c_str(), m_saves[i].m_filename.c_str());
				return NO_INDEX;
			}
		}

		u16 numBlocks = BE16(gci.m_gci_header.BlockCount);
		// largest number of free blocks on a memory card
		// in reality, there are not likely any valid gci files > 251 blocks
		if (numBlocks > 2043)
		{
			PanicAlertT("%s\nwas not loaded because it is an invalid gci.\n Number of blocks claimed to be %d",
						gci.m_filename.c_str(), numBlocks);
			return NO_INDEX;
		}

		u32 size = numBlocks * BLOCK_SIZE;
		u64 file_size = gcifile.GetSize();
		if (file_size != size + DENTRY_SIZE)
		{
			PanicAlertT("%s\nwas not loaded because it is an invalid gci.\n File size (%" PRIx64
						") does not match the size recorded in the header (%d)",
						gci.m_filename.c_str(), file_size, size + DENTRY_SIZE);
			return NO_INDEX;
		}

		if (m_GameId == BE32(gci.m_gci_header.Gamecode))
		{
			gci.LoadSaveBlocks();
		}
		else
		{
			if (currentGameOnly)
			{
				return NO_INDEX;
			}
			int totalBlocks = BE16(m_hdr.SizeMb)*MBIT_TO_BLOCKS - MC_FST_BLOCKS;
			int freeBlocks = BE16(m_bat1.FreeBlocks);
			if (totalBlocks > freeBlocks * 10)
			{

				PanicAlertT("%s\nwas not loaded because there is less than 10%% free space on the memorycard\n"\
					"Total Blocks: %d; Free Blocks: %d",
					gci.m_filename.c_str(), totalBlocks, freeBlocks);
				return NO_INDEX;
			}
		}
		u16 first_block = m_bat1.AssignBlocksContiguous(numBlocks);
		if (first_block == 0xFFFF)
		{
			PanicAlertT("%s\nwas not loaded because there are not enough free blocks on virtual memorycard",
						fileName.c_str());
			return NO_INDEX;
		}
		*(u16 *)&gci.m_gci_header.FirstBlock = first_block;
		if (gci.HasCopyProtection() && gci.LoadSaveBlocks())
		{

			GCMemcard::PSO_MakeSaveGameValid(m_hdr, gci.m_gci_header, gci.m_save_data);
			GCMemcard::FZEROGX_MakeSaveGameValid(m_hdr, gci.m_gci_header, gci.m_save_data);
		}
		int idx = (int)m_saves.size();
		m_dir1.Replace(gci.m_gci_header, idx);
		m_saves.push_back(std::move(gci));
		SetUsedBlocks(idx);

		return idx;
	}
	return NO_INDEX;
}

GCMemcardDirectory::GCMemcardDirectory(const std::string& directory, int slot, u16 sizeMb, bool ascii, DiscIO::IVolume::ECountry card_region, int gameId)
: MemoryCardBase(slot, sizeMb)
, m_GameId(gameId)
, m_LastBlock(-1)
, m_hdr(slot, sizeMb, ascii)
, m_bat1(sizeMb)
, m_saves(0)
, m_SaveDirectory(directory)
{
	// Use existing header data if available
	if (File::Exists(m_SaveDirectory + MC_HDR))
	{
		File::IOFile hdrfile((m_SaveDirectory + MC_HDR), "rb");
		hdrfile.ReadBytes(&m_hdr, BLOCK_SIZE);
	}

	File::FSTEntry FST_Temp;
	File::ScanDirectoryTree(m_SaveDirectory, FST_Temp);

	CFileSearch::XStringVector Directory;
	Directory.push_back(m_SaveDirectory);
	CFileSearch::XStringVector Extensions;
	Extensions.push_back("*.gci");

	CFileSearch FileSearch(Extensions, Directory);
	const CFileSearch::XStringVector& rFilenames = FileSearch.GetFileNames();

	if (rFilenames.size() > 112)
	{
		Core::DisplayMessage(
		StringFromFormat("WARNING: There are more than 112 save files on this memorycards"\
		"\n Only loading the first 112 in the folder, unless the gameid is the current games id"),
		4000);
	}

	for (auto gciFile : rFilenames)
	{
		if (m_saves.size() == DIRLEN)
		{
			PanicAlertT("There are too many gci files in the folder\n%s\nOnly the first 127 will be available",
				m_SaveDirectory.c_str());
			break;
		}
		int index = LoadGCI(gciFile, card_region, m_saves.size() > 112 );
		if (index != NO_INDEX)
		{
			m_loaded_saves.push_back(m_saves.at(index).m_gci_header.GCI_FileName());
		}

	}

	m_loaded_saves.clear();
	m_dir1.fixChecksums();
	m_dir2 = m_dir1;
	m_bat2 = m_bat1;
}

GCMemcardDirectory::~GCMemcardDirectory()
{
	FlushToFile();
}

s32 GCMemcardDirectory::Read(u32 address, s32 length, u8 *destaddress)
{

	s32 block = address / BLOCK_SIZE;
	u32 offset = address % BLOCK_SIZE;
	s32 extra = 0; // used for read calls that are across multiple blocks

	if (offset + length > BLOCK_SIZE)
	{
		extra = length + offset - BLOCK_SIZE;
		length -= extra;

		// verify that we haven't calculated a length beyond BLOCK_SIZE
		_dbg_assert_msg_(EXPANSIONINTERFACE, (address + length) % BLOCK_SIZE == 0,
						 "Memcard directory Read Logic Error");
	}

	if (m_LastBlock != block)
	{
		switch (block)
		{
		case 0:
			m_LastBlock = block;
			m_LastBlockAddress = (u8 *)&m_hdr;
			break;
		case 1:
			m_LastBlock = -1;
			m_LastBlockAddress = (u8 *)&m_dir1;
			break;
		case 2:
			m_LastBlock = -1;
			m_LastBlockAddress = (u8 *)&m_dir2;
			break;
		case 3:
			m_LastBlock = block;
			m_LastBlockAddress = (u8 *)&m_bat1;
			break;
		case 4:
			m_LastBlock = block;
			m_LastBlockAddress = (u8 *)&m_bat2;
			break;
		default:
			m_LastBlock = SaveAreaRW(block);

			if (m_LastBlock == -1)
			{
				memset(destaddress, 0xFF, length);
				return 0;
			}
		}
	}

	memcpy(destaddress, m_LastBlockAddress + offset, length);
	if (extra)
		extra = Read(address + length, extra, destaddress + length);
	return length + extra;
}

s32 GCMemcardDirectory::Write(u32 destaddress, s32 length, u8 *srcaddress)
{
	if (length != 0x80)
		INFO_LOG(EXPANSIONINTERFACE, "WRITING TO %x, len %x", destaddress, length);
	s32 block = destaddress / BLOCK_SIZE;
	u32 offset = destaddress % BLOCK_SIZE;
	s32 extra = 0; // used for write calls that are across multiple blocks

	if (offset + length > BLOCK_SIZE)
	{
		extra = length + offset - BLOCK_SIZE;
		length -= extra;

		// verify that we haven't calculated a length beyond BLOCK_SIZE
		_dbg_assert_msg_(EXPANSIONINTERFACE, (destaddress + length) % BLOCK_SIZE == 0,
						 "Memcard directory Write Logic Error");
	}

	if (m_LastBlock != block)
	{
		switch (block)
		{
		case 0:
			m_LastBlock = block;
			m_LastBlockAddress = (u8 *)&m_hdr;
			break;
		case 1:
		case 2:
		{
			m_LastBlock = -1;
			s32 bytes_written = 0;
			while (length > 0)
			{
				s32 to_write = std::min<s32>(DENTRY_SIZE, length);
				bytes_written += DirectoryWrite(destaddress + bytes_written, to_write, srcaddress + bytes_written);
				length -= to_write;
			}
			return bytes_written;
		}
		case 3:
			m_LastBlock = block;
			m_LastBlockAddress = (u8 *)&m_bat1;
			break;
		case 4:
			m_LastBlock = block;
			m_LastBlockAddress = (u8 *)&m_bat2;
			break;
		default:
			m_LastBlock = SaveAreaRW(block, true);
			if (m_LastBlock == -1)
			{
				PanicAlertT("Report: GCIFolder Writing to unallocated block %x", block);
				exit(0);
			}
		}
	}

	memcpy(m_LastBlockAddress + offset, srcaddress, length);

	if (extra)
		extra = Write(destaddress + length, extra, srcaddress + length);
	return length + extra;
}

void GCMemcardDirectory::ClearBlock(u32 address)
{
	if (address % BLOCK_SIZE)
	{
		PanicAlertT("GCMemcardDirectory: ClearBlock called with invalid block address");
		return;
	}

	u32 block = address / BLOCK_SIZE;
	INFO_LOG(EXPANSIONINTERFACE, "clearing block %d", block);
	switch (block)
	{
	case 0:
		m_LastBlock = block;
		m_LastBlockAddress = (u8 *)&m_hdr;
		break;
	case 1:
		m_LastBlock = -1;
		m_LastBlockAddress = (u8 *)&m_dir1;
		break;
	case 2:
		m_LastBlock = -1;
		m_LastBlockAddress = (u8 *)&m_dir2;
		break;
	case 3:
		m_LastBlock = block;
		m_LastBlockAddress = (u8 *)&m_bat1;
		break;
	case 4:
		m_LastBlock = block;
		m_LastBlockAddress = (u8 *)&m_bat2;
		break;
	default:
		m_LastBlock = SaveAreaRW(block, true);
		if (m_LastBlock == -1)
			return;
	}
	((GCMBlock *)m_LastBlockAddress)->Erase();
}

inline void GCMemcardDirectory::SyncSaves()
{
	Directory *current = &m_dir2;

	if (BE16(m_dir1.UpdateCounter) > BE16(m_dir2.UpdateCounter))
	{
		current = &m_dir1;
	}

	for (u32 i = 0; i < DIRLEN; ++i)
	{
		if (BE32(current->Dir[i].Gamecode) != 0xFFFFFFFF)
		{
			INFO_LOG(EXPANSIONINTERFACE, "Syncing Save %x", *(u32 *)&(current->Dir[i].Gamecode));
			bool added = false;
			while (i >= m_saves.size())
			{
				GCIFile temp;
				m_saves.push_back(temp);
				added = true;
			}

			if (added || memcmp((u8 *)&(m_saves[i].m_gci_header), (u8 *)&(current->Dir[i]), DENTRY_SIZE))
			{
				m_saves[i].m_dirty = true;
				u32 gamecode = BE32(m_saves[i].m_gci_header.Gamecode);
				u32 newGameCode = BE32(current->Dir[i].Gamecode);
				u32 old_start = BE16(m_saves[i].m_gci_header.FirstBlock);
				u32 new_start = BE16(current->Dir[i].FirstBlock);

				if ((gamecode != 0xFFFFFFFF)
					&& (gamecode != newGameCode))
				{
					PanicAlertT("Game overwrote with another games save, data corruption ahead %x, %x ",
						BE32(m_saves[i].m_gci_header.Gamecode), BE32(current->Dir[i].Gamecode));
				}
				memcpy((u8 *)&(m_saves[i].m_gci_header), (u8 *)&(current->Dir[i]), DENTRY_SIZE);
				if (old_start != new_start)
				{
					INFO_LOG(EXPANSIONINTERFACE, "Save moved from %x to %x", old_start, new_start);
					m_saves[i].m_used_blocks.clear();
					m_saves[i].m_save_data.clear();
				}
				if (m_saves[i].m_used_blocks.size() == 0)
				{
					SetUsedBlocks(i);
				}
			}
		}
		else if ((i < m_saves.size()) && (*(u32 *)&(m_saves[i].m_gci_header) != 0xFFFFFFFF))
		{
			INFO_LOG(EXPANSIONINTERFACE, "Clearing and/or Deleting Save %x", BE32(m_saves[i].m_gci_header.Gamecode));
			*(u32 *)&(m_saves[i].m_gci_header.Gamecode) = 0xFFFFFFFF;
			m_saves[i].m_save_data.clear();
			m_saves[i].m_used_blocks.clear();
			m_saves[i].m_dirty = true;

		}
	}
}
inline s32 GCMemcardDirectory::SaveAreaRW(u32 block, bool writing)
{
	for (u16 i = 0; i < m_saves.size(); ++i)
	{
		if (BE32(m_saves[i].m_gci_header.Gamecode) != 0xFFFFFFFF)
		{
			if (m_saves[i].m_used_blocks.size() == 0)
			{
				SetUsedBlocks(i);
			}

			int idx = m_saves[i].UsesBlock(block);
			if (idx != -1)
			{
				if (!m_saves[i].LoadSaveBlocks())
				{
					int num_blocks = BE16(m_saves[i].m_gci_header.BlockCount);
					while (num_blocks)
					{
						m_saves[i].m_save_data.push_back(GCMBlock());
						num_blocks--;
					}
				}

				if (writing)
				{
					m_saves[i].m_dirty = true;
				}

				m_LastBlock = block;
				m_LastBlockAddress = m_saves[i].m_save_data[idx].block;
				return m_LastBlock;
			}
		}
	}
	return -1;
}

s32 GCMemcardDirectory::DirectoryWrite(u32 destaddress, u32 length, u8 *srcaddress)
{
	u32 block = destaddress / BLOCK_SIZE;
	u32 offset = destaddress % BLOCK_SIZE;
	Directory *dest = (block == 1) ? &m_dir1 : &m_dir2;
	u16 Dnum = offset / DENTRY_SIZE;

	if (Dnum == DIRLEN)
	{
		// first 58 bytes should always be 0xff
		// needed to update the update ctr, checksums
		// could check for writes to the 6 important bytes but doubtful that it improves performance noticably
		memcpy((u8 *)(dest)+offset, srcaddress, length);
		SyncSaves();
	}
	else
		memcpy((u8 *)(dest)+offset, srcaddress, length);
	return length;
}

bool GCMemcardDirectory::SetUsedBlocks(int saveIndex)
{
	BlockAlloc *currentBat;
	if (BE16(m_bat2.UpdateCounter) > BE16(m_bat1.UpdateCounter))
		currentBat = &m_bat2;
	else
		currentBat = &m_bat1;

	u16 block = BE16(m_saves[saveIndex].m_gci_header.FirstBlock);
	while (block != 0xFFFF)
	{
		m_saves[saveIndex].m_used_blocks.push_back(block);
		block = currentBat->GetNextBlock(block);
		if (block == 0)
		{
			PanicAlertT("BAT Incorrect, Dolphin will now exit");
			exit(0);
		}
	}

	u16 num_blocks = BE16(m_saves[saveIndex].m_gci_header.BlockCount);
	u16 blocksFromBat = (u16)m_saves[saveIndex].m_used_blocks.size();
	if (blocksFromBat != num_blocks)
	{
		PanicAlertT("Warning BAT number of blocks %d does not match file header loaded %d", blocksFromBat, num_blocks);
		return false;
	}

	return true;
}

void GCMemcardDirectory::FlushToFile()
{
	int errors = 0;
	DEntry invalid;
	for (u16 i = 0; i < m_saves.size(); ++i)
	{
		if (m_saves[i].m_dirty)
		{
			if (BE32(m_saves[i].m_gci_header.Gamecode) != 0xFFFFFFFF)
			{
				m_saves[i].m_dirty = false;
				if (m_saves[i].m_filename.empty())
				{
					std::string defaultSaveName = m_SaveDirectory + m_saves[i].m_gci_header.GCI_FileName();

					// Check to see if another file is using the same name
					// This seems unlikely except in the case of file corruption
					// otherwise what user would name another file this way?
					for (int j = 0; File::Exists(defaultSaveName) && j < 10; ++j)
					{
						defaultSaveName.insert(defaultSaveName.end() - 4, '0');
					}
					if (File::Exists(defaultSaveName))
						PanicAlertT("Failed to find new filename\n %s\n will be overwritten", defaultSaveName.c_str());
					m_saves[i].m_filename = defaultSaveName;
				}
				File::IOFile GCI(m_saves[i].m_filename, "wb");
				if (GCI)
				{
					GCI.WriteBytes(&m_saves[i].m_gci_header, DENTRY_SIZE);
					GCI.WriteBytes(m_saves[i].m_save_data.data(), BLOCK_SIZE * m_saves[i].m_save_data.size());

					if (GCI.IsGood())
					{
						Core::DisplayMessage(
							StringFromFormat("Wrote save contents to %s", m_saves[i].m_filename.c_str()), 4000);
					}
					else
					{
						++errors;
						Core::DisplayMessage(
							StringFromFormat("Failed to write save contents to %s", m_saves[i].m_filename.c_str()),
							4000);
						ERROR_LOG(EXPANSIONINTERFACE, "Failed to save data to %s", m_saves[i].m_filename.c_str());
					}
				}
			}
			else if (m_saves[i].m_filename.length() != 0)
			{
				m_saves[i].m_dirty = false;
				std::string &oldname = m_saves[i].m_filename;
				std::string deletedname = oldname + ".deleted";
				if (File::Exists(deletedname))
					File::Delete(deletedname);
				File::Rename(oldname, deletedname);
				m_saves[i].m_filename.clear();
				m_saves[i].m_save_data.clear();
				m_saves[i].m_used_blocks.clear();
			}
		}

		// Unload the save data for any game that is not running
		// we could use !m_dirty, but some games have multiple gci files and may not write to them simultaneously
		// this ensures that the save data for all of the current games gci files are stored in the savestate
		u32 gamecode = BE32(m_saves[i].m_gci_header.Gamecode);
		if (gamecode != m_GameId && gamecode != 0xFFFFFFFF && m_saves[i].m_save_data.size())
		{
			INFO_LOG(EXPANSIONINTERFACE, "Flushing savedata to disk for %s", m_saves[i].m_filename.c_str());
			m_saves[i].m_save_data.clear();
		}
	}
#if _WRITE_MC_HEADER
	u8 mc[BLOCK_SIZE * MC_FST_BLOCKS];
	Read(0, BLOCK_SIZE * MC_FST_BLOCKS, mc);
	File::IOFile hdrfile(m_SaveDirectory + MC_HDR, "wb");
	hdrfile.WriteBytes(mc, BLOCK_SIZE * MC_FST_BLOCKS);
#endif
}

void GCMemcardDirectory::DoState(PointerWrap &p)
{
	m_LastBlock = -1;
	m_LastBlockAddress = nullptr;
	p.Do(m_SaveDirectory);
	p.DoPOD<Header>(m_hdr);
	p.DoPOD<Directory>(m_dir1);
	p.DoPOD<Directory>(m_dir2);
	p.DoPOD<BlockAlloc>(m_bat1);
	p.DoPOD<BlockAlloc>(m_bat2);
	int numSaves = (int)m_saves.size();
	p.Do(numSaves);
	m_saves.resize(numSaves);
	for (auto itr = m_saves.begin(); itr != m_saves.end(); ++itr)
	{
		itr->DoState(p);
	}
}

bool GCIFile::LoadSaveBlocks()
{
	if (m_save_data.size() == 0)
	{
		if (m_filename.empty())
			return false;

		File::IOFile savefile(m_filename, "rb");
		if (!savefile)
			return false;

		INFO_LOG(EXPANSIONINTERFACE, "Reading savedata from disk for %s", m_filename.c_str());
		savefile.Seek(DENTRY_SIZE, SEEK_SET);
		u16 num_blocks = BE16(m_gci_header.BlockCount);
		m_save_data.resize(num_blocks);
		if (!savefile.ReadBytes(m_save_data.data(), num_blocks * BLOCK_SIZE))
		{
			PanicAlertT("Failed to read data from gci file %s", m_filename.c_str());
			m_save_data.clear();
			return false;
		}
	}
	return true;
}

int GCIFile::UsesBlock(u16 blocknum)
{
	for (u16 i = 0; i < m_used_blocks.size(); ++i)
	{
		if (m_used_blocks[i] == blocknum)
			return i;
	}
	return -1;
}

void GCIFile::DoState(PointerWrap &p)
{
	p.DoPOD<DEntry>(m_gci_header);
	p.Do(m_dirty);
	p.Do(m_filename);
	int numBlocks = (int)m_save_data.size();
	p.Do(numBlocks);
	m_save_data.resize(numBlocks);
	for (auto itr = m_save_data.begin(); itr != m_save_data.end(); ++itr)
	{
		p.DoPOD<GCMBlock>(*itr);
	}
	p.Do(m_used_blocks);
}

void MigrateFromMemcardFile(const std::string& strDirectoryName, int card_index)
{
	File::CreateFullPath(strDirectoryName);
	std::string ini_memcard =
		(card_index == 0) ? SConfig::GetInstance().m_strMemoryCardA : SConfig::GetInstance().m_strMemoryCardB;
	if (File::Exists(ini_memcard))
	{
		GCMemcard memcard(ini_memcard.c_str());
		if (memcard.IsValid())
		{
			for (u8 i = 0; i < DIRLEN; i++)
			{
				memcard.ExportGci(i, "", strDirectoryName);
			}
		}
	}
}
