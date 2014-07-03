// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.
#include "Core/Core.h"
#include "Core/HW/GCMemcard.h"
#include "Core/HW/GCMemcardRaw.h"
#define SIZE_TO_Mb (1024 * 8 * 16)
#define MC_HDR_SIZE 0xA000

void innerFlush(FlushData *data)
{
	File::IOFile pFile(data->filename, "r+b");
	if (!pFile)
	{
		std::string dir;
		SplitPath(data->filename, &dir, nullptr, nullptr);
		if (!File::IsDirectory(dir))
			File::CreateFullPath(dir);
		pFile.Open(data->filename, "wb");
	}

	if (!pFile) // Note - pFile changed inside above if
	{
		PanicAlertT("Could not write memory card file %s.\n\n"
					"Are you running Dolphin from a CD/DVD, or is the save file maybe write protected?\n\n"
					"Are you receiving this after moving the emulator directory?\nIf so, then you may "
					"need to re-specify your memory card location in the options.",
					data->filename.c_str());
		return;
	}

	pFile.WriteBytes(data->memcardContent, data->memcardSize);

	if (!data->bExiting)
		Core::DisplayMessage(StringFromFormat("Wrote memory card %c contents to %s", data->memcardIndex ? 'B' : 'A',
											  data->filename.c_str()).c_str(),
							 4000);
	return;
}

MemoryCard::MemoryCard(std::string filename, int _card_index, u16 sizeMb)
	: MemoryCardBase(_card_index, sizeMb)
	, m_bDirty(false)
	, m_strFilename(filename)
{
	File::IOFile pFile(m_strFilename, "rb");
	if (pFile)
	{
		// Measure size of the memcard file.
		memory_card_size = (int)pFile.GetSize();
		nintendo_card_id = memory_card_size / SIZE_TO_Mb;
		memory_card_content = new u8[memory_card_size];
		memset(memory_card_content, 0xFF, memory_card_size);

		INFO_LOG(EXPANSIONINTERFACE, "Reading memory card %s", m_strFilename.c_str());
		pFile.ReadBytes(memory_card_content, memory_card_size);
	}
	else
	{
		// Create a new 128Mb memcard
		nintendo_card_id = sizeMb;
		memory_card_size = sizeMb * SIZE_TO_Mb;

		memory_card_content = new u8[memory_card_size];
		GCMemcard::Format(memory_card_content, m_strFilename.find(".JAP.raw") != std::string::npos, sizeMb);
		memset(memory_card_content + MC_HDR_SIZE, 0xFF, memory_card_size - MC_HDR_SIZE);

		WARN_LOG(EXPANSIONINTERFACE, "No memory card found. Will create a new one.");
	}
}

void MemoryCard::joinThread()
{
	if (flushThread.joinable())
	{
		flushThread.join();
	}
}

// Flush memory card contents to disc
void MemoryCard::Flush(bool exiting)
{
	if (!m_bDirty)
		return;

	if (!Core::g_CoreStartupParameter.bEnableMemcardSaving)
		return;

	if (flushThread.joinable())
	{
		flushThread.join();
	}

	if (!exiting)
		Core::DisplayMessage(StringFromFormat("Writing to memory card %c", card_index ? 'B' : 'A'), 1000);

	flushData.filename = m_strFilename;
	flushData.memcardContent = memory_card_content;
	flushData.memcardIndex = card_index;
	flushData.memcardSize = memory_card_size;
	flushData.bExiting = exiting;

	flushThread = std::thread(innerFlush, &flushData);
	if (exiting)
		flushThread.join();

	m_bDirty = false;
}

s32 MemoryCard::Read(u32 srcaddress, s32 length, u8 *destaddress)
{
	if (!memory_card_content)
		return -1;
	if (srcaddress > (memory_card_size - 1))
	{
		PanicAlertT("MemoryCard: Read called with invalid source address, %x", srcaddress);
		return -1;
	}

	memcpy(destaddress, &(memory_card_content[srcaddress]), length);
	return length;
}

s32 MemoryCard::Write(u32 destaddress, s32 length, u8 *srcaddress)
{
	if (!memory_card_content)
		return -1;

	if (destaddress > (memory_card_size - 1))
	{
		PanicAlertT("MemoryCard: Write called with invalid destination address, %x", destaddress);
		return -1;
	}

	m_bDirty = true;
	memcpy(&(memory_card_content[destaddress]), srcaddress, length);
	return length;
}

void MemoryCard::ClearBlock(u32 address)
{
	if (address & (BLOCK_SIZE - 1) || address > (memory_card_size - 1))
		PanicAlertT("MemoryCard: ClearBlock called on invalid address %x", address);
	else
	{
		m_bDirty = true;
		memset(memory_card_content + address, 0xFF, BLOCK_SIZE);
	}
}

void MemoryCard::ClearAll()
{
	m_bDirty = true;
	memset(memory_card_content, 0xFF, memory_card_size);
}

void MemoryCard::DoState(PointerWrap &p)
{
	p.Do(card_index);
	p.Do(memory_card_size);
	p.DoArray(memory_card_content, memory_card_size);
}
