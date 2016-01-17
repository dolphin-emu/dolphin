// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstring>
#include <string>

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/Network.h"
#include "Common/Logging/Log.h"
#include "Core/HW/Memmap.h"
#include "Core/IPC_HLE/Net/MacHelpers.h"
#include "Core/IPC_HLE/Net/WII_IPC_HLE_Device_net_wd_command.h"

// Handle /dev/net/wd/command requests
CWII_IPC_HLE_Device_net_wd_command::CWII_IPC_HLE_Device_net_wd_command(u32 device_id, const std::string& name)
	: IWII_IPC_HLE_Device(device_id, name)
{
}

CWII_IPC_HLE_Device_net_wd_command::~CWII_IPC_HLE_Device_net_wd_command()
{
}

IPCCommandResult CWII_IPC_HLE_Device_net_wd_command::Open(u32 command_address, u32)
{
	INFO_LOG(WII_IPC_NET, "NET_WD_COMMAND: Open");
	Memory::Write_U32(GetDeviceID(), command_address + 4);
	m_Active = true;
	return GetDefaultReply();
}

IPCCommandResult CWII_IPC_HLE_Device_net_wd_command::Close(u32 command_address, bool force)
{
	INFO_LOG(WII_IPC_NET, "NET_WD_COMMAND: Close");

	if (!force)
		Memory::Write_U32(0, command_address + 4);

	m_Active = false;
	return GetDefaultReply();
}

// This is just for debugging / playing around.
// There really is no reason to implement wd unless we can bend it such that
// we can talk to the DS.
IPCCommandResult CWII_IPC_HLE_Device_net_wd_command::IOCtlV(u32 command_address)
{
	u32 return_value = 0;

	SIOCtlVBuffer command_buffer(command_address);

	switch (command_buffer.Parameter)
	{
	case IOCTLV_WD_SCAN:
		{
		// Gives parameters detailing type of scan and what to match
		// XXX - unused
		// ScanInfo *scan = reinterpret_cast<ScanInfo*>(Memory::GetPointer(command_buffer.InBuffer.at(0).m_Address));

		u16* results = reinterpret_cast<u16*>(Memory::GetPointer(command_buffer.PayloadBuffer.at(0).m_Address));
		// first u16 indicates number of BSSInfo following
		results[0] = Common::swap16(1);

		BSSInfo* bss = reinterpret_cast<BSSInfo*>(&results[1]);
		memset(bss, 0, sizeof(BSSInfo));

		bss->length = Common::swap16(sizeof(BSSInfo));
		bss->rssi = Common::swap16(0xffff);

		for (int i = 0; i < BSSID_SIZE; ++i)
			bss->bssid[i] = i;

		const char* ssid = "dolphin-emu";
		strcpy(reinterpret_cast<char*>(bss->ssid), ssid);
		bss->ssid_length = Common::swap16(static_cast<u16>(strlen(ssid)));

		bss->channel = Common::swap16(2);
		}
		break;

	case IOCTLV_WD_GET_INFO:
		{
		Info* info = reinterpret_cast<Info*>(Memory::GetPointer(command_buffer.PayloadBuffer.at(0).m_Address));
		memset(info, 0, sizeof(Info));
		// Probably used to disallow certain channels?
		memcpy(info->country, "US", 2);
		info->ntr_allowed_channels = Common::swap16(0xfffe);

		u8 address[MAC_ADDRESS_SIZE];
		MacHelpers::GetMacAddress(address);
		memcpy(info->mac, address, sizeof(info->mac));
		}
		break;

	case IOCTLV_WD_GET_MODE:
	case IOCTLV_WD_SET_LINKSTATE:
	case IOCTLV_WD_GET_LINKSTATE:
	case IOCTLV_WD_SET_CONFIG:
	case IOCTLV_WD_GET_CONFIG:
	case IOCTLV_WD_CHANGE_BEACON:
	case IOCTLV_WD_DISASSOC:
	case IOCTLV_WD_MP_SEND_FRAME:
	case IOCTLV_WD_SEND_FRAME:
	case IOCTLV_WD_CALL_WL:
	case IOCTLV_WD_MEASURE_CHANNEL:
	case IOCTLV_WD_GET_LASTERROR:
	case IOCTLV_WD_CHANGE_GAMEINFO:
	case IOCTLV_WD_CHANGE_VTSF:
	case IOCTLV_WD_RECV_FRAME:
	case IOCTLV_WD_RECV_NOTIFICATION:
	default:
		INFO_LOG(WII_IPC_NET, "NET_WD_COMMAND IOCtlV %#x in %i out %i",
		         command_buffer.Parameter, command_buffer.NumberInBuffer, command_buffer.NumberPayloadBuffer);
		for (u32 i = 0; i < command_buffer.NumberInBuffer; ++i)
		{
			INFO_LOG(WII_IPC_NET, "in %i addr %x size %i", i,
			         command_buffer.InBuffer.at(i).m_Address, command_buffer.InBuffer.at(i).m_Size);
			INFO_LOG(WII_IPC_NET, "%s",
				ArrayToString(
					Memory::GetPointer(command_buffer.InBuffer.at(i).m_Address),
					command_buffer.InBuffer.at(i).m_Size).c_str()
				);
		}
		for (u32 i = 0; i < command_buffer.NumberPayloadBuffer; ++i)
		{
			INFO_LOG(WII_IPC_NET, "out %i addr %x size %i", i,
			         command_buffer.PayloadBuffer.at(i).m_Address, command_buffer.PayloadBuffer.at(i).m_Size);
		}
		break;
	}

	Memory::Write_U32(return_value, command_address + 4);
	return GetDefaultReply();
}
