// Copyright (C) 2003 Dolphin Project.

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
#include <stdio.h>
#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#endif


extern std::queue<std::pair<u32,std::string> > g_ReplyQueueLater;


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

	u32 ReturnValue = 0;
    switch(Parameter)
    {
    case IOCTL_NWC24_SUSPEND_SCHEDULAR: // NWC24iResumeForCloseLib  from NWC24SuspendScheduler (Input: none, Output: 32 bytes) 
        INFO_LOG(WII_IPC_NET, "NET_KD_REQ: IOCTL_NWC24_SUSPEND_SCHEDULAR - NI");
        ReturnValue = 0;
        break;

    case IOCTL_NWC24_EXEC_TRY_SUSPEND_SCHEDULAR: // NWC24iResumeForCloseLib
        INFO_LOG(WII_IPC_NET, "NET_KD_REQ: IOCTL_NWC24_EXEC_TRY_SUSPEND_SCHEDULAR - NI");
        ReturnValue = 0;
        break;

    case IOCTL_NWC24_UNK_3: // NWC24iResumeForCloseLib
        INFO_LOG(WII_IPC_NET, "NET_KD_REQ: IOCTL_NWC24_UNK_3 - NI");
        ReturnValue = 0;
        break;

    case IOCTL_NWC24_STARTUP_SOCKET:
        INFO_LOG(WII_IPC_NET, "NET_KD_REQ: IOCTL_NWC24_STARTUP_SOCKET - NI");
        ReturnValue = 0;
        break;

    case IOCTL_NWC24_LOCK_SOCKET: // WiiMenu
        INFO_LOG(WII_IPC_NET, "NET_KD_REQ: IOCTL_NWC24_LOCK_SOCKET - NI");
        ReturnValue = 0;
        break;

    case IOCTL_NWC24_UNLOCK_SOCKET:        
        INFO_LOG(WII_IPC_NET, "NET_KD_REQ: IOCTL_NWC24_UNLOCK_SOCKET - NI");
        ReturnValue = 0;
        break;

    case IOCTL_NWC24_REQUEST_GENERATED_USER_ID: // (Input: none, Output: 32 bytes)
        PanicAlert("IOCTL_NWC24_REQUEST_GENERATED_USER_ID");
        ReturnValue = 0;
        break;

    case IOCTL_NWC24_GET_SCHEDULAR_STAT:
        INFO_LOG(WII_IPC_NET, "NET_KD_REQ: IOCTL_NWC24_GET_SCHEDULAR_STAT - NI");
        ReturnValue = 0;
        break;

    case IOCTL_NWC24_SAVE_MAIL_NOW:
        INFO_LOG(WII_IPC_NET, "NET_KD_REQ: IOCTL_NWC24_SAVE_MAIL_NOW - NI");
        ReturnValue = 0;
        break;

    case IOCTL_NWC24_REQUEST_SHUTDOWN:
        INFO_LOG(WII_IPC_NET, "NET_KD_REQ: IOCTL_NWC24_REQUEST_SHUTDOWN - NI");  // if ya set the IOS version to a very high value this happens ...
        ReturnValue = 0;
        break;

    default:
        INFO_LOG(WII_IPC_NET, "/dev/net/kd/request::IOCtl request 0x%x (BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
            Parameter, BufferIn, BufferInSize, BufferOut, BufferOutSize);
        break;
    }

    // g_ReplyQueueLater.push(std::pair<u32, std::string>(_CommandAddress, GetDeviceName()));
	Memory::Write_U32(ReturnValue, _CommandAddress + 4);
    
	return true; 
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
    INFO_LOG(WII_IPC_NET, "NET_NCD_MANAGE: Open");
	Memory::Write_U32(GetDeviceID(), _CommandAddress+4);
	return true;
}

bool CWII_IPC_HLE_Device_net_ncd_manage::Close(u32 _CommandAddress)
{
    INFO_LOG(WII_IPC_NET, "NET_NCD_MANAGE: Close");
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
	case 0x07: // //NCDGetLinkStatus
    case 0x03: // ??? It seems that is related to Write and Read information
    case 0x04:
    case 0x05:
    case 0x06:
    case 0x08:
        //break;

	default:		
		INFO_LOG(WII_IPC_NET,"NET_NCD_MANAGE IOCtlV: %i", CommandBuffer.Parameter);
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
    INFO_LOG(WII_IPC_NET, "NET_IP_TOP: Open");
	Memory::Write_U32(GetDeviceID(), _CommandAddress+4);
	return true;
}

bool CWII_IPC_HLE_Device_net_ip_top::Close(u32 _CommandAddress)
{
    INFO_LOG(WII_IPC_NET, "NET_IP_TOP: Close");
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

//    INFO_LOG(WII_IPC_NET,"%s - Command(0x%08x) BufferIn(0x%08x, 0x%x) BufferOut(0x%08x, 0x%x)\n", GetDeviceName().c_str(), Command, BufferIn, BufferInSize, BufferOut, BufferOutSize);

	u32 ReturnValue = ExecuteCommand(Command, BufferIn, BufferInSize, BufferOut, BufferOutSize);	
    Memory::Write_U32(ReturnValue, _CommandAddress + 0x4);

    return true;
}
struct bind_params {
	u32 socket;
	u32 has_name;
	u8 name[28];
};
struct GC_sockaddr {
  u8 sa_len;
  u8 sa_family;
  s8 sa_data[14];
};
struct GC_in_addr {
  u32 s_addr_;    // this cannot be named s_addr under windows - collides with some crazy define.
};
struct GC_sockaddr_in {
  u8 sin_len;
  u8 sin_family;
  u16 sin_port;
  struct GC_in_addr sin_addr;
  s8 sin_zero[8];
};
u32 CWII_IPC_HLE_Device_net_ip_top::ExecuteCommand(u32 _Command, u32 _BufferIn, u32 BufferInSize, u32 BufferOut, u32 BufferOutSize)
{
	// Clean the location of the output buffer to zeroes as a safety precaution */
	Memory::Memset(BufferOut, 0, BufferOutSize);

	switch (_Command)
	{
	case IOCTL_SO_STARTUP:
		break;
	case IOCTL_SO_SOCKET:
	{
		u32 AF = Memory::Read_U32(_BufferIn);
		u32 TYPE = Memory::Read_U32(_BufferIn + 0x04);
		u32 PROT = Memory::Read_U32(_BufferIn + 0x04 * 2);
		u32 Unk1 = Memory::Read_U32(_BufferIn + 0x04 * 3);
		u32 Socket = socket(AF, TYPE, PROT);
		return Common::swap32(Socket); // So it doesn't get mangled later on
	}
	break;
	case IOCTL_SO_BIND:
	{
		bind_params *addr = (bind_params*)Memory::GetPointer(_BufferIn);
		GC_sockaddr_in addrPC;
		memcpy(&addrPC, addr->name, sizeof(GC_sockaddr_in));
		sockaddr_in address;
		  address.sin_family = addrPC.sin_family;
		  address.sin_addr.s_addr = addrPC.sin_addr.s_addr_;
		  address.sin_port = htons(addrPC.sin_port);
		int Return = bind(addr->socket, (sockaddr*)&address, sizeof(address));
		return Return;
		//int bind(int s, struct sockaddr *addr, int addrlen);
	}
	break;
	case IOCTL_SO_LISTEN:
	{
		u32 S = Memory::Read_U32(_BufferIn);
		u32 BACKLOG = Memory::Read_U32(_BufferIn + 0x04);
		u32 Return = listen(S, BACKLOG);
		return Return;
	}
	break;
	case  IOCTL_SO_ACCEPT:
	{
		//TODO: (Sonic)Check if this is correct
		u32 S = Memory::Read_U32(_BufferIn);
		socklen_t addrlen;
		struct sockaddr_in address;
		int Return = accept(S, (struct sockaddr *)&address, &addrlen);
		GC_sockaddr_in *addr = (GC_sockaddr_in*)Memory::GetPointer(BufferOut);
		addr->sin_family = (u8)address.sin_family;
		addr->sin_addr.s_addr_ = address.sin_addr.s_addr;
		addr->sin_port = address.sin_port;
		socklen_t *Len = (socklen_t *)Memory::GetPointer(BufferOut + 0x04);
		*Len = addrlen;
		return Return;
		//int accept(int s, struct sockaddr *addr, int *addrlen);
		///dev/net/ip/top::IOCtl request 0x1 (BufferIn: (000318c0, 4), BufferOut: (00058a4c, 8)
	}
	break;
	case IOCTL_SO_GETHOSTID: 
		return 127 << 24 | 1;
		break;

	default:
		INFO_LOG(WII_IPC_NET,"/dev/net/ip/top::IOCtl request 0x%x (BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
			_Command, _BufferIn, BufferInSize, BufferOut, BufferOutSize);
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
		case 2:
		case 12:
		case 15:
		case 16:
		case 31:
		//break;
		default:
			INFO_LOG(WII_IPC_NET, "NET_IP_TOP IOCtlV: 0x%08X\n", CommandBuffer.Parameter);
		break;
	}

	Memory::Write_U32(ReturnValue, _CommandAddress+4);

	return true; 
}
