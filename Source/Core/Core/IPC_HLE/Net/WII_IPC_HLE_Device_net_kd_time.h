// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "Common/CommonTypes.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device.h"

class CWII_IPC_HLE_Device_net_kd_time final : public IWII_IPC_HLE_Device
{
public:
	CWII_IPC_HLE_Device_net_kd_time(u32 device_id, const std::string& name);
	~CWII_IPC_HLE_Device_net_kd_time();

	IPCCommandResult Open(u32 command_address, u32 mode) override;
	IPCCommandResult Close(u32 command_address, bool force) override;
	IPCCommandResult IOCtl(u32 command_address) override;

private:
	enum
	{
		IOCTL_NW24_GET_UNIVERSAL_TIME = 0x14,
		IOCTL_NW24_SET_UNIVERSAL_TIME = 0x15,
		IOCTL_NW24_UNIMPLEMENTED      = 0x16,
		IOCTL_NW24_SET_RTC_COUNTER    = 0x17,
		IOCTL_NW24_GET_TIME_DIFF      = 0x18,
	};

	u64 m_rtc = 0;
	s64 m_utc_difference = 0;

	// TODO: depending on CEXIIPL is a hack which I don't feel like removing
	// because the function itself is pretty hackish; wait until I re-port my
	// netplay rewrite; also, is that random 16:00:38 actually meaningful?
	// seems very very doubtful since Wii was released in 2006

	// Seconds between 1.1.2000 and 4.1.2008 16:00:38
	static constexpr u64 wii_bias = 0x477E5826 - 0x386D4380;

	// Returns seconds since Wii epoch
	// +/- any bias set from IOCTL_NW24_SET_UNIVERSAL_TIME
	u64 GetAdjustedUTC() const;

	// Store the difference between what the Wii thinks is UTC and
	// what the host OS thinks
	void SetAdjustedUTC(u64 wii_utc);
};
