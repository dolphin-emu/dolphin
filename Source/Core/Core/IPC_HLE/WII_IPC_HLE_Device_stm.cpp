// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Core/HW/Memmap.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_stm.h"

// The /dev/stm/immediate
CWII_IPC_HLE_Device_stm_immediate::CWII_IPC_HLE_Device_stm_immediate(u32 device_id, const std::string& name)
	: IWII_IPC_HLE_Device(device_id, name)
{
}

CWII_IPC_HLE_Device_stm_immediate::~CWII_IPC_HLE_Device_stm_immediate()
{
}

IPCCommandResult CWII_IPC_HLE_Device_stm_immediate::Open(u32 command_address, u32)
{
	INFO_LOG(WII_IPC_STM, "STM immediate: Open");
	Memory::Write_U32(GetDeviceID(), command_address + 4);
	m_Active = true;
	return GetDefaultReply();
}

IPCCommandResult CWII_IPC_HLE_Device_stm_immediate::Close(u32 command_address, bool force)
{
	INFO_LOG(WII_IPC_STM, "STM immediate: Close");

	if (!force)
		Memory::Write_U32(0, command_address + 4);

	m_Active = false;
	return GetDefaultReply();
}

IPCCommandResult CWII_IPC_HLE_Device_stm_immediate::IOCtl(u32 command_address)
{
	u32 parameter       = Memory::Read_U32(command_address + 0x0C);
	u32 buffer_in       = Memory::Read_U32(command_address + 0x10);
	u32 buffer_in_size  = Memory::Read_U32(command_address + 0x14);
	u32 buffer_out      = Memory::Read_U32(command_address + 0x18);
	u32 buffer_out_size = Memory::Read_U32(command_address + 0x1C);

	// Prepare the out buffer(s) with zeroes as a safety precaution
	// to avoid returning bad values
	Memory::Memset(buffer_out, 0, buffer_out_size);
	u32 return_value = 0;

	switch (parameter)
	{
	case IOCTL_STM_RELEASE_EH:
		INFO_LOG(WII_IPC_STM, "%s - IOCtl:", GetDeviceName().c_str());
		INFO_LOG(WII_IPC_STM, "    IOCTL_STM_RELEASE_EH");
		break;

	case IOCTL_STM_HOTRESET:
		INFO_LOG(WII_IPC_STM, "%s - IOCtl:", GetDeviceName().c_str());
		INFO_LOG(WII_IPC_STM, "    IOCTL_STM_HOTRESET");
		break;

	case IOCTL_STM_VIDIMMING: // (Input: 20 bytes, Output: 20 bytes)
		INFO_LOG(WII_IPC_STM, "%s - IOCtl:", GetDeviceName().c_str());
		INFO_LOG(WII_IPC_STM, "    IOCTL_STM_VIDIMMING");
		//DumpCommands(BufferIn, BufferInSize / 4, LogTypes::WII_IPC_STM);
		//Memory::Write_U32(1, BufferOut);
		//ReturnValue = 1;
		break;

	case IOCTL_STM_LEDMODE:  // (Input: 20 bytes, Output: 20 bytes)
		INFO_LOG(WII_IPC_STM, "%s - IOCtl:", GetDeviceName().c_str());
		INFO_LOG(WII_IPC_STM, "    IOCTL_STM_LEDMODE");
		break;

	default:
		{
			_dbg_assert_msg_(WII_IPC_STM, 0, "CWII_IPC_HLE_Device_stm_immediate: 0x%x", parameter);

			INFO_LOG(WII_IPC_STM, "%s - IOCtl:", GetDeviceName().c_str());
			DEBUG_LOG(WII_IPC_STM, "    Parameter: 0x%x", parameter);
			DEBUG_LOG(WII_IPC_STM, "    InBuffer: 0x%08x", buffer_in);
			DEBUG_LOG(WII_IPC_STM, "    InBufferSize: 0x%08x", buffer_in_size);
			DEBUG_LOG(WII_IPC_STM, "    OutBuffer: 0x%08x", buffer_out);
			DEBUG_LOG(WII_IPC_STM, "    OutBufferSize: 0x%08x", buffer_out_size);
		}
		break;
	}

	// Write return value to the IPC call
	Memory::Write_U32(return_value, command_address + 0x4);
	return GetDefaultReply();
}

// The /dev/stm/eventhook
CWII_IPC_HLE_Device_stm_eventhook::CWII_IPC_HLE_Device_stm_eventhook(u32 device_id, const std::string& name)
	: IWII_IPC_HLE_Device(device_id, name)
{
}

CWII_IPC_HLE_Device_stm_eventhook::~CWII_IPC_HLE_Device_stm_eventhook()
{
}

IPCCommandResult CWII_IPC_HLE_Device_stm_eventhook::Open(u32 command_address, u32)
{
	Memory::Write_U32(GetDeviceID(), command_address + 4);
	m_Active = true;
	return GetDefaultReply();
}

IPCCommandResult CWII_IPC_HLE_Device_stm_eventhook::Close(u32 command_address, bool force)
{
	m_EventHookAddress = 0;

	INFO_LOG(WII_IPC_STM, "STM eventhook: Close");

	if (!force)
		Memory::Write_U32(0, command_address + 4);

	m_Active = false;
	return GetDefaultReply();
}

IPCCommandResult CWII_IPC_HLE_Device_stm_eventhook::IOCtl(u32 command_address)
{
	u32 parameter       = Memory::Read_U32(command_address + 0x0C);
	u32 buffer_in       = Memory::Read_U32(command_address + 0x10);
	u32 buffer_in_size  = Memory::Read_U32(command_address + 0x14);
	u32 buffer_out      = Memory::Read_U32(command_address + 0x18);
	u32 buffer_out_size = Memory::Read_U32(command_address + 0x1C);

	// Prepare the out buffer(s) with zeros as a safety precaution
	// to avoid returning bad values
	Memory::Memset(buffer_out, 0, buffer_out_size);
	u32 return_value = 0;

	// write return value
	switch (parameter)
	{
	case IOCTL_STM_EVENTHOOK:
		{
			m_EventHookAddress = command_address;

			INFO_LOG(WII_IPC_STM, "%s registers event hook:", GetDeviceName().c_str());
			DEBUG_LOG(WII_IPC_STM, "%x - IOCTL_STM_EVENTHOOK", parameter);
			DEBUG_LOG(WII_IPC_STM, "BufferIn: 0x%08x", buffer_in);
			DEBUG_LOG(WII_IPC_STM, "BufferInSize: 0x%08x", buffer_in_size);
			DEBUG_LOG(WII_IPC_STM, "BufferOut: 0x%08x", buffer_out);
			DEBUG_LOG(WII_IPC_STM, "BufferOutSize: 0x%08x", buffer_out_size);

			DumpCommands(buffer_in, buffer_in_size / 4, LogTypes::WII_IPC_STM);
		}
		break;

	default:
		_dbg_assert_msg_(WII_IPC_STM, 0, "unknown %s ioctl %x",
			GetDeviceName().c_str(), parameter);
		break;
	}

	// Write return value to the IPC call, 0 means success
	Memory::Write_U32(return_value, command_address + 0x4);
	return GetDefaultReply();
}
