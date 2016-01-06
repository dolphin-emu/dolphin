// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/HW/SI_Device.h"
#include "Core/HW/SI_DeviceGCController.h"
#include "InputCommon/GCAdapter.h"
#include "InputCommon/GCPadStatus.h"

class CSIDevice_GCAdapter : public CSIDevice_GCController
{
public:
	CSIDevice_GCAdapter(SIDevices device, int _iDeviceNumber);

	GCPadStatus GetPadStatus() override;
	int RunBuffer(u8* _pBuffer, int _iLength) override;
	void SendCommand(u32 _Cmd, u8 _Poll) override;
};
