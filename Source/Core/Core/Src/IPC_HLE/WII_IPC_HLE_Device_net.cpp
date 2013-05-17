// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.


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
#pragma warning(disable : 4065)  // switch statement contains 'default' but no 'case' labels
#endif

#include "WII_IPC_HLE_Device_net.h"
#include "../ConfigManager.h"
#include "FileUtil.h"
#include <stdio.h>
#include <string>
#ifdef _WIN32
#include <ws2tcpip.h>
#include <iphlpapi.h>
#elif defined(__linux__)
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

extern std::queue<std::pair<u32,std::string> > g_ReplyQueueLater;

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

	case IOCTL_NWC24_UNK_3: // NWC24iResumeForCloseLib
		INFO_LOG(WII_IPC_NET, "NET_KD_REQ: IOCTL_NWC24_UNK_3 - NI");
		break;

	case IOCTL_NWC24_STARTUP_SOCKET: // NWC24iStartupSocket
		INFO_LOG(WII_IPC_NET, "NET_KD_REQ: IOCTL_NWC24_STARTUP_SOCKET - NI");
		break;

	case IOCTL_NWC24_LOCK_SOCKET: // WiiMenu
		INFO_LOG(WII_IPC_NET, "NET_KD_REQ: IOCTL_NWC24_LOCK_SOCKET - NI");
		break;

	case IOCTL_NWC24_UNLOCK_SOCKET:
		INFO_LOG(WII_IPC_NET, "NET_KD_REQ: IOCTL_NWC24_UNLOCK_SOCKET - NI");
		break;

	case IOCTL_NWC24_REQUEST_GENERATED_USER_ID: // (Input: none, Output: 32 bytes)
		INFO_LOG(WII_IPC_NET, "NET_KD_REQ: IOCTL_NWC24_REQUEST_GENERATED_USER_ID");
		memcpy(Memory::GetPointer(BufferOut), m_UserID.c_str(), m_UserID.length() + 1);
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
		m_Ifconfig.header4 = 1; // got one "valid" connection
		m_Ifconfig.header6 = 7; // this is always 7?
		m_Ifconfig.connection[0].flags = 167;
		m_Ifconfig.connection[0].ip[0] = 192;
		m_Ifconfig.connection[0].ip[1] = 168;
		m_Ifconfig.connection[0].ip[2] =   1;
		m_Ifconfig.connection[0].ip[3] =   1;
		m_Ifconfig.connection[0].netmask[0] = 255;
		m_Ifconfig.connection[0].netmask[1] = 255;
		m_Ifconfig.connection[0].netmask[2] = 255;
		m_Ifconfig.connection[0].netmask[3] = 255;
		m_Ifconfig.connection[0].gateway[0] = 192;
		m_Ifconfig.connection[0].gateway[1] = 168;
		m_Ifconfig.connection[0].gateway[2] =   1;
		m_Ifconfig.connection[0].gateway[3] =   2;
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
	case IOCTLV_NCD_READCONFIG: // 7004 out, 32 out
	// first out buffer gets filled with contents of /shared2/sys/net/02/config.dat
	// TODO: What's the second output buffer for?
	{
		INFO_LOG(WII_IPC_NET, "NET_NCD_MANAGE: IOCTLV_NCD_GETIFCONFIG");

		// fill output buffer, taking care of endianness
		u32 addr = CommandBuffer.PayloadBuffer.at(0).m_Address;
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
		ReturnValue = 0;
		break;
	}

	case IOCTLV_NCD_UNK4: // 7004 In, 32 Out. 4th
		INFO_LOG(WII_IPC_NET, "NET_NCD_MANAGE: IOCTLV_NCD_UNK4");
		break;

	case 0x05: // 7004 Out, 32 Out. 2nd, 3rd
		INFO_LOG(WII_IPC_NET, "NET_NCD_MANAGE: IOCtlV 0x5");
		break;

	case IOCTLV_NCD_GETLINKSTATUS: // 32 Out. 5th
		INFO_LOG(WII_IPC_NET, "NET_NCD_MANAGE: IOCTLV_NCD_GETLINKSTATUS");
		break;

	case IOCTLV_NCD_GETWIRELESSMACADDRESS: // 32 Out, 6 Out. 1st
	// TODO: What's the first output buffer for?
	// second out buffer gets filled with first four bytes of the wireless MAC address.
	//     No idea why the fifth and sixth bytes are left untouched.
	{
		// hardcoded address as a fallback
		const u8 default_address[] = { 0x00, 0x19, 0x1e, 0xfd, 0x71, 0x84 };

		INFO_LOG(WII_IPC_NET, "NET_NCD_MANAGE: IOCTLV_NCD_GETWIRELESSMACADDRESS");

		if (!SConfig::GetInstance().m_WirelessMac.empty())
		{
			int x = 0;
			int tmpaddress[6];
			for (unsigned int i = 0; i < SConfig::GetInstance().m_WirelessMac.length() && x < 6; i++)
			{
				if (SConfig::GetInstance().m_WirelessMac[i] == ':' || SConfig::GetInstance().m_WirelessMac[i] == '-')
					continue;

				std::stringstream ss;
				ss << std::hex << SConfig::GetInstance().m_WirelessMac[i];
				if (SConfig::GetInstance().m_WirelessMac[i+1] != ':' && SConfig::GetInstance().m_WirelessMac[i+1] != '-')
				{
					ss << std::hex << SConfig::GetInstance().m_WirelessMac[i+1];
					i++;
				}
				ss >> tmpaddress[x];
				x++;
			}
			u8 address[6];
			for (int i = 0; i < 6;i++)
				address[i] = tmpaddress[i];
			Memory::WriteBigEData(address, CommandBuffer.PayloadBuffer.at(1).m_Address, 4);
			break;
		}

#if defined(__linux__)
		const char *check_devices[3] = { "wlan0", "ath0", "eth0" };
		int fd, ret;
		struct ifreq ifr;

		fd = socket(AF_INET, SOCK_DGRAM, 0);
		ifr.ifr_addr.sa_family = AF_INET;

		for (unsigned int dev = 0; dev < 3; dev++ )
		{
			strncpy(ifr.ifr_name, check_devices[dev], IFNAMSIZ-1);
			ret = ioctl(fd, SIOCGIFHWADDR, &ifr);
			if (ret == 0)
			{
				INFO_LOG(WII_IPC_NET, "NET_NCD_MANAGE: IOCTLV_NCD_GETWIRELESSMACADDRESS returning local MAC address of %s", check_devices[dev]);
				Memory::WriteBigEData((const u8*)ifr.ifr_hwaddr.sa_data, CommandBuffer.PayloadBuffer.at(1).m_Address, 4);
				break;
			}
		}
		if (ret != 0)
		{
			// fall back to the hardcoded address
			Memory::WriteBigEData(default_address, CommandBuffer.PayloadBuffer.at(1).m_Address, 4);
		}
		close(fd);

#elif defined(_WIN32)
		IP_ADAPTER_INFO *adapter_info = NULL;
		DWORD len = 0;

		DWORD ret = GetAdaptersInfo(adapter_info, &len);
		if (ret != ERROR_BUFFER_OVERFLOW || !len)
		{
			Memory::WriteBigEData(default_address, CommandBuffer.PayloadBuffer.at(1).m_Address, 4);
			break;
		}

		// LPFaint99: len is sizeof(IP_ADAPTER_INFO) * nics - 0x20
		adapter_info = new IP_ADAPTER_INFO[(len / sizeof(IP_ADAPTER_INFO)) + 1];
		ret = GetAdaptersInfo(adapter_info, &len);

		if (SUCCEEDED(ret)) Memory::WriteBigEData(adapter_info->Address, CommandBuffer.PayloadBuffer.at(1).m_Address, 4);
		else Memory::WriteBigEData(default_address, CommandBuffer.PayloadBuffer.at(1).m_Address, 4);
		delete[] adapter_info;
#else
		Memory::WriteBigEData(default_address, CommandBuffer.PayloadBuffer.at(1).m_Address, 4);
#endif
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
}

CWII_IPC_HLE_Device_net_ip_top::~CWII_IPC_HLE_Device_net_ip_top() 
{
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

//	INFO_LOG(WII_IPC_NET,"%s - Command(0x%08x) BufferIn(0x%08x, 0x%x) BufferOut(0x%08x, 0x%x)\n", GetDeviceName().c_str(), Command, BufferIn, BufferInSize, BufferOut, BufferOutSize);

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
	u32 s_addr_;    // this cannot be named s_addr under windows - collides with some crazy define.
};

struct GC_sockaddr_in
{
	u8 sin_len;
	u8 sin_family;
	u16 sin_port;
	struct GC_in_addr sin_addr;
	s8 sin_zero[8];
};

u32 CWII_IPC_HLE_Device_net_ip_top::ExecuteCommand(u32 _Command, u32 _BufferIn, u32 BufferInSize, u32 BufferOut, u32 BufferOutSize)
{
	// Clean the location of the output buffer to zeros as a safety precaution */
	Memory::Memset(BufferOut, 0, BufferOutSize);

	switch (_Command)
	{
	case IOCTL_SO_STARTUP:
		break;

	case IOCTL_SO_SOCKET:
	{
		u32 AF		= Memory::Read_U32(_BufferIn);
		u32 TYPE	= Memory::Read_U32(_BufferIn + 0x04);
		u32 PROT	= Memory::Read_U32(_BufferIn + 0x04 * 2);
//		u32 Unk1	= Memory::Read_U32(_BufferIn + 0x04 * 3);
		u32 Socket	= (u32)socket(AF, TYPE, PROT);
		return Common::swap32(Socket); // So it doesn't get mangled later on
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
		address.sin_port = htons(addrPC.sin_port);
		int Return = bind(addr->socket, (sockaddr*)&address, sizeof(address));
		return Return;
		//int bind(int s, struct sockaddr *addr, int addrlen);
		break;
	}

	case IOCTL_SO_LISTEN:
	{
		u32 S = Memory::Read_U32(_BufferIn);
		u32 BACKLOG = Memory::Read_U32(_BufferIn + 0x04);
		u32 Return = listen(S, BACKLOG);
		return Return;
		break;
	}

	case IOCTL_SO_ACCEPT:
	{
		//TODO: (Sonic)Check if this is correct
		u32 S = Memory::Read_U32(_BufferIn);
		socklen_t addrlen;
		struct sockaddr_in address;
		u32 Return = (u32)accept(S, (struct sockaddr *)&address, &addrlen);
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
	switch (CommandBuffer.Parameter)
	{
		case IOCTL_SO_BIND:
		case IOCTLV_SO_RECVFROM:
		case IOCTL_SO_SOCKET:
		case IOCTL_SO_GETHOSTID:
		case IOCTL_SO_STARTUP:
		//break;

		default:
			INFO_LOG(WII_IPC_NET, "NET_IP_TOP IOCtlV: 0x%08X\n", CommandBuffer.Parameter);
			break;
	}

	Memory::Write_U32(ReturnValue, _CommandAddress+4);
	return true; 
}
