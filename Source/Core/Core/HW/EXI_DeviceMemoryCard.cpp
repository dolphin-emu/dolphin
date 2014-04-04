// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/Common.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/Movie.h"
#include "Core/HW/EXI.h"
#include "Core/HW/EXI_Device.h"
#include "Core/HW/EXI_DeviceMemoryCard.h"
#include "Core/HW/GCMemcard.h"
#include "Core/HW/Sram.h"

#define MC_STATUS_BUSY              0x80
#define MC_STATUS_UNLOCKED          0x40
#define MC_STATUS_SLEEP             0x20
#define MC_STATUS_ERASEERROR        0x10
#define MC_STATUS_PROGRAMEERROR     0x08
#define MC_STATUS_READY             0x01
#define SIZE_TO_Mb (1024 * 8 * 16)
#define MC_HDR_SIZE 0xA000

void CEXIMemoryCard::FlushCallback(u64 userdata, int cyclesLate)
{
	// note that userdata is forbidden to be a pointer, due to the implementation of EventDoState
	int card_index = (int)userdata;
	CEXIMemoryCard* pThis = (CEXIMemoryCard*)ExpansionInterface::FindDevice(EXIDEVICE_MEMORYCARD, card_index);
	if (pThis)
		pThis->Flush();
}

void CEXIMemoryCard::CmdDoneCallback(u64 userdata, int cyclesLate)
{
	int card_index = (int)userdata;
	CEXIMemoryCard* pThis = (CEXIMemoryCard*)ExpansionInterface::FindDevice(EXIDEVICE_MEMORYCARD, card_index);
	if (pThis)
		pThis->CmdDone();
}

CEXIMemoryCard::CEXIMemoryCard(const int index)
	: card_index(index)
	, m_bDirty(false)
{
	m_strFilename = (card_index == 0) ? SConfig::GetInstance().m_strMemoryCardA : SConfig::GetInstance().m_strMemoryCardB;
	if (Movie::IsPlayingInput() && Movie::IsConfigSaved() && Movie::IsUsingMemcard() && Movie::IsStartingFromClearSave())
		m_strFilename = File::GetUserPath(D_GCUSER_IDX) + "Movie.raw";

	// we're potentially leaking events here, since there's no UnregisterEvent until emu shutdown, but I guess it's inconsequential
	et_this_card = CoreTiming::RegisterEvent((card_index == 0) ? "memcardFlushA" : "memcardFlushB", FlushCallback);
	et_cmd_done = CoreTiming::RegisterEvent((card_index == 0) ? "memcardDoneA" : "memcardDoneB", CmdDoneCallback);

	interruptSwitch = 0;
	m_bInterruptSet = 0;
	command = 0;
	status = MC_STATUS_BUSY | MC_STATUS_UNLOCKED | MC_STATUS_READY;
	m_uPosition = 0;
	memset(programming_buffer, 0, sizeof(programming_buffer));
	formatDelay = 0;

	//Nintendo Memory Card EXI IDs
	//0x00000004 Memory Card 59     4Mbit
	//0x00000008 Memory Card 123    8Mb
	//0x00000010 Memory Card 251    16Mb
	//0x00000020 Memory Card 507    32Mb
	//0x00000040 Memory Card 1019   64Mb
	//0x00000080 Memory Card 2043   128Mb

	//0x00000510 16Mb "bigben" card
	//card_id = 0xc243;

	// The following games have issues with memory cards bigger than 16Mb
	// Darkened Skye GDQE6S GDQP6S
	// WTA Tour Tennis GWTEA4 GWTJA4 GWTPA4
	// Disney Sports : Skate Boarding GDXEA4 GDXPA4 GDXJA4
	// Disney Sports : Soccer GDKEA4
	// Use a 16Mb (251 block) memory card for these games
	bool useMC251;
	IniFile gameIni = Core::g_CoreStartupParameter.LoadGameIni();
	gameIni.Get("Core", "MemoryCard251", &useMC251, false);
	nintendo_card_id = MemCard2043Mb;
	if (useMC251)
	{
		nintendo_card_id = MemCard251Mb;
		m_strFilename.insert(m_strFilename.find_last_of("."), ".251");
	}
	card_id = 0xc221; // It's a Nintendo brand memcard

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
		// Create a new memcard
		memory_card_size = nintendo_card_id * SIZE_TO_Mb;

		memory_card_content = new u8[memory_card_size];
		GCMemcard::Format(memory_card_content, m_strFilename.find(".JAP.raw") != std::string::npos, nintendo_card_id);
		memset(memory_card_content+MC_HDR_SIZE, 0xFF, memory_card_size-MC_HDR_SIZE);
		WARN_LOG(EXPANSIONINTERFACE, "No memory card found. Will create a new one.");
	}
	SetCardFlashID(memory_card_content, card_index);
}

void innerFlush(FlushData* data)
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
			"need to re-specify your memory card location in the options.", data->filename.c_str());
		return;
	}

	pFile.WriteBytes(data->memcardContent, data->memcardSize);

	if (!data->bExiting)
		Core::DisplayMessage(StringFromFormat("Wrote memory card %c contents to %s",
			data->memcardIndex ? 'B' : 'A', data->filename.c_str()).c_str(), 4000);
	return;
}

// Flush memory card contents to disc
void CEXIMemoryCard::Flush(bool exiting)
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

CEXIMemoryCard::~CEXIMemoryCard()
{
	CoreTiming::RemoveEvent(et_this_card);
	Flush(true);
	delete[] memory_card_content;
	memory_card_content = nullptr;

	if (flushThread.joinable())
	{
		flushThread.join();
	}
}

bool CEXIMemoryCard::IsPresent()
{
	return true;
}

void CEXIMemoryCard::CmdDone()
{
	status |= MC_STATUS_READY;
	status &= ~MC_STATUS_BUSY;

	m_bInterruptSet = 1;
	m_bDirty = true;
}

void CEXIMemoryCard::CmdDoneLater(u64 cycles)
{
	CoreTiming::RemoveEvent(et_cmd_done);
	CoreTiming::ScheduleEvent((int)cycles, et_cmd_done, (u64)card_index);
}

void CEXIMemoryCard::SetCS(int cs)
{
	// So that memory card won't be invalidated during flushing
	if (flushThread.joinable())
	{
		flushThread.join();
	}

	if (cs)  // not-selected to selected
	{
		m_uPosition = 0;
	}
	else
	{
		switch (command)
		{
		case cmdSectorErase:
			if (m_uPosition > 2)
			{
				memset(memory_card_content + (address & (memory_card_size-1)), 0xFF, 0x2000);
				status |= MC_STATUS_BUSY;
				status &= ~MC_STATUS_READY;

				//???

				CmdDoneLater(5000);
			}
			break;

		case cmdChipErase:
			if (m_uPosition > 2)
			{
				memset(memory_card_content, 0xFF, memory_card_size);
				status &= ~MC_STATUS_BUSY;
				m_bDirty = true;
			}
			break;

		case cmdPageProgram:
			if (m_uPosition >= 5)
			{
				int count = m_uPosition - 5;
				int i=0;
				status &= ~0x80;

				while (count--)
				{
					memory_card_content[address] = programming_buffer[i++];
					i &= 127;
					address = (address & ~0x1FF) | ((address+1) & 0x1FF);
				}

				CmdDoneLater(5000);
			}

			// Page written to memory card, not just to buffer - let's schedule a flush 0.5b cycles into the future (1 sec)
			// But first we unschedule already scheduled flushes - no point in flushing once per page for a large write.
			CoreTiming::RemoveEvent(et_this_card);
			CoreTiming::ScheduleEvent(500000000, et_this_card, (u64)card_index);
			break;
		}
	}
}

void CEXIMemoryCard::Update()
{
	if (formatDelay)
	{
		formatDelay--;

		if (!formatDelay)
		{
			status |= MC_STATUS_READY;
			status &= ~MC_STATUS_BUSY;

			m_bInterruptSet = 1;
		}
	}
}

bool CEXIMemoryCard::IsInterruptSet()
{
	if (interruptSwitch)
		return m_bInterruptSet;
	return false;
}

void CEXIMemoryCard::TransferByte(u8 &byte)
{
	DEBUG_LOG(EXPANSIONINTERFACE, "EXI MEMCARD: > %02x", byte);
	if (m_uPosition == 0)
	{
		command = byte;  // first byte is command
		byte = 0xFF; // would be tristate, but we don't care.

		switch (command) // This seems silly, do we really need it?
		{
		case cmdNintendoID:
		case cmdReadArray:
		case cmdArrayToBuffer:
		case cmdSetInterrupt:
		case cmdWriteBuffer:
		case cmdReadStatus:
		case cmdReadID:
		case cmdReadErrorBuffer:
		case cmdWakeUp:
		case cmdSleep:
		case cmdClearStatus:
		case cmdSectorErase:
		case cmdPageProgram:
		case cmdExtraByteProgram:
		case cmdChipErase:
			INFO_LOG(EXPANSIONINTERFACE, "EXI MEMCARD: command %02x at position 0. seems normal.", command);
			break;
		default:
			WARN_LOG(EXPANSIONINTERFACE, "EXI MEMCARD: command %02x at position 0", command);
			break;
		}
		if (command == cmdClearStatus)
		{
			status &= ~MC_STATUS_PROGRAMEERROR;
			status &= ~MC_STATUS_ERASEERROR;

			status |= MC_STATUS_READY;

			m_bInterruptSet = 0;

			byte = 0xFF;
			m_uPosition = 0;
		}
	}
	else
	{
		switch (command)
		{
		case cmdNintendoID:
			//
			// Nintendo card:
			// 00 | 80 00 00 00 10 00 00 00
			// "bigben" card:
			// 00 | ff 00 00 05 10 00 00 00 00 00 00 00 00 00 00
			// we do it the Nintendo way.
			if (m_uPosition == 1)
				byte = 0x80; // dummy cycle
			else
				byte = (u8)(nintendo_card_id >> (24-(((m_uPosition-2) & 3) * 8)));
			break;

		case cmdReadArray:
			switch (m_uPosition)
			{
			case 1: // AD1
				address = byte << 17;
				byte = 0xFF;
				break;
			case 2: // AD2
				address |= byte << 9;
				break;
			case 3: // AD3
				address |= (byte & 3) << 7;
				break;
			case 4: // BA
				address |= (byte & 0x7F);
				break;
			}
			if (m_uPosition > 1) // not specified for 1..8, anyway
			{
				byte = memory_card_content[address & (memory_card_size-1)];
				// after 9 bytes, we start incrementing the address,
				// but only the sector offset - the pointer wraps around
				if (m_uPosition >= 9)
					address = (address & ~0x1FF) | ((address+1) & 0x1FF);
			}
			break;

		case cmdReadStatus:
			// (unspecified for byte 1)
			byte = status;
			break;

		case cmdReadID:
			if (m_uPosition == 1) // (unspecified)
				byte = (u8)(card_id >> 8);
			else
				byte = (u8)((m_uPosition & 1) ? (card_id) : (card_id >> 8));
			break;

		case cmdSectorErase:
			switch (m_uPosition)
			{
			case 1: // AD1
				address = byte << 17;
				break;
			case 2: // AD2
				address |= byte << 9;
				break;
			}
			byte = 0xFF;
			break;

		case cmdSetInterrupt:
			if (m_uPosition == 1)
			{
				interruptSwitch = byte;
			}
			byte = 0xFF;
			break;

		case cmdChipErase:
			byte = 0xFF;
			break;

		case cmdPageProgram:
			switch (m_uPosition)
			{
			case 1: // AD1
				address = byte << 17;
				break;
			case 2: // AD2
				address |= byte << 9;
				break;
			case 3: // AD3
				address |= (byte & 3) << 7;
				break;
			case 4: // BA
				address |= (byte & 0x7F);
				break;
			}

			if (m_uPosition >= 5)
				programming_buffer[((m_uPosition - 5) & 0x7F)] = byte; // wrap around after 128 bytes

			byte = 0xFF;
			break;

		default:
			WARN_LOG(EXPANSIONINTERFACE, "EXI MEMCARD: unknown command byte %02x\n", byte);
			byte = 0xFF;
		}
	}
	m_uPosition++;
	DEBUG_LOG(EXPANSIONINTERFACE, "EXI MEMCARD: < %02x", byte);
}

void CEXIMemoryCard::PauseAndLock(bool doLock, bool unpauseOnUnlock)
{
	if (doLock)
	{
		// we don't exactly have anything to pause,
		// but let's make sure the flush thread isn't running.
		if (flushThread.joinable())
		{
			flushThread.join();
		}
	}
}

void CEXIMemoryCard::DoState(PointerWrap &p)
{
	// for movie sync, we need to save/load memory card contents (and other data) in savestates.
	// otherwise, we'll assume the user wants to keep their memcards and saves separate,
	// unless we're loading (in which case we let the savestate contents decide, in order to stay aligned with them).
	bool storeContents = (Movie::IsRecordingInput() || Movie::IsPlayingInput());
	p.Do(storeContents);

	if (storeContents)
	{
		p.Do(interruptSwitch);
		p.Do(m_bInterruptSet);
		p.Do(command);
		p.Do(status);
		p.Do(m_uPosition);
		p.Do(programming_buffer);
		p.Do(formatDelay);
		p.Do(m_bDirty);
		p.Do(address);

		p.Do(nintendo_card_id);
		p.Do(card_id);
		p.Do(memory_card_size);
		p.DoArray(memory_card_content, memory_card_size);
		p.Do(card_index);
	}
}

IEXIDevice* CEXIMemoryCard::FindDevice(TEXIDevices device_type, int customIndex)
{
	if (device_type != m_deviceType)
		return nullptr;
	if (customIndex != card_index)
		return nullptr;
	return this;
}
