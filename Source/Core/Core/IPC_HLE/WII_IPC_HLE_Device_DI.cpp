// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cinttypes>

#include "Common/CommonTypes.h"
#include "Common/Logging/LogManager.h"

#include "Core/ConfigManager.h"
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

u64 CWII_IPC_HLE_Device_di::Open(u32 _CommandAddress, u32 _Mode)
{
	Memory::Write_U32(GetDeviceID(), _CommandAddress + 4);
	m_Active = true;
	return DEFAULT_REPLY_DELAY;
}

u64 CWII_IPC_HLE_Device_di::Close(u32 _CommandAddress, bool _bForce)
{
	if (!_bForce)
		Memory::Write_U32(0, _CommandAddress + 4);
	m_Active = false;
	return DEFAULT_REPLY_DELAY;
}

u64 CWII_IPC_HLE_Device_di::IOCtl(u32 _CommandAddress)
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
		// Set out buffer to zeroes as a safety precaution to avoid answering
		// nonsense values
		Memory::Memset(BufferOut, 0, BufferOutSize);
	}

	DVDCommandResult result = ExecuteCommand(command_0, command_1, command_2, BufferOut, BufferOutSize, false);
	Memory::Write_U32(result.interrupt_type, _CommandAddress + 0x4);

	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bFastDiscSpeed)
		return 1;
	else
		return result.ticks_until_completion;
}

u64 CWII_IPC_HLE_Device_di::IOCtlV(u32 _CommandAddress)
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

			// Get TMD offset for requested partition...
			u64 const TMDOffset = ((u64)Memory::Read_U32(CommandBuffer.InBuffer[0].m_Address + 4) << 2 ) + 0x2c0;

			INFO_LOG(WII_IPC_DVD, "DVDLowOpenPartition: TMDOffset 0x%016" PRIx64, TMDOffset);

			static u32 const TMDsz = 0x208; //CommandBuffer.PayloadBuffer[0].m_Size;
			u8 pTMD[TMDsz];

			// Read TMD to the buffer
			VolumeHandler::RAWReadToPtr(pTMD, TMDOffset, TMDsz);

			Memory::CopyToEmu(CommandBuffer.PayloadBuffer[0].m_Address, pTMD, TMDsz);
			WII_IPC_HLE_Interface::ES_DIVerify(pTMD, TMDsz);

			ReturnValue = 1;
		}
		break;

	default:
		ERROR_LOG(WII_IPC_DVD, "IOCtlV: %i", CommandBuffer.Parameter);
		_dbg_assert_msg_(WII_IPC_DVD, 0, "IOCtlV: %i", CommandBuffer.Parameter);
		break;
	}

	Memory::Write_U32(ReturnValue, _CommandAddress + 4);
	return DEFAULT_REPLY_DELAY;
}
