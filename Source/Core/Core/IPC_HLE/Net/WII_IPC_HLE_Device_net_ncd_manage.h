// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/CommonTypes.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device.h"
#include "Core/IPC_HLE/Net/WiiNetConfig.h"

// Interface for reading and changing network configuration (probably some other stuff as well)
class CWII_IPC_HLE_Device_net_ncd_manage final : public IWII_IPC_HLE_Device
{
public:
	CWII_IPC_HLE_Device_net_ncd_manage(u32 device_id, const std::string& name);
	~CWII_IPC_HLE_Device_net_ncd_manage();

	IPCCommandResult Open(u32 command_address, u32 mode) override;
	IPCCommandResult Close(u32 command_address, bool force) override;
	IPCCommandResult IOCtlV(u32 command_address) override;

private:
	enum
	{
		IOCTLV_NCD_LOCKWIRELESSDRIVER    = 0x1,  // NCDLockWirelessDriver
		IOCTLV_NCD_UNLOCKWIRELESSDRIVER  = 0x2,  // NCDUnlockWirelessDriver
		IOCTLV_NCD_GETCONFIG             = 0x3,  // NCDiGetConfig
		IOCTLV_NCD_SETCONFIG             = 0x4,  // NCDiSetConfig
		IOCTLV_NCD_READCONFIG            = 0x5,
		IOCTLV_NCD_WRITECONFIG           = 0x6,
		IOCTLV_NCD_GETLINKSTATUS         = 0x7,  // NCDGetLinkStatus
		IOCTLV_NCD_GETWIRELESSMACADDRESS = 0x8,  // NCDGetWirelessMacAddress
	};

	WiiNetConfig m_config;
};
