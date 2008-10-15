// Copyright (C) 2003-2008 Dolphin Project.

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
#include "EXI_DeviceMic.h"

#define MC_STATUS_BUSY					0x80   
#define MC_STATUS_UNLOCKED				0x40
#define MC_STATUS_SLEEP					0x20
#define MC_STATUS_ERASEERROR			0x10
#define MC_STATUS_PROGRAMEERROR			0x08
#define MC_STATUS_READY					0x01


CEXIMic::CEXIMic(int _Index)
{
	Index = _Index;
	//et_this_card = CoreTiming::RegisterEvent(_rName.c_str(), FlushCallback);
 
	interruptSwitch = 0;
	m_bInterruptSet = 0;
	command = 0;
	//status = MC_STATUS_BUSY | MC_STATUS_UNLOCKED | MC_STATUS_READY;
	m_uPosition = 0;
	formatDelay = 0;
	ID = 0x0a000000;

}


CEXIMic::~CEXIMic()
{
}

bool CEXIMic::IsPresent() 
{
	//return false;
	return true;
}

void CEXIMic::SetCS(int cs)
{
	if (cs)  // not-selected to selected
		m_uPosition = 0;
	else
	{	
		switch (command)
		{
			default:
				//printf("Don't know Command %x\n", command);
			break;
		}
	}
}

void CEXIMic::Update()
{
}

bool CEXIMic::IsInterruptSet()
{
	if (interruptSwitch)
		return m_bInterruptSet;
	return false;
}

void CEXIMic::TransferByte(u8 &byte)
{
	if (m_uPosition == 0)
	{
		command = byte;  // first byte is command
		byte = 0xFF; // would be tristate, but we don't care.

		if(command == cmdClearStatus)
		{
			byte = 0xFF;
			m_uPosition = 0;
		}
	} 
	else
	{
		switch (command)
		{
		case cmdID:
			if (m_uPosition == 1)
				;//byte = 0x80; // dummy cycle
			else
				byte = (u8)(ID >> (24-(((m_uPosition-2) & 3) * 8)));
		break;
		case cmdStatus:
		{
			byte = (u8)(Status.U16 >> (24-(((m_uPosition - 2) & 3) * 8)));
		}
		break;
		case cmdSetStatus:
		{
			Status.U8[m_uPosition - 1] = byte;
			if(m_uPosition == 2)
				printf("Status is 0x%04x\n", Status.U16);
		}
		break;
		default:
			printf("Don't know command %x in Byte transfer\n", command);
		break;
		/*
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
			LOG(EXPANSIONINTERFACE, "EXI MEMCARD: unknown command byte %02x\n", byte);
			byte = 0xFF;	*/
		}
	}
	m_uPosition++;
}
