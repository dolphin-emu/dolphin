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

#include "Memmap.h"

#include "EXI_Device.h"
#include "EXI_DeviceIPL.h"
#include "EXI_DeviceMemoryCard.h"
#include "EXI_DeviceAD16.h"
#include "EXI_DeviceMic.h"
#include "EXI_DeviceEthernet.h"

#include "../Core.h"
#include "../ConfigManager.h"

//////////////////////////////////////////////////////////////////////////
// --- interface IEXIDevice ---
//////////////////////////////////////////////////////////////////////////
void IEXIDevice::ImmWrite(u32 _uData,  u32 _uSize)
{
	while (_uSize--)
	{
		u8 uByte = _uData >> 24;
		TransferByte(uByte);
		_uData <<= 8;
	}
}

u32 IEXIDevice::ImmRead(u32 _uSize)
{
	u32 uResult = 0;
	u32 uPosition = 0;
	while (_uSize--)
	{
		u8 uByte = 0;
		TransferByte(uByte);
		uResult |= uByte << (24-(uPosition++ * 8));
	}
	return uResult;
}

void IEXIDevice::DMAWrite(u32 _uAddr, u32 _uSize)
{
//	_dbg_assert_(EXPANSIONINTERFACE, 0);
	while (_uSize--)
	{
		u8 uByte = Memory::Read_U8(_uAddr++);
		TransferByte(uByte);
	}
}

void IEXIDevice::DMARead(u32 _uAddr, u32 _uSize)
{
//	_dbg_assert_(EXPANSIONINTERFACE, 0);
	while (_uSize--)
	{
		u8 uByte = 0;
		TransferByte(uByte);
		Memory::Write_U8(uByte, _uAddr++);
	}
};

bool IEXIDevice::IsPresent()
{
	return false;
}

void IEXIDevice::Update()
{
}

bool IEXIDevice::IsInterruptSet()
{
	return false;
}

void IEXIDevice::SetCS(int _iCS)
{
}

//////////////////////////////////////////////////////////////////////////
// --- class CEXIDummy ---
//////////////////////////////////////////////////////////////////////////
// Just a dummy that logs reads and writes
// to be used for EXI devices we haven't emulated
class CEXIDummy : public IEXIDevice
{
	std::string m_strName;

	void TransferByte(u8& _byte) {}

public:
	CEXIDummy(const std::string& _strName) :
	  m_strName(_strName)
	{
	}

	virtual ~CEXIDummy(){}

	void ImmWrite(u32 data, u32 size)	{INFO_LOG(EXPANSIONINTERFACE, "EXI DUMMY %s ImmWrite: %08x", m_strName.c_str(), data);}
	u32 ImmRead (u32 size)				{INFO_LOG(EXPANSIONINTERFACE, "EXI DUMMY %s ImmRead", m_strName.c_str()); return 0;}
	void DMAWrite(u32 addr, u32 size)	{INFO_LOG(EXPANSIONINTERFACE, "EXI DUMMY %s DMAWrite: %08x bytes, from %08x to device", m_strName.c_str(), size, addr);}
	void DMARead (u32 addr, u32 size)	{INFO_LOG(EXPANSIONINTERFACE, "EXI DUMMY %s DMARead:  %08x bytes, from device to %08x", m_strName.c_str(), size, addr);}
};

//////////////////////////////////////////////////////////////////////////
// F A C T O R Y /////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

IEXIDevice* EXIDevice_Create(TEXIDevices _EXIDevice)
{
	switch(_EXIDevice)
	{
	case EXIDEVICE_DUMMY:
		return new CEXIDummy("Dummy");
		break;

	case EXIDEVICE_MEMORYCARD_A:
        return new CEXIMemoryCard("MemoryCardA", SConfig::GetInstance().m_strMemoryCardA, 0);
		break;

	case EXIDEVICE_MEMORYCARD_B:
		return new CEXIMemoryCard("MemoryCardB", SConfig::GetInstance().m_strMemoryCardB, 1);
		break;

	case EXIDEVICE_IPL:
		return new CEXIIPL();
		break;

	case EXIDEVICE_AD16:
		return new CEXIAD16();
		break;

	case EXIDEVICE_MIC:
		return new CEXIMic(1);
		break;

	case EXIDEVICE_ETH:
		return new CEXIETHERNET();
		break;

	}
	return NULL;
}
