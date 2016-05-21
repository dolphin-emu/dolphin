// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <queue>
#include <string>
#include <vector>

#include "Common/ChunkFile.h"
#include "Common/StringUtil.h"
#include "Common/Logging/Log.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/SystemTimers.h"
#include "Core/IPC_HLE/WII_IPC_HLE.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device.h"

SIOCtlVBuffer::SIOCtlVBuffer(u32 address)
	: m_Address(address)
{
	// These are the Ioctlv parameters in the IOS communication. The BufferVector
	// is a memory address offset at where the in and out buffer addresses are
	// stored.
	Parameter           = Memory::Read_U32(m_Address + 0x0C); // command 3, arg0
	NumberInBuffer      = Memory::Read_U32(m_Address + 0x10); // 4, arg1
	NumberPayloadBuffer = Memory::Read_U32(m_Address + 0x14); // 5, arg2
	BufferVector        = Memory::Read_U32(m_Address + 0x18); // 6, arg3

	// The start of the out buffer
	u32 BufferVectorOffset = BufferVector;

	// Write the address and size for all in messages
	for (u32 i = 0; i < NumberInBuffer; i++)
	{
		SBuffer Buffer;
		Buffer.m_Address = Memory::Read_U32(BufferVectorOffset);
		BufferVectorOffset += 4;
		Buffer.m_Size    = Memory::Read_U32(BufferVectorOffset);
		BufferVectorOffset += 4;
		InBuffer.push_back(Buffer);
		DEBUG_LOG(WII_IPC_HLE, "SIOCtlVBuffer in%i: 0x%08x, 0x%x",
		          i, Buffer.m_Address, Buffer.m_Size);
	}

	// Write the address and size for all out or in-out messages
	for (u32 i = 0; i < NumberPayloadBuffer; i++)
	{
		SBuffer Buffer;
		Buffer.m_Address = Memory::Read_U32(BufferVectorOffset);
		BufferVectorOffset += 4;
		Buffer.m_Size    = Memory::Read_U32(BufferVectorOffset);
		BufferVectorOffset += 4;
		PayloadBuffer.push_back(Buffer);
		DEBUG_LOG(WII_IPC_HLE, "SIOCtlVBuffer io%i: 0x%08x, 0x%x",
		          i, Buffer.m_Address, Buffer.m_Size);
	}
}

IWII_IPC_HLE_Device::IWII_IPC_HLE_Device(u32 device_id, const std::string& name, bool hardware)
	: m_Name(name)
	, m_DeviceID(device_id)
	, m_Hardware(hardware)
	, m_Active(false)
{
}

IWII_IPC_HLE_Device::~IWII_IPC_HLE_Device()
{
}

void IWII_IPC_HLE_Device::DoState(PointerWrap& p)
{
	DoStateShared(p);
	p.Do(m_Active);
}

void IWII_IPC_HLE_Device::DoStateShared(PointerWrap& p)
{
	p.Do(m_Name);
	p.Do(m_DeviceID);
	p.Do(m_Hardware);
	p.Do(m_Active);
}

IPCCommandResult IWII_IPC_HLE_Device::Open(u32 command_address, u32)
{
	WARN_LOG(WII_IPC_HLE, "%s does not support Open()", m_Name.c_str());
	Memory::Write_U32(FS_ENOENT, command_address + 4);
	m_Active = true;
	return GetDefaultReply();
}

IPCCommandResult IWII_IPC_HLE_Device::Close(u32 command_address, bool force)
{
	WARN_LOG(WII_IPC_HLE, "%s does not support Close()", m_Name.c_str());

	if (!force)
		Memory::Write_U32(FS_EINVAL, command_address + 4);

	m_Active = false;
	return GetDefaultReply();
}

IPCCommandResult IWII_IPC_HLE_Device::Seek(u32)
{
	WARN_LOG(WII_IPC_HLE, "%s does not support Seek()", m_Name.c_str());
	return GetDefaultReply();
}

IPCCommandResult IWII_IPC_HLE_Device::Read(u32)
{
	WARN_LOG(WII_IPC_HLE, "%s does not support Read()", m_Name.c_str());
	return GetDefaultReply();
}

IPCCommandResult IWII_IPC_HLE_Device::Write(u32)
{
	WARN_LOG(WII_IPC_HLE, "%s does not support Write()", m_Name.c_str());
	return GetDefaultReply();
}

IPCCommandResult IWII_IPC_HLE_Device::IOCtl(u32)
{
	WARN_LOG(WII_IPC_HLE, "%s does not support IOCtl()", m_Name.c_str());
	return GetDefaultReply();
}

IPCCommandResult IWII_IPC_HLE_Device::IOCtlV(u32)
{
	WARN_LOG(WII_IPC_HLE, "%s does not support IOCtlV()", m_Name.c_str());
	return GetDefaultReply();
}

IPCCommandResult IWII_IPC_HLE_Device::GetDefaultReply()
{
	return { true, SystemTimers::GetTicksPerSecond() / 4000 };
}

IPCCommandResult IWII_IPC_HLE_Device::GetNoReply()
{
	return { false, 0 };
}

void IWII_IPC_HLE_Device::DumpCommands(u32 command_address, size_t number_of_commands, LogTypes::LOG_TYPE type, LogTypes::LOG_LEVELS verbosity) const
{
	GENERIC_LOG(type, verbosity, "CommandDump of %s", GetDeviceName().c_str());

	for (u32 i = 0; i < number_of_commands; i++)
	{
		GENERIC_LOG(type, verbosity, "    Command%02i: 0x%08x", i,
		            Memory::Read_U32(command_address + i*4));
	}
}

void IWII_IPC_HLE_Device::DumpAsync(u32 buffer_vector, u32 number_in_buffer, u32 number_out_buffer, LogTypes::LOG_TYPE type, LogTypes::LOG_LEVELS verbosity) const
{
	GENERIC_LOG(type, verbosity, "======= DumpAsync ======");

	u32 buffer_offset = buffer_vector;
	for (u32 i = 0; i < number_in_buffer; i++)
	{
		u32 in_buffer      = Memory::Read_U32(buffer_offset); buffer_offset += 4;
		u32 in_buffer_size = Memory::Read_U32(buffer_offset); buffer_offset += 4;

		GENERIC_LOG(type, LogTypes::LINFO, "%s - IOCtlV InBuffer[%i]:", GetDeviceName().c_str(), i);

		std::string temp;
		for (u32 j = 0; j < in_buffer_size; j++)
		{
			temp += StringFromFormat("%02x ", Memory::Read_U8(in_buffer + j));
		}

		GENERIC_LOG(type, LogTypes::LDEBUG, "    Buffer: %s", temp.c_str());
	}

	for (u32 i = 0; i < number_out_buffer; i++)
	{
		u32 out_buffer      = Memory::Read_U32(buffer_offset); buffer_offset += 4;
		u32 out_buffer_size = Memory::Read_U32(buffer_offset); buffer_offset += 4;

		GENERIC_LOG(type, LogTypes::LINFO, "%s - IOCtlV OutBuffer[%i]:", GetDeviceName().c_str(), i);
		GENERIC_LOG(type, LogTypes::LINFO, "    OutBuffer: 0x%08x (0x%x):", out_buffer, out_buffer_size);

		if (verbosity >= LogTypes::LOG_LEVELS::LINFO)
			DumpCommands(out_buffer, out_buffer_size, type, verbosity);
	}
}
