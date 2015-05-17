// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <chrono>
#include "Common/ChunkFile.h"
#include "Common/FileUtil.h"
#include "Common/StdMakeUnique.h"
#include "Common/Thread.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/GCMemcard.h"
#include "Core/HW/GCMemcardRaw.h"

#define SIZE_TO_Mb (1024 * 8 * 16)
#define MC_HDR_SIZE 0xA000

MemoryCard::MemoryCard(std::string filename, int _card_index, u16 sizeMb)
	: MemoryCardBase(_card_index, sizeMb)
	, m_filename(filename)
{
	File::IOFile pFile(m_filename, "rb");
	if (pFile)
	{
		// Measure size of the existing memcard file.
		memory_card_size = (u32)pFile.GetSize();
		nintendo_card_id = memory_card_size / SIZE_TO_Mb;
		m_memcard_data = std::make_unique<u8[]>(memory_card_size);
		memset(&m_memcard_data[0], 0xFF, memory_card_size);

		INFO_LOG(EXPANSIONINTERFACE, "Reading memory card %s", m_filename.c_str());
		pFile.ReadBytes(&m_memcard_data[0], memory_card_size);
	}
	else
	{
		// Create a new 128Mb memcard
		nintendo_card_id = sizeMb;
		memory_card_size = sizeMb * SIZE_TO_Mb;

		m_memcard_data = std::make_unique<u8[]>(memory_card_size);
		// Fills in MC_HDR_SIZE bytes
		GCMemcard::Format(&m_memcard_data[0], m_filename.find(".JAP.raw") != std::string::npos, sizeMb);
		memset(&m_memcard_data[MC_HDR_SIZE], 0xFF, memory_card_size - MC_HDR_SIZE);

		INFO_LOG(EXPANSIONINTERFACE, "No memory card found - a new one was created.");
	}

	// Class members (including inherited ones) have now been initialized, so
	// it's safe to startup the flush thread (which reads them).
	m_flush_buffer = std::make_unique<u8[]>(memory_card_size);
	m_flush_thread = std::thread(&MemoryCard::FlushThread, this);
}

MemoryCard::~MemoryCard()
{
	if (m_flush_thread.joinable())
	{
		m_flush_trigger.Set();

		m_flush_thread.join();
	}
}

void MemoryCard::FlushThread()
{
	if (!SConfig::GetInstance().m_LocalCoreStartupParameter.bEnableMemcardSaving)
	{
		return;
	}

	Common::SetCurrentThreadName(
		StringFromFormat("Memcard%x-Flush", card_index).c_str());

	const auto flush_interval = std::chrono::seconds(15);

	while (true)
	{
		// If triggered, we're exiting.
		// If timed out, check if we need to flush.
		bool do_exit = m_flush_trigger.WaitFor(flush_interval);
		if (!do_exit)
		{
			bool is_dirty = m_dirty.TestAndClear();
			if (!is_dirty)
			{
				continue;
			}
		}

		// Opening the file is purposefully done each iteration to ensure the
		// file doesn't disappear out from under us after the first check.
		File::IOFile pFile(m_filename, "r+b");

		if (!pFile)
		{
			std::string dir;
			SplitPath(m_filename, &dir, nullptr, nullptr);
			if (!File::IsDirectory(dir))
			{
				File::CreateFullPath(dir);
			}
			pFile.Open(m_filename, "wb");
		}

		// Note - pFile may have changed above, after ctor
		if (!pFile)
		{
			PanicAlertT(
				"Could not write memory card file %s.\n\n"
				"Are you running Dolphin from a CD/DVD, or is the save file maybe write protected?\n\n"
				"Are you receiving this after moving the emulator directory?\nIf so, then you may "
				"need to re-specify your memory card location in the options.",
				m_filename.c_str());

			// Exit the flushing thread - further flushes will be ignored unless
			// the thread is recreated.
			return;
		}

		{
			std::unique_lock<std::mutex> l(m_flush_mutex);
			memcpy(&m_flush_buffer[0], &m_memcard_data[0], memory_card_size);
		}
		pFile.WriteBytes(&m_flush_buffer[0], memory_card_size);

		if (!do_exit)
		{
			Core::DisplayMessage(
				StringFromFormat("Wrote memory card %c contents to %s",
				card_index ? 'B' : 'A', m_filename.c_str()).c_str(),
				4000);
		}
		else
		{
			return;
		}
	}
}

void MemoryCard::MakeDirty()
{
	m_dirty.Set();
}

s32 MemoryCard::Read(u32 srcaddress, s32 length, u8 *destaddress)
{
	if (!IsAddressInBounds(srcaddress))
	{
		PanicAlertT("MemoryCard: Read called with invalid source address, %x",
					srcaddress);
		return -1;
	}

	memcpy(destaddress, &m_memcard_data[srcaddress], length);
	return length;
}

s32 MemoryCard::Write(u32 destaddress, s32 length, u8 *srcaddress)
{
	if (!IsAddressInBounds(destaddress))
	{
		PanicAlertT("MemoryCard: Write called with invalid destination address, %x",
					destaddress);
		return -1;
	}

	{
		std::unique_lock<std::mutex> l(m_flush_mutex);
		memcpy(&m_memcard_data[destaddress], srcaddress, length);
	}
	MakeDirty();
	return length;
}

void MemoryCard::ClearBlock(u32 address)
{
	if (address & (BLOCK_SIZE - 1) || !IsAddressInBounds(address))
	{
		PanicAlertT("MemoryCard: ClearBlock called on invalid address %x",
			address);
		return;
	}
	else
	{
		std::unique_lock<std::mutex> l(m_flush_mutex);
		memset(&m_memcard_data[address], 0xFF, BLOCK_SIZE);
	}
	MakeDirty();
}

void MemoryCard::ClearAll()
{
	{
		std::unique_lock<std::mutex> l(m_flush_mutex);
		memset(&m_memcard_data[0], 0xFF, memory_card_size);
	}
	MakeDirty();
}

void MemoryCard::DoState(PointerWrap &p)
{
	p.Do(card_index);
	p.Do(memory_card_size);
	p.DoArray(&m_memcard_data[0], memory_card_size);
}
