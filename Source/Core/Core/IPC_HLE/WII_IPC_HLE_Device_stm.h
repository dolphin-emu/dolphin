// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "Core/IPC_HLE/WII_IPC_HLE_Device.h"

enum
{
	IOCTL_STM_EVENTHOOK       = 0x1000,
	IOCTL_STM_GET_IDLEMODE    = 0x3001,
	IOCTL_STM_RELEASE_EH      = 0x3002,
	IOCTL_STM_HOTRESET        = 0x2001,
	IOCTL_STM_HOTRESET_FOR_PD = 0x2002,
	IOCTL_STM_SHUTDOWN        = 0x2003,
	IOCTL_STM_IDLE            = 0x2004,
	IOCTL_STM_WAKEUP          = 0x2005,
	IOCTL_STM_VIDIMMING       = 0x5001,
	IOCTL_STM_LEDFLASH        = 0x6001,
	IOCTL_STM_LEDMODE         = 0x6002,
	IOCTL_STM_READVER         = 0x7001,
	IOCTL_STM_READDDRREG      = 0x4001,
	IOCTL_STM_READDDRREG2     = 0x4002,
};

// The /dev/stm/immediate
class CWII_IPC_HLE_Device_stm_immediate final : public IWII_IPC_HLE_Device
{
public:
	CWII_IPC_HLE_Device_stm_immediate(u32 device_id, const std::string& name);
	virtual ~CWII_IPC_HLE_Device_stm_immediate();

	IPCCommandResult Open(u32 command_address, u32 mode) override;
	IPCCommandResult Close(u32 command_address, bool force) override;
	IPCCommandResult IOCtl(u32 command_address) override;
};

// The /dev/stm/eventhook
class CWII_IPC_HLE_Device_stm_eventhook final : public IWII_IPC_HLE_Device
{
public:
	CWII_IPC_HLE_Device_stm_eventhook(u32 device_id, const std::string& name);
	virtual ~CWII_IPC_HLE_Device_stm_eventhook();

	IPCCommandResult Open(u32 command_address, u32 mode) override;
	IPCCommandResult Close(u32 command_address, bool force) override;
	IPCCommandResult IOCtl(u32 command_address) override;

	// STATE_TO_SAVE
	u32 m_EventHookAddress = 0;
};
