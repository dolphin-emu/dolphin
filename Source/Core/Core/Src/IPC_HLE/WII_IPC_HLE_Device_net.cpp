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


/*
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
was unavailable and prevent if from looking for shared2/wc24 files (and do a PPCHalt when
it failed)
*/

#ifdef _MSC_VER
#pragma warning(disable: 4748)
#pragma optimize("",off)
#endif

#include "WII_IPC_HLE_Device_net.h"
#include "FileUtil.h"
#include <stdio.h>
#include <string>

#ifdef _WIN32
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <iphlpapi.h>

#include "fakepoll.h"
#define MALLOC(x) HeapAlloc(GetProcessHeap(), 0, (x)) 
#define FREE(x) HeapFree(GetProcessHeap(), 0, (x))

#elif defined(__linux__)
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <errno.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <netinet/in.h>
#include <errno.h>
#endif

extern std::queue<std::pair<u32,std::string> > g_ReplyQueueLater;
const u8 default_address[] = { 0x00, 0x17, 0xAB, 0x99, 0x99, 0x99 };
int status = 5;
// **********************************************************************************
// Handle /dev/net/kd/request requests
CWII_IPC_HLE_Device_net_kd_request::CWII_IPC_HLE_Device_net_kd_request(u32 _DeviceID, const std::string& _rDeviceName) 
	: IWII_IPC_HLE_Device(_DeviceID, _rDeviceName)
	, m_UserID("Dolphin-EMU")
	// TODO: Dump the true ID from real Wii
{
}

CWII_IPC_HLE_Device_net_kd_request::~CWII_IPC_HLE_Device_net_kd_request()
{
}

bool CWII_IPC_HLE_Device_net_kd_request::Open(u32 _CommandAddress, u32 _Mode)
{
	INFO_LOG(WII_IPC_NET, "NET_KD_REQ: Open");
	Memory::Write_U32(GetDeviceID(), _CommandAddress + 4);
	m_Active = true;
	return true;
}

bool CWII_IPC_HLE_Device_net_kd_request::Close(u32 _CommandAddress, bool _bForce)
{
	INFO_LOG(WII_IPC_NET, "NET_KD_REQ: Close");
	if (!_bForce)
		Memory::Write_U32(0, _CommandAddress + 4);
	m_Active = false;
	return true;
}

bool CWII_IPC_HLE_Device_net_kd_request::IOCtl(u32 _CommandAddress) 
{
	u32 Parameter		= Memory::Read_U32(_CommandAddress + 0xC);
	u32 BufferIn		= Memory::Read_U32(_CommandAddress + 0x10);
	u32 BufferInSize	= Memory::Read_U32(_CommandAddress + 0x14);
	u32 BufferOut		= Memory::Read_U32(_CommandAddress + 0x18);
	u32 BufferOutSize	= Memory::Read_U32(_CommandAddress + 0x1C);    

	u32 ReturnValue = 0;
	switch (Parameter)
	{
	case IOCTL_NWC24_SUSPEND_SCHEDULAR:
		// NWC24iResumeForCloseLib  from NWC24SuspendScheduler (Input: none, Output: 32 bytes) 
		INFO_LOG(WII_IPC_NET, "NET_KD_REQ: IOCTL_NWC24_SUSPEND_SCHEDULAR - NI");
		break;

	case IOCTL_NWC24_EXEC_TRY_SUSPEND_SCHEDULAR: // NWC24iResumeForCloseLib
		INFO_LOG(WII_IPC_NET, "NET_KD_REQ: IOCTL_NWC24_EXEC_TRY_SUSPEND_SCHEDULAR - NI");
		break;

	case IOCTL_NWC24_EXEC_RESUME_SCHEDULAR : // NWC24iResumeForCloseLib
		INFO_LOG(WII_IPC_NET, "NET_KD_REQ: IOCTL_NWC24_EXEC_RESUME_SCHEDULAR - NI");
		break;

	case IOCTL_NWC24_STARTUP_SOCKET: // NWC24iStartupSocket
		Memory::Write_U32(0, BufferOut);
		Memory::Write_U32(0, BufferOut+4);
		ReturnValue = 0;
		INFO_LOG(WII_IPC_NET, "NET_KD_REQ: IOCTL_NWC24_STARTUP_SOCKET - NI");
		break;
	case IOCTL_NWC24_CLEANUP_SOCKET:
		Memory::Memset(BufferOut, 0, BufferOutSize);
		INFO_LOG(WII_IPC_NET, "NET_KD_REQ: IOCTL_NWC24_CLEANUP_SOCKET - NI");
		break;
	case IOCTL_NWC24_LOCK_SOCKET: // WiiMenu
		INFO_LOG(WII_IPC_NET, "NET_KD_REQ: IOCTL_NWC24_LOCK_SOCKET - NI");
		break;

	case IOCTL_NWC24_UNLOCK_SOCKET:
		INFO_LOG(WII_IPC_NET, "NET_KD_REQ: IOCTL_NWC24_UNLOCK_SOCKET - NI");
		break;

	case IOCTL_NWC24_REQUEST_GENERATED_USER_ID: // (Input: none, Output: 32 bytes)
		INFO_LOG(WII_IPC_NET, "NET_KD_REQ: IOCTL_NWC24_REQUEST_GENERATED_USER_ID");
		//Memory::Write_U32(0xFFFFFFDC, BufferOut);
		//Memory::Write_U32(0x00050495, BufferOut + 4);
		//Memory::Write_U32(0x90CFBF35, BufferOut + 8);
		//Memory::Write_U32(0x00000002, BufferOut + 0xC);
		Memory::WriteBigEData((u8*)m_UserID.c_str(), BufferOut, m_UserID.length() + 1);
		break;

	case IOCTL_NWC24_GET_SCHEDULAR_STAT:
		INFO_LOG(WII_IPC_NET, "NET_KD_REQ: IOCTL_NWC24_GET_SCHEDULAR_STAT - NI");
		break;

	case IOCTL_NWC24_SAVE_MAIL_NOW:
		INFO_LOG(WII_IPC_NET, "NET_KD_REQ: IOCTL_NWC24_SAVE_MAIL_NOW - NI");
		break;

	case IOCTL_NWC24_REQUEST_SHUTDOWN:
		// if ya set the IOS version to a very high value this happens ...
		INFO_LOG(WII_IPC_NET, "NET_KD_REQ: IOCTL_NWC24_REQUEST_SHUTDOWN - NI");
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
{
	// store network configuration
	const std::string filename(File::GetUserPath(D_WIIUSER_IDX) + "shared2/sys/net/02/config.dat");

	File::IOFile file(filename, "rb");
	if (!file.ReadBytes(&m_Ifconfig, 1))
	{
		INFO_LOG(WII_IPC_NET, "NET_NCD_MANAGE: Failed to load /shared2/sys/net/02/config.dat, using dummy configuration");

		// wired connection on IP 192.168.1.1 using gateway 192.168.1.2
		memset(&m_Ifconfig, 0, sizeof(m_Ifconfig));
		m_Ifconfig.connType = 1;
		m_Ifconfig.linkTimeout = 7;
		m_Ifconfig.connection[0].flags = 167;
		m_Ifconfig.connection[0].ip[0] = 192;
		m_Ifconfig.connection[0].ip[1] = 168;
		m_Ifconfig.connection[0].ip[2] =   1;
		m_Ifconfig.connection[0].ip[3] =   150;
		m_Ifconfig.connection[0].netmask[0] = 255;
		m_Ifconfig.connection[0].netmask[1] = 255;
		m_Ifconfig.connection[0].netmask[2] = 255;
		m_Ifconfig.connection[0].netmask[3] = 0;
		m_Ifconfig.connection[0].gateway[0] = 192;
		m_Ifconfig.connection[0].gateway[1] = 168;
		m_Ifconfig.connection[0].gateway[2] =   1;
		m_Ifconfig.connection[0].gateway[3] =   1;
		m_Ifconfig.connection[0].dns1[0] = 8;
		m_Ifconfig.connection[0].dns1[1] = 8;
		m_Ifconfig.connection[0].dns1[2] = 8;
		m_Ifconfig.connection[0].dns1[3] = 8;
		m_Ifconfig.connection[1].flags = 167;
		m_Ifconfig.connection[1].ip[0] = 192;
		m_Ifconfig.connection[1].ip[1] = 168;
		m_Ifconfig.connection[1].ip[2] =   1;
		m_Ifconfig.connection[1].ip[3] =   150;
		m_Ifconfig.connection[1].netmask[0] = 255;
		m_Ifconfig.connection[1].netmask[1] = 255;
		m_Ifconfig.connection[1].netmask[2] = 255;
		m_Ifconfig.connection[1].netmask[3] = 0;
		m_Ifconfig.connection[1].gateway[0] = 192;
		m_Ifconfig.connection[1].gateway[1] = 168;
		m_Ifconfig.connection[1].gateway[2] =   1;
		m_Ifconfig.connection[1].gateway[3] =   1;
		m_Ifconfig.connection[1].dns1[0] = 8;
		m_Ifconfig.connection[1].dns1[1] = 8;
		m_Ifconfig.connection[1].dns1[2] = 8;
		m_Ifconfig.connection[1].dns1[3] = 8;
		m_Ifconfig.connection[2].flags = 167;
		m_Ifconfig.connection[2].ip[0] = 192;
		m_Ifconfig.connection[2].ip[1] = 168;
		m_Ifconfig.connection[2].ip[2] =   1;
		m_Ifconfig.connection[2].ip[3] =   150;
		m_Ifconfig.connection[2].netmask[0] = 255;
		m_Ifconfig.connection[2].netmask[1] = 255;
		m_Ifconfig.connection[2].netmask[2] = 255;
		m_Ifconfig.connection[2].netmask[3] = 0;
		m_Ifconfig.connection[2].gateway[0] = 192;
		m_Ifconfig.connection[2].gateway[1] = 168;
		m_Ifconfig.connection[2].gateway[2] =   1;
		m_Ifconfig.connection[2].gateway[3] =   1;
		m_Ifconfig.connection[2].dns1[0] = 8;
		m_Ifconfig.connection[2].dns1[1] = 8;
		m_Ifconfig.connection[2].dns1[2] = 8;
		m_Ifconfig.connection[2].dns1[3] = 8;

	}
}

CWII_IPC_HLE_Device_net_ncd_manage::~CWII_IPC_HLE_Device_net_ncd_manage() 
{
}

bool CWII_IPC_HLE_Device_net_ncd_manage::Open(u32 _CommandAddress, u32 _Mode)
{
	INFO_LOG(WII_IPC_NET, "NET_NCD_MANAGE: Open");
	Memory::Write_U32(GetDeviceID(), _CommandAddress+4);
	m_Active = true;
	return true;
}

bool CWII_IPC_HLE_Device_net_ncd_manage::Close(u32 _CommandAddress, bool _bForce)
{
	INFO_LOG(WII_IPC_NET, "NET_NCD_MANAGE: Close");
	if (!_bForce)
		Memory::Write_U32(0, _CommandAddress + 4);
	m_Active = false;
	return true;
}

bool CWII_IPC_HLE_Device_net_ncd_manage::IOCtlV(u32 _CommandAddress)
{
	u32 ReturnValue = 0;
	SIOCtlVBuffer CommandBuffer(_CommandAddress);

	switch (CommandBuffer.Parameter)
	{
	case IOCTLV_NCD_READCONFIG: // 7004 Out, 32 Out. 2nd, 3rd
		INFO_LOG(WII_IPC_NET, "NET_NCD_MANAGE: IOCTLV_NCD_READCONFIG");
		//break;

	case IOCTLV_NCD_GETCONFIG: // 7004 out, 32 out
		// first out buffer gets filled with contents of /shared2/sys/net/02/config.dat
		// TODO: What's the second output buffer for?
		{
			INFO_LOG(WII_IPC_NET, "NET_NCD_MANAGE: IOCTLV_NCD_GETCONFIG");
			u8 params1[0x1b5c];
			u8 params2[0x20];

			// fill output buffer, taking care of endianness
			u32 addr = CommandBuffer.PayloadBuffer.at(0).m_Address;
			u32 _BufferOut2 = CommandBuffer.PayloadBuffer.at(1).m_Address;

			Memory::ReadBigEData((u8*)params1, addr, 0x1b5c);
			Memory::ReadBigEData((u8*)params2, CommandBuffer.PayloadBuffer.at(1).m_Address, 0x20);

			Memory::WriteBigEData((const u8*)&m_Ifconfig, addr, 8);
			addr += 8;
			for (unsigned int i = 0; i < 3; i++)
			{
				netcfg_connection_t *conn = &m_Ifconfig.connection[i];

				Memory::WriteBigEData((const u8*)conn, addr, 26);
				Memory::Write_U16(Common::swap16(conn->mtu), addr+26);
				Memory::WriteBigEData((const u8*)conn->padding_3, addr+28, 8);

				Memory::WriteBigEData((const u8*)&conn->proxy_settings, addr+36, 260);
				Memory::Write_U16(Common::swap16(conn->proxy_settings.proxy_port), addr+296);
				Memory::WriteBigEData((const u8*)&conn->proxy_settings.proxy_username, addr+298, 65);
				Memory::Write_U8(conn->padding_4, addr+363);

				Memory::WriteBigEData((const u8*)&conn->proxy_settings_copy, addr+364, 260);
				Memory::Write_U16(Common::swap16(conn->proxy_settings_copy.proxy_port), addr+624);
				Memory::WriteBigEData((const u8*)&conn->proxy_settings_copy.proxy_username, addr+626, 65);
				Memory::WriteBigEData((const u8*)conn->padding_5, addr+691, 1641);
				addr += sizeof(netcfg_connection_t);
			}
			Memory::Write_U32(0, _BufferOut2);
			Memory::Write_U32(0, _BufferOut2+4);
			ReturnValue = 0;
			break;
		}

	case IOCTLV_NCD_SETCONFIG: // 7004 In, 32 Out. 4th
		u8 param1[7004];
		Memory::ReadBigEData(param1,CommandBuffer.InBuffer[0].m_Address, 100);
		/*if (param1[4] == 2)
		status = 3;
		if (param1[4] == 1)
		status = 5;
		if (param1[4] == 0)
		status = 2;*/
		INFO_LOG(WII_IPC_NET, "NET_NCD_MANAGE: IOCTLV_NCD_SETCONFIG");
		break;

	case IOCTLV_NCD_GETLINKSTATUS: // 32 Out. 5th
		{
			INFO_LOG(WII_IPC_NET, "NET_NCD_MANAGE: IOCTLV_NCD_GETLINKSTATUS");
			Memory::Write_U32(0x00000000, CommandBuffer.PayloadBuffer[0].m_Address);
			Memory::Write_U32(status, CommandBuffer.PayloadBuffer[0].m_Address+4);
			break;
		}

	case IOCTLV_NCD_GETWIRELESSMACADDRESS: // 32 Out, 6 Out. 1st
		{
			INFO_LOG(WII_IPC_NET, "NET_NCD_MANAGE: IOCTLV_NCD_GETWIRELESSMACADDRESS");
			Memory::Write_U32(0, CommandBuffer.PayloadBuffer.at(0).m_Address);
			Memory::WriteBigEData(default_address, CommandBuffer.PayloadBuffer.at(1).m_Address, 6);
			break;
		}

	default:
		INFO_LOG(WII_IPC_NET, "NET_NCD_MANAGE IOCtlV: %#x", CommandBuffer.Parameter);
		break;
	}

	Memory::Write_U32(ReturnValue, _CommandAddress+4);
	return true;
}

// **********************************************************************************
// Handle /dev/net/ip/top requests
CWII_IPC_HLE_Device_net_ip_top::CWII_IPC_HLE_Device_net_ip_top(u32 _DeviceID, const std::string& _rDeviceName) 
	: IWII_IPC_HLE_Device(_DeviceID, _rDeviceName)
{
#ifdef _WIN32
	int ret = WSAStartup(MAKEWORD(2,2), &InitData);
	INFO_LOG(WII_IPC_NET, "WSAStartup: %d", ret);
#endif
}

CWII_IPC_HLE_Device_net_ip_top::~CWII_IPC_HLE_Device_net_ip_top() 
{
#ifdef _WIN32
	WSACleanup();
#endif
}

bool CWII_IPC_HLE_Device_net_ip_top::Open(u32 _CommandAddress, u32 _Mode)
{
	INFO_LOG(WII_IPC_NET, "NET_IP_TOP: Open");
	Memory::Write_U32(GetDeviceID(), _CommandAddress+4);
	m_Active = true;
	return true;
}

bool CWII_IPC_HLE_Device_net_ip_top::Close(u32 _CommandAddress, bool _bForce)
{
	INFO_LOG(WII_IPC_NET, "NET_IP_TOP: Close");
	if (!_bForce)
		Memory::Write_U32(0, _CommandAddress + 4);
	m_Active = false;
	return true;
}

bool CWII_IPC_HLE_Device_net_ip_top::IOCtl(u32 _CommandAddress) 
{ 
	u32 BufferIn		= Memory::Read_U32(_CommandAddress + 0x10);
	u32 BufferInSize	= Memory::Read_U32(_CommandAddress + 0x14);
	u32 BufferOut		= Memory::Read_U32(_CommandAddress + 0x18);
	u32 BufferOutSize	= Memory::Read_U32(_CommandAddress + 0x1C);
	u32 Command			= Memory::Read_U32(_CommandAddress + 0x0C);

	u32 ReturnValue = ExecuteCommand(Command, BufferIn, BufferInSize, BufferOut, BufferOutSize);	
	Memory::Write_U32(ReturnValue, _CommandAddress + 0x4);

	return true;
}

struct bind_params
{
	u32 socket;
	u32 has_name;
	u8 name[28];
};

struct GC_sockaddr
{
	u8 sa_len;
	u8 sa_family;
	s8 sa_data[14];
};

struct GC_in_addr
{
	// this cannot be named s_addr under windows - collides with some crazy define.
	u32 s_addr_;
};

struct GC_sockaddr_in
{
	u8 sin_len;
	u8 sin_family;
	u16 sin_port;
	struct GC_in_addr sin_addr;
	s8 sin_zero[8];
};

#ifdef _WIN32
char *DecodeError(int ErrorCode)
{
	static char Message[1024];

	// If this program was multi-threaded, we'd want to use FORMAT_MESSAGE_ALLOCATE_BUFFER
	// instead of a static buffer here.
	// (And of course, free the buffer when we were done with it)
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS |
		FORMAT_MESSAGE_MAX_WIDTH_MASK, NULL, ErrorCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)Message, 1024, NULL);
	return Message;
}
#endif

static int getNetErrorCode(int ret, std::string caller, bool isRW)
{
#ifdef _WIN32
	int errorCode = WSAGetLastError();
	if (ret>= 0)
		return ret;

	WARN_LOG(WII_IPC_NET, "%s failed with error %d: %s, ret= %d\n",
		caller.c_str(), errorCode, DecodeError(errorCode), ret);

	switch (errorCode)
	{
	case 10040:
		WARN_LOG(WII_IPC_NET, "Find out why this happened, looks like PEEK failure?\n");
		return -1;
	case 10054:
		return -15;
	case 10056:
		return -30;
	case 10057:
		return -6;
	case 10035:
		if(isRW){
			return -6;
		}else{
			return -26;
		}
	case 6:
		return -26;
	default:
		return -1;
	}
#else
	INFO_LOG(WII_IPC_NET, "%s failed with error %d: %s, ret= %d\n",
		caller.c_str(), 0x1337, "hmm", ret);
	return ret;
#endif
}

static int convertWiiSockOpt(int level, int optname)
{
	switch (optname)
	{
	case 0x1009:
		return 0x1007; //SO_ERROR
	default:
		return optname;
	}
}

static int inet_pton(const char *src, unsigned char *dst)
{
	static const char digits[] = "0123456789";
	int saw_digit, octets, ch;
	unsigned char tmp[4], *tp;

	saw_digit = 0;
	octets = 0;
	*(tp = tmp) = 0;
	while ((ch = *src++) != '\0') {
		const char *pch;

		if ((pch = strchr(digits, ch)) != NULL) {
			unsigned int newt = *tp * 10 + (pch - digits);

			if (newt > 255)
				return (0);
			*tp = newt;
			if (! saw_digit) {
				if (++octets > 4)
					return (0);
				saw_digit = 1;
			}
		} else if (ch == '.' && saw_digit) {
			if (octets == 4)
				return (0);
			*++tp = 0;
			saw_digit = 0;
		} else
			return (0);
	}
	if (octets < 4)
		return (0);
	memcpy(dst, tmp, 4);
	return (1);
}


u32 CWII_IPC_HLE_Device_net_ip_top::ExecuteCommand(u32 _Command,
	u32 _BufferIn, u32 BufferInSize,
	u32 _BufferOut, u32 BufferOutSize)
{		
	switch (_Command)
	{
	case IOCTL_SO_STARTUP:
		INFO_LOG(WII_IPC_NET, "/dev/net/ip/top::IOCtl request IOCTL_SO_STARTUP "
			"BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
			_BufferIn, BufferInSize, _BufferOut, BufferOutSize); 
		break;

	case IOCTL_SO_CONNECT:
		{
			//struct sockaddr_in echoServAddr;
			struct connect_params{
				u32 socket;
				u32 has_addr;
				u8 addr[28];
			};
			struct connect_params params;
			struct sockaddr_in serverAddr;

			Memory::ReadBigEData((u8*)&params, _BufferIn, sizeof(connect_params));

			if (Common::swap32(params.has_addr) != 1)
				return -1;

			memset(&serverAddr, 0, sizeof(serverAddr));
			memcpy(&serverAddr, params.addr, params.addr[0]);

			// GC/Wii sockets have a length param as well, we dont really care :)
			serverAddr.sin_family = serverAddr.sin_family >> 8;

			int ret = connect(Common::swap32(params.socket), (struct sockaddr *) &serverAddr, sizeof(serverAddr));
			ret = getNetErrorCode(ret, "SO_CONNECT", false);
			INFO_LOG(WII_IPC_NET,"/dev/net/ip/top::IOCtl request IOCTL_SO_CONNECT (%08x, %s:%d)",
				Common::swap32(params.socket), inet_ntoa(serverAddr.sin_addr), Common::swap16(serverAddr.sin_port));

			return ret;
			break;
		}

	case IOCTL_SO_SHUTDOWN:
		{
			INFO_LOG(WII_IPC_NET, "/dev/net/ip/top::IOCtl request IOCTL_SO_SHUTDOWN "
				"BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
				_BufferIn, BufferInSize, _BufferOut, BufferOutSize);

			u32 sock	= Memory::Read_U32(_BufferIn);
			u32 how		= Memory::Read_U32(_BufferIn+4);
			int ret = shutdown(sock, how);
			return getNetErrorCode(ret, "SO_SHUTDOWN", false);
			break;
		}

	case IOCTL_SO_CLOSE:
		{
			u32 sock = Memory::Read_U32(_BufferIn);
			INFO_LOG(WII_IPC_NET, "/dev/net/ip/top::IOCtl request IOCTL_SO_CLOSE (%08x)", sock);

#ifdef _WIN32
			u32 ret = closesocket(sock);

			return getNetErrorCode(ret, "IOCTL_SO_CLOSE", false);
#else
			return close(sock);
#endif
			break;
		}

	case IOCTL_SO_SOCKET:
		{
			u32 AF		= Memory::Read_U32(_BufferIn);
			u32 TYPE	= Memory::Read_U32(_BufferIn + 0x04);
			u32 PROT	= Memory::Read_U32(_BufferIn + 0x08);
			u32 s		= (u32)socket(AF, TYPE, PROT);
			INFO_LOG(WII_IPC_NET, "/dev/net/ip/top::IOCtl request IOCTL_SO_SOCKET "
				"Socket: %08x (%d,%d,%d), BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
				s, AF, TYPE, PROT, _BufferIn, BufferInSize, _BufferOut, BufferOutSize);
			return getNetErrorCode(s, "SO_SOCKET", false);
			break;
		}

	case IOCTL_SO_BIND:
		{
			bind_params *addr = (bind_params*)Memory::GetPointer(_BufferIn);
			GC_sockaddr_in addrPC;
			memcpy(&addrPC, addr->name, sizeof(GC_sockaddr_in));
			sockaddr_in address;
			address.sin_family = addrPC.sin_family;
			address.sin_addr.s_addr = addrPC.sin_addr.s_addr_;
			address.sin_port = addrPC.sin_port;
			int ret = bind(Common::swap32(addr->socket), (sockaddr*)&address, sizeof(address));

			INFO_LOG(WII_IPC_NET, "/dev/net/ip/top::IOCtl request IOCTL_SO_BIND (%s:%d) "
				"Socket: %08X, BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
				inet_ntoa(address.sin_addr), Common::swap16(address.sin_port),
				Common::swap32(addr->socket), _BufferIn, BufferInSize, _BufferOut, BufferOutSize);

			return getNetErrorCode(ret, "SO_BIND", false);
			break;
		}

	case IOCTL_SO_LISTEN:
		{
			INFO_LOG(WII_IPC_NET, "/dev/net/ip/top::IOCtl request IOCTL_SO_LISTEN "
				"BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
				_BufferIn, BufferInSize, _BufferOut, BufferOutSize);

			u32 S = Memory::Read_U32(_BufferIn);
			u32 BACKLOG = Memory::Read_U32(_BufferIn + 0x04);
			u32 ret = listen(S, BACKLOG);
			return getNetErrorCode(ret, "SO_LISTEN", false);
			break;
		}

	case IOCTL_SO_ACCEPT:
		{
			INFO_LOG(WII_IPC_NET, "/dev/net/ip/top::IOCtl request IOCTL_SO_ACCEPT "
				"BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
				_BufferIn, BufferInSize, _BufferOut, BufferOutSize);

			u32 S = Memory::Read_U32(_BufferIn);
			struct sockaddr* addr = (struct sockaddr*) Memory::GetPointer(_BufferOut);
			socklen_t* addrlen = (socklen_t*) Memory::GetPointer(BufferOutSize);
			*addrlen = sizeof(struct sockaddr);
			int ret = accept(S, addr, addrlen);
			return getNetErrorCode(ret, "SO_ACCEPT", false);
		}

	case IOCTL_SO_GETSOCKOPT:
		{
			u32 sock = Memory::Read_U32(_BufferOut);
			u32 level = Memory::Read_U32(_BufferOut + 4);
			u32 optname = Memory::Read_U32(_BufferOut + 8);



			INFO_LOG(WII_IPC_NET,"/dev/net/ip/top::IOCtl request IOCTL_SO_GETSOCKOPT(%08x, %08x, %08x)"
				"BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
				sock, level, optname,
				_BufferIn, BufferInSize, _BufferOut, BufferOutSize);

			optname = convertWiiSockOpt(level,optname);

			u8 optval[20];
			u32 optlen = 4;

			//for(int i=0; i<BufferOutSize; i++) INFO_LOG(WII_IPC_NET,"%02x ", Memory::Read_U8(_BufferOut + i));

			int ret = getsockopt (sock, level, optname, (char *) &optval, (socklen_t*)&optlen);

			ret = getNetErrorCode(ret, "SO_GETSOCKOPT", false);


			Memory::Write_U32(optlen, _BufferOut + 0xC);
			Memory::WriteBigEData((u8 *) optval, _BufferOut + 0x10, optlen);

			if(optname == 0x1007){
				s32 errorcode = Memory::Read_U32(_BufferOut + 0x10);
				INFO_LOG(WII_IPC_NET,"/dev/net/ip/top::IOCtl request IOCTL_SO_GETSOCKOPT error code = %i", errorcode);
			}
			return ret;
		}

	case IOCTL_SO_SETSOCKOPT:
		{
			u32 S = Memory::Read_U32(_BufferIn);
			u32 level = Memory::Read_U32(_BufferIn + 4);
			u32 optname = Memory::Read_U32(_BufferIn + 8);
			u32 optlen = Memory::Read_U32(_BufferIn + 0xc);
			u8 optval[20];
			Memory::ReadBigEData(optval, _BufferIn + 0x10, optlen);

			//TODO: bug booto about this, 0x2005 most likely timeout related, default value on wii is , 0x2001 is most likely tcpnodelay
			if (level == 6 && (optname == 0x2005 || optname == 0x2001)){
				return 0;
			}
			INFO_LOG(WII_IPC_NET, "/dev/net/ip/top::IOCtl request IOCTL_SO_SETSOCKOPT(%08x, %08x, %08x, %08x) "
				"BufferIn: (%08x, %i), BufferOut: (%08x, %i)"
				"%02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx",
				S, level, optname, optlen, _BufferIn, BufferInSize, _BufferOut, BufferOutSize, optval[0], optval[1], optval[2], optval[3], optval[4], optval[5], optval[6], optval[7], optval[8], optval[9], optval[10], optval[11], optval[12], optval[13], optval[14], optval[15], optval[16], optval[17], optval[18], optval[19]);

			int ret = setsockopt(S, level, optname, (char*)optval, optlen);

			ret = getNetErrorCode(ret, "SO_SETSOCKOPT", false);

			return ret;
		}

	case IOCTL_SO_FCNTL:
		{
			u32 sock	= Memory::Read_U32(_BufferIn);
			u32 cmd		= Memory::Read_U32(_BufferIn + 4);
			u32 arg		= Memory::Read_U32(_BufferIn + 8);

			INFO_LOG(WII_IPC_NET, "/dev/net/ip/top::IOCtl request IOCTL_SO_FCNTL(%08X, %08X) "
				"Socket: %08x, BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
				cmd, arg,
				sock, _BufferIn, BufferInSize, _BufferOut, BufferOutSize);
#ifdef _WIN32
#define F_GETFL			3
#define F_SETFL			4
#define F_NONBLOCK		4
			if (cmd == F_GETFL)
			{
				INFO_LOG(WII_IPC_NET, "F_GETFL WTF?");
			}
			else if (cmd == F_SETFL)
			{
				u_long iMode = 0;
				if (arg & F_NONBLOCK)
					iMode = 1;
				int ioctlret = ioctlsocket(sock, FIONBIO, &iMode);
				return getNetErrorCode(ioctlret, "SO_FCNTL", false);
			}
			else
			{
				INFO_LOG(WII_IPC_NET, "UNKNOWN WTF?");
			}
			return 0;
#else
			return fcntl(sock, cmd, arg);
#endif
		}

	case IOCTL_SO_GETSOCKNAME:
		{
			u32 sock = Memory::Read_U32(_BufferIn);

			INFO_LOG(WII_IPC_NET, "/dev/net/ip/top::IOCtl request IOCTL_SO_GETSOCKNAME "
				"Socket: %08X, BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
				sock, _BufferIn, BufferInSize, _BufferOut, BufferOutSize);

			struct sockaddr sa;
			socklen_t sa_len;
			sa_len = sizeof(sa);                           
			int ret = getsockname(sock, &sa, &sa_len);

			Memory::Write_U8(BufferOutSize, _BufferOut);
			Memory::Write_U8(sa.sa_family & 0xFF, _BufferOut+1);
			Memory::WriteBigEData((u8*)&sa.sa_data, _BufferOut+2, BufferOutSize-2);
			return ret;
		}

	case IOCTL_SO_GETHOSTID:
		INFO_LOG(WII_IPC_NET, "/dev/net/ip/top::IOCtl request IOCTL_SO_GETHOSTID "
			"(BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
			_BufferIn, BufferInSize, _BufferOut, BufferOutSize);
		return 192 << 24 | 168 << 16 | 1 << 8 | 150;

	case IOCTL_SO_INETATON:
		{
			INFO_LOG(WII_IPC_NET, "/dev/net/ip/top::IOCtl request IOCTL_SO_INETATON "
				"%s, BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
				(char*)Memory::GetPointer(_BufferIn), _BufferIn, BufferInSize, _BufferOut, BufferOutSize);
			struct hostent *remoteHost = gethostbyname((char*)Memory::GetPointer(_BufferIn));
			Memory::Write_U32(Common::swap32(*(u_long *)remoteHost->h_addr_list[0]), _BufferOut);
			return remoteHost->h_addr_list[0] == 0 ? -1 : 0;
		}

	case IOCTL_SO_INETPTON:
		INFO_LOG(WII_IPC_NET, "/dev/net/ip/top::IOCtl request IOCTL_SO_INETPTON "
			"(Translating: %s)", Memory::GetPointer(_BufferIn));
		return inet_pton((char*)Memory::GetPointer(_BufferIn), Memory::GetPointer(_BufferOut+4));
		break;

	case IOCTL_SO_POLL:
		{
#ifdef _WIN32
			u32 unknown = Memory::Read_U32(_BufferIn);
			u32 timeout = Memory::Read_U32(_BufferIn + 4);

			int nfds = BufferOutSize / 0xc;
			if (nfds == 0)
				ERROR_LOG(WII_IPC_NET,"Hidden POLL");
			pollfd_t* ufds = (pollfd_t *)malloc(sizeof(pollfd_t) * nfds);
			if (ufds == NULL)
				return -1;
			for (int i = 0; i < nfds; i++)
			{
				ufds[i].fd = Memory::Read_U32(_BufferOut + 0xc*i);
				ufds[i].events = Memory::Read_U32(_BufferOut + 0xc*i + 4);
				ufds[i].revents = Memory::Read_U32(_BufferOut + 0xc*i + 8);
				INFO_LOG(WII_IPC_NET, "/dev/net/ip/top::IOCtl request IOCTL_SO_POLL(%d) "
					"Sock: %08x, Unknown: %08x, Events: %08x, "
					"BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
					i, ufds[i].fd, unknown, ufds[i].events,
					_BufferIn, BufferInSize, _BufferOut, BufferOutSize);
			}
			int ret = poll(ufds, nfds, timeout);

			ret = getNetErrorCode(ret, "SO_SETSOCKOPT", false);

			for (int i = 0; i<nfds; i++)
			{
				Memory::Write_U32(ufds[i].fd, _BufferOut + 0xc*i);
				Memory::Write_U32(ufds[i].events, _BufferOut + 0xc*i + 4);
				Memory::Write_U32(ufds[i].revents, _BufferOut + 0xc*i + 8);
			}
			free(ufds);
			return ret;
#else
#pragma warning /dev/net/ip/top::IOCtl IOCTL_SO_POLL unimplemented
#endif
			break;
		}

	case IOCTL_SO_GETHOSTBYNAME:
		{
			int i;
			struct hostent *remoteHost = gethostbyname((char*)Memory::GetPointer(_BufferIn));

			INFO_LOG(WII_IPC_NET, "/dev/net/ip/top::IOCtl request IOCTL_SO_GETHOSTBYNAME "
				"Address: %s, BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
				(char*)Memory::GetPointer(_BufferIn), _BufferIn, BufferInSize, _BufferOut, BufferOutSize);

			if (remoteHost != NULL)
			{
				Memory::Write_U32(_BufferOut + 0x10, _BufferOut);

				//strnlen?! huh, u mean you DONT want an overflow?
				int hnamelen = strnlen(remoteHost->h_name, 255);
				Memory::WriteBigEData((u8*)remoteHost->h_name, _BufferOut + 0x10, hnamelen);

				Memory::Write_U16(remoteHost->h_addrtype, _BufferOut + 0x8);
				Memory::Write_U16(remoteHost->h_length, _BufferOut + 0xA);
				Memory::Write_U32(_BufferOut + 0x340, _BufferOut + 0xC);

				for (i = 0; remoteHost->h_addr_list[i] != 0; i++)
				{
					u32 ip = *(u_long *)remoteHost->h_addr_list[i];
					Memory::Write_U32(_BufferOut + 0x110 + i*4 ,_BufferOut + 0x340 + i*4);
					Memory::Write_U32(Common::swap32(ip), _BufferOut + 0x110 + i*4);
				}
				i++;
				Memory::Write_U32(_BufferOut + 0x340 + i*4, _BufferOut + 0x4);
				return 0;
			}
			else
			{
				return -1;
			}

			break;
		}

	default:
		INFO_LOG(WII_IPC_NET,"/dev/net/ip/top::IOCtl request 0x%x "
			"BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
			_Command, _BufferIn, BufferInSize, _BufferOut, BufferOutSize);
		break;
	}

	// We return a success for any potential unknown requests
	return 0;
}

u32 CWII_IPC_HLE_Device_net_ip_top::ExecuteCommandV(u32 _Parameter, SIOCtlVBuffer CommandBuffer)
{
	u32 _BufferIn = 0, _BufferIn2 = 0, _BufferIn3 = 0;
	u32 BufferInSize = 0, BufferInSize2 = 0, BufferInSize3 = 0;

	u32 _BufferOut = 0, _BufferOut2 = 0, _BufferOut3 = 0;
	u32 BufferOutSize = 0, BufferOutSize2 = 0, BufferOutSize3 = 0;

	if (CommandBuffer.InBuffer.size() > 0)
	{
		_BufferIn = CommandBuffer.InBuffer.at(0).m_Address;
		BufferInSize = CommandBuffer.InBuffer.at(0).m_Size;
	}
	if (CommandBuffer.InBuffer.size() > 1)
	{
		_BufferIn2 = CommandBuffer.InBuffer.at(1).m_Address;
		BufferInSize2 = CommandBuffer.InBuffer.at(1).m_Size;
	}
	if (CommandBuffer.InBuffer.size() > 2)
	{
		_BufferIn3 = CommandBuffer.InBuffer.at(2).m_Address;
		BufferInSize3 = CommandBuffer.InBuffer.at(2).m_Size;
	}

	if (CommandBuffer.PayloadBuffer.size() > 0)
	{
		_BufferOut = CommandBuffer.PayloadBuffer.at(0).m_Address;
		BufferOutSize = CommandBuffer.PayloadBuffer.at(0).m_Size;
	}
	if (CommandBuffer.PayloadBuffer.size() > 1)
	{
		_BufferOut2 = CommandBuffer.PayloadBuffer.at(1).m_Address;
		BufferOutSize2 = CommandBuffer.PayloadBuffer.at(1).m_Size;
	}
	if (CommandBuffer.PayloadBuffer.size() > 2)
	{
		_BufferOut3 = CommandBuffer.PayloadBuffer.at(2).m_Address;
		BufferOutSize3 = CommandBuffer.PayloadBuffer.at(2).m_Size;
	}

	//struct ifreq {  /* BUGBUG: reduced form of ifreq just for this hack */
	//	char ifr_name[16];
	//	struct sockaddr ifr_addr;
	//};

	//struct ifreq ifr; struct sockaddr_in saddr;
	//int fd;

#ifdef _WIN32
	PIP_ADAPTER_ADDRESSES AdapterAddresses = NULL;
	ULONG OutBufferLength = 0;
	ULONG RetVal = 0, i;
#endif

	u32 param = 0, param2 = 0, param3, param4, param5 = 0;

	switch (_Parameter)
	{
	case IOCTLV_SO_GETINTERFACEOPT:
		{
			param = Memory::Read_U32(_BufferIn);
			param2 = Memory::Read_U32(_BufferIn+4);
			param3 = Memory::Read_U32(_BufferOut);
			param4 = Memory::Read_U32(_BufferOut2);
			if (BufferOutSize >= 8)
			{

				param5 = Memory::Read_U32(_BufferOut+4);
			}

			INFO_LOG(WII_IPC_NET,"/dev/net/ip/top::IOCtlV request IOCTLV_SO_GETINTERFACEOPT(%08X, %08X) "
				"BufferIn: (%08x, %i), BufferIn2: (%08x, %i)",
				param, param2,
				_BufferIn, BufferInSize, _BufferIn2, BufferInSize2);

			switch (param2)
			{
			case 0xb003:
				{
					u32 address = 0;
					/*fd=socket(PF_INET,SOCK_STREAM,0);
					strcpy(ifr.ifr_name,"name of interface");
					ioctl(fd,SIOCGIFADDR,&ifr);
					saddr=*((struct sockaddr_in *)(&(ifr.ifr_addr)));
					*/
#ifdef _WIN32
					for (i = 0; i < 5; i++)
					{
						RetVal = GetAdaptersAddresses(
							AF_INET, 
							0,
							NULL, 
							AdapterAddresses, 
							&OutBufferLength);

						if (RetVal != ERROR_BUFFER_OVERFLOW) {
							break;
						}

						if (AdapterAddresses != NULL) {
							FREE(AdapterAddresses);
						}

						AdapterAddresses = (PIP_ADAPTER_ADDRESSES)MALLOC(OutBufferLength);
						if (AdapterAddresses == NULL) {
							RetVal = GetLastError();
							break;
						}
					}
					if (RetVal == NO_ERROR)
					{
						unsigned long dwBestIfIndex = 0;
						IPAddr dwDestAddr = (IPAddr)0x08080808; 
						// If successful, output some information from the data we received
						PIP_ADAPTER_ADDRESSES AdapterList = AdapterAddresses;
						if (GetBestInterface(dwDestAddr,&dwBestIfIndex) == NO_ERROR)
						{
							while (AdapterList)
							{
								if (AdapterList->IfIndex == dwBestIfIndex &&
									AdapterList->FirstDnsServerAddress &&
									AdapterList->OperStatus == IfOperStatusUp)
								{
									INFO_LOG(WII_IPC_NET, "Name of valid interface: %S", AdapterList->FriendlyName);
									INFO_LOG(WII_IPC_NET, "DNS: %u.%u.%u.%u",
										(unsigned char)AdapterList->FirstDnsServerAddress->Address.lpSockaddr->sa_data[2],
										(unsigned char)AdapterList->FirstDnsServerAddress->Address.lpSockaddr->sa_data[3],
										(unsigned char)AdapterList->FirstDnsServerAddress->Address.lpSockaddr->sa_data[4],
										(unsigned char)AdapterList->FirstDnsServerAddress->Address.lpSockaddr->sa_data[5]);
									address = Common::swap32(*(u32*)(&AdapterList->FirstDnsServerAddress->Address.lpSockaddr->sa_data[2]));
									break;
								}
								AdapterList = AdapterList->Next;
							}
						}
					}
					if (AdapterAddresses != NULL) {
						FREE(AdapterAddresses);
					}
#endif
					if (address == 0)
						address = 0x08080808;

					Memory::Write_U32(address, _BufferOut);
					Memory::Write_U32(0x08080808, _BufferOut+4);
					break;
				}

			case 0x1003:
				Memory::Write_U32(0, _BufferOut);
				break;

			case 0x1004:
				Memory::WriteBigEData(default_address, _BufferOut, 6);
				break;
				
			case 0x1005:
				Memory::Write_U32(1, _BufferOut);
				Memory::Write_U32(4, _BufferOut2);
				break;
			case 0x4002:
				Memory::Write_U32(2, _BufferOut);
				break;

			case 0x4003:
				Memory::Write_U32(0xC, _BufferOut2);
				Memory::Write_U32(10 << 24 | 1 << 8 | 30, _BufferOut);
				Memory::Write_U32(255 << 24 | 255 << 16  | 255 << 8 | 0, _BufferOut+4);
				Memory::Write_U32(10 << 24 | 0 << 16  | 255 << 8 | 255, _BufferOut+8);
				break;

			default:
				ERROR_LOG(WII_IPC_NET, "Unknown param2: %08X", param2);
				break;

			}

			return 0;
			break;
		}

	case IOCTLV_SO_SENDTO:
		{
			struct sendto_params {
				u32 socket;
				u32 flags;
				u32 has_destaddr;
				u8 destaddr[28];
			};
			struct sendto_params params;
			Memory::ReadBigEData((u8*)&params, _BufferIn2, BufferInSize2);
			INFO_LOG(WII_IPC_NET, "/dev/net/ip/top::IOCtlV request IOCTLV_SO_SENDTO "
				"Socket: %08x, BufferIn: (%08x, %i), BufferIn2: (%08x, %i)",
				Common::swap32(params.socket), _BufferIn, BufferInSize, _BufferIn2, BufferInSize2);

			if (params.has_destaddr)
			{
				struct sockaddr_in* addr = (struct sockaddr_in*)&params.destaddr;
				u8 len = sizeof(sockaddr); //addr->sin_family & 0xFF;
				addr->sin_family = addr->sin_family >> 8;
				int ret = sendto(Common::swap32(params.socket), (char*)Memory::GetPointer(_BufferIn),
					BufferInSize, Common::swap32(params.flags), (struct sockaddr*)addr, len);
				return getNetErrorCode(ret, "SO_SENDTO", true);
			}
			else
			{
				int ret = send(Common::swap32(params.socket), (char*)Memory::GetPointer(_BufferIn),
					BufferInSize, Common::swap32(params.flags));
				return getNetErrorCode(ret, "SO_SEND", true);
			}
			break;
		}

	case IOCTLV_SO_RECVFROM:
		{
			u32 sock	= Memory::Read_U32(_BufferIn);
			u32 flags	= Memory::Read_U32(_BufferIn + 4);

			INFO_LOG(WII_IPC_NET, "/dev/net/ip/top::IOCtlV request IOCTLV_SO_RECVFROM "
				"Socket: %08X, Flags: %08X, "
				"BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
				"BufferOut: (%08x, %i), BufferOut2: (%08x, %i)",
				sock, flags,
				_BufferIn, BufferInSize, _BufferIn2, BufferInSize2,
				_BufferOut, BufferOutSize, _BufferOut2, BufferOutSize2);

			char *buf	= (char *)Memory::GetPointer(_BufferOut);
			int len		= BufferOutSize;
			struct sockaddr_in addr;
			socklen_t fromlen = 0;

			if (BufferOutSize2 != 0)
			{
				fromlen = BufferOutSize2 >= sizeof(struct sockaddr) ? BufferOutSize2 : sizeof(struct sockaddr);
			}

			if (flags != 2)
				flags = 0;
			else
				flags = MSG_PEEK;

			int ret;
#ifdef _WIN32
			if(flags & MSG_PEEK){
				unsigned long totallen = 0;
				ioctlsocket(sock, FIONREAD, &totallen);
				return totallen;
			}else
#endif
				ret = recvfrom(sock, buf, len, flags,
				fromlen ? (struct sockaddr*) &addr : NULL, fromlen ? &fromlen : 0);
			if (BufferOutSize2 != 0)
			{
				//something not quite right below, need to verify
				addr.sin_family = (addr.sin_family << 8) | (BufferOutSize2&0xFF);
				Memory::WriteBigEData((u8*)&addr, _BufferOut2, BufferOutSize2);
			}
			return getNetErrorCode(ret, "SO_RECVFROM", true);
			break;
		}

	case IOCTLV_SO_GETADDRINFO:
		{
			struct addrinfo hints;
			struct addrinfo *result = NULL;

			if (BufferInSize3)
			{
				hints.ai_flags		= Memory::Read_U32(_BufferIn3);
				hints.ai_family		= Memory::Read_U32(_BufferIn3 + 0x4);
				hints.ai_socktype	= Memory::Read_U32(_BufferIn3 + 0x8);
				hints.ai_protocol	= Memory::Read_U32(_BufferIn3 + 0xC);
				hints.ai_addrlen	= Memory::Read_U32(_BufferIn3 + 0x10);
				hints.ai_canonname	= (char*)Memory::Read_U32(_BufferIn3 + 0x14);
				hints.ai_addr		= (sockaddr *)Memory::Read_U32(_BufferIn3 + 0x18);
				hints.ai_next		= (addrinfo *)Memory::Read_U32(_BufferIn3 + 0x1C);
			}

			char* pNodeName = NULL;
			if (BufferInSize > 0)
				pNodeName = (char*)Memory::GetPointer(_BufferIn);

			char* pServiceName = NULL;
			if (BufferInSize2 > 0)
				pServiceName = (char*)Memory::GetPointer(_BufferIn2);

			int ret = getaddrinfo(pNodeName, pServiceName, BufferInSize3 ? &hints : NULL, &result);
			u32 addr = _BufferOut;
			u32 sockoffset = addr + 0x460;
			if (ret >= 0)
			{
				while (result != NULL)
				{
					Memory::Write_U32(result->ai_flags, addr);
					Memory::Write_U32(result->ai_family, addr + 0x04);
					Memory::Write_U32(result->ai_socktype, addr + 0x08);
					Memory::Write_U32(result->ai_protocol, addr + 0x0C);
					Memory::Write_U32(result->ai_addrlen, addr + 0x10);
					// what to do? where to put? the buffer of 0x834 doesn't allow space for this
					Memory::Write_U32(/*result->ai_cannonname*/ 0, addr + 0x14);

					if (result->ai_addr)
					{
						Memory::Write_U32(sockoffset, addr + 0x18);
						Memory::Write_U16(((result->ai_addr->sa_family & 0xFF) << 8) | (result->ai_addrlen & 0xFF), sockoffset);
						Memory::WriteBigEData((u8*)result->ai_addr->sa_data, sockoffset + 0x2, sizeof(result->ai_addr->sa_data));
						sockoffset += 0x1C;
					}
					else
					{
						Memory::Write_U32(0, addr + 0x18);
					}

					if (result->ai_next)
					{
						Memory::Write_U32(addr + sizeof(addrinfo), addr + 0x1C);
					}
					else
					{
						Memory::Write_U32(0, addr + 0x1C);
					}

					addr += sizeof(addrinfo);
					result = result->ai_next;
				}
			}

			INFO_LOG(WII_IPC_NET, "/dev/net/ip/top::IOCtlV request IOCTLV_SO_GETADDRINFO "
				"(BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
				_BufferIn, BufferInSize, _BufferOut, BufferOutSize);
			INFO_LOG(WII_IPC_NET, "IOCTLV_SO_GETADDRINFO: %s", Memory::GetPointer(_BufferIn));
			return ret;
		}

	default:
		INFO_LOG(WII_IPC_NET,"/dev/net/ip/top::IOCtlV request 0x%x (BufferIn: (%08x, %i), BufferIn2: (%08x, %i)",
			_Parameter, _BufferIn, BufferInSize, _BufferIn2, BufferInSize2);
		break;
	}
	return 0;
}

bool CWII_IPC_HLE_Device_net_ip_top::IOCtlV(u32 _CommandAddress) 
{ 
	u32 ReturnValue = 0;
	SIOCtlVBuffer CommandBuffer(_CommandAddress);
	switch (CommandBuffer.Parameter)
	{
	case IOCTLV_SO_SENDTO:
	case IOCTL_SO_BIND:
	case IOCTLV_SO_RECVFROM:
	case IOCTL_SO_SOCKET:
	case IOCTL_SO_GETHOSTID:
	case IOCTL_SO_STARTUP:
	default:
		ReturnValue = ExecuteCommandV(CommandBuffer.Parameter, CommandBuffer);
		break;
	}

	Memory::Write_U32(ReturnValue, _CommandAddress+4);
	return true; 
}

#ifdef _MSC_VER
#pragma optimize("",on)
#endif
