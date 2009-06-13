// Copyright (C) 2003-2009 Dolphin Project.

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

#include "Common.h"
#include "FileUtil.h"
#include "StringUtil.h"
#include "../Core.h"
#include "../CoreTiming.h"

#include "EXI_Device.h"
#include "EXI_DeviceMemoryCard.h"

#define MC_STATUS_BUSY					0x80   
#define MC_STATUS_UNLOCKED				0x40
#define MC_STATUS_SLEEP					0x20
#define MC_STATUS_ERASEERROR			0x10
#define MC_STATUS_PROGRAMEERROR			0x08
#define MC_STATUS_READY					0x01
#define SIZE_TO_Mb (1024 * 8 * 16)

static CEXIMemoryCard *cards[2];

void CEXIMemoryCard::FlushCallback(u64 userdata, int cyclesLate)
{
	CEXIMemoryCard *ptr = cards[userdata];
	ptr->Flush();
}

CEXIMemoryCard::CEXIMemoryCard(const std::string& _rName, const std::string& _rFilename, int _card_index) :
	m_strFilename(_rFilename),
	card_index(_card_index)
{
	cards[_card_index] = this;
	et_this_card = CoreTiming::RegisterEvent(_rName.c_str(), FlushCallback);
 
	interruptSwitch = 0;
	m_bInterruptSet = 0;
	command = 0;
	status = MC_STATUS_BUSY | MC_STATUS_UNLOCKED | MC_STATUS_READY;
	m_uPosition = 0;
	memset(programming_buffer, 0, sizeof(programming_buffer));
	formatDelay = 0;
 
	//Nintendo Memory Card EXI IDs
	//0x00000004 Memory Card 59		4Mbit
	//0x00000008 Memory Card 123	8Mb
	//0x00000010 Memory Card 251	16Mb
	//0x00000020 Memory Card 507	32Mb
	//0x00000040 Memory Card 1019	64Mb
	//0x00000080 Memory Card 2043	128Mb
 
	//0x00000510 16Mb "bigben" card
	//card_id = 0xc243;
 
	card_id = 0xc221; // It's a nintendo brand memcard
 
	FILE* pFile = NULL;
	pFile = fopen(m_strFilename.c_str(), "rb");
	if (pFile)
	{
		// Measure size of the memcard file.
		fseek(pFile, 0L, SEEK_END);
		u64 MemFileSize = ftell(pFile);
		fseek(pFile, 0L, SEEK_SET);

		memory_card_size = (int)MemFileSize;
		nintendo_card_id = memory_card_size / SIZE_TO_Mb;
		memory_card_content = new u8[memory_card_size];
		memset(memory_card_content, 0xFF, memory_card_size);
 
		INFO_LOG(EXPANSIONINTERFACE, "Reading memory card %s", m_strFilename.c_str());
		fread(memory_card_content, 1, memory_card_size, pFile);
		fclose(pFile);
	}
	else
	{
		// Create a new 128Mb memcard
		nintendo_card_id = 0x00000080;
		memory_card_size = nintendo_card_id * SIZE_TO_Mb;

		memory_card_content = new u8[memory_card_size];
		memset(memory_card_content, 0xFF, memory_card_size);
 
		WARN_LOG(EXPANSIONINTERFACE, "No memory card found. Will create new.");
		Flush();
	}
	flushThread = NULL;
}

THREAD_RETURN innerFlush(void *pArgs)
{
	flushStruct *data = ((flushStruct *)pArgs);
	FILE* pFile = NULL;
	pFile = fopen(data->filename.c_str(), "wb");

	if (!pFile)
	{
		std::string dir;
		SplitPath(data->filename, &dir, 0, 0);
		if(!File::IsDirectory(dir.c_str()))
			File::CreateFullPath(dir.c_str());
		pFile = fopen(data->filename.c_str(), "wb");
	}

	if (!pFile) // Note - pFile changed inside above if
	{
		PanicAlert("Could not write memory card file %s.\n\n"
			"Are you running Dolphin from a CD/DVD, or is the save file maybe write protected?", data->filename.c_str());
		delete data;
		return 0;
	}

	fwrite(data->memcardContent, data->memcardSize, 1, pFile);
	fclose(pFile);

	if (!data->bExiting)
		Core::DisplayMessage(StringFromFormat("Wrote memory card %c contents to %s", data->memcardIndex ? 'B' : 'A', 
						     data->filename.c_str()).c_str(), 4000);

	delete data;
	return 0;
}

// Flush memory card contents to disc
void CEXIMemoryCard::Flush(bool exiting)
{
	if(flushThread)
		delete flushThread;

	if(!exiting)
		Core::DisplayMessage(StringFromFormat("Writing to memory card %c", card_index ? 'B' : 'A'), 1000);

	flushStruct *fs = new flushStruct;
	fs->filename = m_strFilename;
	fs->memcardContent = memory_card_content;
	fs->memcardIndex = card_index;
	fs->memcardSize = memory_card_size;
	fs->bExiting = exiting;

	flushThread = new Common::Thread(innerFlush, fs);
	if(exiting)
		flushThread->WaitForDeath();
}

CEXIMemoryCard::~CEXIMemoryCard()
{
	Flush(true);
	delete [] memory_card_content;
	memory_card_content = NULL;
	if(flushThread)
		delete flushThread;
	flushThread = NULL;
}

bool CEXIMemoryCard::IsPresent() 
{
	//return false;
	return true;
}

void CEXIMemoryCard::SetCS(int cs)
{
	// So that memory card won't be invalidated during flushing
	if(flushThread)
	{
		delete flushThread;
		flushThread = NULL;
	}

	if (cs)  // not-selected to selected
		m_uPosition = 0;
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

				status |= MC_STATUS_READY;
				status &= ~MC_STATUS_BUSY;

				m_bInterruptSet = 1;
			}
			break;

		case cmdChipErase:
			if (m_uPosition > 2)
			{
				memset(memory_card_content, 0xFF, memory_card_size);
				status &= ~MC_STATUS_BUSY;
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

				status |= MC_STATUS_READY;
				status &= ~MC_STATUS_BUSY;

				m_bInterruptSet = 1;
			}
			
			// Page written to memory card, not just to buffer - let's schedule a flush 0.5b cycles into the future (1 sec)
			// But first we unschedule already scheduled flushes - no point in flushing once per page for a large write.
			CoreTiming::RemoveEvent(et_this_card);
			CoreTiming::ScheduleEvent(500000000, et_this_card, card_index);
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

		switch (command)
		{
		case 0x52:
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
			// nintendo card:
			// 00 | 80 00 00 00 10 00 00 00 
			// "bigben" card:
			// 00 | ff 00 00 05 10 00 00 00 00 00 00 00 00 00 00
			// we do it the nintendo way.
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

			if(m_uPosition >= 5)
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
