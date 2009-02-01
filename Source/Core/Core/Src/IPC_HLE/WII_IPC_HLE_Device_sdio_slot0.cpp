// Copyright (C) 2003-2008 Dolphin Project.

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
#include "../HW/SDInterface.h"
#include "../Core.h"

using namespace SDInterface;

CWII_IPC_HLE_Device_sdio_slot0::CWII_IPC_HLE_Device_sdio_slot0(u32 _DeviceID, const std::string& _rDeviceName )
    : IWII_IPC_HLE_Device(_DeviceID, _rDeviceName)
{

}

CWII_IPC_HLE_Device_sdio_slot0::~CWII_IPC_HLE_Device_sdio_slot0()
{

}

bool CWII_IPC_HLE_Device_sdio_slot0::Open(u32 _CommandAddress, u32 _Mode)
{
	LOG(WII_IPC_SD, "SD: Open");
    Memory::Write_U32(GetDeviceID(), _CommandAddress + 0x4);
    return true;
}

bool CWII_IPC_HLE_Device_sdio_slot0::Close(u32 _CommandAddress)
{
	LOG(WII_IPC_SD, "SD: Close");
    Memory::Write_U32(0, _CommandAddress + 0x4);
    return true;
}

// The front SD slot
bool CWII_IPC_HLE_Device_sdio_slot0::IOCtl(u32 _CommandAddress) 
{
	//LOG(WII_IPC_FILEIO, "*************************************");
	//LOG(WII_IPC_FILEIO, "CWII_IPC_HLE_Device_sdio_slot0::IOCtl");
	//LOG(WII_IPC_FILEIO, "*************************************");

    // DumpCommands(_CommandAddress);

	u32 Cmd =  Memory::Read_U32(_CommandAddress + 0xC);
	// TODO: Use Cmd for something?

	u32 BufferIn =  Memory::Read_U32(_CommandAddress + 0x10);
	u32 BufferInSize =  Memory::Read_U32(_CommandAddress + 0x14);
    u32 BufferOut = Memory::Read_U32(_CommandAddress + 0x18);
    u32 BufferOutSize = Memory::Read_U32(_CommandAddress + 0x1C);

    //LOG(WII_IPC_SD, "%s Cmd 0x%x - BufferIn(0x%08x, 0x%x) BufferOut(0x%08x, 0x%x)",
	//	GetDeviceName().c_str(), Cmd, BufferIn, BufferInSize, BufferOut, BufferOutSize);
	
	/* As a safety precaution we fill the out buffer with zeroes to avoid
	   returning nonsense values */
	Memory::Memset(BufferOut, 0, BufferOutSize);
	
	u32 ReturnValue = 0;
	switch (Cmd) {
	case 1: // set_hc_reg
		LOGV(WII_IPC_SD, 0, "SD: set_hc_reg");
		break;
	case 2: // get_hc_reg
		LOGV(WII_IPC_SD, 0, "SD: get_hc_reg");
		break;

	case 4: // reset, do nothing ?
		LOGV(WII_IPC_SD, 0, "SD: reset");
		break;

	case 6: // sd_clock
		LOGV(WII_IPC_SD, 0, "SD: sd_clock");
		break;

	case 7: // Send cmd (Input: 24 bytes, Output: 10 bytes)
		LOGV(WII_IPC_SD, 0, "SD: sendcmd");
		ReturnValue = ExecuteCommand(BufferIn, BufferInSize, BufferOut, BufferOutSize);	
		break;

	case 11: // sd_get_status
		if (IsCardInserted())
		{
			// TODO
		}
		else
		{
			LOGV(WII_IPC_SD, 0, "SD: sd_get_status. Answer: SD card is not inserted", BufferOut);
			Memory::Write_U32(2, BufferOut); // SD card is not inserted
		}
		break;
	default:
		PanicAlert("Unknown SD command (0x%08x)", Cmd);
		break;
	}

	//DumpCommands(_CommandAddress);

	//LOG(WII_IPC_SD, "InBuffer");
	//DumpCommands(BufferIn, BufferInSize / 4, LogTypes::WII_IPC_SD);

	//LOG(WII_IPC_SD, "OutBuffer");
	//DumpCommands(BufferOut, BufferOutSize);
	Memory::Write_U32(ReturnValue, _CommandAddress + 0x4);

	return true;
}

bool CWII_IPC_HLE_Device_sdio_slot0::IOCtlV(u32 _CommandAddress) 
{  
    PanicAlert("CWII_IPC_HLE_Device_sdio_slot0::IOCtlV() unknown");
	// SD_Read uses this

    DumpCommands(_CommandAddress);

    return true;
}

u32 CWII_IPC_HLE_Device_sdio_slot0::ExecuteCommand(u32 _BufferIn, u32 _BufferInSize, u32 _BufferOut, u32 _BufferOutSize)
{
	/* The game will send us a SendCMD with this information. To be able to read and write
	   to a file we need to prepare a 10 byte output buffer as response. */
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
	//switch (req.command)
	{
	}
    return 0;
}
