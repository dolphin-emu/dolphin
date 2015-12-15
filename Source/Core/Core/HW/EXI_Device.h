// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "Common/CommonTypes.h"

class PointerWrap;

enum TEXIDevices : int
{
	EXIDEVICE_DUMMY,
	EXIDEVICE_MEMORYCARD,
	EXIDEVICE_MASKROM,
	EXIDEVICE_AD16,
	EXIDEVICE_MIC,
	EXIDEVICE_ETH,
	EXIDEVICE_AM_BASEBOARD,
	EXIDEVICE_GECKO,
	EXIDEVICE_MEMORYCARDFOLDER, // Only used when creating a device by EXIDevice_Create
	                            // Converted to EXIDEVICE_MEMORYCARD internally
	EXIDEVICE_AGP,
	EXIDEVICE_NONE = (u8)-1
};

class IEXIDevice
{
private:
	// Byte transfer function for this device
	virtual void TransferByte(u8&) {}

public:
	// Immediate copy functions
	virtual void ImmWrite(u32 _uData,  u32 _uSize);
	virtual u32  ImmRead(u32 _uSize);
	virtual void ImmReadWrite(u32 &/*_uData*/, u32 /*_uSize*/) {}

	// DMA copy functions
	virtual void DMAWrite(u32 _uAddr, u32 _uSize);
	virtual void DMARead (u32 _uAddr, u32 _uSize);

	virtual bool UseDelayedTransferCompletion() const { return false; }

	virtual bool IsPresent() const { return false; }
	virtual void SetCS(int) {}
	virtual void DoState(PointerWrap&) {}
	virtual void PauseAndLock(bool doLock, bool unpauseOnUnlock=true) {}
	virtual IEXIDevice* FindDevice(TEXIDevices device_type, int customIndex=-1) { return (device_type == m_deviceType) ? this : nullptr; }

	// Is generating interrupt ?
	virtual bool IsInterruptSet() {return false;}
	virtual ~IEXIDevice() {}

	// for savestates. storing it here seemed cleaner than requiring each implementation to report its type.
	// I know this class is set up like an interface, but no code requires it to be strictly such.
	TEXIDevices m_deviceType;
};

std::unique_ptr<IEXIDevice> EXIDevice_Create(const TEXIDevices device_type, const int channel_num);
