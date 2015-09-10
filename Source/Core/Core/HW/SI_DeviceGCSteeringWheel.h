// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/HW/SI_DeviceGCController.h"

class CSIDevice_GCSteeringWheel : public CSIDevice_GCController
{
private:
	// Commands
	enum EBufferCommands
	{
		CMD_RESET       = 0x00,
		CMD_ORIGIN      = 0x41,
		CMD_RECALIBRATE = 0x42,
		CMD_ID          = 0xff,
	};

	enum EDirectCommands
	{
		CMD_FORCE = 0x30,
		CMD_WRITE = 0x40
	};

public:
	CSIDevice_GCSteeringWheel(SIDevices device, int _iDeviceNumber);

	int RunBuffer(u8* _pBuffer, int _iLength) override;
	bool GetData(u32& _Hi, u32& _Low) override;
	void SendCommand(u32 _Cmd, u8 _Poll) override;
};
