// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <deque>
#include "Core/IPC_HLE/WII_IPC_HLE_Device.h"

class PointerWrap;
namespace DVDInterface { enum DIInterruptType : int; }

class CWII_IPC_HLE_Device_di : public IWII_IPC_HLE_Device
{
public:

	CWII_IPC_HLE_Device_di(u32 _DeviceID, const std::string& _rDeviceName);

	virtual ~CWII_IPC_HLE_Device_di();

	void DoState(PointerWrap& p) override;

	IPCCommandResult Open(u32 _CommandAddress, u32 _Mode) override;
	IPCCommandResult Close(u32 _CommandAddress, bool _bForce) override;

	IPCCommandResult IOCtl(u32 _CommandAddress) override;
	IPCCommandResult IOCtlV(u32 _CommandAddress) override;

	void FinishIOCtl(DVDInterface::DIInterruptType interrupt_type);
private:

	void StartIOCtl(u32 command_address);

	std::deque<u32> m_commands_to_execute;
};
