// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/HW/SI_Device.h"
#include "Core/HW/SI_DeviceGCController.h"
#include "InputCommon/GCPadStatus.h"

class CSIDevice_GCAdapter : public CSIDevice_GCController
{
public:
	CSIDevice_GCAdapter(SIDevices device, int _iDeviceNumber);

	GCPadStatus GetPadStatus() override;
	int RunBuffer(u8* buffer, int length) override;
};
