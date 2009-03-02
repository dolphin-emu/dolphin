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



// =======================================================
// File description
// -------------
/*  Here we handle /dev/net and /dev/net/ncd/manage requests.


	// -----------------------
	The /dev/net/kd/request requests are part of what is called WiiConnect24,
	it's used by for example SSBB, Mario Kart, Metroid Prime 3

	0x01 SuspendScheduler (Input: none, Output: 32 bytes)
	0x02 ExecTrySuspendScheduler (Input: 32 bytes, Output: 32 bytes) // Sounds like it will
		check if it should suspend the updates scheduler or not. If I returned
		(OutBuffer: 0, Ret: -1) to Metroid Prime 3 it got stuck in an endless loops of
		requests, probably harmless but I changed it to (OutBuffer: 1, Ret: 0) to stop
		the calls. However then it also calls 0x3 and then changes its error message
		to a Wii Memory error message from just a general Error message.

	0x03 ? (Input: none, Output: 32 bytes) // This is only called if 0x02
		does not return -1
	0x0f NWC24iRequestGenerateUserId (Input: none, Output: 32 bytes)
	
	Requests are made in this order by these games
	   Mario Kart: 2, 1, f, 3
	   SSBB: 2, 3

	 For Mario Kart I had to return -1 from at least 2, f and 3 to convince it that the network
	 was unavaliable and prevent if from looking for shared2/wc24 files (and do a PPCHalt when
	 it failed)
	// -------

*/
// =============

#ifdef _MSC_VER
#pragma warning(disable : 4065)  // switch statement contains 'default' but no 'case' labels
#endif

#include "WII_IPC_HLE_Device_net.h"

#define IOCTL_NWC24_STARTUP                     0x06
enum {
        IOCTL_SO_ACCEPT = 1,
        IOCTL_SO_BIND,   
        IOCTL_SO_CLOSE, 
        IOCTL_SO_CONNECT, 
        IOCTL_SO_FCNTL,
        IOCTL_SO_GETPEERNAME, // todo
        IOCTL_SO_GETSOCKNAME, // todo
        IOCTL_SO_GETSOCKOPT,  // todo    8
        IOCTL_SO_SETSOCKOPT,  
        IOCTL_SO_LISTEN,
        IOCTL_SO_POLL,        // todo    b
        IOCTLV_SO_RECVFROM,
        IOCTLV_SO_SENDTO,
        IOCTL_SO_SHUTDOWN,    // todo    e
        IOCTL_SO_SOCKET,
        IOCTL_SO_GETHOSTID,
        IOCTL_SO_GETHOSTBYNAME,
        IOCTL_SO_GETHOSTBYADDR,// todo
        IOCTLV_SO_GETNAMEINFO, // todo   13
        IOCTL_SO_UNK14,        // todo
        IOCTL_SO_INETATON,     // todo
        IOCTL_SO_INETPTON,     // todo
        IOCTL_SO_INETNTOP,     // todo
        IOCTLV_SO_GETADDRINFO, // todo
        IOCTL_SO_SOCKATMARK,   // todo
        IOCTLV_SO_UNK1A,       // todo
        IOCTLV_SO_UNK1B,       // todo
        IOCTLV_SO_GETINTERFACEOPT, // todo
        IOCTLV_SO_SETINTERFACEOPT, // todo
        IOCTL_SO_SETINTERFACE,     // todo
        IOCTL_SO_STARTUP,           // 0x1f
        IOCTL_SO_ICMPSOCKET =   0x30, // todo
        IOCTLV_SO_ICMPPING,         // todo
        IOCTL_SO_ICMPCANCEL,        // todo
        IOCTL_SO_ICMPCLOSE          // todo
};


// **********************************************************************************
// Handle /dev/net/kd/request requests

CWII_IPC_HLE_Device_net_kd_request::CWII_IPC_HLE_Device_net_kd_request(u32 _DeviceID, const std::string& _rDeviceName) 
	: IWII_IPC_HLE_Device(_DeviceID, _rDeviceName)
{
}

CWII_IPC_HLE_Device_net_kd_request::~CWII_IPC_HLE_Device_net_kd_request()
{

}

bool CWII_IPC_HLE_Device_net_kd_request::Open(u32 _CommandAddress, u32 _Mode)
{
	INFO_LOG(WII_IPC_NET, "NET_KD_REQ: Open");
	Memory::Write_U32(GetDeviceID(), _CommandAddress + 4);
	return true;
}

bool CWII_IPC_HLE_Device_net_kd_request::Close(u32 _CommandAddress)
{
	INFO_LOG(WII_IPC_NET, "NET_KD_REQ: Close");
	Memory::Write_U32(0, _CommandAddress + 4);
	return true;
}

bool CWII_IPC_HLE_Device_net_kd_request::IOCtl(u32 _CommandAddress) 
{
	u32 Parameter =  Memory::Read_U32(_CommandAddress + 0xC);
	u32 BufferIn =  Memory::Read_U32(_CommandAddress + 0x10);
	u32 BufferInSize =  Memory::Read_U32(_CommandAddress + 0x14);
	u32 BufferOut = Memory::Read_U32(_CommandAddress + 0x18);
	u32 BufferOutSize = Memory::Read_U32(_CommandAddress + 0x1C);

	u32 ReturnValue = ExecuteCommand(Parameter, BufferIn, BufferInSize, BufferOut, BufferOutSize);	

	INFO_LOG(WII_IPC_NET, "NET_KD_REQ: IOCtl (Device=%s) (Parameter: 0x%02x)", GetDeviceName().c_str(), Parameter);

	Memory::Write_U32(ReturnValue, _CommandAddress + 4);

	return true; 
}

s32 CWII_IPC_HLE_Device_net_kd_request::ExecuteCommand(u32 _Parameter, u32 _BufferIn, u32 _BufferInSize, u32 _BufferOut, u32 _BufferOutSize)
{
	// Clean the location of the output buffer to zeroes as a safety precaution */
	Memory::Memset(_BufferOut, 0, _BufferOutSize);

	switch(_Parameter)
	{
	case 1: // SuspendScheduler (Input: none, Output: 32 bytes) 
		//Memory::Write_U32(0, _BufferOut);
		return -1;
		break;
	case 2: // ExecTrySuspendScheduler (Input: 32 bytes, Output: 32 bytes).
		DumpCommands(_BufferIn, _BufferInSize / 4, LogTypes::WII_IPC_NET);
		Memory::Write_U32(1, _BufferOut);
		return 0;
		break;
	case 3: // ? (Input: none, Output: 32 bytes)
		//Memory::Write_U32(0, _BufferOut);
		return -1;
		break;
	case IOCTL_NWC24_STARTUP:
		return 0;
	
    case IOCTL_SO_GETSOCKOPT: // WiiMenu
    case IOCTL_SO_SETSOCKOPT:
        return 0;

	case 0xf: // NWC24iRequestGenerateUserId (Input: none, Output: 32 bytes)
		//Memory::Write_U32(0, _BufferOut);
		return -1;
		break;
	default:
		_dbg_assert_msg_(WII_IPC_NET, 0, "/dev/net/kd/request::IOCtl request 0x%x (BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
			_Parameter, _BufferIn, _BufferInSize, _BufferOut, _BufferOutSize);
		break;
	}

	// We return a success for any potential unknown requests
	return 0;
}


// **********************************************************************************
// Handle /dev/net/ncd/manage requests

CWII_IPC_HLE_Device_net_ncd_manage::CWII_IPC_HLE_Device_net_ncd_manage(u32 _DeviceID, const std::string& _rDeviceName) 
	: IWII_IPC_HLE_Device(_DeviceID, _rDeviceName)
{}

CWII_IPC_HLE_Device_net_ncd_manage::~CWII_IPC_HLE_Device_net_ncd_manage() 
{}

bool CWII_IPC_HLE_Device_net_ncd_manage::Open(u32 _CommandAddress, u32 _Mode)
{
	Memory::Write_U32(GetDeviceID(), _CommandAddress+4);
	return true;
}

bool CWII_IPC_HLE_Device_net_ncd_manage::Close(u32 _CommandAddress)
{
	Memory::Write_U32(0, _CommandAddress + 4);
	return true;
}

bool CWII_IPC_HLE_Device_net_ncd_manage::IOCtlV(u32 _CommandAddress) 
{ 
	u32 ReturnValue = 0;

	SIOCtlVBuffer CommandBuffer(_CommandAddress);

	switch (CommandBuffer.Parameter)
	{
      // WiiMenu
    case 0x03:
    case 0x04:
    case 0x05:
    case 0x06:
    case 0x07:
    case 0x08:
        break;

	default:
		INFO_LOG(WII_IPC_NET, "NET_NCD_MANAGE IOCtlV: %i", CommandBuffer.Parameter);
		_dbg_assert_msg_(WII_IPC_NET, 0, "NET_NCD_MANAGE IOCtlV: %i", CommandBuffer.Parameter);
		break;
	}

	Memory::Write_U32(ReturnValue, _CommandAddress+4);

	return true; 
}

// **********************************************************************************
// Handle /dev/net/ip/top requests

CWII_IPC_HLE_Device_net_ip_top::CWII_IPC_HLE_Device_net_ip_top(u32 _DeviceID, const std::string& _rDeviceName) 
	: IWII_IPC_HLE_Device(_DeviceID, _rDeviceName)
{}

CWII_IPC_HLE_Device_net_ip_top::~CWII_IPC_HLE_Device_net_ip_top() 
{}

bool CWII_IPC_HLE_Device_net_ip_top::Open(u32 _CommandAddress, u32 _Mode)
{
	Memory::Write_U32(GetDeviceID(), _CommandAddress+4);
	return true;
}

bool CWII_IPC_HLE_Device_net_ip_top::Close(u32 _CommandAddress)
{
	Memory::Write_U32(0, _CommandAddress + 4);
	return true;
}

bool CWII_IPC_HLE_Device_net_ip_top::IOCtl(u32 _CommandAddress) 
{ 
	u32 BufferIn =  Memory::Read_U32(_CommandAddress + 0x10);
	u32 BufferInSize =  Memory::Read_U32(_CommandAddress + 0x14);
    u32 BufferOut = Memory::Read_U32(_CommandAddress + 0x18);
    u32 BufferOutSize = Memory::Read_U32(_CommandAddress + 0x1C);
	u32 Command = Memory::Read_U32(_CommandAddress + 0x0C);

//    printf("%s - Command(0x%08x) BufferIn(0x%08x, 0x%x) BufferOut(0x%08x, 0x%x)\n", GetDeviceName().c_str(), Command, BufferIn, BufferInSize, BufferOut, BufferOutSize);

	u32 ReturnValue = ExecuteCommand(Command, BufferIn, BufferInSize, BufferOut, BufferOutSize);	
    Memory::Write_U32(ReturnValue, _CommandAddress + 0x4);

    return true;
}

s32 CWII_IPC_HLE_Device_net_ip_top::ExecuteCommand(u32 _Command, u32 _BufferIn, u32 _BufferInSize, u32 _BufferOut, u32 _BufferOutSize)
{
	// Clean the location of the output buffer to zeroes as a safety precaution */
	Memory::Memset(_BufferOut, 0, _BufferOutSize);

	switch(_Command)
	{
	case IOCTL_SO_GETHOSTID: return 127 << 24 | 1;

	default:
		_dbg_assert_msg_(WII_IPC_NET, 0, "/dev/net/ip/top::IOCtl request 0x%x (BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
			_Command, _BufferIn, _BufferInSize, _BufferOut, _BufferOutSize);
		break;
	}

	// We return a success for any potential unknown requests
	return 0;
}

bool CWII_IPC_HLE_Device_net_ip_top::IOCtlV(u32 _CommandAddress) 
{ 
	u32 ReturnValue = 0;

	SIOCtlVBuffer CommandBuffer(_CommandAddress);

	switch(CommandBuffer.Parameter)
	{
	default:
		INFO_LOG(WII_IPC_NET, "NET_IP_TOP IOCtlV: %i", CommandBuffer.Parameter);
		break;
	}

	Memory::Write_U32(ReturnValue, _CommandAddress+4);

	return true; 
}
