// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Core/HW/EXI_DeviceIPL.h"
#include "Core/HW/Memmap.h"
#include "Core/IPC_HLE/Net/WII_IPC_HLE_Device_net_kd_time.h"


CWII_IPC_HLE_Device_net_kd_time::CWII_IPC_HLE_Device_net_kd_time(u32 device_id, const std::string& name)
	: IWII_IPC_HLE_Device(device_id, name)
{
}

CWII_IPC_HLE_Device_net_kd_time::~CWII_IPC_HLE_Device_net_kd_time()
{
}

IPCCommandResult CWII_IPC_HLE_Device_net_kd_time::Open(u32 command_address, u32)
{
	INFO_LOG(WII_IPC_NET, "NET_KD_TIME: Open");
	Memory::Write_U32(GetDeviceID(), command_address + 4);
	return GetDefaultReply();
}

IPCCommandResult CWII_IPC_HLE_Device_net_kd_time::Close(u32 command_address, bool force)
{
	INFO_LOG(WII_IPC_NET, "NET_KD_TIME: Close");

	if (!force)
		Memory::Write_U32(0, command_address + 4);

	return GetDefaultReply();
}

IPCCommandResult CWII_IPC_HLE_Device_net_kd_time::IOCtl(u32 command_address)
{
	u32 parameter  = Memory::Read_U32(command_address + 0x0C);
	u32 buffer_in  = Memory::Read_U32(command_address + 0x10);
	u32 buffer_out = Memory::Read_U32(command_address + 0x18);

	u32 result = 0;
	u32 common_result = 0;
	// TODO Writes stuff to /shared2/nwc24/misc.bin
	//u32 update_misc = 0;

	switch (parameter)
	{
	case IOCTL_NW24_GET_UNIVERSAL_TIME:
		Memory::Write_U64(GetAdjustedUTC(), buffer_out + 4);
		break;

	case IOCTL_NW24_SET_UNIVERSAL_TIME:
		SetAdjustedUTC(Memory::Read_U64(buffer_in));
		//update_misc = Memory::Read_U32(buffer_in + 8);
		break;

	case IOCTL_NW24_SET_RTC_COUNTER:
		m_rtc = Memory::Read_U32(buffer_in);
		//update_misc = Memory::Read_U32(buffer_in + 4);
		break;

	case IOCTL_NW24_GET_TIME_DIFF:
		Memory::Write_U64(GetAdjustedUTC() - m_rtc, buffer_out + 4);
		break;

	case IOCTL_NW24_UNIMPLEMENTED:
		result = -9;
		break;

	default:
		ERROR_LOG(WII_IPC_NET, "%s - unknown IOCtl: %x\n", GetDeviceName().c_str(), parameter);
		break;
	}

	// Write return values
	Memory::Write_U32(common_result, buffer_out);
	Memory::Write_U32(result, command_address + 4);
	return GetDefaultReply();
}

// Returns seconds since Wii epoch
// +/- any bias set from IOCTL_NW24_SET_UNIVERSAL_TIME
u64 CWII_IPC_HLE_Device_net_kd_time::GetAdjustedUTC() const
{
	return CEXIIPL::GetGCTime() - wii_bias + m_utc_difference;
}

// Store the difference between what the Wii thinks is UTC and
// what the host OS thinks
void CWII_IPC_HLE_Device_net_kd_time::SetAdjustedUTC(u64 wii_utc)
{
	m_utc_difference = CEXIIPL::GetGCTime() - wii_bias - wii_utc;
}
