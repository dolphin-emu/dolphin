// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Memmap.h"

#include "EXI_Device.h"
#include "EXI_DeviceIPL.h"
#include "EXI_DeviceMemoryCard.h"
#include "EXI_DeviceAD16.h"
#include "EXI_DeviceMic.h"
#include "EXI_DeviceEthernet.h"
#include "EXI_DeviceAMBaseboard.h"
#include "EXI_DeviceGecko.h"

#include "../Core.h"
#include "../ConfigManager.h"


// --- interface IEXIDevice ---
void IEXIDevice::ImmWrite(u32 _uData, u32 _uSize)
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


// --- class CEXIDummy ---
// Just a dummy that logs reads and writes
// to be used for EXI devices we haven't emulated
// DOES NOT FUNCTION AS "NO DEVICE INSERTED" -> Appears as unknown device
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
	u32  ImmRead (u32 size)				{INFO_LOG(EXPANSIONINTERFACE, "EXI DUMMY %s ImmRead", m_strName.c_str()); return 0;}
	void DMAWrite(u32 addr, u32 size)	{INFO_LOG(EXPANSIONINTERFACE, "EXI DUMMY %s DMAWrite: %08x bytes, from %08x to device", m_strName.c_str(), size, addr);}
	void DMARead (u32 addr, u32 size)	{INFO_LOG(EXPANSIONINTERFACE, "EXI DUMMY %s DMARead:  %08x bytes, from device to %08x", m_strName.c_str(), size, addr);}
};


// F A C T O R Y 
IEXIDevice* EXIDevice_Create(TEXIDevices device_type, const int channel_num)
{
	IEXIDevice* result = NULL;

	switch (device_type)
	{
	case EXIDEVICE_DUMMY:
		result = new CEXIDummy("Dummy");
		break;

	case EXIDEVICE_MEMORYCARD:
		result = new CEXIMemoryCard(channel_num);
		break;

	case EXIDEVICE_MASKROM:
		result = new CEXIIPL();
		break;

	case EXIDEVICE_AD16:
		result = new CEXIAD16();
		break;

	case EXIDEVICE_MIC:
		result = new CEXIMic(channel_num);
		break;

	case EXIDEVICE_ETH:
		result = new CEXIETHERNET();
		break;

	case EXIDEVICE_AM_BASEBOARD:
		result = new CEXIAMBaseboard();
		break;

	case EXIDEVICE_GECKO:
		result = new CEXIGecko();
		break;

	case EXIDEVICE_NONE:
	default:
		result = new IEXIDevice();
		break;
	}

	if (result != NULL)
		result->m_deviceType = device_type;

	return result;
}
