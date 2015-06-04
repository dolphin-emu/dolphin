// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cinttypes>
#include <memory>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/LogManager.h"
#include "Core/ConfigManager.h"
#include "Core/CoreTiming.h"
#include "Core/HW/DVDInterface.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/SystemTimers.h"
#include "Core/IPC_HLE/WII_IPC_HLE.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_DI.h"

static CWII_IPC_HLE_Device_di* g_di_pointer;
static int ioctl_callback;

static void IOCtlCallback(u64 userdata, int cycles_late)
{
	if (g_di_pointer != nullptr)
		g_di_pointer->FinishIOCtl((DVDInterface::DIInterruptType)userdata);

	// If g_di_pointer == nullptr, IOS was probably shut down,
	// so the command shouldn't be completed
}

CWII_IPC_HLE_Device_di::CWII_IPC_HLE_Device_di(u32 _DeviceID, const std::string& _rDeviceName)
	: IWII_IPC_HLE_Device(_DeviceID, _rDeviceName)
{
	if (g_di_pointer == nullptr)
		ERROR_LOG(WII_IPC_DVD, "Trying to run two DI devices at once. IOCtl may not behave as expected.");

	g_di_pointer = this;
	ioctl_callback = CoreTiming::RegisterEvent("IOCtlCallbackDI", IOCtlCallback);
}

CWII_IPC_HLE_Device_di::~CWII_IPC_HLE_Device_di()
{
	g_di_pointer = nullptr;
}

void CWII_IPC_HLE_Device_di::DoState(PointerWrap& p)
{
	DoStateShared(p);
	p.Do(m_commands_to_execute);
}

IPCCommandResult CWII_IPC_HLE_Device_di::Open(u32 _CommandAddress, u32 _Mode)
{
	Memory::Write_U32(GetDeviceID(), _CommandAddress + 4);
	m_Active = true;
	return IPC_DEFAULT_REPLY;
}

IPCCommandResult CWII_IPC_HLE_Device_di::Close(u32 _CommandAddress, bool _bForce)
{
	if (!_bForce)
		Memory::Write_U32(0, _CommandAddress + 4);
	m_Active = false;
	return IPC_DEFAULT_REPLY;
}

IPCCommandResult CWII_IPC_HLE_Device_di::IOCtl(u32 _CommandAddress)
{
	// DI IOCtls are handled in a special way by Dolphin
	// compared to other WII_IPC_HLE functions.
	// This is a wrapper around DVDInterface's ExecuteCommand,
	// which will execute commands more or less asynchronously.
	// Only one command can be executed at a time, so commands
	// are queued until DVDInterface is ready to handle them.

	bool ready_to_execute = m_commands_to_execute.empty();
	m_commands_to_execute.push_back(_CommandAddress);
	if (ready_to_execute)
		StartIOCtl(_CommandAddress);

	// DVDInterface handles the timing, and we handle the reply,
	// so WII_IPC_HLE shouldn't do any of that.
	return IPC_NO_REPLY;
}

void CWII_IPC_HLE_Device_di::StartIOCtl(u32 command_address)
{
	u32 BufferIn = Memory::Read_U32(command_address + 0x10);
	u32 BufferInSize = Memory::Read_U32(command_address + 0x14);
	u32 BufferOut = Memory::Read_U32(command_address + 0x18);
	u32 BufferOutSize = Memory::Read_U32(command_address + 0x1C);

	u32 command_0 = Memory::Read_U32(BufferIn);
	u32 command_1 = Memory::Read_U32(BufferIn + 4);
	u32 command_2 = Memory::Read_U32(BufferIn + 8);

	DEBUG_LOG(WII_IPC_DVD, "IOCtl Command(0x%08x) BufferIn(0x%08x, 0x%x) BufferOut(0x%08x, 0x%x)",
		command_0, BufferIn, BufferInSize, BufferOut, BufferOutSize);

	// TATSUNOKO VS CAPCOM: Gets here with BufferOut == 0!!!
	if (BufferOut != 0)
	{
		// Set out buffer to zeroes as a safety precaution
		// to avoid answering nonsense values
		Memory::Memset(BufferOut, 0, BufferOutSize);
	}

	// DVDInterface's ExecuteCommand handles most of the work.
	// The IOCtl callback is used to generate a reply afterwards.
	DVDInterface::ExecuteCommand(command_0, command_1, command_2, BufferOut, BufferOutSize,
	                             false, ioctl_callback);
}

void CWII_IPC_HLE_Device_di::FinishIOCtl(DVDInterface::DIInterruptType interrupt_type)
{
	if (m_commands_to_execute.empty())
	{
		PanicAlert("WII_IPC_HLE_Device_DI: There is no command to execute!");
		return;
	}

	// This command has been executed, so it's removed from the queue
	u32 command_address = m_commands_to_execute.front();
	m_commands_to_execute.pop_front();

	// The DI interrupt type is used as a return value
	Memory::Write_U32(interrupt_type, command_address + 4);

	// The original hardware overwrites the command type with the async reply type.
	Memory::Write_U32(IPC_REP_ASYNC, command_address);
	// IOS also seems to write back the command that was responded to in the FD field.
	Memory::Write_U32(Memory::Read_U32(command_address), command_address + 8);
	// Generate a reply to the IPC command
	WII_IPC_HLE_Interface::EnqueueReply_Immediate(command_address);

	// DVDInterface is now ready to execute another command,
	// so we start executing a command from the queue if there is one
	if (!m_commands_to_execute.empty())
		StartIOCtl(m_commands_to_execute.front());
}

IPCCommandResult CWII_IPC_HLE_Device_di::IOCtlV(u32 _CommandAddress)
{
	SIOCtlVBuffer CommandBuffer(_CommandAddress);

	// Prepare the out buffer(s) with zeros as a safety precaution
	// to avoid returning bad values
	for (u32 i = 0; i < CommandBuffer.NumberPayloadBuffer; i++)
	{
		Memory::Memset(CommandBuffer.PayloadBuffer[i].m_Address, 0,
			CommandBuffer.PayloadBuffer[i].m_Size);
	}

	u32 ReturnValue = 0;
	switch (CommandBuffer.Parameter)
	{
	case DVDInterface::DVDLowOpenPartition:
		{
			_dbg_assert_msg_(WII_IPC_DVD, CommandBuffer.InBuffer[1].m_Address == 0, "DVDLowOpenPartition with ticket");
			_dbg_assert_msg_(WII_IPC_DVD, CommandBuffer.InBuffer[2].m_Address == 0, "DVDLowOpenPartition with cert chain");

			u64 const partition_offset = ((u64)Memory::Read_U32(CommandBuffer.InBuffer[0].m_Address + 4) << 2);
			DVDInterface::ChangePartition(partition_offset);

			INFO_LOG(WII_IPC_DVD, "DVDLowOpenPartition: partition_offset 0x%016" PRIx64, partition_offset);

			// Read TMD to the buffer
			u32 tmd_size;
			std::unique_ptr<u8[]> tmd_buf = DVDInterface::GetVolume().GetTMD(&tmd_size);
			Memory::CopyToEmu(CommandBuffer.PayloadBuffer[0].m_Address, tmd_buf.get(), tmd_size);
			WII_IPC_HLE_Interface::ES_DIVerify(tmd_buf.get(), tmd_size);

			ReturnValue = 1;
		}
		break;

	default:
		ERROR_LOG(WII_IPC_DVD, "IOCtlV: %i", CommandBuffer.Parameter);
		_dbg_assert_msg_(WII_IPC_DVD, 0, "IOCtlV: %i", CommandBuffer.Parameter);
		break;
	}

	Memory::Write_U32(ReturnValue, _CommandAddress + 4);
	return IPC_DEFAULT_REPLY;
}
