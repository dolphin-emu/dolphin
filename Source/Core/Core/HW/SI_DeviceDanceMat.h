// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Core/HW/SI_DeviceGCController.h"

struct GCPadStatus;

class CSIDevice_DanceMat : public CSIDevice_GCController
{
public:
	CSIDevice_DanceMat(SIDevices device, int _iDeviceNumber);
	virtual int RunBuffer(u8* _pBuffer, int _iLength) override;
	virtual u32 MapPadStatus(const GCPadStatus& pad_status) override;
	virtual bool GetData(u32& _Hi, u32& _Low) override;
};
