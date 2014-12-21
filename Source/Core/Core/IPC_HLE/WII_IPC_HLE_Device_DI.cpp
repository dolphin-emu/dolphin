// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cinttypes>
#include <memory>

#include "Common/CommonTypes.h"
#include "Common/Logging/LogManager.h"

#include "Core/VolumeHandler.h"
#include "Core/HW/DVDInterface.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/SystemTimers.h"

#include "Core/IPC_HLE/WII_IPC_HLE.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_DI.h"

using namespace DVDInterface;

CWII_IPC_HLE_Device_di::CWII_IPC_HLE_Device_di(u32 _DeviceID, const std::string& _rDeviceName )
	: IWII_IPC_HLE_Device(_DeviceID, _rDeviceName)
{}

CWII_IPC_HLE_Device_di::~CWII_IPC_HLE_Device_di()
{}

bool CWII_IPC_HLE_Device_di::Open(u32 _CommandAddress, u32 _Mode)
{
	Memory::Write_U32(GetDeviceID(), _CommandAddress + 4);
	m_Active = true;
	return true;
}

bool CWII_IPC_HLE_Device_di::Close(u32 _CommandAddress, bool _bForce)
{
	if (!_bForce)
		Memory::Write_U32(0, _CommandAddress + 4);
	m_Active = false;
	return true;
}

bool CWII_IPC_HLE_Device_di::IOCtl(u32 _CommandAddress)
{
	u32 BufferIn = Memory::Read_U32(_CommandAddress + 0x10);
	u32 BufferInSize = Memory::Read_U32(_CommandAddress + 0x14);
	u32 BufferOut = Memory::Read_U32(_CommandAddress + 0x18);
	u32 BufferOutSize = Memory::Read_U32(_CommandAddress + 0x1C);

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

	DVDCommandResult result = ExecuteCommand(command_0, command_1, command_2,
	                                         BufferOut, BufferOutSize, false);
	Memory::Write_U32(result.interrupt_type, _CommandAddress + 0x4);
	// TODO: Don't discard result.ticks_until_completion
	return true;
}

bool CWII_IPC_HLE_Device_di::IOCtlV(u32 _CommandAddress)
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
	case DVDLowOpenPartition:
		{
			_dbg_assert_msg_(WII_IPC_DVD, CommandBuffer.InBuffer[1].m_Address == 0, "DVDLowOpenPartition with ticket");
			_dbg_assert_msg_(WII_IPC_DVD, CommandBuffer.InBuffer[2].m_Address == 0, "DVDLowOpenPartition with cert chain");

			u64 const partition_offset = ((u64)Memory::Read_U32(CommandBuffer.InBuffer[0].m_Address + 4) << 2);
			VolumeHandler::GetVolume()->ChangePartition(partition_offset);

			INFO_LOG(WII_IPC_DVD, "DVDLowOpenPartition: partition_offset 0x%016" PRIx64, partition_offset);

			// Read TMD to the buffer
			u32 tmd_size;
			std::unique_ptr<u8[]> tmd_buf = VolumeHandler::GetVolume()->GetTMD(&tmd_size);
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
	return true;
}

int CWII_IPC_HLE_Device_di::GetCmdDelay(u32 _CommandAddress)
{
	u32 BufferIn = Memory::Read_U32(_CommandAddress + 0x10);
	u32 Command  = Memory::Read_U32(BufferIn) >> 24;

	// Hacks below

	switch (Command)
	{
	case DVDLowRead:
	case DVDLowUnencryptedRead:
	{
		u32 const Size = Memory::Read_U32(BufferIn + 0x04);
		// Delay depends on size of read, that makes sense, right?
		// More than ~1150K "bytes / sec" hangs NSMBWii on boot.
		// Less than ~800K "bytes / sec" hangs DKCR randomly (ok, probably not true)
		return SystemTimers::GetTicksPerSecond() / 975000 * Size;
	}

	case DVDLowClearCoverInterrupt:
		// Less than ~1/155th of a second hangs Oregon Trail at "loading wheel".
		// More than ~1/140th of a second hangs Resident Evil Archives: Resident Evil Zero.
		return SystemTimers::GetTicksPerSecond() / 146;

	// case DVDLowAudioBufferConfig:
	// case DVDLowInquiry:
	// case DVDLowReadDiskID:
	// case DVDLowWaitForCoverClose:
	// case DVDLowGetCoverReg:
	// case DVDLowGetCoverStatus:
	// case DVDLowReset:
	// case DVDLowClosePartition:
	default:
		// random numbers here!
		// More than ~1/2000th of a second hangs DKCR with DSP HLE, maybe.
		return SystemTimers::GetTicksPerSecond() / 15000;
	}
}
