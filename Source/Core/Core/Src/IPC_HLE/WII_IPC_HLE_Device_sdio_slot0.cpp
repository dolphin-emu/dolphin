// Copyright (C) 2003-2009 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "Common.h"

#include "WII_IPC_HLE_Device_sdio_slot0.h"

#include "../HW/CPU.h"
#include "../HW/Memmap.h"
#include "HW/SDInterface.h"
#include "../Core.h"

using namespace SDInterface;

CWII_IPC_HLE_Device_sdio_slot0::CWII_IPC_HLE_Device_sdio_slot0(u32 _DeviceID, const std::string& _rDeviceName)
    : IWII_IPC_HLE_Device(_DeviceID, _rDeviceName)
{
	m_status = CARD_INSERTED;
}

CWII_IPC_HLE_Device_sdio_slot0::~CWII_IPC_HLE_Device_sdio_slot0()
{

}

bool CWII_IPC_HLE_Device_sdio_slot0::Open(u32 _CommandAddress, u32 _Mode)
{
	INFO_LOG(WII_IPC_SD, "Open");
    Memory::Write_U32(GetDeviceID(), _CommandAddress + 0x4);
    return true;
}

bool CWII_IPC_HLE_Device_sdio_slot0::Close(u32 _CommandAddress)
{
	INFO_LOG(WII_IPC_SD, "Close");
    Memory::Write_U32(0, _CommandAddress + 0x4);
    return true;
}

// The front SD slot
bool CWII_IPC_HLE_Device_sdio_slot0::IOCtl(u32 _CommandAddress) 
{
	ERROR_LOG(WII_IPC_SD, "*************************************");
	ERROR_LOG(WII_IPC_SD, "CWII_IPC_HLE_Device_sdio_slot0::IOCtl");
	ERROR_LOG(WII_IPC_SD, "*************************************");

	u32 Cmd = Memory::Read_U32(_CommandAddress + 0xC);

	u32 BufferIn		= Memory::Read_U32(_CommandAddress + 0x10);
	u32 BufferInSize	= Memory::Read_U32(_CommandAddress + 0x14);
    u32 BufferOut		= Memory::Read_U32(_CommandAddress + 0x18);
    u32 BufferOutSize	= Memory::Read_U32(_CommandAddress + 0x1C);

    ERROR_LOG(WII_IPC_SD, "BufferIn(0x%08x, 0x%x) BufferOut(0x%08x, 0x%x)",
		BufferIn, BufferInSize, BufferOut, BufferOutSize);
	
	// As a safety precaution we fill the out buffer with zeros to avoid
	// returning nonsense values
	Memory::Memset(BufferOut, 0, BufferOutSize);
	
	u32 ReturnValue = 0;
	switch (Cmd) {
	case IOCTL_WRITEHCREG:
		// Store the 4th element of input array to the reg offset specified by the 0 element
		Memory::Write_U32(Memory::Read_U32(BufferIn + 16), SDIO_BASE + Memory::Read_U32(BufferIn));
		ERROR_LOG(WII_IPC_SD, "IOCTL_WRITEHCREG");
		break;

	case IOCTL_READHCREG:
		// Load the specified reg into the out buffer
		Memory::Write_U32(Memory::Read_U32(SDIO_BASE + Memory::Read_U32(BufferIn)), BufferOut);
		ERROR_LOG(WII_IPC_SD, "IOCTL_READHCREG");
		break;

	case IOCTL_RESETCARD:
		m_status |= CARD_INITIALIZED;
		ERROR_LOG(WII_IPC_SD, "IOCTL_RESETCARD");
		break;

	case IOCTL_SETCLK:
		{
		// libogc only sets it to 1 and makes sure the return isn't negative...
		// 0 = 25MHz, 1 = 50MHz?, probably shouldn't matter in any case
		u32 clock = Memory::Read_U32(BufferIn);
		if (clock != 1)
			ERROR_LOG(WII_IPC_SD, "Setting to %i, interesting", clock);
		ERROR_LOG(WII_IPC_SD, "IOCTL_SETCLK");
		}
		break;

	case IOCTL_SENDCMD:
		// Input: 24 bytes, Output: 10 bytes
		ERROR_LOG(WII_IPC_SD, "IOCTL_SENDCMD");
		ReturnValue = ExecuteCommand(BufferIn, BufferInSize, BufferOut, BufferOutSize);	
		break;

	case IOCTL_GETSTATUS:
		ERROR_LOG(WII_IPC_SD, "IOCTL_GETSTATUS. Replying that SD card is %s%s",
			(m_status & CARD_INSERTED) ? "inserted" : "",
			(m_status & CARD_INITIALIZED) ? " and initialized" : "");
		Memory::Write_U32(m_status, BufferOut);
		break;

	default:
		PanicAlert("Unknown SD IOCtl command (0x%08x)", Cmd);
		break;
	}

	ERROR_LOG(WII_IPC_SD, "InBuffer");
	DumpCommands(BufferIn, BufferInSize / 4, LogTypes::WII_IPC_SD);

	ERROR_LOG(WII_IPC_SD, "OutBuffer");
	DumpCommands(BufferOut, BufferOutSize/4, LogTypes::WII_IPC_SD);
	Memory::Write_U32(ReturnValue, _CommandAddress + 0x4);

	return true;
}

bool CWII_IPC_HLE_Device_sdio_slot0::IOCtlV(u32 _CommandAddress) 
{
	// PPC sending commands

	ERROR_LOG(WII_IPC_SD, "*************************************");
	ERROR_LOG(WII_IPC_SD, "CWII_IPC_HLE_Device_sdio_slot0::IOCtlV");
	ERROR_LOG(WII_IPC_SD, "*************************************");

	u32 Cmd = Memory::Read_U32(_CommandAddress + 0xC);

	switch (Cmd) {
	case IOCTLV_SENDCMD:
		ERROR_LOG(WII_IPC_SD, "IOCTLV_SENDCMD");
		break;

	default:
		PanicAlert("unknown SD IOCtlV command 0x%08x", Cmd);
		break;
	}

    return true;
}

u32 CWII_IPC_HLE_Device_sdio_slot0::ExecuteCommand(u32 _BufferIn, u32 _BufferInSize,
												   u32 _BufferOut, u32 _BufferOutSize)
{
	// The game will send us a SendCMD with this information. To be able to read and write
	// to a file we need to prepare a 10 byte output buffer as response.

	struct Request {
		u32 command;
		u32 type;
		u32 resp;
		u32 arg;
		u32 blocks;
		u32 bsize;
		u32 addr;
	} req;

    req.command = Memory::Read_U32(_BufferIn + 0);
    req.type    = Memory::Read_U32(_BufferIn + 4);
    req.resp    = Memory::Read_U32(_BufferIn + 8);
    req.arg     = Memory::Read_U32(_BufferIn + 12);
    req.blocks  = Memory::Read_U32(_BufferIn + 16);
    req.bsize   = Memory::Read_U32(_BufferIn + 20);
    req.addr    = Memory::Read_U32(_BufferIn + 24);

	switch (req.command)
	{
	case SELECT_CARD:
		//return R1b
		break;

	case SET_BLOCKLEN:
		//return R1
		break;

	case APP_CMD_NEXT:
		// Next cmd is going to be ACMD_*
		break;

	case ACMD_SETBUSWIDTH:
		break;

	default:
		ERROR_LOG(WII_IPC_SD, "Unknown SD command 0x%08x", req.command);
		break;
	}

    return 0;
}
