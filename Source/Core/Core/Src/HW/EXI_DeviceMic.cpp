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

bool MicButton = false;

void SetMic(bool Value)
{
	MicButton = Value;
}
bool GetMic()
{
	return MicButton;
}

CEXIMic::CEXIMic(int _Index)
{
	Index = _Index;
 
	command = 0;
	Sample = 0;
	m_uPosition = 0;
	formatDelay = 0;
	ID = 0x0a000000;
	m_bInterruptSet = false;

}


CEXIMic::~CEXIMic()
{
}

bool CEXIMic::IsPresent() 
{
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
	if(m_bInterruptSet)
	{
		//m_bInterruptSet = false;
		return true;
	}
	else
	{
		return false;
	}
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
		// Setting Bits: REMEMBER THIS! D:<
		// <ector--> var |= (1 << bitnum_from_0)
		// <ector--> var &= ~(1 << bitnum_from_0) clears
		case cmdStatus:
		{
			if(GetMic())
			{
				Status.U16 |= (1 << 8);
			}
			else
			{
				Status.U16 &= ~(1 << 8);
			}
			if(Sampling)
			{
				//printf("Sample %d\n",Sample);
				if(Sample >= SNum)
				{
					Sample = 0;
					m_bInterruptSet = true;
				}
				else
					Sample++;
			}
			byte = (u8)(Status.U16 >> (24-(((m_uPosition-2) & 3) * 8)));
			
		}
		break;
		case cmdSetStatus:
		{
			Status.U8[ (m_uPosition - 1) ? 0 : 1] = byte;
			if(m_uPosition == 2)
			{
				printf("Status is 0x%04x ", Status.U16);
				//Status is 0x7273 1 1 0 0 1 1 1 0\ 0 1 0 0 1 1 1 0
				//Status is 0x4b00 
				// 0 0 0 0 0 0 0 0	: Bit 0-7:	Unknown
				// 1	: Bit 8		: 1 : Button Pressed 
				// 1	: Bit 9		: 1 ? Overflow? 
				// 0	: Bit 10	: Unknown related to 0 and 15 values It seems
				// 1 0	: Bit 11-12	: Sample Rate, 00-11025, 01-22050, 10-44100, 11-??
				// 0 1	: Bit 13-14	: Period Length, 00-32, 01-64, 10-128, 11-??? 
				// 0	: Bit 15	: If We Are Sampling or Not 
				
				if((Status.U16 >> 15) & 1) // We ARE Sampling
				{
					printf("We are now Sampling");
					Sampling = true;
				}
				else
				{
					Sampling = false;
					m_bInterruptSet = false;
				}
				if(!(Status.U16 >> 11) & 1)
					if((Status.U16 >> 12) & 1 )
						SFreq = 22050;
					else
						SFreq = 11025;
				else
					SFreq = 44100;
					
				if(!(Status.U16 >> 13) & 1)
					if((Status.U16 >> 14) & 1)
						SNum = 64;
					else
						SNum = 32;
				else
					SNum = 128;
			
				for(int a = 0;a < 16;a++)
					printf("%d ", (Status.U16 >> a) & 1);
				printf("\n");
			}
		}
		break;
		case cmdGetBuffer:
			printf("POS %d\n", m_uPosition);
			if(m_uPosition == SNum / 2) // It's 16bit Audio, so we divide by two
				;//m_bInterruptSet = false;
			byte = rand() % 0xFF;
		break;
		default:
			printf("Don't know command %x in Byte transfer\n", command);
		break;
		}
	}
	m_uPosition++;
}
