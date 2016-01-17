// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>
#include "Common/CommonTypes.h"
#include "Common/Network.h"
#include "Core/IPC_HLE/Net/MacHelpers.h"
#include "Core/IPC_HLE/Net/WII_IPC_HLE_Device_net_ncd_manage.h"

// Handle /dev/net/ncd/manage requests
CWII_IPC_HLE_Device_net_ncd_manage::CWII_IPC_HLE_Device_net_ncd_manage(u32 device_id, const std::string& name)
	: IWII_IPC_HLE_Device(device_id, name)
{
}

CWII_IPC_HLE_Device_net_ncd_manage::~CWII_IPC_HLE_Device_net_ncd_manage()
{
}

IPCCommandResult CWII_IPC_HLE_Device_net_ncd_manage::Open(u32 command_address, u32)
{
	INFO_LOG(WII_IPC_NET, "NET_NCD_MANAGE: Open");
	Memory::Write_U32(GetDeviceID(), command_address + 4);
	m_Active = true;
	return GetDefaultReply();
}

IPCCommandResult CWII_IPC_HLE_Device_net_ncd_manage::Close(u32 command_address, bool force)
{
	INFO_LOG(WII_IPC_NET, "NET_NCD_MANAGE: Close");

	if (!force)
		Memory::Write_U32(0, command_address + 4);

	m_Active = false;
	return GetDefaultReply();
}

IPCCommandResult CWII_IPC_HLE_Device_net_ncd_manage::IOCtlV(u32 command_address)
{
	u32 return_value = 0;
	u32 common_result = 0;
	u32 common_vector = 0;

	SIOCtlVBuffer command_buffer(command_address);

	switch (command_buffer.Parameter)
	{
	case IOCTLV_NCD_LOCKWIRELESSDRIVER:
		break;

	case IOCTLV_NCD_UNLOCKWIRELESSDRIVER:
		//Memory::Read_U32(command_buffer.InBuffer.at(0).m_Address);
		break;

	case IOCTLV_NCD_GETCONFIG:
		INFO_LOG(WII_IPC_NET, "NET_NCD_MANAGE: IOCTLV_NCD_GETCONFIG");
		m_config.WriteToMem(command_buffer.PayloadBuffer.at(0).m_Address);
		common_vector = 1;
		break;

	case IOCTLV_NCD_SETCONFIG:
		INFO_LOG(WII_IPC_NET, "NET_NCD_MANAGE: IOCTLV_NCD_SETCONFIG");
		m_config.ReadFromMem(command_buffer.InBuffer.at(0).m_Address);
		break;

	case IOCTLV_NCD_READCONFIG:
		INFO_LOG(WII_IPC_NET, "NET_NCD_MANAGE: IOCTLV_NCD_READCONFIG");
		m_config.ReadConfig();
		m_config.WriteToMem(command_buffer.PayloadBuffer.at(0).m_Address);
		common_vector = 1;
		break;

	case IOCTLV_NCD_WRITECONFIG:
		INFO_LOG(WII_IPC_NET, "NET_NCD_MANAGE: IOCTLV_NCD_WRITECONFIG");
		m_config.ReadFromMem(command_buffer.InBuffer.at(0).m_Address);
		m_config.WriteConfig();
		break;

	case IOCTLV_NCD_GETLINKSTATUS:
		INFO_LOG(WII_IPC_NET, "NET_NCD_MANAGE: IOCTLV_NCD_GETLINKSTATUS");
		// Always connected
		Memory::Write_U32(WiiNetConfig::Status::LINK_WIRED, command_buffer.PayloadBuffer.at(0).m_Address + 4);
		break;

	case IOCTLV_NCD_GETWIRELESSMACADDRESS:
		INFO_LOG(WII_IPC_NET, "NET_NCD_MANAGE: IOCTLV_NCD_GETWIRELESSMACADDRESS");

		u8 address[MAC_ADDRESS_SIZE];
		MacHelpers::GetMacAddress(address);
		Memory::CopyToEmu(command_buffer.PayloadBuffer.at(1).m_Address, address, sizeof(address));
		break;

	default:
		INFO_LOG(WII_IPC_NET, "NET_NCD_MANAGE IOCtlV: %#x", command_buffer.Parameter);
		break;
	}

	Memory::Write_U32(common_result, command_buffer.PayloadBuffer.at(common_vector).m_Address);
	if (common_vector == 1)
	{
		Memory::Write_U32(common_result, command_buffer.PayloadBuffer.at(common_vector).m_Address + 4);
	}
	Memory::Write_U32(return_value, command_address + 4);
	return GetDefaultReply();
}
