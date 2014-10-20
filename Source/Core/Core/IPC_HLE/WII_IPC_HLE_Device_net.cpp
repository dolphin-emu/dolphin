// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/NandPaths.h"
#include "Common/Network.h"
#include "Common/SettingsHandler.h"
#include "Common/StringUtil.h"

#include "Core/ConfigManager.h"
#include "Core/ec_wii.h"
#include "Core/IPC_HLE/ICMP.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_es.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device_net.h"
#include "Core/IPC_HLE/WII_Socket.h"

#ifdef _WIN32
#include <ws2tcpip.h>
#include <iphlpapi.h>

#define MALLOC(x) HeapAlloc(GetProcessHeap(), 0, (x))
#define FREE(x) HeapFree(GetProcessHeap(), 0, (x))

#elif defined(__linux__) or defined(__APPLE__)
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <errno.h>
#include <poll.h>
#include <string.h>

typedef struct pollfd pollfd_t;
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#endif

// **********************************************************************************
// Handle /dev/net/kd/request requests
CWII_IPC_HLE_Device_net_kd_request::CWII_IPC_HLE_Device_net_kd_request(u32 _DeviceID, const std::string& _rDeviceName)
	: IWII_IPC_HLE_Device(_DeviceID, _rDeviceName)
{
}

CWII_IPC_HLE_Device_net_kd_request::~CWII_IPC_HLE_Device_net_kd_request()
{
	WiiSockMan::GetInstance().Clean();
}

bool CWII_IPC_HLE_Device_net_kd_request::Open(u32 _CommandAddress, u32 _Mode)
{
	INFO_LOG(WII_IPC_WC24, "NET_KD_REQ: Open");
	Memory::Write_U32(GetDeviceID(), _CommandAddress + 4);
	m_Active = true;
	return true;
}

bool CWII_IPC_HLE_Device_net_kd_request::Close(u32 _CommandAddress, bool _bForce)
{
	INFO_LOG(WII_IPC_WC24, "NET_KD_REQ: Close");
	if (!_bForce)
		Memory::Write_U32(0, _CommandAddress + 4);
	m_Active = false;
	return true;
}

bool CWII_IPC_HLE_Device_net_kd_request::IOCtl(u32 _CommandAddress)
{
	u32 Parameter     = Memory::Read_U32(_CommandAddress + 0xC);
	u32 BufferIn      = Memory::Read_U32(_CommandAddress + 0x10);
	u32 BufferInSize  = Memory::Read_U32(_CommandAddress + 0x14);
	u32 BufferOut     = Memory::Read_U32(_CommandAddress + 0x18);
	u32 BufferOutSize = Memory::Read_U32(_CommandAddress + 0x1C);

	u32 ReturnValue = 0;
	switch (Parameter)
	{
	case IOCTL_NWC24_SUSPEND_SCHEDULAR:
		// NWC24iResumeForCloseLib  from NWC24SuspendScheduler (Input: none, Output: 32 bytes)
		INFO_LOG(WII_IPC_WC24, "NET_KD_REQ: IOCTL_NWC24_SUSPEND_SCHEDULAR - NI");
		Memory::Write_U32(0, BufferOut); // no error
		break;

	case IOCTL_NWC24_EXEC_TRY_SUSPEND_SCHEDULAR: // NWC24iResumeForCloseLib
		INFO_LOG(WII_IPC_WC24, "NET_KD_REQ: IOCTL_NWC24_EXEC_TRY_SUSPEND_SCHEDULAR - NI");
		break;

	case IOCTL_NWC24_EXEC_RESUME_SCHEDULAR : // NWC24iResumeForCloseLib
		INFO_LOG(WII_IPC_WC24, "NET_KD_REQ: IOCTL_NWC24_EXEC_RESUME_SCHEDULAR - NI");
		Memory::Write_U32(0, BufferOut); // no error
		break;

	case IOCTL_NWC24_STARTUP_SOCKET: // NWC24iStartupSocket
		Memory::Write_U32(0, BufferOut);
		Memory::Write_U32(0, BufferOut+4);
		ReturnValue = 0;
		INFO_LOG(WII_IPC_WC24, "NET_KD_REQ: IOCTL_NWC24_STARTUP_SOCKET - NI");
		break;

	case IOCTL_NWC24_CLEANUP_SOCKET:
		Memory::Memset(BufferOut, 0, BufferOutSize);
		INFO_LOG(WII_IPC_WC24, "NET_KD_REQ: IOCTL_NWC24_CLEANUP_SOCKET - NI");
		break;

	case IOCTL_NWC24_LOCK_SOCKET: // WiiMenu
		INFO_LOG(WII_IPC_WC24, "NET_KD_REQ: IOCTL_NWC24_LOCK_SOCKET - NI");
		break;

	case IOCTL_NWC24_UNLOCK_SOCKET:
		INFO_LOG(WII_IPC_WC24, "NET_KD_REQ: IOCTL_NWC24_UNLOCK_SOCKET - NI");
		break;

	case IOCTL_NWC24_REQUEST_REGISTER_USER_ID:
		INFO_LOG(WII_IPC_WC24, "NET_KD_REQ: IOCTL_NWC24_REQUEST_REGISTER_USER_ID");
		Memory::Write_U32(0, BufferOut);
		Memory::Write_U32(0, BufferOut+4);
		break;

	case IOCTL_NWC24_REQUEST_GENERATED_USER_ID: // (Input: none, Output: 32 bytes)
		INFO_LOG(WII_IPC_WC24, "NET_KD_REQ: IOCTL_NWC24_REQUEST_GENERATED_USER_ID");
		if (config.CreationStage() == nwc24_config_t::NWC24_IDCS_INITIAL)
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
				u8 area_code = GetAreaCode(area);
				u8 id_ctr = config.IdGen();
				u8 hardware_model = GetHardwareModel(model);

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
				Memory::Write_U32(WC24_ERR_FATAL, BufferOut);
			}

		}
		else if (config.CreationStage() == nwc24_config_t::NWC24_IDCS_GENERATED)
		{
			Memory::Write_U32(WC24_ERR_ID_GENERATED, BufferOut);
		}
		else if (config.CreationStage() == nwc24_config_t::NWC24_IDCS_REGISTERED)
		{
			Memory::Write_U32(WC24_ERR_ID_REGISTERED, BufferOut);
		}
		Memory::Write_U64(config.Id(), BufferOut + 4);
		Memory::Write_U32(config.CreationStage(), BufferOut + 0xC);
		break;

	case IOCTL_NWC24_GET_SCHEDULAR_STAT:
		INFO_LOG(WII_IPC_WC24, "NET_KD_REQ: IOCTL_NWC24_GET_SCHEDULAR_STAT - NI");
		break;

	case IOCTL_NWC24_SAVE_MAIL_NOW:
		INFO_LOG(WII_IPC_WC24, "NET_KD_REQ: IOCTL_NWC24_SAVE_MAIL_NOW - NI");
		break;

	case IOCTL_NWC24_REQUEST_SHUTDOWN:
		// if ya set the IOS version to a very high value this happens ...
		INFO_LOG(WII_IPC_WC24, "NET_KD_REQ: IOCTL_NWC24_REQUEST_SHUTDOWN - NI");
		break;

	default:
		INFO_LOG(WII_IPC_WC24, "/dev/net/kd/request::IOCtl request 0x%x (BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
			Parameter, BufferIn, BufferInSize, BufferOut, BufferOutSize);
		break;
	}

	Memory::Write_U32(ReturnValue, _CommandAddress + 4);
	return true;
}


u8 CWII_IPC_HLE_Device_net_kd_request::GetAreaCode(const std::string& area)
{
	static std::map<const std::string, u8> regions = {
		{ "JPN", 0 }, { "USA", 1 }, { "EUR", 2 },
		{ "AUS", 2 }, { "BRA", 1 }, { "TWN", 3 },
		{ "ROC", 3 }, { "KOR", 4 }, { "HKG", 5 },
		{ "ASI", 5 }, { "LTN", 1 }, { "SAF", 2 },
		{ "CHN", 6 },
	};

	auto entryPos = regions.find(area);
	if (entryPos != regions.end())
		return entryPos->second;
	else
		return 7; // Unknown
}

u8 CWII_IPC_HLE_Device_net_kd_request::GetHardwareModel(const std::string& model)
{
	static std::map<const std::string, u8> models = {
		{ "RVL", MODEL_RVL },
		{ "RVT", MODEL_RVT },
		{ "RVV", MODEL_RVV },
		{ "RVD", MODEL_RVD },
	};

	auto entryPos = models.find(model);
	if (entryPos != models.end())
		return entryPos->second;
	else
		return MODEL_ELSE;
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
		return WC24_ERR_FATAL;

	return WC24_OK;
}

static void SaveMacAddress(u8* mac)
{
	SConfig::GetInstance().m_WirelessMac = MacAddressToString(mac);
	SConfig::GetInstance().SaveSettings();
}

static void GetMacAddress(u8* mac)
{
	// Parse MAC address from config, and generate a new one if it doesn't
	// exist or can't be parsed.
	std::string wireless_mac = SConfig::GetInstance().m_WirelessMac;
	if (!StringToMacAddress(wireless_mac, mac))
	{
		GenerateMacAddress(IOS, mac);
		SaveMacAddress(mac);
		if (!wireless_mac.empty())
		{
			ERROR_LOG(WII_IPC_NET, "The MAC provided (%s) is invalid. We have "\
			                       "generated another one for you.",
			                       MacAddressToString(mac).c_str());
		}
	}
	INFO_LOG(WII_IPC_NET, "Using MAC address: %s", MacAddressToString(mac).c_str());
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
		Memory::Write_U32(netcfg_connection_t::LINK_WIRED, CommandBuffer.PayloadBuffer.at(0).m_Address + 4);
		break;

	case IOCTLV_NCD_GETWIRELESSMACADDRESS:
		INFO_LOG(WII_IPC_NET, "NET_NCD_MANAGE: IOCTLV_NCD_GETWIRELESSMACADDRESS");

		u8 address[MAC_ADDRESS_SIZE];
		GetMacAddress(address);
		Memory::CopyToEmu(CommandBuffer.PayloadBuffer.at(1).m_Address, address, sizeof(address));
		break;

	default:
		INFO_LOG(WII_IPC_NET, "NET_NCD_MANAGE IOCtlV: %#x", CommandBuffer.Parameter);
		break;
	}

	Memory::Write_U32(common_result, CommandBuffer.PayloadBuffer.at(common_vector).m_Address);
	if (common_vector == 1)
	{
		Memory::Write_U32(common_result, CommandBuffer.PayloadBuffer.at(common_vector).m_Address + 4);
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
bool CWII_IPC_HLE_Device_net_wd_command::IOCtlV(u32 CommandAddress)
{
	u32 return_value = 0;

	SIOCtlVBuffer CommandBuffer(CommandAddress);

	switch (CommandBuffer.Parameter)
	{
	case IOCTLV_WD_SCAN:
		{
		// Gives parameters detailing type of scan and what to match
		// XXX - unused
		// ScanInfo *scan = (ScanInfo *)Memory::GetPointer(CommandBuffer.InBuffer.at(0).m_Address);

		u16* results = (u16*)Memory::GetPointer(CommandBuffer.PayloadBuffer.at(0).m_Address);
		// first u16 indicates number of BSSInfo following
		results[0] = Common::swap16(1);

		BSSInfo* bss = (BSSInfo*)&results[1];
		memset(bss, 0, sizeof(BSSInfo));

		bss->length = Common::swap16(sizeof(BSSInfo));
		bss->rssi = Common::swap16(0xffff);

		for (int i = 0; i < BSSID_SIZE; ++i)
			bss->bssid[i] = i;

		const char* ssid = "dolphin-emu";
		strcpy((char*)bss->ssid, ssid);
		bss->ssid_length = Common::swap16((u16)strlen(ssid));

		bss->channel = Common::swap16(2);
		}
		break;

	case IOCTLV_WD_GET_INFO:
		{
		Info* info = (Info*)Memory::GetPointer(CommandBuffer.PayloadBuffer.at(0).m_Address);
		memset(info, 0, sizeof(Info));
		// Probably used to disallow certain channels?
		memcpy(info->country, "US", 2);
		info->ntr_allowed_channels = Common::swap16(0xfffe);

		u8 address[MAC_ADDRESS_SIZE];
		GetMacAddress(address);
		memcpy(info->mac, address, sizeof(info->mac));
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
		INFO_LOG(WII_IPC_NET, "NET_WD_COMMAND IOCtlV %#x in %i out %i",
			CommandBuffer.Parameter, CommandBuffer.NumberInBuffer, CommandBuffer.NumberPayloadBuffer);
		for (u32 i = 0; i < CommandBuffer.NumberInBuffer; ++i)
		{
			INFO_LOG(WII_IPC_NET, "in %i addr %x size %i", i,
				CommandBuffer.InBuffer.at(i).m_Address, CommandBuffer.InBuffer.at(i).m_Size);
			INFO_LOG(WII_IPC_NET, "%s",
				ArrayToString(
					Memory::GetPointer(CommandBuffer.InBuffer.at(i).m_Address),
					CommandBuffer.InBuffer.at(i).m_Size).c_str()
				);
		}
		for (u32 i = 0; i < CommandBuffer.NumberPayloadBuffer; ++i)
		{
			INFO_LOG(WII_IPC_NET, "out %i addr %x size %i", i,
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

static int inet_pton(const char* src, unsigned char* dst)
{
	int saw_digit, octets;
	char ch;
	unsigned char tmp[4], *tp;

	saw_digit = 0;
	octets = 0;
	*(tp = tmp) = 0;
	while ((ch = *src++) != '\0')
	{
		if (ch >= '0' && ch <= '9')
		{
			unsigned int newt = (*tp * 10) + (ch - '0');

			if (newt > 255)
				return 0;
			*tp = newt;
			if (!saw_digit)
			{
				if (++octets > 4)
					return 0;
				saw_digit = 1;
			}
		}
		else if (ch == '.' && saw_digit)
		{
			if (octets == 4)
				return 0;
			*++tp = 0;
			saw_digit = 0;
		}
		else
		{
			return 0;
		}
	}
	if (octets < 4)
		return 0;
	memcpy(dst, tmp, 4);
	return 1;
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

bool CWII_IPC_HLE_Device_net_ip_top::IOCtl(u32 _CommandAddress)
{
	u32 Command       = Memory::Read_U32(_CommandAddress + 0x0C);
	u32 BufferIn      = Memory::Read_U32(_CommandAddress + 0x10);
	u32 BufferInSize  = Memory::Read_U32(_CommandAddress + 0x14);
	u32 BufferOut     = Memory::Read_U32(_CommandAddress + 0x18);
	u32 BufferOutSize = Memory::Read_U32(_CommandAddress + 0x1C);

	u32 ReturnValue = 0;
	switch (Command)
	{
	case IOCTL_SO_STARTUP:
	{
		INFO_LOG(WII_IPC_NET, "IOCTL_SO_STARTUP "
			"BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
			BufferIn, BufferInSize, BufferOut, BufferOutSize);
		break;
	}
	case IOCTL_SO_SOCKET:
	{
		u32 af   = Memory::Read_U32(BufferIn);
		u32 type = Memory::Read_U32(BufferIn + 0x04);
		u32 prot = Memory::Read_U32(BufferIn + 0x08);

		WiiSockMan &sm = WiiSockMan::GetInstance();
		ReturnValue = sm.NewSocket(af, type, prot);
		INFO_LOG(WII_IPC_NET, "IOCTL_SO_SOCKET "
			"Socket: %08x (%d,%d,%d), BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
			ReturnValue, af, type, prot, BufferIn, BufferInSize, BufferOut, BufferOutSize);
		break;
	}
	case IOCTL_SO_ICMPSOCKET:
	{
		u32 pf = Memory::Read_U32(BufferIn);

		WiiSockMan &sm = WiiSockMan::GetInstance();
		ReturnValue = sm.NewSocket(pf, SOCK_RAW, IPPROTO_ICMP);
		INFO_LOG(WII_IPC_NET, "IOCTL_SO_ICMPSOCKET(%x) %d", pf, ReturnValue);
		break;
	}
	case IOCTL_SO_CLOSE:
	case IOCTL_SO_ICMPCLOSE:
	{
		u32 fd = Memory::Read_U32(BufferIn);
		WiiSockMan &sm = WiiSockMan::GetInstance();
		ReturnValue = sm.DeleteSocket(fd);
		DEBUG_LOG(WII_IPC_NET, "%s(%x) %x",
			Command == IOCTL_SO_ICMPCLOSE ? "IOCTL_SO_ICMPCLOSE" : "IOCTL_SO_CLOSE",
			fd, ReturnValue);
		break;
	}
	case IOCTL_SO_ACCEPT:
	case IOCTL_SO_BIND:
	case IOCTL_SO_CONNECT:
	case IOCTL_SO_FCNTL:
	{
		u32 fd = Memory::Read_U32(BufferIn);
		WiiSockMan &sm = WiiSockMan::GetInstance();
		sm.DoSock(fd, _CommandAddress, (NET_IOCTL)Command);
		return false;
	}
	/////////////////////////////////////////////////////////////
	//                  TODO: Tidy all below                   //
	/////////////////////////////////////////////////////////////
	case IOCTL_SO_SHUTDOWN:
	{
		INFO_LOG(WII_IPC_NET, "IOCTL_SO_SHUTDOWN "
			"BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
			BufferIn, BufferInSize, BufferOut, BufferOutSize);

		u32 fd = Memory::Read_U32(BufferIn);
		u32 how = Memory::Read_U32(BufferIn+4);
		int ret = shutdown(fd, how);
		ReturnValue = WiiSockMan::GetNetErrorCode(ret, "SO_SHUTDOWN", false);
		break;
	}
	case IOCTL_SO_LISTEN:
	{

		u32 fd = Memory::Read_U32(BufferIn);
		u32 BACKLOG = Memory::Read_U32(BufferIn + 0x04);
		u32 ret = listen(fd, BACKLOG);
		ReturnValue = WiiSockMan::GetNetErrorCode(ret, "SO_LISTEN", false);
		INFO_LOG(WII_IPC_NET, "IOCTL_SO_LISTEN = %d "
			"BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
			ReturnValue, BufferIn, BufferInSize, BufferOut, BufferOutSize);
		break;
	}
	case IOCTL_SO_GETSOCKOPT:
	{
		u32 fd = Memory::Read_U32(BufferOut);
		u32 level = Memory::Read_U32(BufferOut + 4);
		u32 optname = Memory::Read_U32(BufferOut + 8);

		INFO_LOG(WII_IPC_NET,"IOCTL_SO_GETSOCKOPT(%08x, %08x, %08x)"
			"BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
			fd, level, optname,
			BufferIn, BufferInSize, BufferOut, BufferOutSize);

		// Do the level/optname translation
		int nat_level = -1, nat_optname = -1;

		for (auto& map : opt_level_mapping)
			if (level == map[1])
				nat_level = map[0];

		for (auto& map : opt_name_mapping)
			if (optname == map[1])
				nat_optname = map[0];

		u8 optval[20];
		u32 optlen = 4;

		int ret = getsockopt (fd, nat_level, nat_optname, (char*) &optval, (socklen_t*)&optlen);
		ReturnValue = WiiSockMan::GetNetErrorCode(ret, "SO_GETSOCKOPT", false);


		Memory::Write_U32(optlen, BufferOut + 0xC);
		Memory::CopyToEmu(BufferOut + 0x10, optval, optlen);

		if (optname == SO_ERROR)
		{
			s32 last_error = WiiSockMan::GetInstance().GetLastNetError();

			Memory::Write_U32(sizeof(s32), BufferOut + 0xC);
			Memory::Write_U32(last_error, BufferOut + 0x10);
		}
		break;
	}

	case IOCTL_SO_SETSOCKOPT:
	{
		u32 fd = Memory::Read_U32(BufferIn);
		u32 level = Memory::Read_U32(BufferIn + 4);
		u32 optname = Memory::Read_U32(BufferIn + 8);
		u32 optlen = Memory::Read_U32(BufferIn + 0xc);
		u8 optval[20];
		optlen = std::min(optlen, (u32)sizeof(optval));
		Memory::CopyFromEmu(optval, BufferIn + 0x10, optlen);

		INFO_LOG(WII_IPC_NET, "IOCTL_SO_SETSOCKOPT(%08x, %08x, %08x, %08x) "
			"BufferIn: (%08x, %i), BufferOut: (%08x, %i)"
			"%02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx %02hhx",
			fd, level, optname, optlen, BufferIn, BufferInSize, BufferOut, BufferOutSize, optval[0], optval[1], optval[2], optval[3],
			optval[4], optval[5],optval[6], optval[7], optval[8], optval[9], optval[10], optval[11], optval[12], optval[13], optval[14],
			optval[15], optval[16], optval[17], optval[18], optval[19]);

		//TODO: bug booto about this, 0x2005 most likely timeout related, default value on wii is , 0x2001 is most likely tcpnodelay
		if (level == 6 && (optname == 0x2005 || optname == 0x2001))
		{
			ReturnValue = 0;
			break;
		}

		// Do the level/optname translation
		int nat_level = -1, nat_optname = -1;

		for (auto& map : opt_level_mapping)
			if (level == map[1])
				nat_level = map[0];

		for (auto& map : opt_name_mapping)
			if (optname == map[1])
				nat_optname = map[0];

		if (nat_level == -1 || nat_optname == -1)
		{
			INFO_LOG(WII_IPC_NET, "SO_SETSOCKOPT: unknown level %d or optname %d", level, optname);

			// Default to the given level/optname. They match on Windows...
			nat_level = level;
			nat_optname = optname;
		}

		int ret = setsockopt(fd, nat_level, nat_optname, (char*)optval, optlen);
		ReturnValue = WiiSockMan::GetNetErrorCode(ret, "SO_SETSOCKOPT", false);
		break;
	}
	case IOCTL_SO_GETSOCKNAME:
	{
		u32 fd = Memory::Read_U32(BufferIn);

		INFO_LOG(WII_IPC_NET, "IOCTL_SO_GETSOCKNAME "
			"Socket: %08X, BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
			fd, BufferIn, BufferInSize, BufferOut, BufferOutSize);

		sockaddr sa;
		socklen_t sa_len;
		sa_len = sizeof(sa);
		int ret = getsockname(fd, &sa, &sa_len);

		Memory::Write_U8(BufferOutSize, BufferOut);
		Memory::Write_U8(sa.sa_family & 0xFF, BufferOut + 1);
		Memory::CopyToEmu(BufferOut + 2, &sa.sa_data, BufferOutSize - 2);
		ReturnValue = ret;
		break;
	}
	case IOCTL_SO_GETPEERNAME:
	{
		u32 fd = Memory::Read_U32(BufferIn);

		sockaddr sa;
		socklen_t sa_len;
		sa_len = sizeof(sa);

		int ret = getpeername(fd, &sa, &sa_len);

		Memory::Write_U8(BufferOutSize, BufferOut);
		Memory::Write_U8(AF_INET, BufferOut + 1);
		Memory::CopyToEmu(BufferOut + 2, &sa.sa_data, BufferOutSize - 2);

		INFO_LOG(WII_IPC_NET, "IOCTL_SO_GETPEERNAME(%x)", fd);

		ReturnValue = ret;
		break;
	}

	case IOCTL_SO_GETHOSTID:
	{
		INFO_LOG(WII_IPC_NET, "IOCTL_SO_GETHOSTID "
			"(BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
			BufferIn, BufferInSize, BufferOut, BufferOutSize);

#ifdef _WIN32
		DWORD forwardTableSize, ipTableSize, result;
		DWORD ifIndex = -1;
		std::unique_ptr<MIB_IPFORWARDTABLE> forwardTable;
		std::unique_ptr<MIB_IPADDRTABLE> ipTable;

		forwardTableSize = 0;
		if (GetIpForwardTable(nullptr, &forwardTableSize, FALSE) == ERROR_INSUFFICIENT_BUFFER)
		{
			forwardTable = std::unique_ptr<MIB_IPFORWARDTABLE>((PMIB_IPFORWARDTABLE)operator new(forwardTableSize));
		}

		ipTableSize = 0;
		if (GetIpAddrTable(nullptr, &ipTableSize, FALSE) == ERROR_INSUFFICIENT_BUFFER)
		{
			ipTable = std::unique_ptr<MIB_IPADDRTABLE>((PMIB_IPADDRTABLE)operator new(ipTableSize));
		}

		// find the interface IP used for the default route and use that
		result = GetIpForwardTable(forwardTable.get(), &forwardTableSize, FALSE);
		while (result == NO_ERROR || result == ERROR_MORE_DATA) // can return ERROR_MORE_DATA on XP even after the first call
		{
			for (DWORD i = 0; i < forwardTable->dwNumEntries; ++i)
			{
				if (forwardTable->table[i].dwForwardDest == 0)
				{
					ifIndex = forwardTable->table[i].dwForwardIfIndex;
					break;
				}
			}

			if (result == NO_ERROR || ifIndex != -1)
				break;

			result = GetIpForwardTable(forwardTable.get(), &forwardTableSize, FALSE);
		}

		if (ifIndex != -1 && GetIpAddrTable(ipTable.get(), &ipTableSize, FALSE) == NO_ERROR)
		{
			for (DWORD i = 0; i < ipTable->dwNumEntries; ++i)
			{
				if (ipTable->table[i].dwIndex == ifIndex)
				{
					ReturnValue = Common::swap32(ipTable->table[i].dwAddr);
					break;
				}
			}
		}
#endif

		// default placeholder, in case of failure
		if (ReturnValue == 0)
			ReturnValue = 192 << 24 | 168 << 16 | 1 << 8 | 150;
		break;
	}

	case IOCTL_SO_INETATON:
	{
		struct hostent* remoteHost = gethostbyname((char*)Memory::GetPointer(BufferIn));

		Memory::Write_U32(Common::swap32(*(u32*)remoteHost->h_addr_list[0]), BufferOut);
		INFO_LOG(WII_IPC_NET, "IOCTL_SO_INETATON = %d "
			"%s, BufferIn: (%08x, %i), BufferOut: (%08x, %i), IP Found: %08X",remoteHost->h_addr_list[0] == nullptr ? -1 : 0,
			(char*)Memory::GetPointer(BufferIn), BufferIn, BufferInSize, BufferOut, BufferOutSize, Common::swap32(*(u32*)remoteHost->h_addr_list[0]));
		ReturnValue = remoteHost->h_addr_list[0] == nullptr ? 0 : 1;
		break;
	}

	case IOCTL_SO_INETPTON:
	{
		INFO_LOG(WII_IPC_NET, "IOCTL_SO_INETPTON "
			"(Translating: %s)", Memory::GetPointer(BufferIn));
		ReturnValue = inet_pton((char*)Memory::GetPointer(BufferIn), Memory::GetPointer(BufferOut+4));
		break;
	}

	case IOCTL_SO_INETNTOP:
	{
		//u32 af = Memory::Read_U32(BufferIn);
		//u32 validAddress = Memory::Read_U32(BufferIn + 4);
		//u32 src = Memory::Read_U32(BufferIn + 8);
		char ip_s[16];
		sprintf(ip_s, "%i.%i.%i.%i",
			Memory::Read_U8(BufferIn + 8),
			Memory::Read_U8(BufferIn + 8 + 1),
			Memory::Read_U8(BufferIn + 8 + 2),
			Memory::Read_U8(BufferIn + 8 + 3)
			);
		INFO_LOG(WII_IPC_NET, "IOCTL_SO_INETNTOP %s", ip_s);
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
				ERROR_LOG(WII_IPC_NET, "Hidden POLL");

			pollfd_t* ufds = (pollfd_t*)malloc(sizeof(pollfd_t) * nfds);
			if (ufds == nullptr)
			{
				ReturnValue = -1;
				break;
			}

			for (int i = 0; i < nfds; ++i)
			{
				ufds[i].fd = Memory::Read_U32(BufferOut + 0xc*i);          //fd
				int events = Memory::Read_U32(BufferOut + 0xc*i + 4);      //events
				ufds[i].revents = Memory::Read_U32(BufferOut + 0xc*i + 8); //revents

				// Translate Wii to native events
				int unhandled_events = events;
				ufds[i].events = 0;
				for (auto& map : mapping)
				{
					if (events & map[1])
						ufds[i].events |= map[0];
					unhandled_events &= ~map[1];
				}

				DEBUG_LOG(WII_IPC_NET, "IOCTL_SO_POLL(%d) "
					"Sock: %08x, Unknown: %08x, Events: %08x, "
					"NativeEvents: %08x",
					i, ufds[i].fd, unknown, events, ufds[i].events
				);

				if (unhandled_events)
					ERROR_LOG(WII_IPC_NET, "SO_POLL: unhandled Wii event types: %04x", unhandled_events);
			}

			int ret = poll(ufds, nfds, timeout);
			ret = WiiSockMan::GetNetErrorCode(ret, "SO_POLL", false);

			for (int i = 0; i < nfds; ++i)
			{

				// Translate native to Wii events
				int revents = 0;
				for (auto& map : mapping)
				{
					if (ufds[i].revents & map[0])
						revents |= map[1];
				}

				// No need to change fd or events as they are input only.
				//Memory::Write_U32(ufds[i].fd, BufferOut + 0xc*i); //fd
				//Memory::Write_U32(events, BufferOut + 0xc*i + 4); //events
				Memory::Write_U32(revents, BufferOut + 0xc*i + 8);  //revents

				DEBUG_LOG(WII_IPC_NET, "IOCTL_SO_POLL socket %d revents %08X events %08X", i, revents, ufds[i].events);
			}
			free(ufds);

			ReturnValue = ret;
			break;
		}

	case IOCTL_SO_GETHOSTBYNAME:
		{
			hostent* remoteHost = gethostbyname((char*)Memory::GetPointer(BufferIn));

			INFO_LOG(WII_IPC_NET, "IOCTL_SO_GETHOSTBYNAME "
				"Address: %s, BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
				(char*)Memory::GetPointer(BufferIn), BufferIn, BufferInSize, BufferOut, BufferOutSize);

			if (remoteHost)
			{
				for (int i = 0; remoteHost->h_aliases[i]; ++i)
				{
					INFO_LOG(WII_IPC_NET, "alias%i:%s", i, remoteHost->h_aliases[i]);
				}

				for (int i = 0; remoteHost->h_addr_list[i]; ++i)
				{
					u32 ip = Common::swap32(*(u32*)(remoteHost->h_addr_list[i]));
					std::string ip_s = StringFromFormat("%i.%i.%i.%i", ip >> 24, (ip >> 16) & 0xff, (ip >> 8) & 0xff, ip & 0xff);
					DEBUG_LOG(WII_IPC_NET, "addr%i:%s", i, ip_s.c_str());
				}

				Memory::Memset(BufferOut, 0, BufferOutSize);
				u32 wii_addr = BufferOut + 4 * 3 + 2 * 2;

				u32 name_length = (u32)strlen(remoteHost->h_name) + 1;
				Memory::CopyToEmu(wii_addr, remoteHost->h_name, name_length);
				Memory::Write_U32(wii_addr, BufferOut);
				wii_addr += (name_length + 4) & ~3;

				// aliases - empty
				Memory::Write_U32(wii_addr, BufferOut + 4);
				Memory::Write_U32(wii_addr + sizeof(u32), wii_addr);
				wii_addr += sizeof(u32);
				Memory::Write_U32(0, wii_addr);
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
				// null-terminated list
				Memory::Write_U32(0, wii_addr);
				wii_addr += sizeof(u32);
				// The actual IPs
				for (int i = 0; remoteHost->h_addr_list[i]; ++i)
				{
					Memory::Write_U32_Swap(*(u32*)(remoteHost->h_addr_list[i]), wii_addr);
					wii_addr += sizeof(u32);
				}

				//ERROR_LOG(WII_IPC_NET, "\n%s",
				// ArrayToString(Memory::GetPointer(BufferOut), BufferOutSize, 16).c_str());
				ReturnValue = 0;
			}
			else
			{
				ReturnValue = -1;
			}

			break;
		}

	case IOCTL_SO_ICMPCANCEL:
		ERROR_LOG(WII_IPC_NET, "IOCTL_SO_ICMPCANCEL");
		goto default_;

	default:
		INFO_LOG(WII_IPC_NET,"0x%x "
			"BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
			Command, BufferIn, BufferInSize, BufferOut, BufferOutSize);
	default_:
		if (BufferInSize)
		{
			ERROR_LOG(WII_IPC_NET, "in addr %x size %x", BufferIn, BufferInSize);
			ERROR_LOG(WII_IPC_NET, "\n%s", ArrayToString(Memory::GetPointer(BufferIn), BufferInSize, 4).c_str());
		}

		if (BufferOutSize)
		{
			ERROR_LOG(WII_IPC_NET, "out addr %x size %x", BufferOut, BufferOutSize);
		}
		break;
	}

	Memory::Write_U32(ReturnValue, _CommandAddress + 0x4);

	return true;
}


bool CWII_IPC_HLE_Device_net_ip_top::IOCtlV(u32 CommandAddress)
{
	SIOCtlVBuffer CommandBuffer(CommandAddress);

	s32 ReturnValue = 0;


	u32 _BufferIn = 0, _BufferIn2 = 0, _BufferIn3 = 0;
	u32 BufferInSize = 0, BufferInSize2 = 0, BufferInSize3 = 0;

	u32 _BufferOut = 0, _BufferOut2 = 0;
	u32 BufferOutSize = 0;

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
	}

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

		INFO_LOG(WII_IPC_NET,"IOCTLV_SO_GETINTERFACEOPT(%08X, %08X, %X, %X, %X) "
			"BufferIn: (%08x, %i), BufferIn2: (%08x, %i) ",
			param, param2, param3, param4, param5,
			_BufferIn, BufferInSize, _BufferIn2, BufferInSize2);

		switch (param2)
		{
		case 0xb003: // dns server table
		{
			u32 address = 0;
#ifdef _WIN32
			PIP_ADAPTER_ADDRESSES AdapterAddresses = nullptr;
			ULONG OutBufferLength = 0;
			ULONG RetVal = 0, i;
			for (i = 0; i < 5; ++i)
			{
				RetVal = GetAdaptersAddresses(
					AF_INET,
					0,
					nullptr,
					AdapterAddresses,
					&OutBufferLength);

				if (RetVal != ERROR_BUFFER_OVERFLOW)
				{
					break;
				}

				if (AdapterAddresses != nullptr)
				{
					FREE(AdapterAddresses);
				}

				AdapterAddresses = (PIP_ADAPTER_ADDRESSES)MALLOC(OutBufferLength);
				if (AdapterAddresses == nullptr)
				{
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
			if (AdapterAddresses != nullptr)
			{
				FREE(AdapterAddresses);
			}
#endif
			if (address == 0)
				address = 0x08080808;

			Memory::Write_U32(address, _BufferOut);
			Memory::Write_U32(0x08080404, _BufferOut+4);
			break;
		}
		case 0x1003: // error
			Memory::Write_U32(0, _BufferOut);
			break;

		case 0x1004: // mac address
			u8 address[MAC_ADDRESS_SIZE];
			GetMacAddress(address);
			Memory::CopyToEmu(_BufferOut, address, sizeof(address));
			break;

		case 0x1005: // link state
			Memory::Write_U32(1, _BufferOut);
			break;

		case 0x4002: // ip addr number
			Memory::Write_U32(1, _BufferOut);
			break;

		case 0x4003: // ip addr table
			Memory::Write_U32(0xC, _BufferOut2);
			Memory::Write_U32(10 << 24 | 1 << 8 | 30, _BufferOut);
			Memory::Write_U32(255 << 24 | 255 << 16  | 255 << 8 | 0, _BufferOut+4);
			Memory::Write_U32(10 << 24 | 0 << 16  | 255 << 8 | 255, _BufferOut+8);
			break;

		default:
			ERROR_LOG(WII_IPC_NET, "Unknown param2: %08X", param2);
			break;
		}
		break;
	}
	case IOCTLV_SO_SENDTO:
	{
		u32 fd = Memory::Read_U32(_BufferIn2);
		WiiSockMan &sm = WiiSockMan::GetInstance();
		sm.DoSock(fd, CommandAddress, IOCTLV_SO_SENDTO);
		return false;
		break;
	}
	case IOCTLV_SO_RECVFROM:
	{
		u32 fd = Memory::Read_U32(_BufferIn);
		WiiSockMan &sm = WiiSockMan::GetInstance();
		sm.DoSock(fd, CommandAddress, IOCTLV_SO_RECVFROM);
		return false;
		break;
	}
	case IOCTLV_SO_GETADDRINFO:
	{
		struct addrinfo hints;
		struct addrinfo* result = nullptr;

		if (BufferInSize3)
		{
			hints.ai_flags     = Memory::Read_U32(_BufferIn3);
			hints.ai_family    = Memory::Read_U32(_BufferIn3 + 0x4);
			hints.ai_socktype  = Memory::Read_U32(_BufferIn3 + 0x8);
			hints.ai_protocol  = Memory::Read_U32(_BufferIn3 + 0xC);
			hints.ai_addrlen   = Memory::Read_U32(_BufferIn3 + 0x10);
			hints.ai_canonname = nullptr;
			hints.ai_addr      = nullptr;
			hints.ai_next      = nullptr;
		}

		char* pNodeName = nullptr;
		if (BufferInSize > 0)
			pNodeName = (char*)Memory::GetPointer(_BufferIn);

		char* pServiceName = nullptr;
		if (BufferInSize2 > 0)
			pServiceName = (char*)Memory::GetPointer(_BufferIn2);

		int ret = getaddrinfo(pNodeName, pServiceName, BufferInSize3 ? &hints : nullptr, &result);
		u32 addr = _BufferOut;
		u32 sockoffset = addr + 0x460;
		if (ret == 0)
		{
			while (result != nullptr)
			{
				Memory::Write_U32(result->ai_flags, addr);
				Memory::Write_U32(result->ai_family, addr + 0x04);
				Memory::Write_U32(result->ai_socktype, addr + 0x08);
				Memory::Write_U32(result->ai_protocol, addr + 0x0C);
				Memory::Write_U32((u32)result->ai_addrlen, addr + 0x10);
				// what to do? where to put? the buffer of 0x834 doesn't allow space for this
				Memory::Write_U32(/*result->ai_cannonname*/ 0, addr + 0x14);

				if (result->ai_addr)
				{
					Memory::Write_U32(sockoffset, addr + 0x18);
					Memory::Write_U16(((result->ai_addr->sa_family & 0xFF) << 8) | (result->ai_addrlen & 0xFF), sockoffset);
					Memory::CopyToEmu(sockoffset + 0x2, result->ai_addr->sa_data, sizeof(result->ai_addr->sa_data));
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
		else
		{
			// Host not found
			ret = -305;
		}

		INFO_LOG(WII_IPC_NET, "IOCTLV_SO_GETADDRINFO "
			"(BufferIn: (%08x, %i), BufferOut: (%08x, %i)",
			_BufferIn, BufferInSize, _BufferOut, BufferOutSize);
		INFO_LOG(WII_IPC_NET, "IOCTLV_SO_GETADDRINFO: %s", Memory::GetPointer(_BufferIn));
		ReturnValue = ret;
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

		u32 fd = Memory::Read_U32(_BufferIn);
		u32 num_ip = Memory::Read_U32(_BufferIn + 4);
		u64 timeout = Memory::Read_U64(_BufferIn + 8);

		if (num_ip != 1)
		{
			INFO_LOG(WII_IPC_NET, "IOCTLV_SO_ICMPPING %i IPs", num_ip);
		}

		ip_info.length      = Memory::Read_U8(_BufferIn + 16);
		ip_info.addr_family = Memory::Read_U8(_BufferIn + 17);
		ip_info.icmp_id     = Memory::Read_U16(_BufferIn + 18);
		ip_info.ip          = Memory::Read_U32(_BufferIn + 20);

		if (ip_info.length != 8 || ip_info.addr_family != AF_INET)
		{
			INFO_LOG(WII_IPC_NET, "IOCTLV_SO_ICMPPING strange IPInfo:\n"
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
			((u16*)data)[0] = Common::swap16(ip_info.icmp_id);
			icmp_length = 22;
		}

		int ret = icmp_echo_req(fd, &addr, data, icmp_length);
		if (ret == icmp_length)
		{
			ret = icmp_echo_rep(fd, &addr, (u32)timeout, icmp_length);
		}

		// TODO proper error codes
		ReturnValue = 0;
		break;
	}
	default:
		INFO_LOG(WII_IPC_NET,"0x%x (BufferIn: (%08x, %i), BufferIn2: (%08x, %i)",
			CommandBuffer.Parameter, _BufferIn, BufferInSize, _BufferIn2, BufferInSize2);
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
u32 CWII_IPC_HLE_Device_net_ip_top::Update()
{
	WiiSockMan::GetInstance().Update();
	return 0;
}
