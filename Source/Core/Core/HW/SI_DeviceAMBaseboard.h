// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/HW/SI_Device.h"

// triforce (GC-AM) baseboard
class CSIDevice_AMBaseboard : public ISIDevice
{
private:
	enum EBufferCommands
	{
		CMD_RESET = 0x00,
		CMD_GCAM  = 0x70,
	};

public:
	// constructor
	CSIDevice_AMBaseboard(SIDevices device, int _iDeviceNumber);

	// run the SI Buffer
	int RunBuffer(u8* _pBuffer, int _iLength) override;

	// return true on new data
	bool GetData(u32& _Hi, u32& _Low) override;

	// send a command directly
	void SendCommand(u32 _Cmd, u8 _Poll) override;
};
