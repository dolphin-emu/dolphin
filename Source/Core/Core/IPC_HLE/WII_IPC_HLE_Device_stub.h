// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/CommonTypes.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device.h"

class CWII_IPC_HLE_Device_stub : public IWII_IPC_HLE_Device
{
public:
	CWII_IPC_HLE_Device_stub(u32 device_id, const std::string& name);

	IPCCommandResult Open(u32 command_address, u32 mode) override;
	IPCCommandResult Close(u32 command_address, bool force = false) override;
	IPCCommandResult IOCtl(u32 command_address) override;
	IPCCommandResult IOCtlV(u32 command_address) override;
};
