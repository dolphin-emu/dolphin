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
#include "../ConfigManager.h"
#include "FileUtil.h"
#include <stdio.h>
#include <string>
#include "ICMP.h"

#ifdef _WIN32
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <iphlpapi.h>

#include "fakepoll.h"
#define MALLOC(x) HeapAlloc(GetProcessHeap(), 0, (x)) 
#define FREE(x) HeapFree(GetProcessHeap(), 0, (x))

#elif defined(__linux__) or defined(__APPLE__)
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <errno.h>
#include <poll.h>
#include <string.h>

typedef struct pollfd pollfd_t;
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <netinet/in.h>
#include <errno.h>
#endif

extern std::queue<std::pair<u32,std::string> > g_ReplyQueueLater;
const u8 default_address[] = { 0x00, 0x17, 0xAB, 0x99, 0x99, 0x99 };

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
		WARN_LOG(WII_IPC_NET, "NET_KD_REQ: IOCTL_NWC24_SUSPEND_SCHEDULAR - NI");
		break;

	case IOCTL_NWC24_EXEC_TRY_SUSPEND_SCHEDULAR: // NWC24iResumeForCloseLib
		WARN_LOG(WII_IPC_NET, "NET_KD_REQ: IOCTL_NWC24_EXEC_TRY_SUSPEND_SCHEDULAR - NI");
		break;

	case IOCTL_NWC24_EXEC_RESUME_SCHEDULAR : // NWC24iResumeForCloseLib
		WARN_LOG(WII_IPC_NET, "NET_KD_REQ: IOCTL_NWC24_EXEC_RESUME_SCHEDULAR - NI");
		break;

	case IOCTL_NWC24_STARTUP_SOCKET: // NWC24iStartupSocket
		Memory::Write_U32(0, BufferOut);
		Memory::Write_U32(0, BufferOut+4);
		ReturnValue = 0;
		WARN_LOG(WII_IPC_NET, "NET_KD_REQ: IOCTL_NWC24_STARTUP_SOCKET - NI");
		break;
	case IOCTL_NWC24_CLEANUP_SOCKET:
		Memory::Memset(BufferOut, 0, BufferOutSize);
		WARN_LOG(WII_IPC_NET, "NET_KD_REQ: IOCTL_NWC24_CLEANUP_SOCKET - NI");
		break;
	case IOCTL_NWC24_LOCK_SOCKET: // WiiMenu
		WARN_LOG(WII_IPC_NET, "NET_KD_REQ: IOCTL_NWC24_LOCK_SOCKET - NI");
		break;

	case IOCTL_NWC24_UNLOCK_SOCKET:
		WARN_LOG(WII_IPC_NET, "NET_KD_REQ: IOCTL_NWC24_UNLOCK_SOCKET - NI");
		break;

	case IOCTL_NWC24_REQUEST_REGISTER_USER_ID:
		WARN_LOG(WII_IPC_NET, "NET_KD_REQ: IOCTL_NWC24_REQUEST_REGISTER_USER_ID");
		Memory::Write_U32(0, BufferOut);
		Memory::Write_U32(0, BufferOut+4);
		break;

	case IOCTL_NWC24_REQUEST_GENERATED_USER_ID: // (Input: none, Output: 32 bytes)
		WARN_LOG(WII_IPC_NET, "NET_KD_REQ: IOCTL_NWC24_REQUEST_GENERATED_USER_ID");
		//Memory::Write_U32(0xFFFFFFDC, BufferOut);
		//Memory::Write_U32(0x00050495, BufferOut + 4);
		//Memory::Write_U32(0x90CFBF35, BufferOut + 8);
		//Memory::Write_U32(0x00000002, BufferOut + 0xC);
		Memory::WriteBigEData((u8*)m_UserID.c_str(), BufferOut, m_UserID.length() + 1);
		break;

	case IOCTL_NWC24_GET_SCHEDULAR_STAT:
		WARN_LOG(WII_IPC_NET, "NET_KD_REQ: IOCTL_NWC24_GET_SCHEDULAR_STAT - NI");
		break;

	case IOCTL_NWC24_SAVE_MAIL_NOW:
		WARN_LOG(WII_IPC_NET, "NET_KD_REQ: IOCTL_NWC24_SAVE_MAIL_NOW - NI");
		break;

	case IOCTL_NWC24_REQUEST_SHUTDOWN:
		// if ya set the IOS version to a very high value this happens ...
		WARN_LOG(WII_IPC_NET, "NET_KD_REQ: IOCTL_NWC24_REQUEST_SHUTDOWN - NI");
		break;

	default:
		WARN_LOG(WII_IPC_NET, "/dev/net/kd/request::IOCtl request 0x%x (BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
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
	u32 return_value = 0;
	u32 common_result = 0;
	u32 common_vector = 0;

	SIOCtlVBuffer CommandBuffer(_CommandAddress);

	switch (CommandBuffer.Parameter)
	{
	case IOCTLV_NCD_LOCKWIRELESSDRIVER:
		break;
	case IOCTLV_NCD_UNLOCKWIRELESSDRIVER:
		//Memory::Read_U32(CommandBuffer.InBuffer.at(0).m_Address);
		break;

	case IOCTLV_NCD_GETCONFIG:
		INFO_LOG(WII_IPC_NET, "NET_NCD_MANAGE: IOCTLV_NCD_GETCONFIG");
		config.WriteToMem(CommandBuffer.PayloadBuffer.at(0).m_Address);
		common_vector = 1;
		break;

	case IOCTLV_NCD_SETCONFIG:
		INFO_LOG(WII_IPC_NET, "NET_NCD_MANAGE: IOCTLV_NCD_SETCONFIG");
		config.ReadFromMem(CommandBuffer.InBuffer.at(0).m_Address);
		break;

	case IOCTLV_NCD_READCONFIG:
		INFO_LOG(WII_IPC_NET, "NET_NCD_MANAGE: IOCTLV_NCD_READCONFIG");
		config.ReadConfig();
		config.WriteToMem(CommandBuffer.PayloadBuffer.at(0).m_Address);
		common_vector = 1;
		break;

	case IOCTLV_NCD_WRITECONFIG:
		INFO_LOG(WII_IPC_NET, "NET_NCD_MANAGE: IOCTLV_NCD_WRITECONFIG");
		config.ReadFromMem(CommandBuffer.InBuffer.at(0).m_Address);
		config.WriteConfig();
		break;

	case IOCTLV_NCD_GETLINKSTATUS:
		INFO_LOG(WII_IPC_NET, "NET_NCD_MANAGE: IOCTLV_NCD_GETLINKSTATUS");
		// Always connected
		Memory::Write_U32(netcfg_connection_t::LINK_WIRED,
			CommandBuffer.PayloadBuffer.at(0).m_Address + 4);
		break;

	case IOCTLV_NCD_GETWIRELESSMACADDRESS:
		INFO_LOG(WII_IPC_NET, "NET_NCD_MANAGE: IOCTLV_NCD_GETWIRELESSMACADDRESS");
		Memory::WriteBigEData(default_address,
			CommandBuffer.PayloadBuffer.at(1).m_Address, sizeof(default_address));
		break;

	default:
		WARN_LOG(WII_IPC_NET, "NET_NCD_MANAGE IOCtlV: %#x", CommandBuffer.Parameter);
		break;
	}

	Memory::Write_U32(common_result,
		CommandBuffer.PayloadBuffer.at(common_vector).m_Address);
	if (common_vector == 1)
	{
		Memory::Write_U32(common_result,
			CommandBuffer.PayloadBuffer.at(common_vector).m_Address + 4);
	}
	Memory::Write_U32(return_value, _CommandAddress + 4);
	return true;
}

// **********************************************************************************
// Handle /dev/net/wd/command requests
CWII_IPC_HLE_Device_net_wd_command::CWII_IPC_HLE_Device_net_wd_command(u32 DeviceID, const std::string& DeviceName) 
	: IWII_IPC_HLE_Device(DeviceID, DeviceName)
{
}

CWII_IPC_HLE_Device_net_wd_command::~CWII_IPC_HLE_Device_net_wd_command() 
{
}

bool CWII_IPC_HLE_Device_net_wd_command::Open(u32 CommandAddress, u32 Mode)
{
	INFO_LOG(WII_IPC_NET, "NET_WD_COMMAND: Open");
	Memory::Write_U32(GetDeviceID(), CommandAddress + 4);
	m_Active = true;
	return true;
}

bool CWII_IPC_HLE_Device_net_wd_command::Close(u32 CommandAddress, bool Force)
{
	INFO_LOG(WII_IPC_NET, "NET_WD_COMMAND: Close");
	if (!Force)
		Memory::Write_U32(0, CommandAddress + 4);
	m_Active = false;
	return true;
}

// This is just for debugging / playing around.
// There really is no reason to implement wd unless we can bend it such that
// we can talk to the DS.
#include "StringUtil.h"
bool CWII_IPC_HLE_Device_net_wd_command::IOCtlV(u32 CommandAddress)
{
	u32 return_value = 0;

	SIOCtlVBuffer CommandBuffer(CommandAddress);

	switch (CommandBuffer.Parameter)
	{
	case IOCTLV_WD_SCAN:
		{
		// Gives parameters detailing type of scan and what to match
		ScanInfo *scan = (ScanInfo *)Memory::GetPointer(CommandBuffer.InBuffer.at(0).m_Address);

		u16 *results = (u16 *)Memory::GetPointer(CommandBuffer.PayloadBuffer.at(0).m_Address);
		// first u16 indicates number of BSSInfo following
		results[0] = Common::swap16(1);

		BSSInfo *bss = (BSSInfo *)&results[1];
		memset(bss, 0, sizeof(BSSInfo));

		bss->length = Common::swap16(sizeof(BSSInfo));
		bss->rssi = Common::swap16(0xffff);

		for (int i = 0; i < BSSID_SIZE; ++i)
			bss->bssid[i] = i;

		const char *ssid = "dolphin-emu";
		strcpy((char *)bss->ssid, ssid);
		bss->ssid_length = Common::swap16(strlen(ssid));

		bss->channel = Common::swap16(2);
		}
		break;

	case IOCTLV_WD_GET_INFO:
		{
		Info *info = (Info *)Memory::GetPointer(CommandBuffer.PayloadBuffer.at(0).m_Address);
		memset(info, 0, sizeof(Info));
		// Probably used to disallow certain channels?
		memcpy(info->country, "US", 2);
		info->ntr_allowed_channels = Common::swap16(0xfffe);
		memcpy(info->mac, default_address, 6);
		}
		break;

	case IOCTLV_WD_GET_MODE:
	case IOCTLV_WD_SET_LINKSTATE:
	case IOCTLV_WD_GET_LINKSTATE:
	case IOCTLV_WD_SET_CONFIG:
	case IOCTLV_WD_GET_CONFIG:
	case IOCTLV_WD_CHANGE_BEACON:
	case IOCTLV_WD_DISASSOC:
	case IOCTLV_WD_MP_SEND_FRAME:
	case IOCTLV_WD_SEND_FRAME:
	case IOCTLV_WD_CALL_WL:
	case IOCTLV_WD_MEASURE_CHANNEL:
	case IOCTLV_WD_GET_LASTERROR:
	case IOCTLV_WD_CHANGE_GAMEINFO:
	case IOCTLV_WD_CHANGE_VTSF:
	case IOCTLV_WD_RECV_FRAME:
	case IOCTLV_WD_RECV_NOTIFICATION:
	default:
		WARN_LOG(WII_IPC_NET, "NET_WD_COMMAND IOCtlV %#x in %i out %i",
			CommandBuffer.Parameter, CommandBuffer.NumberInBuffer, CommandBuffer.NumberPayloadBuffer);
		for (u32 i = 0; i < CommandBuffer.NumberInBuffer; ++i)
		{
			WARN_LOG(WII_IPC_NET, "in %i addr %x size %i", i,
				CommandBuffer.InBuffer.at(i).m_Address, CommandBuffer.InBuffer.at(i).m_Size);
			WARN_LOG(WII_IPC_NET, "%s", 
				ArrayToString(
					Memory::GetPointer(CommandBuffer.InBuffer.at(i).m_Address),
					CommandBuffer.InBuffer.at(i).m_Size).c_str()
				);
		}
		for (u32 i = 0; i < CommandBuffer.NumberPayloadBuffer; ++i)
		{
			WARN_LOG(WII_IPC_NET, "out %i addr %x size %i", i,
				CommandBuffer.PayloadBuffer.at(i).m_Address, CommandBuffer.PayloadBuffer.at(i).m_Size);
		}
		break;
	}

	Memory::Write_U32(return_value, CommandAddress + 4);
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

char* DecodeError(int ErrorCode)
{
	static char Message[1024];

#ifdef _WIN32
	// If this program was multi-threaded, we'd want to use FORMAT_MESSAGE_ALLOCATE_BUFFER
	// instead of a static buffer here.
	// (And of course, free the buffer when we were done with it)
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS |
		FORMAT_MESSAGE_MAX_WIDTH_MASK, NULL, ErrorCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)Message, 1024, NULL);
	return Message;
#else
	return strerror(ErrorCode);
#endif
}

static int getNetErrorCode(int ret, std::string caller, bool isRW)
{
#ifdef _WIN32
	int errorCode = WSAGetLastError();
#else
	int errorCode = errno;
#endif

	if (ret >= 0)
		return ret;

	WARN_LOG(WII_IPC_NET, "%s failed with error %d: %s, ret= %d",
		caller.c_str(), errorCode, DecodeError(errorCode), ret);

#ifdef _WIN32
#define ERRORCODE(name) WSA ## name
#define EITHER(win32, posix) win32
#else
#define ERRORCODE(name) name
#define EITHER(win32, posix) posix
#endif

	switch (errorCode)
	{
	case ERRORCODE(EMSGSIZE):
		WARN_LOG(WII_IPC_NET, "Find out why this happened, looks like PEEK failure?");
		return -1;
	case ERRORCODE(ECONNRESET):
		return -15; // ECONNRESET
	case ERRORCODE(EISCONN):
		return -30; // EISCONN
	case ERRORCODE(ENOTCONN):
		return -6; // EAGAIN
	case EITHER(WSAEWOULDBLOCK, EAGAIN):
	case ERRORCODE(EINPROGRESS):
		if(isRW){
			return -6; // EAGAIN
		}else{
			return -26; // EINPROGRESS
		}
	case EITHER(WSA_INVALID_HANDLE, EBADF):
		return -26; // EINPROGRESS
	default:
		return -1;
	}

#undef ERRORCODE
#undef EITHER
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

// Maps SOCKOPT level from native to Wii
static unsigned int opt_level_mapping[][2] = {
	{ SOL_SOCKET, 0xFFFF }
};

// Maps SOCKOPT optname from native to Wii
static unsigned int opt_name_mapping[][2] = {
	{ SO_REUSEADDR, 0x4 },
	{ SO_SNDBUF, 0x1001 },
	{ SO_RCVBUF, 0x1002 },
	{ SO_ERROR, 0x1009 }
};

u32 CWII_IPC_HLE_Device_net_ip_top::ExecuteCommand(u32 _Command,
	u32 _BufferIn, u32 BufferInSize,
	u32 _BufferOut, u32 BufferOutSize)
{		
	switch (_Command)
	{
	case IOCTL_SO_STARTUP:
		WARN_LOG(WII_IPC_NET, "IOCTL_SO_STARTUP "
			"BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
			_BufferIn, BufferInSize, _BufferOut, BufferOutSize); 
		break;

	case IOCTL_SO_CONNECT:
		{
			//struct sockaddr_in echoServAddr;
			struct connect_params
			{
				u32 socket;
				u32 has_addr;
				u8 addr[28];
			} params;
			sockaddr_in serverAddr;

			Memory::ReadBigEData((u8*)&params, _BufferIn, sizeof(connect_params));

			if (Common::swap32(params.has_addr) != 1)
				return -1;

			memset(&serverAddr, 0, sizeof(serverAddr));
			memcpy(&serverAddr, params.addr, params.addr[0]);

			// GC/Wii sockets have a length param as well, we dont really care :)
			serverAddr.sin_family = serverAddr.sin_family >> 8;

			int ret = connect(Common::swap32(params.socket), (struct sockaddr *) &serverAddr, sizeof(serverAddr));
			ret = getNetErrorCode(ret, "SO_CONNECT", false);
			WARN_LOG(WII_IPC_NET,"IOCTL_SO_CONNECT (%08x, %s:%d)",
				Common::swap32(params.socket), inet_ntoa(serverAddr.sin_addr), Common::swap16(serverAddr.sin_port));
			return ret;
			break;
		}

	case IOCTL_SO_SHUTDOWN:
		{
			WARN_LOG(WII_IPC_NET, "IOCTL_SO_SHUTDOWN "
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
			WARN_LOG(WII_IPC_NET, "IOCTL_SO_CLOSE (%08x)", sock);

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
			WARN_LOG(WII_IPC_NET, "IOCTL_SO_SOCKET "
				"Socket: %08x (%d,%d,%d), BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
				s, AF, TYPE, PROT, _BufferIn, BufferInSize, _BufferOut, BufferOutSize);
			
			int ret = getNetErrorCode(s, "SO_SOCKET", false);
			if(ret>0){
				u32 millis = 3000;

				setsockopt(s, SOL_SOCKET, SO_RCVTIMEO,(char *)&millis,4);
			}
			return ret;
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

			WARN_LOG(WII_IPC_NET, "IOCTL_SO_BIND (%s:%d) = %d "
				"Socket: %08X, BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
				inet_ntoa(address.sin_addr), Common::swap16(address.sin_port), ret,
				Common::swap32(addr->socket), _BufferIn, BufferInSize, _BufferOut, BufferOutSize);

			return getNetErrorCode(ret, "SO_BIND", false);
			break;
		}

	case IOCTL_SO_LISTEN:
		{
			WARN_LOG(WII_IPC_NET, "IOCTL_SO_LISTEN "
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
			WARN_LOG(WII_IPC_NET, "IOCTL_SO_ACCEPT "
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

			WARN_LOG(WII_IPC_NET,"IOCTL_SO_GETSOCKOPT(%08x, %08x, %08x)"
				"BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
				sock, level, optname,
				_BufferIn, BufferInSize, _BufferOut, BufferOutSize);

			// Do the level/optname translation
			int nat_level = -1, nat_optname = -1;

			for (unsigned int i = 0; i < sizeof (opt_level_mapping) / sizeof (opt_level_mapping[0]); ++i)
				if (level == opt_level_mapping[i][1])
					nat_level = opt_level_mapping[i][0];

			for (unsigned int i = 0; i < sizeof (opt_name_mapping) / sizeof (opt_name_mapping[0]); ++i)
				if (optname == opt_name_mapping[i][1])
					nat_optname = opt_name_mapping[i][0];

			u8 optval[20];
			u32 optlen = 4;

			int ret = getsockopt (sock, nat_level, nat_optname, (char *) &optval, (socklen_t*)&optlen);

			ret = getNetErrorCode(ret, "SO_GETSOCKOPT", false);


			Memory::Write_U32(optlen, _BufferOut + 0xC);
			Memory::WriteBigEData((u8 *) optval, _BufferOut + 0x10, optlen);

			if(optname == 0x1007){
				s32 errorcode = Memory::Read_U32(_BufferOut + 0x10);
				WARN_LOG(WII_IPC_NET,"IOCTL_SO_GETSOCKOPT error code = %i", errorcode);
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
			WARN_LOG(WII_IPC_NET, "IOCTL_SO_SETSOCKOPT(%08x, %08x, %08x, %08x) "
				"BufferIn: (%08x, %i), BufferOut: (%08x, %i)"
				"%02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx",
				S, level, optname, optlen, _BufferIn, BufferInSize, _BufferOut, BufferOutSize, optval[0], optval[1], optval[2], optval[3], optval[4], optval[5], optval[6], optval[7], optval[8], optval[9], optval[10], optval[11], optval[12], optval[13], optval[14], optval[15], optval[16], optval[17], optval[18], optval[19]);

			// Do the level/optname translation
			int nat_level = -1, nat_optname = -1;

			for (unsigned int i = 0; i < sizeof (opt_level_mapping) / sizeof (opt_level_mapping[0]); ++i)
				if (level == opt_level_mapping[i][1])
					nat_level = opt_level_mapping[i][0];

			for (unsigned int i = 0; i < sizeof (opt_name_mapping) / sizeof (opt_name_mapping[0]); ++i)
				if (optname == opt_name_mapping[i][1])
					nat_optname = opt_name_mapping[i][0];

			if (nat_level == -1 || nat_optname == -1)
			{
				WARN_LOG(WII_IPC_NET, "SO_SETSOCKOPT: unknown level %d or optname %d", level, optname);

				// Default to the given level/optname. They match on Windows...
				nat_level = level;
				nat_optname = optname;
			}

			int ret = setsockopt(S, nat_level, nat_optname, (char*)optval, optlen);

			ret = getNetErrorCode(ret, "SO_SETSOCKOPT", false);

			return ret;
		}

	case IOCTL_SO_FCNTL:
		{
			u32 sock	= Memory::Read_U32(_BufferIn);
			u32 cmd		= Memory::Read_U32(_BufferIn + 4);
			u32 arg		= Memory::Read_U32(_BufferIn + 8);

			WARN_LOG(WII_IPC_NET, "IOCTL_SO_FCNTL(%08X, %08X) "
				"Socket: %08x, BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
				cmd, arg,
				sock, _BufferIn, BufferInSize, _BufferOut, BufferOutSize);
#ifdef _WIN32
#define F_GETFL			3
#define F_SETFL			4
#define F_NONBLOCK		4
			if (cmd == F_GETFL)
			{
				WARN_LOG(WII_IPC_NET, "F_GETFL WTF?");
			}
			else if (cmd == F_SETFL)
			{
				u_long iMode = 0;
				//if (arg & F_NONBLOCK)
					iMode = 1;
				int ioctlret = ioctlsocket(sock, FIONBIO, &iMode);
				return getNetErrorCode(ioctlret, "SO_FCNTL", false);
			}
			else
			{
				WARN_LOG(WII_IPC_NET, "SO_FCNTL unknown command");
			}
			return 0;
#else
			// Map POSIX <-> Wii socket flags
			// First one is POSIX, second one is Wii
			static int mapping[][2] = {
				{ O_NONBLOCK, 0x4 },
			};

			if (cmd == F_GETFL)
			{
				int flags = fcntl(sock, F_GETFL, 0);
				int ret = 0;

				for (unsigned int i = 0; i < sizeof (mapping) / sizeof (mapping[0]); ++i)
					if (flags & mapping[i][0])
						ret |= mapping[i][1];
				return ret;
			}
			else if (cmd == F_SETFL)
			{
				int posix_flags = 0;

				for (unsigned int i = 0; i < sizeof (mapping) / sizeof (mapping[0]); ++i)
				{
					if (arg & mapping[i][1])
					{
						posix_flags |= mapping[i][0];
						arg &= ~mapping[i][1];
					}
				}

				if (arg)
					WARN_LOG(WII_IPC_NET, "SO_FCNTL F_SETFL unhandled flags: %08x", arg);

				int ret = fcntl(sock, F_SETFL, posix_flags);
				return getNetErrorCode(ret, "SO_FCNTL", false);
			}
			else
			{
				WARN_LOG(WII_IPC_NET, "SO_FCNTL unknown command");
			}
			return 0;
#endif
		}

	case IOCTL_SO_GETSOCKNAME:
		{
			u32 sock = Memory::Read_U32(_BufferIn);

			WARN_LOG(WII_IPC_NET, "IOCTL_SO_GETSOCKNAME "
				"Socket: %08X, BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
				sock, _BufferIn, BufferInSize, _BufferOut, BufferOutSize);

			sockaddr sa;
			socklen_t sa_len;
			sa_len = sizeof(sa);
			int ret = getsockname(sock, &sa, &sa_len);

			Memory::Write_U8(BufferOutSize, _BufferOut);
			Memory::Write_U8(sa.sa_family & 0xFF, _BufferOut + 1);
			Memory::WriteBigEData((u8*)&sa.sa_data, _BufferOut + 2, BufferOutSize - 2);
			return ret;
		}
	case IOCTL_SO_GETPEERNAME:
		{
			u32 sock = Memory::Read_U32(_BufferIn);

			sockaddr sa;
			socklen_t sa_len;
			sa_len = sizeof(sa);

			int ret = getpeername(sock, &sa, &sa_len);

			Memory::Write_U8(BufferOutSize, _BufferOut);
			Memory::Write_U8(AF_INET, _BufferOut + 1);
			Memory::WriteBigEData((u8*)&sa.sa_data, _BufferOut + 2, BufferOutSize - 2);
			
			WARN_LOG(WII_IPC_NET, "IOCTL_SO_GETPEERNAME(%x)", sock);

			return ret;
		}

	case IOCTL_SO_GETHOSTID:
		{
			WARN_LOG(WII_IPC_NET, "IOCTL_SO_GETHOSTID "
				"(BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
				_BufferIn, BufferInSize, _BufferOut, BufferOutSize);
			return 192 << 24 | 168 << 16 | 1 << 8 | 150;
		}

	case IOCTL_SO_INETATON:
		{
			struct hostent *remoteHost = gethostbyname((char*)Memory::GetPointer(_BufferIn));

			Memory::Write_U32(Common::swap32(*(u32 *)remoteHost->h_addr_list[0]), _BufferOut);
			WARN_LOG(WII_IPC_NET, "IOCTL_SO_INETATON = %d "
				"%s, BufferIn: (%08x, %i), BufferOut: (%08x, %i), IP Found: %08X",remoteHost->h_addr_list[0] == 0 ? -1 : 0,
				(char*)Memory::GetPointer(_BufferIn), _BufferIn, BufferInSize, _BufferOut, BufferOutSize, Common::swap32(*(u32 *)remoteHost->h_addr_list[0]));
			return remoteHost->h_addr_list[0] == 0 ? 0 : 1;
		}

	case IOCTL_SO_INETPTON:
		{
			WARN_LOG(WII_IPC_NET, "IOCTL_SO_INETPTON "
				"(Translating: %s)", Memory::GetPointer(_BufferIn));
			return inet_pton((char*)Memory::GetPointer(_BufferIn), Memory::GetPointer(_BufferOut+4));
			break;
		}

	case IOCTL_SO_INETNTOP:
		{
			u32 af = Memory::Read_U32(_BufferIn);
			//u32 af = Memory::Read_U32(_BufferIn + 4);
			u32 src = Memory::Read_U32(_BufferIn + 8);
			//u32 af = Memory::Read_U32(_BufferIn + 12);
			//u32 af = Memory::Read_U32(_BufferIn + 16);
			//u32 af = Memory::Read_U32(_BufferIn + 20);
			char ip_s[16];
			sprintf(ip_s, "%i.%i.%i.%i",
				Memory::Read_U8(_BufferIn + 8),
				Memory::Read_U8(_BufferIn + 8 + 1),
				Memory::Read_U8(_BufferIn + 8 + 2),
				Memory::Read_U8(_BufferIn + 8 + 3)
				);
			WARN_LOG(WII_IPC_NET, "IOCTL_SO_INETNTOP %s", ip_s);
			memset(Memory::GetPointer(_BufferOut), 0, BufferOutSize);
			memcpy(Memory::GetPointer(_BufferOut), ip_s, strlen(ip_s));
			return 0;
		}

	case IOCTL_SO_POLL:
		{
			// Map Wii/native poll events types
			unsigned int mapping[][2] = {
				{ POLLIN, 0x0001 },
				{ POLLOUT, 0x0008 },
				{ POLLHUP, 0x0040 },
			};

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
				int events = Memory::Read_U32(_BufferOut + 0xc*i + 4);
				ufds[i].revents = Memory::Read_U32(_BufferOut + 0xc*i + 8);

				// Translate Wii to native events
				int unhandled_events = events;
				ufds[i].events = 0;
				for (unsigned int j = 0; j < sizeof (mapping) / sizeof (mapping[0]); ++j)
				{
					if (events & mapping[j][1])
						ufds[i].events |= mapping[j][0];
					unhandled_events &= ~mapping[j][1];
				}

				WARN_LOG(WII_IPC_NET, "IOCTL_SO_POLL(%d) "
					"Sock: %08x, Unknown: %08x, Events: %08x, "
					"NativeEvents: %08x",
					i, ufds[i].fd, unknown, events, ufds[i].events
				);

				if (unhandled_events)
					ERROR_LOG(WII_IPC_NET, "SO_POLL: unhandled Wii event types: %04x", unhandled_events);
			}

			int ret = poll(ufds, nfds, timeout);

			ret = getNetErrorCode(ret, "SO_POLL", false);

			for (int i = 0; i<nfds; i++)
			{
				Memory::Write_U32(ufds[i].fd, _BufferOut + 0xc*i);
				Memory::Write_U32(ufds[i].events, _BufferOut + 0xc*i + 4);

				// Translate native to Wii events
				int revents = 0;
				for (unsigned int j = 0; j < sizeof (mapping) / sizeof (mapping[0]); ++j)
				{
					if (ufds[i].revents & mapping[j][0])
						revents |= mapping[j][1];
				}
				WARN_LOG(WII_IPC_NET, "IOCTL_SO_POLL socket %d revents %08X", i, revents);

				Memory::Write_U32(revents, _BufferOut + 0xc*i + 8);
			}
			free(ufds);
			return ret;
		}

	case IOCTL_SO_GETHOSTBYNAME:
		{
			hostent *remoteHost = gethostbyname((char*)Memory::GetPointer(_BufferIn));

			WARN_LOG(WII_IPC_NET, "IOCTL_SO_GETHOSTBYNAME "
				"Address: %s, BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
				(char*)Memory::GetPointer(_BufferIn), _BufferIn, BufferInSize, _BufferOut, BufferOutSize);

			if (remoteHost)
			{
				for (int i = 0; remoteHost->h_aliases[i]; ++i)
				{
					WARN_LOG(WII_IPC_NET, "alias%i:%s", i, remoteHost->h_aliases[i]);
				}

				for (int i = 0; remoteHost->h_addr_list[i]; ++i)
				{
					u32 ip = Common::swap32(*(u32*)(remoteHost->h_addr_list[i]));
					char ip_s[16];
					sprintf(ip_s, "%i.%i.%i.%i",
						ip >> 24, (ip >> 16) & 0xff, (ip >> 8) & 0xff, ip & 0xff);
					DEBUG_LOG(WII_IPC_NET, "addr%i:%s", i, ip_s);
				}

				Memory::Memset(_BufferOut, 0, BufferOutSize);
				u32 wii_addr = _BufferOut + 4 * 3 + 2 * 2;

				u32 name_length = strlen(remoteHost->h_name) + 1;
				Memory::WriteBigEData((const u8 *)remoteHost->h_name, wii_addr, name_length);
				Memory::Write_U32(wii_addr, _BufferOut);
				wii_addr += (name_length + 4) & ~3;

				// aliases - empty
				Memory::Write_U32(wii_addr, _BufferOut + 4);
				Memory::Write_U32(wii_addr + sizeof(u32), wii_addr);
				wii_addr += sizeof(u32);
				Memory::Write_U32((u32)NULL, wii_addr);
				wii_addr += sizeof(u32);

				// hardcode to ipv4
				_dbg_assert_msg_(WII_IPC_NET,
					remoteHost->h_addrtype == AF_INET && remoteHost->h_length == sizeof(u32),
					"returned host info is not IPv4");
				Memory::Write_U16(AF_INET, _BufferOut + 8);
				Memory::Write_U16(sizeof(u32), _BufferOut + 10);

				// addrlist - probably only really need to return 1 anyways...
				Memory::Write_U32(wii_addr, _BufferOut + 12);
				u32 num_addr = 0;
				while (remoteHost->h_addr_list[num_addr])
					num_addr++;
				for (u32 i = 0; i < num_addr; ++i)
				{
					Memory::Write_U32(wii_addr + sizeof(u32) * (num_addr + 1), wii_addr);
					wii_addr += sizeof(u32);
				}
				// NULL terminated list
				Memory::Write_U32((u32)NULL, wii_addr);
				wii_addr += sizeof(u32);
				// The actual IPs
				for (int i = 0; remoteHost->h_addr_list[i]; i++)
				{
					Memory::Write_U32_Swap(*(u32*)(remoteHost->h_addr_list[i]), wii_addr);
					wii_addr += sizeof(u32);
				}

				//ERROR_LOG(WII_IPC_NET, "\n%s",
				//	ArrayToString(Memory::GetPointer(_BufferOut), BufferOutSize, 16).c_str());
				return 0;
			}
			else
			{
				return -1;
			}

			break;
		}

	case IOCTL_SO_ICMPSOCKET:
		{
			// AF type?
			u32 arg = Memory::Read_U32(_BufferIn);
			u32 sock = (u32)socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
			DEBUG_LOG(WII_IPC_NET, "IOCTL_SO_ICMPSOCKET(%x) %x", arg, sock);
			return getNetErrorCode(sock, "IOCTL_SO_ICMPSOCKET", false);
		}

	case IOCTL_SO_ICMPCANCEL:
		ERROR_LOG(WII_IPC_NET, "IOCTL_SO_ICMPCANCEL");
		goto default_;

	case IOCTL_SO_ICMPCLOSE:
		{
			u32 sock = Memory::Read_U32(_BufferIn);
#ifdef _WIN32
			u32 ret = closesocket(sock);
#else
			u32 ret = close(sock);
#endif
			DEBUG_LOG(WII_IPC_NET, "IOCTL_SO_ICMPCLOSE(%x) %x", sock, ret);
			return getNetErrorCode(ret, "IOCTL_SO_ICMPCLOSE", false);
		}

	default:
		WARN_LOG(WII_IPC_NET,"0x%x "
			"BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
			_Command, _BufferIn, BufferInSize, _BufferOut, BufferOutSize);
	default_:
		if (BufferInSize)
		{
			ERROR_LOG(WII_IPC_NET, "in addr %x size %x", _BufferIn, BufferInSize);
			ERROR_LOG(WII_IPC_NET, "\n%s", 
				ArrayToString(Memory::GetPointer(_BufferIn), BufferInSize, 4).c_str()
				);
		}

		if (BufferOutSize)
		{
			ERROR_LOG(WII_IPC_NET, "out addr %x size %x", _BufferOut, BufferOutSize);
		}
		break;
	}

	// We return a success for any potential unknown requests
	return 0;
}

u32 CWII_IPC_HLE_Device_net_ip_top::ExecuteCommandV(SIOCtlVBuffer& CommandBuffer)
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

	switch (CommandBuffer.Parameter)
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

			WARN_LOG(WII_IPC_NET,"IOCTLV_SO_GETINTERFACEOPT(%08X, %08X) "
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
									WARN_LOG(WII_IPC_NET, "Name of valid interface: %S", AdapterList->FriendlyName);
									WARN_LOG(WII_IPC_NET, "DNS: %u.%u.%u.%u",
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
			struct sendto_params
			{
				u32 socket;
				u32 flags;
				u32 has_destaddr;
				u8 destaddr[28];
			} params;

			char * data = (char*)Memory::GetPointer(_BufferIn);
			Memory::ReadBigEData((u8*)&params, _BufferIn2, BufferInSize2);

			if (params.has_destaddr)
			{
				struct sockaddr_in* addr = (struct sockaddr_in*)&params.destaddr;
				u8 len = sizeof(sockaddr); //addr->sin_family & 0xFF;
				addr->sin_family = addr->sin_family >> 8;

				int ret = sendto(Common::swap32(params.socket), data,
					BufferInSize, Common::swap32(params.flags), (struct sockaddr*)addr, len);

				WARN_LOG(WII_IPC_NET,
					"IOCTLV_SO_SENDTO = %d Socket: %08x, BufferIn: (%08x, %i), BufferIn2: (%08x, %i), %u.%u.%u.%u",
					ret, Common::swap32(params.socket), _BufferIn, BufferInSize,
					_BufferIn2, BufferInSize2,
					addr->sin_addr.s_addr & 0xFF,
					(addr->sin_addr.s_addr >> 8) & 0xFF,
					(addr->sin_addr.s_addr >> 16) & 0xFF,
					(addr->sin_addr.s_addr >> 24) & 0xFF
					);

				return getNetErrorCode(ret, "SO_SENDTO", true);
			}
			else
			{
				int ret = send(Common::swap32(params.socket), data,
					BufferInSize, Common::swap32(params.flags));
				WARN_LOG(WII_IPC_NET, "IOCTLV_SO_SEND = %d Socket: %08x, BufferIn: (%08x, %i), BufferIn2: (%08x, %i)",
					ret, Common::swap32(params.socket), _BufferIn, BufferInSize,
					_BufferIn2, BufferInSize2);

				return getNetErrorCode(ret, "SO_SEND", true);
			}
			break;
		}

	case IOCTLV_SO_RECVFROM:
		{
			u32 sock	= Memory::Read_U32(_BufferIn);
			u32 flags	= Memory::Read_U32(_BufferIn + 4);

			char *buf	= (char *)Memory::GetPointer(_BufferOut);
			int len		= BufferOutSize;
			struct sockaddr_in addr;
			memset(&addr, 0, sizeof(sockaddr_in));
			socklen_t fromlen = 0;

			if (BufferOutSize2 != 0)
			{
				fromlen = BufferOutSize2 >= sizeof(struct sockaddr) ? BufferOutSize2 : sizeof(struct sockaddr);
			}

			
			

			if (flags != 2)
				flags = 0;
			else
				flags = MSG_PEEK;

			static int ret;
#ifdef _WIN32
			if(flags & MSG_PEEK){
				unsigned long totallen = 0;
				ioctlsocket(sock, FIONREAD, &totallen);
				return totallen;
			}
#endif

			ret = recvfrom(sock, buf, len, flags,
			               fromlen ? (struct sockaddr*) &addr : NULL,
			               fromlen ? &fromlen : 0);

			int err = getNetErrorCode(ret, fromlen ? "SO_RECVFROM" : "SO_RECV", true);

			
			WARN_LOG(WII_IPC_NET, "%s(%d, %p) Socket: %08X, Flags: %08X, "
			"BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
			"BufferOut: (%08x, %i), BufferOut2: (%08x, %i)",fromlen ? "IOCTLV_SO_RECVFROM " : "IOCTLV_SO_RECV ",
			ret, buf, sock, flags,
			_BufferIn, BufferInSize, _BufferIn2, BufferInSize2,
			_BufferOut, BufferOutSize, _BufferOut2, BufferOutSize2);

			if (BufferOutSize2 != 0)
			{
				addr.sin_family = (addr.sin_family << 8) | (BufferOutSize2&0xFF);
				Memory::WriteBigEData((u8*)&addr, _BufferOut2, BufferOutSize2);
			}

			return err;
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
				hints.ai_canonname	= (char*)Memory::GetPointer(Memory::Read_U32(_BufferIn3 + 0x14));
				hints.ai_addr		= (sockaddr *)Memory::GetPointer(Memory::Read_U32(_BufferIn3 + 0x18));
				hints.ai_next		= (addrinfo *)Memory::GetPointer(Memory::Read_U32(_BufferIn3 + 0x1C));
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

			WARN_LOG(WII_IPC_NET, "IOCTLV_SO_GETADDRINFO "
				"(BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
				_BufferIn, BufferInSize, _BufferOut, BufferOutSize);
			WARN_LOG(WII_IPC_NET, "IOCTLV_SO_GETADDRINFO: %s", Memory::GetPointer(_BufferIn));
			return ret;
		}

	case IOCTLV_SO_ICMPPING:
		{
			struct
			{
				u8 length;
				u8 addr_family;
				u16 icmp_id;
				u32 ip;
			} ip_info;

			u32 sock	= Memory::Read_U32(_BufferIn);
			u32 num_ip	= Memory::Read_U32(_BufferIn + 4);
			u64 timeout	= Memory::Read_U64(_BufferIn + 8);

			if (num_ip != 1)
			{
				WARN_LOG(WII_IPC_NET, "IOCTLV_SO_ICMPPING %i IPs", num_ip);
			}

			ip_info.length		= Memory::Read_U8(_BufferIn + 16);
			ip_info.addr_family	= Memory::Read_U8(_BufferIn + 17);
			ip_info.icmp_id		= Memory::Read_U16(_BufferIn + 18);
			ip_info.ip			= Memory::Read_U32(_BufferIn + 20);

			if (ip_info.length != 8 || ip_info.addr_family != AF_INET)
			{
				WARN_LOG(WII_IPC_NET, "IOCTLV_SO_ICMPPING strange IPInfo:\n"
					"length %x addr_family %x",
					ip_info.length, ip_info.addr_family);
			}

			DEBUG_LOG(WII_IPC_NET, "IOCTLV_SO_ICMPPING %x", ip_info.ip);
			
			sockaddr_in addr;
			addr.sin_family = AF_INET;
			addr.sin_addr.s_addr = Common::swap32(ip_info.ip);
			memset(addr.sin_zero, 0, 8);

			u8 data[0x20];
			memset(data, 0, sizeof(data));
			s32 icmp_length = sizeof(data);

			if (BufferInSize2 == sizeof(data))
				memcpy(data, Memory::GetPointer(_BufferIn2), BufferInSize2);
			else
			{
				// TODO sequence number is incremented either statically, by
				// port, or by socket. Doesn't seem to matter, so we just leave
				// it 0
				((u16 *)data)[0] = Common::swap16(ip_info.icmp_id);
				icmp_length = 22;
			}
			
			int ret = icmp_echo_req(sock, &addr, data, icmp_length);
			if (ret == icmp_length)
			{
				ret = icmp_echo_rep(sock, &addr, (u32)timeout, icmp_length);
			}

			// TODO proper error codes
			return 0;
		}

	default:
		WARN_LOG(WII_IPC_NET,"0x%x (BufferIn: (%08x, %i), BufferIn2: (%08x, %i)",
			CommandBuffer.Parameter, _BufferIn, BufferInSize, _BufferIn2, BufferInSize2);
	default_:
		for (u32 i = 0; i < CommandBuffer.NumberInBuffer; ++i)
		{
			ERROR_LOG(WII_IPC_NET, "in %i addr %x size %x", i,
				CommandBuffer.InBuffer.at(i).m_Address, CommandBuffer.InBuffer.at(i).m_Size);
			ERROR_LOG(WII_IPC_NET, "\n%s", 
				ArrayToString(
				Memory::GetPointer(CommandBuffer.InBuffer.at(i).m_Address),
				CommandBuffer.InBuffer.at(i).m_Size, 4).c_str()
				);
		}
		for (u32 i = 0; i < CommandBuffer.NumberPayloadBuffer; ++i)
		{
			ERROR_LOG(WII_IPC_NET, "out %i addr %x size %x", i,
				CommandBuffer.PayloadBuffer.at(i).m_Address, CommandBuffer.PayloadBuffer.at(i).m_Size);
		}
		break;
	}

	return 0;
}

bool CWII_IPC_HLE_Device_net_ip_top::IOCtlV(u32 CommandAddress) 
{ 
	SIOCtlVBuffer buf(CommandAddress);

	u32 return_value = ExecuteCommandV(buf);
	Memory::Write_U32(return_value, CommandAddress + 4);
	return true; 
}

#ifdef _MSC_VER
#pragma optimize("",off)
#endif
