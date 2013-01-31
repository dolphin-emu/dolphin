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
#include "WII_IPC_HLE_Device_es.h"
#include "../ConfigManager.h"
#include "FileUtil.h"
#include <stdio.h>
#include <string>
#include "ICMP.h"
#include "CommonPaths.h"
#include "SettingsHandler.h"
#include "WII_IPC_HLE.h"
#include "ec_wii.h"


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
{
}

CWII_IPC_HLE_Device_net_kd_request::~CWII_IPC_HLE_Device_net_kd_request()
{
}

bool CWII_IPC_HLE_Device_net_kd_request::Open(u32 CommandAddress, u32 _Mode)
{
	INFO_LOG(WII_IPC_WC24, "NET_KD_REQ: Open");
	Memory::Write_U32(GetDeviceID(), CommandAddress + 4);
	m_Active = true;
	return true;
}

bool CWII_IPC_HLE_Device_net_kd_request::Close(u32 CommandAddress, bool _bForce)
{
	INFO_LOG(WII_IPC_WC24, "NET_KD_REQ: Close");
	if (!_bForce)
		Memory::Write_U32(0, CommandAddress + 4);
	m_Active = false;
	return true;
}

bool CWII_IPC_HLE_Device_net_kd_request::IOCtl(u32 CommandAddress) 
{
	u32 Parameter		= Memory::Read_U32(CommandAddress + 0xC);
	u32 BufferIn		= Memory::Read_U32(CommandAddress + 0x10);
	u32 BufferInSize	= Memory::Read_U32(CommandAddress + 0x14);
	u32 BufferOut		= Memory::Read_U32(CommandAddress + 0x18);
	u32 BufferOutSize	= Memory::Read_U32(CommandAddress + 0x1C);    

	u32 ReturnValue = 0;
	switch (Parameter)
	{
	case IOCTL_NWC24_SUSPEND_SCHEDULAR:
		// NWC24iResumeForCloseLib  from NWC24SuspendScheduler (Input: none, Output: 32 bytes) 
		WARN_LOG(WII_IPC_WC24, "NET_KD_REQ: IOCTL_NWC24_SUSPEND_SCHEDULAR - NI");
		Memory::Write_U32(0, BufferOut); // no error
		break;

	case IOCTL_NWC24_EXEC_TRY_SUSPEND_SCHEDULAR: // NWC24iResumeForCloseLib
		WARN_LOG(WII_IPC_WC24, "NET_KD_REQ: IOCTL_NWC24_EXEC_TRY_SUSPEND_SCHEDULAR - NI");
		break;

	case IOCTL_NWC24_EXEC_RESUME_SCHEDULAR : // NWC24iResumeForCloseLib
		WARN_LOG(WII_IPC_WC24, "NET_KD_REQ: IOCTL_NWC24_EXEC_RESUME_SCHEDULAR - NI");
		Memory::Write_U32(0, BufferOut); // no error
		break;

	case IOCTL_NWC24_STARTUP_SOCKET: // NWC24iStartupSocket
		Memory::Write_U32(0, BufferOut);
		Memory::Write_U32(0, BufferOut+4);
		ReturnValue = 0;
		WARN_LOG(WII_IPC_WC24, "NET_KD_REQ: IOCTL_NWC24_STARTUP_SOCKET - NI");
		break;
	case IOCTL_NWC24_CLEANUP_SOCKET:
		Memory::Memset(BufferOut, 0, BufferOutSize);
		WARN_LOG(WII_IPC_WC24, "NET_KD_REQ: IOCTL_NWC24_CLEANUP_SOCKET - NI");
		break;
	case IOCTL_NWC24_LOCK_SOCKET: // WiiMenu
		WARN_LOG(WII_IPC_WC24, "NET_KD_REQ: IOCTL_NWC24_LOCK_SOCKET - NI");
		break;

	case IOCTL_NWC24_UNLOCK_SOCKET:
		WARN_LOG(WII_IPC_WC24, "NET_KD_REQ: IOCTL_NWC24_UNLOCK_SOCKET - NI");
		break;

	case IOCTL_NWC24_REQUEST_REGISTER_USER_ID:
		WARN_LOG(WII_IPC_WC24, "NET_KD_REQ: IOCTL_NWC24_REQUEST_REGISTER_USER_ID");
		Memory::Write_U32(0, BufferOut);
		Memory::Write_U32(0, BufferOut+4);
		break;

	case IOCTL_NWC24_REQUEST_GENERATED_USER_ID: // (Input: none, Output: 32 bytes)
		WARN_LOG(WII_IPC_WC24, "NET_KD_REQ: IOCTL_NWC24_REQUEST_GENERATED_USER_ID");
		if(config.CreationStage() == nwc24_config_t::NWC24_IDCS_INITIAL)
		{
			std::string settings_Filename(Common::GetTitleDataPath(TITLEID_SYSMENU) + WII_SETTING);
			SettingsHandler gen;
			std::string area, model;
			bool _GotSettings = false;
			
			if (File::Exists(settings_Filename))
			{
				File::IOFile settingsFileHandle(settings_Filename, "rb");
				if (settingsFileHandle.ReadBytes((void*)gen.GetData(), SettingsHandler::SETTINGS_SIZE))
				{
					gen.Decrypt();
					area = gen.GetValue("AREA");
					model = gen.GetValue("MODEL");
					_GotSettings = true;
				}
				
			}
			if (_GotSettings)
			{
				u8 area_code = GetAreaCode(area.c_str());
				u8 id_ctr = config.IdGen();
				u8 hardware_model = GetHardwareModel(model.c_str());
				
				EcWii &ec = EcWii::GetInstance();
				u32 HollywoodID = ec.getNgId();
				u64 UserID = 0;
				
				s32 ret = NWC24MakeUserID(&UserID, HollywoodID, id_ctr, hardware_model, area_code);
				config.SetId(UserID);
				config.IncrementIdGen();
				config.SetCreationStage(nwc24_config_t::NWC24_IDCS_GENERATED);
				config.WriteConfig();
				
				Memory::Write_U32(ret, BufferOut);
			}
			else
			{
				Memory::Write_U32(nwc24_err_t::WC24_ERR_FATAL, BufferOut);	
			}
			
		}
		else if(config.CreationStage() == nwc24_config_t::NWC24_IDCS_GENERATED)
		{
			Memory::Write_U32(nwc24_err_t::WC24_ERR_ID_GENERATED, BufferOut);
		}
		else if(config.CreationStage() == nwc24_config_t::NWC24_IDCS_REGISTERED)
		{
			Memory::Write_U32(nwc24_err_t::WC24_ERR_ID_REGISTERED, BufferOut);
		}
		Memory::Write_U64(config.Id(), BufferOut + 4);
		Memory::Write_U32(config.CreationStage(), BufferOut + 0xC);
		break;
	case IOCTL_NWC24_GET_SCHEDULAR_STAT:
		WARN_LOG(WII_IPC_WC24, "NET_KD_REQ: IOCTL_NWC24_GET_SCHEDULAR_STAT - NI");
		break;

	case IOCTL_NWC24_SAVE_MAIL_NOW:
		WARN_LOG(WII_IPC_WC24, "NET_KD_REQ: IOCTL_NWC24_SAVE_MAIL_NOW - NI");
		break;

	case IOCTL_NWC24_REQUEST_SHUTDOWN:
		// if ya set the IOS version to a very high value this happens ...
		WARN_LOG(WII_IPC_WC24, "NET_KD_REQ: IOCTL_NWC24_REQUEST_SHUTDOWN - NI");
		break;

	default:
		WARN_LOG(WII_IPC_WC24, "/dev/net/kd/request::IOCtl request 0x%x (BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
			Parameter, BufferIn, BufferInSize, BufferOut, BufferOutSize);
		break;
	}

	// g_ReplyQueueLater.push(std::pair<u32, std::string>(CommandAddress, GetDeviceName()));
	Memory::Write_U32(ReturnValue, CommandAddress + 4);

	return true; 
}


u8 CWII_IPC_HLE_Device_net_kd_request::GetAreaCode( const char * area )
{
	u32 i;
	u8 regions_[] = {0,1,2,2,1,3,3,4,5,5,1,2,6,7};
	const char* regions[] = {"JPN", "USA", "EUR", "AUS", "BRA", "TWN", "ROC", "KOR", "HKG", "ASI", "LTN", "SAF", "CHN", ""};
	for (i=0; i<sizeof(regions)/sizeof(*regions); i++)
	{
		if (!strncmp(regions[i], area, 4))
		{
			return regions_[i];
		}
	}
	
	return 7;
}

u8 CWII_IPC_HLE_Device_net_kd_request::GetHardwareModel(const char * model)
{
	u8 mdl;
	if (!strncmp(model, "RVL", 4))
	{
		mdl = MODEL_RVL;
	}else if (!strncmp(model, "RVT", 4))
	{
		mdl = MODEL_RVT;
	}else if (!strncmp(model, "RVV", 4))
	{
		mdl = MODEL_RVV;
	}else if (!strncmp(model, "RVD", 4))
	{
		mdl = MODEL_RVD;
	}else
	{
		mdl = MODEL_ELSE;
	}
	return mdl;
}

static inline u8 u64_get_byte(u64 value, u8 shift)
{
	return (u8)(value >> (shift*8));
}

static inline u64 u64_insert_byte(u64 value, u8 shift, u8 byte)
{
	u64 mask = 0x00000000000000FFULL << (shift*8);
	u64 inst = (u64)byte << (shift*8);
	return (value & ~mask) | inst;
}

s32 CWII_IPC_HLE_Device_net_kd_request::NWC24MakeUserID(u64* nwc24_id, u32 hollywood_id, u16 id_ctr, u8 hardware_model, u8 area_code)
{
	const u8 table2[8]  = {0x1, 0x5, 0x0, 0x4, 0x2, 0x3, 0x6, 0x7};
	const u8 table1[16] = {0x4, 0xB, 0x7, 0x9, 0xF, 0x1, 0xD, 0x3, 0xC, 0x2, 0x6, 0xE, 0x8, 0x0, 0xA, 0x5};
	
	u64 mix_id = ((u64)area_code<<50) | ((u64)hardware_model<<47) | ((u64)hollywood_id<<15) | ((u64)id_ctr<<10);
	u64 mix_id_copy1 = mix_id;
	
	int ctr = 0;
	for (ctr = 0; ctr <= 42; ctr++)
	{
		u64 value = mix_id >> (52-ctr);
		if (value & 1)
		{
			value = 0x0000000000000635ULL << (42-ctr);
			mix_id ^= value;
		}
	}
	
	mix_id = (mix_id_copy1 | (mix_id & 0xFFFFFFFFUL)) ^ 0x0000B3B3B3B3B3B3ULL;
	mix_id = (mix_id >> 10) | ((mix_id & 0x3FF) << (11+32));
	
	for (ctr = 0; ctr <= 5; ctr++)
	{
		u8 ret = u64_get_byte(mix_id, ctr);
		u8 foobar = ((table1[(ret>>4)&0xF])<<4) | (table1[ret&0xF]);
		mix_id = u64_insert_byte(mix_id, ctr, foobar & 0xff);
	}	
	u64 mix_id_copy2 = mix_id;
	
	for (ctr = 0; ctr <= 5; ctr++)
	{
		u8 ret = u64_get_byte(mix_id_copy2, ctr);
		mix_id = u64_insert_byte(mix_id, table2[ctr], ret);
	}
	
	mix_id &= 0x001FFFFFFFFFFFFFULL;
	mix_id = (mix_id << 1) | ((mix_id >> 52) & 1);
	
	mix_id ^= 0x00005E5E5E5E5E5EULL;
	mix_id &= 0x001FFFFFFFFFFFFFULL;
	
	*nwc24_id = mix_id;
	
	if (mix_id > 9999999999999999ULL)
		return nwc24_err_t::WC24_ERR_FATAL;
	
	return nwc24_err_t::WC24_OK;
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

bool CWII_IPC_HLE_Device_net_ncd_manage::Open(u32 CommandAddress, u32 _Mode)
{
	INFO_LOG(WII_IPC_NET, "NET_NCD_MANAGE: Open");
	Memory::Write_U32(GetDeviceID(), CommandAddress+4);
	m_Active = true;
	return true;
}

bool CWII_IPC_HLE_Device_net_ncd_manage::Close(u32 CommandAddress, bool _bForce)
{
	INFO_LOG(WII_IPC_NET, "NET_NCD_MANAGE: Close");
	if (!_bForce)
		Memory::Write_U32(0, CommandAddress + 4);
	m_Active = false;
	return true;
}

bool CWII_IPC_HLE_Device_net_ncd_manage::IOCtlV(u32 CommandAddress)
{
	u32 return_value = 0;
	u32 common_result = 0;
	u32 common_vector = 0;

	SIOCtlVBuffer CommandBuffer(CommandAddress);

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
	Memory::Write_U32(return_value, CommandAddress + 4);
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

bool CWII_IPC_HLE_Device_net_ip_top::Open(u32 CommandAddress, u32 _Mode)
{
	INFO_LOG(WII_IPC_NET, "NET_IP_TOP: Open");
	Memory::Write_U32(GetDeviceID(), CommandAddress+4);
	m_Active = true;
	return true;
}

bool CWII_IPC_HLE_Device_net_ip_top::Close(u32 CommandAddress, bool _bForce)
{
	INFO_LOG(WII_IPC_NET, "NET_IP_TOP: Close");
	if (!_bForce)
		Memory::Write_U32(0, CommandAddress + 4);
	m_Active = false;
	return true;
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


bool CWII_IPC_HLE_Device_net_ip_top::IOCtl(u32 CommandAddress) 
{ 
	u32 BufferIn		= Memory::Read_U32(CommandAddress + 0x10);
	u32 BufferInSize	= Memory::Read_U32(CommandAddress + 0x14);
	u32 BufferOut		= Memory::Read_U32(CommandAddress + 0x18);
	u32 BufferOutSize	= Memory::Read_U32(CommandAddress + 0x1C);
	u32 Command			= Memory::Read_U32(CommandAddress + 0x0C);
	s32 ReturnValue 	= 0;
	
	switch (Command)
	{
		case IOCTL_SO_CONNECT:
		case IOCTL_SO_ACCEPT:
		{
			u32 s = Memory::Read_U32(BufferIn);
			_tSocket * sock = NULL;
			{
				std::unique_lock<std::mutex> lk(socketMapMutex);
				if(socketMap.find(s) != socketMap.end())
					sock = socketMap[s];
			}
			if(sock)
			{
				sock->addCommand(CommandAddress);
				return false;
			}
			else
			{
				ReturnValue = -8; // EBADF
			}
			
			ERROR_LOG(WII_IPC_NET, "Failed to find socket %X", s);
			break;
		}
		
		case IOCTL_SO_SOCKET:
		{
			u32 AF		= Memory::Read_U32(BufferIn);
			u32 TYPE	= Memory::Read_U32(BufferIn + 0x04);
			u32 PROT	= Memory::Read_U32(BufferIn + 0x08);
			u32 s		= (u32)socket(AF, TYPE, PROT);
			WARN_LOG(WII_IPC_NET, "IOCTL_SO_SOCKET "
			"Socket: %08x (%d,%d,%d), BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
					 s, AF, TYPE, PROT, BufferIn, BufferInSize, BufferOut, BufferOutSize);
			
			ReturnValue = getNetErrorCode(s, "SO_SOCKET", false);
			
			if (ReturnValue > 0)
			{
				std::unique_lock<std::mutex> lk(socketMapMutex);
				_tSocket* sock = new _tSocket();
				socketMap[s] = sock;
				sock->thread = new std::thread(&CWII_IPC_HLE_Device_net_ip_top::socketProcessor, this, s);
			}
			
			break;
		}
		case IOCTL_SO_STARTUP:
		{
			WARN_LOG(WII_IPC_NET, "IOCTL_SO_STARTUP "
			"BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
					 BufferIn, BufferInSize, BufferOut, BufferOutSize);
			break;
		}
		
		case IOCTL_SO_SHUTDOWN:
		{
			WARN_LOG(WII_IPC_NET, "IOCTL_SO_SHUTDOWN "
			"BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
					 BufferIn, BufferInSize, BufferOut, BufferOutSize);
			
			u32 sock	= Memory::Read_U32(BufferIn);
			u32 how		= Memory::Read_U32(BufferIn+4);
			ReturnValue = shutdown(sock, how);
			ReturnValue = getNetErrorCode(ReturnValue, "SO_SHUTDOWN", false);
			break;
		}
		
		case IOCTL_SO_CLOSE:
		{
			u32 s = Memory::Read_U32(BufferIn);
			WARN_LOG(WII_IPC_NET, "IOCTL_SO_CLOSE (%08x)", s);
			
			#ifdef _WIN32
				ReturnValue = closesocket(s);
				
				ReturnValue = getNetErrorCode(ReturnValue, "IOCTL_SO_CLOSE", false);
			#else
				ReturnValue = close(s);
			#endif
				
			_tSocket * sock = NULL;
			{
				std::unique_lock<std::mutex> lk(socketMapMutex);
				if(socketMap.find(s) != socketMap.end())
					sock = socketMap[s];
				if(sock)
				{
					//cascading failures hopefully
					sock->StopAndJoin();
					delete sock;
					socketMap.erase(s);
				}
			}
			
				
			break;
		}
		case IOCTL_SO_BIND:
		{
			bind_params *addr = (bind_params*)Memory::GetPointer(BufferIn);
			GC_sockaddr_in addrPC;
			memcpy(&addrPC, addr->name, sizeof(GC_sockaddr_in));
			sockaddr_in address;
			address.sin_family = addrPC.sin_family;
			address.sin_addr.s_addr = addrPC.sin_addr.s_addr_;
			address.sin_port = addrPC.sin_port;
			ReturnValue = bind(Common::swap32(addr->socket), (sockaddr*)&address, sizeof(address));
			
			WARN_LOG(WII_IPC_NET, "IOCTL_SO_BIND (%s:%d) = %d "
			"Socket: %08X, BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
					 inet_ntoa(address.sin_addr), Common::swap16(address.sin_port), ReturnValue,
					 Common::swap32(addr->socket), BufferIn, BufferInSize, BufferOut, BufferOutSize);
			
			ReturnValue = getNetErrorCode(ReturnValue, "SO_BIND", false);
			break;
		}
		
		case IOCTL_SO_LISTEN:
		{
			WARN_LOG(WII_IPC_NET, "IOCTL_SO_LISTEN "
			"BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
					 BufferIn, BufferInSize, BufferOut, BufferOutSize);
			
			u32 S = Memory::Read_U32(BufferIn);
			u32 BACKLOG = Memory::Read_U32(BufferIn + 0x04);
			ReturnValue = listen(S, BACKLOG);
			ReturnValue = getNetErrorCode(ReturnValue, "SO_LISTEN", false);
			break;
		}
		
		case IOCTL_SO_GETSOCKOPT:
		{
			u32 sock = Memory::Read_U32(BufferOut);
			u32 level = Memory::Read_U32(BufferOut + 4);
			u32 optname = Memory::Read_U32(BufferOut + 8);
			
			WARN_LOG(WII_IPC_NET,"IOCTL_SO_GETSOCKOPT(%08x, %08x, %08x)"
			"BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
					 sock, level, optname,
			BufferIn, BufferInSize, BufferOut, BufferOutSize);
			
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
			
			ReturnValue = getsockopt (sock, nat_level, nat_optname, (char *) &optval, (socklen_t*)&optlen);
			
			ReturnValue = getNetErrorCode(ReturnValue, "SO_GETSOCKOPT", false);
			
			
			Memory::Write_U32(optlen, BufferOut + 0xC);
			Memory::WriteBigEData((u8 *) optval, BufferOut + 0x10, optlen);
			
			if(optname == 0x1007){
				s32 errorcode = Memory::Read_U32(BufferOut + 0x10);
				WARN_LOG(WII_IPC_NET,"IOCTL_SO_GETSOCKOPT error code = %i", errorcode);
			}
			break;
		}
		
		case IOCTL_SO_SETSOCKOPT:
		{
			u32 S = Memory::Read_U32(BufferIn);
			u32 level = Memory::Read_U32(BufferIn + 4);
			u32 optname = Memory::Read_U32(BufferIn + 8);
			u32 optlen = Memory::Read_U32(BufferIn + 0xc);
			u8 optval[20];
			Memory::ReadBigEData(optval, BufferIn + 0x10, optlen);
			
			//TODO: bug booto about this, 0x2005 most likely timeout related, default value on wii is , 0x2001 is most likely tcpnodelay
			if (level == 6 && (optname == 0x2005 || optname == 0x2001)){
				ReturnValue = 0;
				break;
			}
			WARN_LOG(WII_IPC_NET, "IOCTL_SO_SETSOCKOPT(%08x, %08x, %08x, %08x) "
				"BufferIn: (%08x, %i), BufferOut: (%08x, %i)"
				"%02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx",
				S, level, optname, optlen, BufferIn, BufferInSize, BufferOut, BufferOutSize, 
				optval[0], optval[1], optval[2], optval[3], 
				optval[4], optval[5], optval[6], optval[7], 
				optval[8], optval[9], optval[10], optval[11], 
				optval[12], optval[13], optval[14], optval[15], 
				optval[16], optval[17], optval[18], optval[19]);
			
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
			
			ReturnValue = setsockopt(S, nat_level, nat_optname, (char*)optval, optlen);
			ReturnValue = getNetErrorCode(ReturnValue, "SO_SETSOCKOPT", false);
			
			break;
		}
		
		case IOCTL_SO_FCNTL:
		{
			u32 sock	= Memory::Read_U32(BufferIn);
			u32 cmd		= Memory::Read_U32(BufferIn + 4);
			u32 arg		= Memory::Read_U32(BufferIn + 8);
			
			WARN_LOG(WII_IPC_NET, "IOCTL_SO_FCNTL(%08X, %08X) "
			"Socket: %08x, BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
					 cmd, arg,
			sock, BufferIn, BufferInSize, BufferOut, BufferOutSize);
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
					if (arg & F_NONBLOCK)
						iMode = 1;
					ReturnValue = ioctlsocket(sock, FIONBIO, &iMode);
					ReturnValue = getNetErrorCode(ReturnValue, "SO_FCNTL", false);
				}
				else
				{
					WARN_LOG(WII_IPC_NET, "SO_FCNTL unknown command");
				}
			#else
				// Map POSIX <-> Wii socket flags
				// First one is POSIX, second one is Wii
				static int mapping[][2] = {
					{ O_NONBLOCK, 0x4 },
				};
				
				if (cmd == F_GETFL)
				{
					int flags = fcntl(sock, F_GETFL, 0);
					ReturnValue = 0;
					
					for (unsigned int i = 0; i < sizeof (mapping) / sizeof (mapping[0]); ++i)
						if (flags & mapping[i][0])
							ReturnValue |= mapping[i][1];
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
					
					ReturnValue = fcntl(sock, F_SETFL, posix_flags);
					ReturnValue = getNetErrorCode(ReturnValue, "SO_FCNTL", false);
				}
				else
				{
					WARN_LOG(WII_IPC_NET, "SO_FCNTL unknown command");
				}
			#endif
			break;
		}
		
		case IOCTL_SO_GETSOCKNAME:
		{
			u32 sock = Memory::Read_U32(BufferIn);
			
			WARN_LOG(WII_IPC_NET, "IOCTL_SO_GETSOCKNAME "
			"Socket: %08X, BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
					 sock, BufferIn, BufferInSize, BufferOut, BufferOutSize);
			
			sockaddr sa;
			socklen_t sa_len;
			sa_len = sizeof(sa);
			ReturnValue = getsockname(sock, &sa, &sa_len);
			
			Memory::Write_U8(BufferOutSize, BufferOut);
			Memory::Write_U8(sa.sa_family & 0xFF, BufferOut + 1);
			Memory::WriteBigEData((u8*)&sa.sa_data, BufferOut + 2, BufferOutSize - 2);
			break;
		}
		case IOCTL_SO_GETPEERNAME:
		{
			u32 sock = Memory::Read_U32(BufferIn);
			
			sockaddr sa;
			socklen_t sa_len;
			sa_len = sizeof(sa);
			
			ReturnValue = getpeername(sock, &sa, &sa_len);
			
			Memory::Write_U8(BufferOutSize, BufferOut);
			Memory::Write_U8(AF_INET, BufferOut + 1);
			Memory::WriteBigEData((u8*)&sa.sa_data, BufferOut + 2, BufferOutSize - 2);
			
			WARN_LOG(WII_IPC_NET, "IOCTL_SO_GETPEERNAME(%x)", sock);
			
			break;
		}
		case IOCTL_SO_GETHOSTID:
		{
			WARN_LOG(WII_IPC_NET, "IOCTL_SO_GETHOSTID "
			"(BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
					 BufferIn, BufferInSize, BufferOut, BufferOutSize);
			ReturnValue = 192 << 24 | 168 << 16 | 1 << 8 | 150;
			break;
		}
		case IOCTL_SO_INETATON:
		{
			struct hostent *remoteHost = gethostbyname((char*)Memory::GetPointer(BufferIn));
			
			Memory::Write_U32(Common::swap32(*(u32 *)remoteHost->h_addr_list[0]), BufferOut);
			WARN_LOG(WII_IPC_NET, "IOCTL_SO_INETATON = %d "
			"%s, BufferIn: (%08x, %i), BufferOut: (%08x, %i), IP Found: %08X",remoteHost->h_addr_list[0] == 0 ? -1 : 0,
					 (char*)Memory::GetPointer(BufferIn), BufferIn, BufferInSize, BufferOut, BufferOutSize, Common::swap32(*(u32 *)remoteHost->h_addr_list[0]));
			ReturnValue = remoteHost->h_addr_list[0] == 0 ? 0 : 1;
			break;
		}
		case IOCTL_SO_INETPTON:
		{
			WARN_LOG(WII_IPC_NET, "IOCTL_SO_INETPTON "
			"(Translating: %s)", Memory::GetPointer(BufferIn));
			ReturnValue = inet_pton((char*)Memory::GetPointer(BufferIn), Memory::GetPointer(BufferOut+4));
			break;
		}
		case IOCTL_SO_INETNTOP:
		{
			u32 af = Memory::Read_U32(BufferIn);
			//u32 af = Memory::Read_U32(BufferIn + 4);
			u32 src = Memory::Read_U32(BufferIn + 8);
			//u32 af = Memory::Read_U32(BufferIn + 12);
			//u32 af = Memory::Read_U32(BufferIn + 16);
			//u32 af = Memory::Read_U32(BufferIn + 20);
			char ip_s[16];
			sprintf(ip_s, "%i.%i.%i.%i",
					Memory::Read_U8(BufferIn + 8),
					Memory::Read_U8(BufferIn + 8 + 1),
					Memory::Read_U8(BufferIn + 8 + 2),
					Memory::Read_U8(BufferIn + 8 + 3)
			);
			WARN_LOG(WII_IPC_NET, "IOCTL_SO_INETNTOP %s", ip_s);
			memset(Memory::GetPointer(BufferOut), 0, BufferOutSize);
			memcpy(Memory::GetPointer(BufferOut), ip_s, strlen(ip_s));
			break;
		}
		
		case IOCTL_SO_POLL:
		{
			// Map Wii/native poll events types
			unsigned int mapping[][2] = {
				{ POLLIN, 0x0001 },
				{ POLLOUT, 0x0008 },
				{ POLLHUP, 0x0040 },
			};
			
			u32 unknown = Memory::Read_U32(BufferIn);
			u32 timeout = Memory::Read_U32(BufferIn + 4);
			
			int nfds = BufferOutSize / 0xc;
			if (nfds == 0)
				ERROR_LOG(WII_IPC_NET,"Hidden POLL");
			
			pollfd_t* ufds = (pollfd_t *)malloc(sizeof(pollfd_t) * nfds);
			if (ufds == NULL)
			{
				ReturnValue = -1;
				break;
			}
			
			for (int i = 0; i < nfds; i++)
			{
				ufds[i].fd = Memory::Read_U32(BufferOut + 0xc*i);				//fd
				int events = Memory::Read_U32(BufferOut + 0xc*i + 4);			//events
				ufds[i].revents = Memory::Read_U32(BufferOut + 0xc*i + 8);	//revents
				
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
			
			ReturnValue = poll(ufds, nfds, timeout);
			ReturnValue = getNetErrorCode(ReturnValue, "SO_POLL", false);
			
			for (int i = 0; i<nfds; i++)
			{
				
				// Translate native to Wii events
				int revents = 0;
				for (unsigned int j = 0; j < sizeof (mapping) / sizeof (mapping[0]); ++j)
				{
					if (ufds[i].revents & mapping[j][0])
						revents |= mapping[j][1];
				}
				
				// No need to change fd or events as they are input only.
				// Memory::Write_U32(ufds[i].fd, BufferOut + 0xc*i);	//fd
				// Memory::Write_U32(events, BufferOut + 0xc*i + 4);	//events
				Memory::Write_U32(revents, BufferOut + 0xc*i + 8);	//revents
				
				WARN_LOG(WII_IPC_NET, "IOCTL_SO_POLL socket %d revents %08X events %08X", i, revents, ufds[i].events);
			}
			free(ufds);
			
			break;
		}
		
		case IOCTL_SO_GETHOSTBYNAME:
		{
			hostent *remoteHost = gethostbyname((char*)Memory::GetPointer(BufferIn));
			
			WARN_LOG(WII_IPC_NET, "IOCTL_SO_GETHOSTBYNAME "
			"Address: %s, BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
					 (char*)Memory::GetPointer(BufferIn), BufferIn, BufferInSize, BufferOut, BufferOutSize);
			
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
				
				Memory::Memset(BufferOut, 0, BufferOutSize);
				u32 wii_addr = BufferOut + 4 * 3 + 2 * 2;
				
				u32 name_length = strlen(remoteHost->h_name) + 1;
				Memory::WriteBigEData((const u8 *)remoteHost->h_name, wii_addr, name_length);
				Memory::Write_U32(wii_addr, BufferOut);
				wii_addr += (name_length + 4) & ~3;
				
				// aliases - empty
				Memory::Write_U32(wii_addr, BufferOut + 4);
				Memory::Write_U32(wii_addr + sizeof(u32), wii_addr);
				wii_addr += sizeof(u32);
				Memory::Write_U32((u32)NULL, wii_addr);
				wii_addr += sizeof(u32);
				
				// hardcode to ipv4
				_dbg_assert_msg_(WII_IPC_NET,
								 remoteHost->h_addrtype == AF_INET && remoteHost->h_length == sizeof(u32),
								 "returned host info is not IPv4");
				Memory::Write_U16(AF_INET, BufferOut + 8);
				Memory::Write_U16(sizeof(u32), BufferOut + 10);
				
				// addrlist - probably only really need to return 1 anyways...
				Memory::Write_U32(wii_addr, BufferOut + 12);
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
				//	ArrayToString(Memory::GetPointer(BufferOut), BufferOutSize, 16).c_str());
				ReturnValue = 0;
			}
			else
			{
				ReturnValue = -1;
			}
			
			break;
		}
		
		case IOCTL_SO_ICMPSOCKET:
		{
			// AF type?
			u32 arg = Memory::Read_U32(BufferIn);
			u32 sock = (u32)socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
			DEBUG_LOG(WII_IPC_NET, "IOCTL_SO_ICMPSOCKET(%x) %x", arg, sock);
			ReturnValue = getNetErrorCode(sock, "IOCTL_SO_ICMPSOCKET", false);
			break;
		}
		
		case IOCTL_SO_ICMPCANCEL:
			ERROR_LOG(WII_IPC_NET, "IOCTL_SO_ICMPCANCEL");
			goto default_;
			
		case IOCTL_SO_ICMPCLOSE:
		{
			u32 sock = Memory::Read_U32(BufferIn);
			#ifdef _WIN32
				u32 ReturnValue = closesocket(sock);
			#else
				u32 ReturnValue = close(sock);
			#endif
			DEBUG_LOG(WII_IPC_NET, "IOCTL_SO_ICMPCLOSE(%x) %x", sock, ReturnValue);
			ReturnValue = getNetErrorCode(ReturnValue, "IOCTL_SO_ICMPCLOSE", false);
			break;
		}
		
		default:
		{
			WARN_LOG(WII_IPC_NET,"0x%x "
			"BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
					 Command, BufferIn, BufferInSize, BufferOut, BufferOutSize);
default_:
			if (BufferInSize)
			{
				ERROR_LOG(WII_IPC_NET, "in addr %x size %x", BufferIn, BufferInSize);
				ERROR_LOG(WII_IPC_NET, "\n%s", 
							ArrayToString(Memory::GetPointer(BufferIn), BufferInSize, 4).c_str()
				);
			}
			
			if (BufferOutSize)
			{
				ERROR_LOG(WII_IPC_NET, "out addr %x size %x", BufferOut, BufferOutSize);
			}
			break;
		}
	}
	
	Memory::Write_U32(ReturnValue, CommandAddress + 0x4);

	return true;
}

char* DecodeError(int ErrorCode)
{

#ifdef _WIN32
	char Message[1024];
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS |
		FORMAT_MESSAGE_MAX_WIDTH_MASK, NULL, ErrorCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)Message, 1024, NULL);
	return Message;
#else
	return strerror(ErrorCode);
#endif
}

int getNetErrorCode(int ret, std::string caller, bool isRW)
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
		return -26; // EINPROGRESS (should be -8)
	default:
		return -1;
	}

#undef ERRORCODE
#undef EITHER
}


void CWII_IPC_HLE_Device_net_ip_top::socketProcessor(u32 socket)
{
	ERROR_LOG(WII_IPC_NET, "Socket %d has started a thread.... oh dear.", socket);
	_tSocket* sock = NULL;
	{
		std::unique_lock<std::mutex> lk(socketMapMutex);
		if(socketMap.find(socket) == socketMap.end())
		{
			ERROR_LOG(WII_IPC_NET, "Socket %d could not be found in the socket map.", socket);
			return;
		}
		sock = socketMap[socket];
	}
	while((sock->WaitForEvent(), true) && sock->running)
	{
		u32 CommandAddress = 0;
		while(sock->getCommand(CommandAddress))
		{
			
			using WII_IPC_HLE_Interface::ECommandType;
			using WII_IPC_HLE_Interface::COMMAND_IOCTL;
			using WII_IPC_HLE_Interface::COMMAND_IOCTLV;
			
			s32 ReturnValue = 0;
			
			ECommandType CommandType = static_cast<ECommandType>(Memory::Read_U32(CommandAddress));
			
			if(CommandType == COMMAND_IOCTL)
			{
				u32 BufferIn		= Memory::Read_U32(CommandAddress + 0x10);
				u32 BufferInSize	= Memory::Read_U32(CommandAddress + 0x14);
				u32 BufferOut		= Memory::Read_U32(CommandAddress + 0x18);
				u32 BufferOutSize	= Memory::Read_U32(CommandAddress + 0x1C);
				u32 Command			= Memory::Read_U32(CommandAddress + 0x0C);
				WARN_LOG(WII_IPC_NET, "IOCTL command = %d", Command);
				switch(Command)
				{
					case IOCTL_SO_ACCEPT:
					{
						WARN_LOG(WII_IPC_NET, "IOCTL_SO_ACCEPT(%d) "
						"BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
						socket, BufferIn, BufferInSize, BufferOut, BufferOutSize);
						struct sockaddr* addr = (struct sockaddr*) Memory::GetPointer(BufferOut);
						socklen_t* addrlen = (socklen_t*) Memory::GetPointer(BufferOutSize);
						*addrlen = sizeof(struct sockaddr);
						ReturnValue = accept(socket, addr, addrlen);
						ReturnValue = getNetErrorCode(ReturnValue, "SO_ACCEPT", false);
						break;
					}
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
						
						Memory::ReadBigEData((u8*)&params, BufferIn, sizeof(connect_params));
						
						if (Common::swap32(params.has_addr) != 1)
						{
							WARN_LOG(WII_IPC_NET,"IOCTL_SO_CONNECT: failed");
							ReturnValue = -1;
							break;
						}
						
						memset(&serverAddr, 0, sizeof(serverAddr));
						memcpy(&serverAddr, params.addr, params.addr[0]);
						
						// GC/Wii sockets have a length param as well, we dont really care :)
						serverAddr.sin_family = serverAddr.sin_family >> 8;
						
						ReturnValue = connect(socket, (struct sockaddr *) &serverAddr, sizeof(serverAddr));
						ReturnValue = getNetErrorCode(ReturnValue, "SO_CONNECT", false);
						WARN_LOG(WII_IPC_NET,"IOCTL_SO_CONNECT (%08x, %s:%d)",
								 socket, inet_ntoa(serverAddr.sin_addr), Common::swap16(serverAddr.sin_port));
						break;
					}
					default:
					{
						
						ERROR_LOG(WII_IPC_NET, "Socket %08x has received an unknown IOCTLV %d.", socket, Command);
						break;
					}
				}
				
			}
			else if (CommandType == COMMAND_IOCTLV)
			{
				SIOCtlVBuffer CommandBuffer(CommandAddress);
				
				u32 BufferIn = 0, BufferIn2 = 0, BufferIn3 = 0;
				u32 BufferInSize = 0, BufferInSize2 = 0, BufferInSize3 = 0;
				
				u32 BufferOut = 0, BufferOut2 = 0, BufferOut3 = 0;
				u32 BufferOutSize = 0, BufferOutSize2 = 0, BufferOutSize3 = 0;
				
				if (CommandBuffer.InBuffer.size() > 0)
				{
					BufferIn = CommandBuffer.InBuffer.at(0).m_Address;
					BufferInSize = CommandBuffer.InBuffer.at(0).m_Size;
				}
				if (CommandBuffer.InBuffer.size() > 1)
				{
					BufferIn2 = CommandBuffer.InBuffer.at(1).m_Address;
					BufferInSize2 = CommandBuffer.InBuffer.at(1).m_Size;
				}
				if (CommandBuffer.InBuffer.size() > 2)
				{
					BufferIn3 = CommandBuffer.InBuffer.at(2).m_Address;
					BufferInSize3 = CommandBuffer.InBuffer.at(2).m_Size;
				}
				
				if (CommandBuffer.PayloadBuffer.size() > 0)
				{
					BufferOut = CommandBuffer.PayloadBuffer.at(0).m_Address;
					BufferOutSize = CommandBuffer.PayloadBuffer.at(0).m_Size;
				}
				if (CommandBuffer.PayloadBuffer.size() > 1)
				{
					BufferOut2 = CommandBuffer.PayloadBuffer.at(1).m_Address;
					BufferOutSize2 = CommandBuffer.PayloadBuffer.at(1).m_Size;
				}
				if (CommandBuffer.PayloadBuffer.size() > 2)
				{
					BufferOut3 = CommandBuffer.PayloadBuffer.at(2).m_Address;
					BufferOutSize3 = CommandBuffer.PayloadBuffer.at(2).m_Size;
				}
				switch(CommandBuffer.Parameter)
				{
					case IOCTLV_SO_RECVFROM:
					{
								 
						u32 flags	= Memory::Read_U32(BufferIn + 4);
						
						char *buf	= (char *)Memory::GetPointer(BufferOut);
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
						#ifdef _WIN32
						if(flags & MSG_PEEK){
							unsigned long totallen = 0;
							ioctlsocket(socket, FIONREAD, &totallen);
							ReturnValue = totallen;
							break;
						}
						#endif
						ReturnValue = recvfrom(socket, buf, len, flags,
												fromlen ? (struct sockaddr*) &addr : NULL,
												fromlen ? &fromlen : 0);
						
						ReturnValue = getNetErrorCode(ReturnValue, fromlen ? "SO_RECVFROM" : "SO_RECV", true);
						
						
						WARN_LOG(WII_IPC_NET, "%s(%d, %p) Socket: %08X, Flags: %08X, "
						"BufferIn: (%08x, %i), BufferIn2: (%08x, %i), "
						"BufferOut: (%08x, %i), BufferOut2: (%08x, %i)",fromlen ? "IOCTLV_SO_RECVFROM " : "IOCTLV_SO_RECV ",
						ReturnValue, buf, socket, flags,
						BufferIn, BufferInSize, BufferIn2, BufferInSize2,
						BufferOut, BufferOutSize, BufferOut2, BufferOutSize2);
						
						if (BufferOutSize2 != 0)
						{
							addr.sin_family = (addr.sin_family << 8) | (BufferOutSize2&0xFF);
							Memory::WriteBigEData((u8*)&addr, BufferOut2, BufferOutSize2);
						}
						
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
						
						char * data = (char*)Memory::GetPointer(BufferIn);
						Memory::ReadBigEData((u8*)&params, BufferIn2, BufferInSize2);
						
						if (params.has_destaddr)
						{
							struct sockaddr_in* addr = (struct sockaddr_in*)&params.destaddr;
							u8 len = sizeof(sockaddr); //addr->sin_family & 0xFF;
							addr->sin_family = addr->sin_family >> 8;
							
							ReturnValue = sendto(socket, data,
												 BufferInSize, Common::swap32(params.flags), (struct sockaddr*)addr, len);
							
							WARN_LOG(WII_IPC_NET,
									 "IOCTLV_SO_SENDTO = %d Socket: %08x, BufferIn: (%08x, %i), BufferIn2: (%08x, %i), %u.%u.%u.%u",
									 ReturnValue, socket, BufferIn, BufferInSize,
									 BufferIn2, BufferInSize2,
									 addr->sin_addr.s_addr & 0xFF,
									 (addr->sin_addr.s_addr >> 8) & 0xFF,
									 (addr->sin_addr.s_addr >> 16) & 0xFF,
									 (addr->sin_addr.s_addr >> 24) & 0xFF
							);
							
							ReturnValue = getNetErrorCode(ReturnValue, "SO_SENDTO", true);
						}
						else
						{
							ReturnValue = send(socket, data,
											   BufferInSize, Common::swap32(params.flags));
							WARN_LOG(WII_IPC_NET, "IOCTLV_SO_SEND = %d Socket: %08x, BufferIn: (%08x, %i), BufferIn2: (%08x, %i)",
											ReturnValue, socket, BufferIn, BufferInSize,
											BufferIn2, BufferInSize2);
							
							ReturnValue = getNetErrorCode(ReturnValue, "SO_SEND", true);
						}
						break;
					}
					default:
					{
						
						ERROR_LOG(WII_IPC_NET, "Socket %d has received an unknown IOCTLV %d.", socket, CommandBuffer.Parameter);
						break;
					}
				}
			}
			
			Memory::Write_U32(8, CommandAddress);
			// IOS seems to write back the command that was responded to
			Memory::Write_U32(CommandType, CommandAddress + 8);
			// Return value
			Memory::Write_U32(ReturnValue, CommandAddress + 4);
			WII_IPC_HLE_Interface::EnqReply(CommandAddress);
		}
		
		//DEBUG_LOG(WII_IPC_NET, "Socket %d has processed all events.", socket);
	}
	//DEBUG_LOG(WII_IPC_NET, "Socket %d has died.", socket);
}


bool CWII_IPC_HLE_Device_net_ip_top::IOCtlV(u32 CommandAddress) 
{ 
	SIOCtlVBuffer CommandBuffer(CommandAddress);
	
	s32 ReturnValue = 0;
	
	u32 BufferIn = 0, BufferIn2 = 0, BufferIn3 = 0;
	u32 BufferInSize = 0, BufferInSize2 = 0, BufferInSize3 = 0;

	u32 BufferOut = 0, BufferOut2 = 0, BufferOut3 = 0;
	u32 BufferOutSize = 0, BufferOutSize2 = 0, BufferOutSize3 = 0;

	if (CommandBuffer.InBuffer.size() > 0)
	{
		BufferIn = CommandBuffer.InBuffer.at(0).m_Address;
		BufferInSize = CommandBuffer.InBuffer.at(0).m_Size;
	}
	if (CommandBuffer.InBuffer.size() > 1)
	{
		BufferIn2 = CommandBuffer.InBuffer.at(1).m_Address;
		BufferInSize2 = CommandBuffer.InBuffer.at(1).m_Size;
	}
	if (CommandBuffer.InBuffer.size() > 2)
	{
		BufferIn3 = CommandBuffer.InBuffer.at(2).m_Address;
		BufferInSize3 = CommandBuffer.InBuffer.at(2).m_Size;
	}

	if (CommandBuffer.PayloadBuffer.size() > 0)
	{
		BufferOut = CommandBuffer.PayloadBuffer.at(0).m_Address;
		BufferOutSize = CommandBuffer.PayloadBuffer.at(0).m_Size;
	}
	if (CommandBuffer.PayloadBuffer.size() > 1)
	{
		BufferOut2 = CommandBuffer.PayloadBuffer.at(1).m_Address;
		BufferOutSize2 = CommandBuffer.PayloadBuffer.at(1).m_Size;
	}
	if (CommandBuffer.PayloadBuffer.size() > 2)
	{
		BufferOut3 = CommandBuffer.PayloadBuffer.at(2).m_Address;
		BufferOutSize3 = CommandBuffer.PayloadBuffer.at(2).m_Size;
	}

	switch (CommandBuffer.Parameter)
	{
	case IOCTLV_SO_GETINTERFACEOPT:
	{
			#ifdef _WIN32
			PIP_ADAPTER_ADDRESSES AdapterAddresses = NULL;
			ULONG OutBufferLength = 0;
			ULONG RetVal = 0, i;
			#endif
			
			u32 param = 0, param2 = 0, param3, param4, param5 = 0;
			param = Memory::Read_U32(BufferIn);
			param2 = Memory::Read_U32(BufferIn+4);
			param3 = Memory::Read_U32(BufferOut);
			param4 = Memory::Read_U32(BufferOut2);
			if (BufferOutSize >= 8)
			{

				param5 = Memory::Read_U32(BufferOut+4);
			}

			WARN_LOG(WII_IPC_NET,"IOCTLV_SO_GETINTERFACEOPT(%08X, %08X) "
				"BufferIn: (%08x, %i), BufferIn2: (%08x, %i)",
				param, param2,
				BufferIn, BufferInSize, BufferIn2, BufferInSize2);

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

					Memory::Write_U32(address, BufferOut);
					Memory::Write_U32(0x08080808, BufferOut+4);
					break;
				}

			case 0x1003:
				Memory::Write_U32(0, BufferOut);
				break;

			case 0x1004:
				Memory::WriteBigEData(default_address, BufferOut, 6);
				break;
				
			case 0x1005:
				Memory::Write_U32(1, BufferOut);
				Memory::Write_U32(4, BufferOut2);
				break;
			case 0x4002:
				Memory::Write_U32(2, BufferOut);
				break;

			case 0x4003:
				Memory::Write_U32(0xC, BufferOut2);
				Memory::Write_U32(10 << 24 | 1 << 8 | 30, BufferOut);
				Memory::Write_U32(255 << 24 | 255 << 16  | 255 << 8 | 0, BufferOut+4);
				Memory::Write_U32(10 << 24 | 0 << 16  | 255 << 8 | 255, BufferOut+8);
				break;

			default:
				ERROR_LOG(WII_IPC_NET, "Unknown param2: %08X", param2);
				break;

			}

			ReturnValue = 0;
			break;
		}

	case IOCTLV_SO_SENDTO:
	case IOCTLV_SO_RECVFROM:
	{
		u32 s = 0;
		if (CommandBuffer.Parameter == IOCTLV_SO_SENDTO)
		{
			s = Memory::Read_U32(BufferIn2);
		} else {
			s = Memory::Read_U32(BufferIn);
		}
		
		_tSocket * sock = NULL;
		{
			std::unique_lock<std::mutex> lk(socketMapMutex);
			if(socketMap.find(s) != socketMap.end())
				sock = socketMap[s];
		}
		if(sock)
		{
			sock->addCommand(CommandAddress);
			return false;
		}
		else
		{
			ReturnValue = -8; // EBADF
		}
		
		ERROR_LOG(WII_IPC_NET, "Failed to find socket %d", s);
		break;
	}
		
	case IOCTLV_SO_GETADDRINFO:
		{
			struct addrinfo hints;
			struct addrinfo *result = NULL;

			if (BufferInSize3)
			{
				hints.ai_flags		= Memory::Read_U32(BufferIn3);
				hints.ai_family		= Memory::Read_U32(BufferIn3 + 0x4);
				hints.ai_socktype	= Memory::Read_U32(BufferIn3 + 0x8);
				hints.ai_protocol	= Memory::Read_U32(BufferIn3 + 0xC);
				hints.ai_addrlen	= Memory::Read_U32(BufferIn3 + 0x10);
				hints.ai_canonname	= (char*)Memory::Read_U32(BufferIn3 + 0x14);
				hints.ai_addr		= (sockaddr *)Memory::Read_U32(BufferIn3 + 0x18);
				hints.ai_next		= (addrinfo *)Memory::Read_U32(BufferIn3 + 0x1C);
			}

			char* pNodeName = NULL;
			if (BufferInSize > 0)
				pNodeName = (char*)Memory::GetPointer(BufferIn);

			char* pServiceName = NULL;
			if (BufferInSize2 > 0)
				pServiceName = (char*)Memory::GetPointer(BufferIn2);

			ReturnValue = getaddrinfo(pNodeName, pServiceName, BufferInSize3 ? &hints : NULL, &result);
			u32 addr = BufferOut;
			u32 sockoffset = addr + 0x460;
			if (ReturnValue >= 0)
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
				BufferIn, BufferInSize, BufferOut, BufferOutSize);
			WARN_LOG(WII_IPC_NET, "IOCTLV_SO_GETADDRINFO: %s", Memory::GetPointer(BufferIn));
			break;
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

			u32 sock	= Memory::Read_U32(BufferIn);
			u32 num_ip	= Memory::Read_U32(BufferIn + 4);
			u64 timeout	= Memory::Read_U64(BufferIn + 8);

			if (num_ip != 1)
			{
				WARN_LOG(WII_IPC_NET, "IOCTLV_SO_ICMPPING %i IPs", num_ip);
			}

			ip_info.length		= Memory::Read_U8(BufferIn + 16);
			ip_info.addr_family	= Memory::Read_U8(BufferIn + 17);
			ip_info.icmp_id		= Memory::Read_U16(BufferIn + 18);
			ip_info.ip			= Memory::Read_U32(BufferIn + 20);

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
				memcpy(data, Memory::GetPointer(BufferIn2), BufferInSize2);
			else
			{
				// TODO sequence number is incremented either statically, by
				// port, or by socket. Doesn't seem to matter, so we just leave
				// it 0
				((u16 *)data)[0] = Common::swap16(ip_info.icmp_id);
				icmp_length = 22;
			}
			
			ReturnValue = icmp_echo_req(sock, &addr, data, icmp_length);
			if (ReturnValue == icmp_length)
			{
				ReturnValue = icmp_echo_rep(sock, &addr, (u32)timeout, icmp_length);
			}

			// TODO proper error codes
			ReturnValue = 0;
			break;
		}

	default:
		WARN_LOG(WII_IPC_NET,"0x%x (BufferIn: (%08x, %i), BufferIn2: (%08x, %i)",
			CommandBuffer.Parameter, BufferIn, BufferInSize, BufferIn2, BufferInSize2);
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
	Memory::Write_U32(ReturnValue, CommandAddress + 4);
	return true; 
}

#ifdef _MSC_VER
#pragma optimize("",off)
#endif
