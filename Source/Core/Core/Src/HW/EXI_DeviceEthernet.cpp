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

#include "Memmap.h"

#include "../Core.h"

#include "EXI_Device.h"
#include "EXI_DeviceEthernet.h"
enum {
	EXPECT_NONE = 0,
	EXPECT_ID,
} ;
unsigned int Expecting;
CEXIETHERNET::CEXIETHERNET() :
	m_uPosition(0),
	m_uCommand(0)		
{
	ID = 0x04020200;
	Expecting = EXPECT_NONE;
}

void CEXIETHERNET::SetCS(int cs)
{
	if (cs)
	{
		m_uPosition = 0;
		Expecting = EXPECT_NONE;
	}
}

bool CEXIETHERNET::IsPresent()
{
	return true;
}

void CEXIETHERNET::Update()
{
	return;
}
bool CEXIETHERNET::IsInterruptSet()
{
	return false;
}

void CEXIETHERNET::Transfer(u8& _byte)
{
	//printf("%x %x POS: %d\n", _byte, m_uCommand, m_uPosition);
	//printf("%x %x \n", _byte, m_uCommand);
	if (m_uPosition == 0)
	{
		m_uCommand = _byte;
	}
	else
	{
		switch(m_uCommand)
		{
			case CMD_ID: // ID
				if (m_uPosition != 1)
					_byte = (u8)(ID >> (24-(((m_uPosition-2) & 3) * 8)));
			break;
			case CMD_READ_REG:	// Read from Register
								// Size is 2
				// Todo, Actually read it
			break;
			default:
				printf("Unknown CMD 0x%x\n", m_uCommand);
				exit(0);
			break;
				
		}
	}
	m_uPosition++;
}
bool isActivated()
{
	// Todo: Return actual check
	return true;
}
void CEXIETHERNET::ImmWrite(u32 _uData,  u32 _uSize)
{
	printf("IMM Write, size 0x%x, data 0x%x\n", _uSize, _uData);
	while (_uSize--)
	{
		u8 uByte = _uData >> 24;
		this->Transfer(uByte);
		_uData <<= 8;
	}
}
u32 CEXIETHERNET::ImmRead(u32 _uSize)
{
	u32 uResult = 0;
	u32 uPosition = 0;
	printf("IMM Read, size 0x%x\n", _uSize);
	while (_uSize--)
	{
		u8 uByte = 0;
		this->Transfer(uByte);
		uResult |= uByte << (24-(uPosition++ * 8));
	}
	return uResult;
}

void CEXIETHERNET::DMAWrite(u32 _uAddr, u32 _uSize)
{
	printf("DMAW\n");
	exit(0);
}

void CEXIETHERNET::DMARead(u32 _uAddr, u32 _uSize) 
{
	printf("DMAR\n");
	exit(0);
};
