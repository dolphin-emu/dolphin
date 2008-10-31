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
#include "../Core.h"

// __________________________________________________________________________________________________
//
CWII_IPC_HLE_Device_sdio_slot0::CWII_IPC_HLE_Device_sdio_slot0(u32 _DeviceID, const std::string& _rDeviceName )
    : IWII_IPC_HLE_Device(_DeviceID, _rDeviceName)
{

}

// __________________________________________________________________________________________________
//
CWII_IPC_HLE_Device_sdio_slot0::~CWII_IPC_HLE_Device_sdio_slot0()
{

}

// __________________________________________________________________________________________________
//
bool 
CWII_IPC_HLE_Device_sdio_slot0::Open(u32 _CommandAddress, u32 _Mode)
{
    Memory::Write_U32(GetDeviceID(), _CommandAddress + 0x4);

    return true;
}


// __________________________________________________________________________________________________
//
bool CWII_IPC_HLE_Device_sdio_slot0::IOCtl(u32 _CommandAddress) 
{
    LOG(WII_IPC_HLE, "*************************************");
    LOG(WII_IPC_HLE, "CWII_IPC_HLE_Device_sdio_slot0::IOCtl");
    LOG(WII_IPC_HLE, "*************************************");

    // DumpCommands(_CommandAddress);

	u32 Cmd =  Memory::Read_U32(_CommandAddress + 0xC);
	// TODO: Use Cmd for something?

	u32 BufferIn =  Memory::Read_U32(_CommandAddress + 0x10);
	u32 BufferInSize =  Memory::Read_U32(_CommandAddress + 0x14);
    u32 BufferOut = Memory::Read_U32(_CommandAddress + 0x18);
    u32 BufferOutSize = Memory::Read_U32(_CommandAddress + 0x1C);

    LOG(WII_IPC_HLE, "%s - BufferIn(0x%08x, 0x%x) BufferOut(0x%08x, 0x%x)", GetDeviceName().c_str(), BufferIn, BufferInSize, BufferOut, BufferOutSize);
	
	u32 ReturnValue = 0;
	switch (Cmd) {
	case 1: //set_hc_reg
		break;
	case 2: //get_hc_reg
		break;

	case 4: //reset
		// do nothing ?
		break;

	case 6: //sd_clock
		break;

	case 7: //sendcmd
		ReturnValue = ExecuteCommand(BufferIn, BufferInSize, BufferOut, BufferOutSize);	
		break;
	default:
		PanicAlert("Unknown SD command");
		break;
	}

/*    DumpCommands(_CommandAddress);

    LOG(WII_IPC_HLE, "InBuffer");
    DumpCommands(BufferIn, BufferInSize);

//    LOG(WII_IPC_HLE, "OutBuffer");
  //  DumpCommands(BufferOut, BufferOutSize); */
	Memory::Write_U32(ReturnValue, _CommandAddress + 0x4);

    return true;
}

// __________________________________________________________________________________________________
//
bool CWII_IPC_HLE_Device_sdio_slot0::IOCtlV(u32 _CommandAddress) 
{  
    // PanicAlert("CWII_IPC_HLE_Device_sdio_slot0::IOCtlV() unknown");
	// SD_Read uses this

    DumpCommands(_CommandAddress);

    return true;
}

// __________________________________________________________________________________________________
//
u32 CWII_IPC_HLE_Device_sdio_slot0::ExecuteCommand(u32 _BufferIn, u32 _BufferInSize, u32 _BufferOut, u32 _BufferOutSize)
{
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
    return 1;
}
