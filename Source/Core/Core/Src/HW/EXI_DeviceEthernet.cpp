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
u32 ReadP, WriteP;
CEXIETHERNET::CEXIETHERNET() :
	m_uPosition(0),
	m_uCommand(0)		
{
	ID = 0x04020200;
}

void CEXIETHERNET::SetCS(int cs)
{
	if (cs)
	{
		ReadP = WriteP = INVALID_P;
		m_uPosition = 0;
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

void CEXIETHERNET::TransferByte(u8& _byte)
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
			default:
				printf("%x %x POS: %d Byte\n", _byte, m_uCommand, m_uPosition);
		}
	}
	m_uPosition++;
}
bool isActivated()
{
	// Todo: Return actual check
	return true;
}
#ifndef _WIN32
inline u32 _rotl(u32 x, int shift) {
	return (x << shift) | (x >> (32 - shift));
}
#endif
void CEXIETHERNET::ImmWrite(u32 _uData,  u32 _uSize)
{
	while (_uSize--)
	{
		u8 uByte = _uData >> 24;
		if(WriteP != INVALID_P)
		{
			if(m_uPosition == 0)
				m_uCommand = uByte;
			else
			{
				switch(uByte)
				{
					case 0x0:
						// Reads the ID in ImmRead after this//
					break;
					case 0x1: /* Network Control Register A, RW */
					// Taken from WhineCube //
					#define RISE(flags) ((uByte & (flags)) && !(RegisterBlock[0x00] & (flags)))
					if(RISE(BBA_NCRA_RESET)) {
						printf("BBA Reset\n");
					}
					if(RISE(BBA_NCRA_SR) && isActivated()) {
						printf("BBA Start Recieve\n");
						//HWGLE(startRecv());
					}
					if(RISE(BBA_NCRA_ST1)) {
						printf("BBA Start Transmit\n");
						if(!mReadyToSend)
						{
							printf("Not ready to Send!\n");
							exit(0);
						}
						//HWGLE(sendPacket(mWriteBuffer.p(), mWriteBuffer.size()));
						mReadyToSend = false;
					}
					//#define MAKE(type, arg) (*(type *)&(arg))
					RegisterBlock[0x00] = *(u8*)&uByte;
					break;
					case 0x42:
					case 0x40: // Should be written
						memcpy(RegisterBlock + WriteP, &uByte, _uSize);
						WriteP = WriteP + _uSize;
					break;
					default:
						printf("%x %x POS: %d Size: %x IMMW POS\n", uByte, m_uCommand, m_uPosition, _uSize);
						exit(0);
					break;
				}
			}
		}
		else if (_uSize == 2 && uByte == 0)
		{
			//printf("BBA device ID read\n");
			m_uCommand = uByte;
			return;
		} else if((_uSize == 4 && (_uData & 0xC0000000) == 0xC0000000) ||
		( _uSize == 2 && (_uData & 0x4000) == 0x4000)) { //Write to BBA register
		/*
		inline DWORD makemaskw(int start, int end) {
	return _rotl((2 << (end - start)) - 1, 31 - end);
}
		inline DWORD getbitsw(DWORD dword, int start, int end) {
	return (dword & makemaskw(start, end)) >> (31 - end);
}*/
			if( _uSize == 4)
				WriteP = (u8)(_uData & (_rotl((2 << ( 23 - 16)) - 1, 23)) >> (31 - 23));
				//WriteP = (BYTE)getbitsw(data, 16, 23);
			else  //size == 2
				WriteP = (u8)((_uData & ~0x4000) & (_rotl((2 << ( 23 - 16)) - 1, 23)) >> (31 - 23));
				//WriteP = (BYTE)getbitsw(data & ~0x4000, 16, 23);  //Dunno about this...
			if(WriteP == 0x48) {
				//mWriteBuffer.clear();
				//mExpectVariableLengthImmWrite = true;
				printf("Prepared for variable length write to address 0x48\n");
			} else {
				printf("BBA Write pointer set to 0x%0*X\n", _uSize, WriteP);
			}
			return;
		}
		//TransferByte(uByte); 
		m_uPosition++;
		_uData <<= 8;
	}
}
u32 CEXIETHERNET::ImmRead(u32 _uSize)
{
	u32 uResult = 0;
	u32 uPosition = 0;
	while (_uSize--)
	{
		u8 uByte = 0;
			switch(m_uCommand)
			{
				case 0x0:
				{
					switch(m_uPosition)
					{
						case 1:
								_dbg_assert_(EXPANSIONINTERFACE, (uByte == 0x00));
						break;
						default:
							//Todo: Could just return the ID here
								uByte = (u8)(ID >> (24-(((m_uPosition - 2) & 3) * 8)));
							printf("Returned ID\n");
						break;
					}
				}
				break;
				default:
					printf("%x %x POS: %d Size: %x IMMR\n", uByte, m_uCommand, m_uPosition, _uSize);
					exit(0);
				break;
			}
		//TransferByte(uByte);
		uResult |= uByte << (24-(uPosition++ * 8));
		m_uPosition++;
	}
	return uResult;
}

void CEXIETHERNET::DMAWrite(u32 _uAddr, u32 _uSize)
{
//	_dbg_assert_(EXPANSIONINTERFACE, 0);
	while (_uSize--) 
	{ 
		u8 uByte = Memory::Read_U8(_uAddr++); 
		if(m_uPosition == 0)
			m_uCommand = uByte;
		else
		{
			switch(m_uCommand)
			{
				default:
					printf("%x %x POS: %d DMAW\n", uByte, m_uCommand, m_uPosition);
				break;
			}
		}
		//TransferByte(uByte); 
		m_uPosition++;
	} 
}

void CEXIETHERNET::DMARead(u32 _uAddr, u32 _uSize) 
{
//	_dbg_assert_(EXPANSIONINTERFACE, 0);
	while (_uSize--) 
	{ 
		u8 uByte = 0; 
		if(m_uPosition == 0)
			m_uCommand = uByte;
		else
		{
			switch(m_uCommand)
			{
				default:
					printf("%x %x POS: %d DMAR\n", uByte, m_uCommand, m_uPosition);
				break;
			}
		}
		//TransferByte(uByte); 
		m_uPosition++;
		Memory::Write_U8(uByte, _uAddr++); 
	}
};
