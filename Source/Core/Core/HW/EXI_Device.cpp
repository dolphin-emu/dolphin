// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <memory>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Core/HW/EXI_Device.h"
#include "Core/HW/EXI_DeviceAD16.h"
#include "Core/HW/EXI_DeviceAGP.h"
#include "Core/HW/EXI_DeviceAMBaseboard.h"
#include "Core/HW/EXI_DeviceEthernet.h"
#include "Core/HW/EXI_DeviceGecko.h"
#include "Core/HW/EXI_DeviceIPL.h"
#include "Core/HW/EXI_DeviceMemoryCard.h"
#include "Core/HW/EXI_DeviceMic.h"
#include "Core/HW/Memmap.h"

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
		uResult |= uByte << (24 - (uPosition++ * 8));
	}
	return uResult;
}

void IEXIDevice::DMAWrite(u32 _uAddr, u32 _uSize)
{
	// _dbg_assert_(EXPANSIONINTERFACE, 0);
	while (_uSize--)
	{
		u8 uByte = Memory::Read_U8(_uAddr++);
		TransferByte(uByte);
	}
}

void IEXIDevice::DMARead(u32 _uAddr, u32 _uSize)
{
	// _dbg_assert_(EXPANSIONINTERFACE, 0);
	while (_uSize--)
	{
		u8 uByte = 0;
		TransferByte(uByte);
		Memory::Write_U8(uByte, _uAddr++);
	}
}


// --- class CEXIDummy ---
// Just a dummy that logs reads and writes
// to be used for EXI devices we haven't emulated
// DOES NOT FUNCTION AS "NO DEVICE INSERTED" -> Appears as unknown device
class CEXIDummy : public IEXIDevice
{
	std::string m_strName;

	void TransferByte(u8& _byte) override {}

public:
	CEXIDummy(const std::string& _strName) :
	  m_strName(_strName)
	{
	}

	virtual ~CEXIDummy(){}

	void ImmWrite(u32 data, u32 size) override {INFO_LOG(EXPANSIONINTERFACE, "EXI DUMMY %s ImmWrite: %08x", m_strName.c_str(), data);}
	u32  ImmRead (u32 size) override           {INFO_LOG(EXPANSIONINTERFACE, "EXI DUMMY %s ImmRead", m_strName.c_str()); return 0;}
	void DMAWrite(u32 addr, u32 size) override {INFO_LOG(EXPANSIONINTERFACE, "EXI DUMMY %s DMAWrite: %08x bytes, from %08x to device", m_strName.c_str(), size, addr);}
	void DMARead (u32 addr, u32 size) override {INFO_LOG(EXPANSIONINTERFACE, "EXI DUMMY %s DMARead:  %08x bytes, from device to %08x", m_strName.c_str(), size, addr);}
	bool IsPresent() const override { return true; }
};


// F A C T O R Y
std::unique_ptr<IEXIDevice> EXIDevice_Create(TEXIDevices device_type, const int channel_num)
{
	std::unique_ptr<IEXIDevice> result;

	switch (device_type)
	{
	case EXIDEVICE_DUMMY:
		result = std::make_unique<CEXIDummy>("Dummy");
		break;

	case EXIDEVICE_MEMORYCARD:
	case EXIDEVICE_MEMORYCARDFOLDER:
	{
		bool gci_folder = (device_type == EXIDEVICE_MEMORYCARDFOLDER);
		result = std::make_unique<CEXIMemoryCard>(channel_num, gci_folder);
		break;
	}
	case EXIDEVICE_MASKROM:
		result = std::make_unique<CEXIIPL>();
		break;

	case EXIDEVICE_AD16:
		result = std::make_unique<CEXIAD16>();
		break;

	case EXIDEVICE_MIC:
		result = std::make_unique<CEXIMic>(channel_num);
		break;

	case EXIDEVICE_ETH:
		result = std::make_unique<CEXIETHERNET>();
		break;

	case EXIDEVICE_AM_BASEBOARD:
		result = std::make_unique<CEXIAMBaseboard>();
		break;

	case EXIDEVICE_GECKO:
		result = std::make_unique<CEXIGecko>();
		break;

	case EXIDEVICE_AGP:
		result = std::make_unique<CEXIAgp>(channel_num);
		break;

	case EXIDEVICE_NONE:
	default:
		result = std::make_unique<IEXIDevice>();
		break;
	}

	if (result != nullptr)
		result->m_deviceType = device_type;

	return result;
}
