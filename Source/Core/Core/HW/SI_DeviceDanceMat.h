// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"
#include "Core/HW/SI_DeviceGCController.h"

struct GCPadStatus;

class CSIDevice_DanceMat : public CSIDevice_GCController
{
public:
	CSIDevice_DanceMat(SIDevices device, int _iDeviceNumber);

	int RunBuffer(u8* _pBuffer, int _iLength) override;
	u32 MapPadStatus(const GCPadStatus& pad_status) override;
	bool GetData(u32& _Hi, u32& _Low) override;
};
