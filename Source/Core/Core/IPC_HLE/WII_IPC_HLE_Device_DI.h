// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Core/IPC_HLE/WII_IPC_HLE_Device.h"

namespace DiscIO
{
	class IVolume;
	class IFileSystem;
}

class CWII_IPC_HLE_Device_di : public IWII_IPC_HLE_Device
{
public:

	CWII_IPC_HLE_Device_di(u32 _DeviceID, const std::string& _rDeviceName);

	virtual ~CWII_IPC_HLE_Device_di();

	bool Open(u32 _CommandAddress, u32 _Mode) override;
	bool Close(u32 _CommandAddress, bool _bForce) override;

	bool IOCtl(u32 _CommandAddress) override;
	bool IOCtlV(u32 _CommandAddress) override;

	int GetCmdDelay(u32) override;
};
