// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/FileUtil.h"
#include "Common/Timer.h"

#include "Core/IPC_HLE/WII_IPC_HLE_Device.h"

#ifdef _WIN32
#include <ws2tcpip.h>
#endif

// data layout of the network configuration file (/shared2/sys/net/02/config.dat)
// needed for /dev/net/ncd/manage
#pragma pack(push, 1)
struct netcfg_proxy_t
{
	u8 use_proxy;
	u8 use_proxy_userandpass;
	u8 padding_1[2];
	u8 proxy_name[255];
	u8 padding_2;
	u16 proxy_port;        // 0-34463
	u8 proxy_username[32];
	u8 padding_3;
	u8 proxy_password[32];
};

struct netcfg_connection_t
{
	enum
	{
		WIRED_IF            =   1, // 0: wifi 1: wired
		DNS_DHCP            =   2, // 0: manual 1: DHCP
		IP_DHCP             =   4, // 0: manual 1: DHCP
		USE_PROXY           =  16,
		CONNECTION_TEST_OK  =  32,
		CONNECTION_SELECTED = 128
	};

	enum
	{
		OPEN     = 0,
		WEP64    = 1,
		WEP128   = 2,
		WPA_TKIP = 4,
		WPA2_AES = 5,
		WPA_AES  = 6
	};

	enum status
	{
		LINK_BUSY      = 1,
		LINK_NONE      = 2,
		LINK_WIRED     = 3,
		LINK_WIFI_DOWN = 4,
		LINK_WIFI_UP   = 5
	};

	enum
	{
		PERM_NONE      = 0,
		PERM_SEND_MAIL = 1,
		PERM_RECV_MAIL = 2,
		PERM_DOWNLOAD  = 4,
		PERM_ALL       = PERM_SEND_MAIL | PERM_RECV_MAIL | PERM_DOWNLOAD
	};

	// settings common to both wired and wireless connections
	u8 flags;
	u8 padding_1[3];
	u8 ip[4];
	u8 netmask[4];
	u8 gateway[4];
	u8 dns1[4];
	u8 dns2[4];
	u8 padding_2[2];
	u16 mtu;
	u8 padding_3[8];
	netcfg_proxy_t proxy_settings;
	u8 padding_4;
	netcfg_proxy_t proxy_settings_copy;
	u8 padding_5[1297];

	// wireless specific settings
	u8 ssid[32];
	u8 padding_6;
	u8 ssid_length;     // length of ssid in bytes.
	u8 padding_7[3];
	u8 encryption;
	u8 padding_8[3];
	u8 key_length;      // length of key in bytes.  0x00 for WEP64 and WEP128.
	u8 unknown;         // 0x00 or 0x01 toggled with a WPA-PSK (TKIP) and with a WEP entered with hex instead of ascii.
	u8 padding_9;
	u8 key[64];         // encryption key; for WEP, key is stored 4 times (20 bytes for WEP64 and 52 bytes for WEP128) then padded with 0x00
	u8 padding_10[236];
};

struct network_config_t
{
	enum
	{
		IF_NONE,
		IF_WIFI,
		IF_WIRED
	};

	u32 version;
	u8 connType;
	u8 linkTimeout;
	u8 nwc24Permission;
	u8 padding;

	netcfg_connection_t connection[3];
};

enum nwc24_err_t
{
	WC24_OK                    =   0,
	WC24_ERR_FATAL             =  -1,
	WC24_ERR_ID_NONEXISTANCE   = -34,
	WC24_ERR_ID_GENERATED      = -35,
	WC24_ERR_ID_REGISTERED     = -36,
	WC24_ERR_ID_NOT_REGISTERED = -44,
};

struct nwc24_config_t
{
	enum
	{
		NWC24_IDCS_INITIAL    = 0,
		NWC24_IDCS_GENERATED  = 1,
		NWC24_IDCS_REGISTERED = 2
	};

	enum
	{
		URL_COUNT           = 0x05,
		MAX_URL_LENGTH      = 0x80,
		MAX_EMAIL_LENGTH    = 0x40,
		MAX_PASSWORD_LENGTH = 0x20,
	};

	u32 magic;      /* 'WcCf' 0x57634366 */
	u32 _unk_04;    /* must be 8 */
	u64 nwc24_id;
	u32 id_generation;
	u32 creation_stage; /* 0==not_generated;1==generated;2==registered; */
	char email[MAX_EMAIL_LENGTH];
	char paswd[MAX_PASSWORD_LENGTH];
	char mlchkid[0x24];
	char http_urls[URL_COUNT][MAX_URL_LENGTH];
	u8 reserved[0xDC];
	u32 enable_booting;
	u32 checksum;
};

#pragma pack(pop)



class NWC24Config
{
private:
	std::string path;
	nwc24_config_t config;

public:
	NWC24Config()
	{
		path = File::GetUserPath(D_WIIWC24_IDX) + "nwc24msg.cfg";
		ReadConfig();
	}

	void ResetConfig()
	{
		if (File::Exists(path))
			File::Delete(path);

		const char* urls[5] = {
			"https://amw.wc24.wii.com/cgi-bin/account.cgi",
			"http://rcw.wc24.wii.com/cgi-bin/check.cgi",
			"http://mtw.wc24.wii.com/cgi-bin/receive.cgi",
			"http://mtw.wc24.wii.com/cgi-bin/delete.cgi",
			"http://mtw.wc24.wii.com/cgi-bin/send.cgi",
		};

		memset(&config, 0, sizeof(config));

		SetMagic(0x57634366);
		SetUnk(8);
		SetCreationStage(nwc24_config_t::NWC24_IDCS_INITIAL);
		SetEnableBooting(0);
		SetEmail("@wii.com");

		for (int i = 0; i < nwc24_config_t::URL_COUNT; ++i)
		{
			strncpy(config.http_urls[i], urls[i], nwc24_config_t::MAX_URL_LENGTH);
		}

		SetChecksum(CalculateNwc24ConfigChecksum());

		WriteConfig();
	}

	void WriteConfig()
	{
		if (!File::Exists(path))
		{
			if (!File::CreateFullPath(File::GetUserPath(D_WIIWC24_IDX)))
			{
				ERROR_LOG(WII_IPC_WC24, "Failed to create directory for WC24");
			}
		}

		File::IOFile(path, "wb").WriteBytes((void*)&config, sizeof(config));
	}


	void ReadConfig()
	{
		if (File::Exists(path))
		{
			if (!File::IOFile(path, "rb").ReadBytes((void*)&config, sizeof(config)))
				ResetConfig();
			else
			{
				s32 config_error = CheckNwc24Config();
				if (config_error)
					ERROR_LOG(WII_IPC_WC24, "There is an error in the config for for WC24: %d", config_error);
			}
		}
		else
		{
			ResetConfig();
		}
	}

	u32 CalculateNwc24ConfigChecksum()
	{
		u32* ptr = (u32*)&config;
		u32 sum = 0;
		for (int i = 0; i < 0xFF; ++i)
		{
			sum += Common::swap32(*ptr++);
		}
		return sum;
	}

	s32 CheckNwc24Config()
	{
		if (Magic() != 0x57634366) /* 'WcCf' magic */
		{
			ERROR_LOG(WII_IPC_WC24, "Magic mismatch");
			return -14;
		}
		u32 checksum = CalculateNwc24ConfigChecksum();
		DEBUG_LOG(WII_IPC_WC24, "Checksum: %X", checksum);
		if (Checksum() != checksum)
		{
			ERROR_LOG(WII_IPC_WC24, "Checksum mismatch expected %X and got %X", checksum, Checksum());
			return -14;
		}
		if (IdGen() > 0x1F)
		{
			ERROR_LOG(WII_IPC_WC24, "Id gen error");
			return -14;
		}
		if (Unk() != 8)
			return -27;

		return 0;
	}

	u32 Magic() {return Common::swap32(config.magic);}
	void SetMagic(u32 magic) {config.magic = Common::swap32(magic);}

	u32 Unk() {return Common::swap32(config._unk_04);}
	void SetUnk(u32 _unk_04) {config._unk_04 = Common::swap32(_unk_04);}

	u32 IdGen() {return Common::swap32(config.id_generation);}
	void SetIdGen(u32 id_generation) {config.id_generation = Common::swap32(id_generation);}

	void IncrementIdGen()
	{
		u32 id_ctr = IdGen();
		id_ctr++;
		id_ctr &= 0x1F;
		SetIdGen(id_ctr);
	}

	u32 Checksum() {return Common::swap32(config.checksum);}
	void SetChecksum(u32 checksum) {config.checksum = Common::swap32(checksum);}

	u32 CreationStage() {return Common::swap32(config.creation_stage);}
	void SetCreationStage(u32 creation_stage) {config.creation_stage = Common::swap32(creation_stage);}

	u32 EnableBooting() {return Common::swap32(config.enable_booting);}
	void SetEnableBooting(u32 enable_booting) {config.enable_booting = Common::swap32(enable_booting);}

	u64 Id() {return Common::swap64(config.nwc24_id);}
	void SetId(u64 nwc24_id) {config.nwc24_id = Common::swap64(nwc24_id);}

	const char* Email() {return config.email;}
	void SetEmail(const char* email)
	{
		strncpy(config.email, email, nwc24_config_t::MAX_EMAIL_LENGTH);
		config.email[nwc24_config_t::MAX_EMAIL_LENGTH-1] = '\0';
	}

};

class WiiNetConfig
{
	std::string path;
	network_config_t config;

public:
	WiiNetConfig()
	{
		path = File::GetUserPath(D_WIISYSCONF_IDX) + "net/02/config.dat";
		ReadConfig();
	}

	void ResetConfig()
	{
		if (File::Exists(path))
			File::Delete(path);

		memset(&config, 0, sizeof(config));
		config.connType = network_config_t::IF_WIRED;
		config.connection[0].flags =
			netcfg_connection_t::WIRED_IF |
			netcfg_connection_t::DNS_DHCP |
			netcfg_connection_t::IP_DHCP |
			netcfg_connection_t::CONNECTION_TEST_OK |
			netcfg_connection_t::CONNECTION_SELECTED;

		WriteConfig();
	}

	void WriteConfig()
	{
		if (!File::Exists(path))
		{
			if (!File::CreateFullPath(std::string(File::GetUserPath(D_WIISYSCONF_IDX) + "net/02/")))
			{
				ERROR_LOG(WII_IPC_NET, "Failed to create directory for network config file");
			}
		}

		File::IOFile(path, "wb").WriteBytes((void*)&config, sizeof(config));
	}

	void WriteToMem(const u32 address)
	{
		Memory::CopyToEmu(address, &config, sizeof(config));
	}

	void ReadFromMem(const u32 address)
	{
		Memory::CopyFromEmu(&config, address, sizeof(config));
	}

	void ReadConfig()
	{
		if (File::Exists(path))
		{
			if (!File::IOFile(path, "rb").ReadBytes((void*)&config, sizeof(config)))
				ResetConfig();
		}
		else
		{
			ResetConfig();
		}
	}
};


//////////////////////////////////////////////////////////////////////////
// KD is the IOS module responsible for implementing WiiConnect24 functionality.
// It can perform HTTPS downloads, send and receive mail via SMTP, and execute a
// JavaScript-like language while the Wii is in standby mode.
class CWII_IPC_HLE_Device_net_kd_request : public IWII_IPC_HLE_Device
{
public:
	CWII_IPC_HLE_Device_net_kd_request(u32 _DeviceID, const std::string& _rDeviceName);

	virtual ~CWII_IPC_HLE_Device_net_kd_request();

	virtual bool Open(u32 _CommandAddress, u32 _Mode) override;
	virtual bool Close(u32 _CommandAddress, bool _bForce) override;
	virtual bool IOCtl(u32 _CommandAddress) override;

private:
	enum
	{
		IOCTL_NWC24_SUSPEND_SCHEDULAR               = 0x01,
		IOCTL_NWC24_EXEC_TRY_SUSPEND_SCHEDULAR      = 0x02,
		IOCTL_NWC24_EXEC_RESUME_SCHEDULAR           = 0x03,
		IOCTL_NWC24_KD_GET_TIME_TRIGGERS            = 0x04,
		IOCTL_NWC24_SET_SCHEDULE_SPAN               = 0x05,
		IOCTL_NWC24_STARTUP_SOCKET                  = 0x06,
		IOCTL_NWC24_CLEANUP_SOCKET                  = 0x07,
		IOCTL_NWC24_LOCK_SOCKET                     = 0x08,
		IOCTL_NWC24_UNLOCK_SOCKET                   = 0x09,
		IOCTL_NWC24_CHECK_MAIL_NOW                  = 0x0A,
		IOCTL_NWC24_SEND_MAIL_NOW                   = 0x0B,
		IOCTL_NWC24_RECEIVE_MAIL_NOW                = 0x0C,
		IOCTL_NWC24_SAVE_MAIL_NOW                   = 0x0D,
		IOCTL_NWC24_DOWNLOAD_NOW_EX                 = 0x0E,
		IOCTL_NWC24_REQUEST_GENERATED_USER_ID       = 0x0F,
		IOCTL_NWC24_REQUEST_REGISTER_USER_ID        = 0x10,
		IOCTL_NWC24_GET_SCHEDULAR_STAT              = 0x1E,
		IOCTL_NWC24_SET_FILTER_MODE                 = 0x1F,
		IOCTL_NWC24_SET_DEBUG_MODE                  = 0x20,
		IOCTL_NWC24_KD_SET_NEXT_WAKEUP              = 0x21,
		IOCTL_NWC24_SET_SCRIPT_MODE                 = 0x22,
		IOCTL_NWC24_REQUEST_SHUTDOWN                = 0x28,
	};

	enum
	{
		MODEL_RVT = 0,
		MODEL_RVV = 0,
		MODEL_RVL = 1,
		MODEL_RVD = 2,
		MODEL_ELSE = 7
	};

	u8 GetAreaCode(const std::string& area);
	u8 GetHardwareModel(const std::string& model);

	s32 NWC24MakeUserID(u64* nwc24_id, u32 hollywood_id, u16 id_ctr, u8 hardware_model, u8 area_code);

	NWC24Config config;
};


//////////////////////////////////////////////////////////////////////////
class CWII_IPC_HLE_Device_net_kd_time : public IWII_IPC_HLE_Device
{
public:
	CWII_IPC_HLE_Device_net_kd_time(u32 _DeviceID, const std::string& _rDeviceName)
		: IWII_IPC_HLE_Device(_DeviceID, _rDeviceName)
		, rtc()
		, utcdiff()
	{
	}

	virtual ~CWII_IPC_HLE_Device_net_kd_time()
	{}

	virtual bool Open(u32 _CommandAddress, u32 _Mode) override
	{
		INFO_LOG(WII_IPC_NET, "NET_KD_TIME: Open");
		Memory::Write_U32(GetDeviceID(), _CommandAddress+4);
		return true;
	}

	virtual bool Close(u32 _CommandAddress, bool _bForce) override
	{
		INFO_LOG(WII_IPC_NET, "NET_KD_TIME: Close");
		if (!_bForce)
			Memory::Write_U32(0, _CommandAddress + 4);
		return true;
	}

	virtual bool IOCtl(u32 _CommandAddress) override
	{
		u32 Parameter = Memory::Read_U32(_CommandAddress + 0x0C);
		u32 BufferIn  = Memory::Read_U32(_CommandAddress + 0x10);
		u32 BufferOut = Memory::Read_U32(_CommandAddress + 0x18);

		u32 result = 0;
		u32 common_result = 0;
		// TODO Writes stuff to /shared2/nwc24/misc.bin
		//u32 update_misc = 0;

		switch (Parameter)
		{
		case IOCTL_NW24_GET_UNIVERSAL_TIME:
			Memory::Write_U64(GetAdjustedUTC(), BufferOut + 4);
			break;

		case IOCTL_NW24_SET_UNIVERSAL_TIME:
			SetAdjustedUTC(Memory::Read_U64(BufferIn));
			//update_misc = Memory::Read_U32(BufferIn + 8);
			break;

		case IOCTL_NW24_SET_RTC_COUNTER:
			rtc = Memory::Read_U32(BufferIn);
			//update_misc = Memory::Read_U32(BufferIn + 4);
			break;

		case IOCTL_NW24_GET_TIME_DIFF:
			Memory::Write_U64(GetAdjustedUTC() - rtc, BufferOut + 4);
			break;

		case IOCTL_NW24_UNIMPLEMENTED:
			result = -9;
			break;

		default:
			ERROR_LOG(WII_IPC_NET, "%s - unknown IOCtl: %x\n", GetDeviceName().c_str(), Parameter);
			break;
		}

		// write return values
		Memory::Write_U32(common_result, BufferOut);
		Memory::Write_U32(result, _CommandAddress + 4);
		return true;
	}

private:
	enum
	{
		IOCTL_NW24_GET_UNIVERSAL_TIME = 0x14,
		IOCTL_NW24_SET_UNIVERSAL_TIME = 0x15,
		IOCTL_NW24_UNIMPLEMENTED      = 0x16,
		IOCTL_NW24_SET_RTC_COUNTER    = 0x17,
		IOCTL_NW24_GET_TIME_DIFF      = 0x18,
	};

	u64 rtc;
	s64 utcdiff;

	// Seconds between 1.1.1970 and 4.1.2008 16:00:38
	static const u64 wii_bias = 0x477E5826;

	// Returns seconds since wii epoch
	// +/- any bias set from IOCTL_NW24_SET_UNIVERSAL_TIME
	u64 GetAdjustedUTC() const
	{
		return Common::Timer::GetTimeSinceJan1970() - wii_bias + utcdiff;
	}

	// Store the difference between what the wii thinks is UTC and
	// what the host OS thinks
	void SetAdjustedUTC(u64 wii_utc)
	{
		utcdiff = Common::Timer::GetTimeSinceJan1970() - wii_bias - wii_utc;
	}
};

enum NET_IOCTL
{
	IOCTL_SO_ACCEPT = 1,
	IOCTL_SO_BIND,
	IOCTL_SO_CLOSE,
	IOCTL_SO_CONNECT,
	IOCTL_SO_FCNTL,
	IOCTL_SO_GETPEERNAME,
	IOCTL_SO_GETSOCKNAME,
	IOCTL_SO_GETSOCKOPT,
	IOCTL_SO_SETSOCKOPT,
	IOCTL_SO_LISTEN,
	IOCTL_SO_POLL,
	IOCTLV_SO_RECVFROM,
	IOCTLV_SO_SENDTO,
	IOCTL_SO_SHUTDOWN,
	IOCTL_SO_SOCKET,
	IOCTL_SO_GETHOSTID,
	IOCTL_SO_GETHOSTBYNAME,
	IOCTL_SO_GETHOSTBYADDR,
	IOCTLV_SO_GETNAMEINFO,
	IOCTL_SO_UNK14,
	IOCTL_SO_INETATON,
	IOCTL_SO_INETPTON,
	IOCTL_SO_INETNTOP,
	IOCTLV_SO_GETADDRINFO,
	IOCTL_SO_SOCKATMARK,
	IOCTLV_SO_UNK1A,
	IOCTLV_SO_UNK1B,
	IOCTLV_SO_GETINTERFACEOPT,
	IOCTLV_SO_SETINTERFACEOPT,
	IOCTL_SO_SETINTERFACE,
	IOCTL_SO_STARTUP,
	IOCTL_SO_ICMPSOCKET = 0x30,
	IOCTLV_SO_ICMPPING,
	IOCTL_SO_ICMPCANCEL,
	IOCTL_SO_ICMPCLOSE
};

//////////////////////////////////////////////////////////////////////////
class CWII_IPC_HLE_Device_net_ip_top : public IWII_IPC_HLE_Device
{
public:
	CWII_IPC_HLE_Device_net_ip_top(u32 _DeviceID, const std::string& _rDeviceName);

	virtual ~CWII_IPC_HLE_Device_net_ip_top();

	virtual bool Open(u32 _CommandAddress, u32 _Mode) override;
	virtual bool Close(u32 _CommandAddress, bool _bForce) override;
	virtual bool IOCtl(u32 _CommandAddress) override;
	virtual bool IOCtlV(u32 _CommandAddress) override;

	virtual u32 Update() override;

private:
#ifdef _WIN32
	WSADATA InitData;
#endif
	u32 ExecuteCommand(u32 _Parameter, u32 _BufferIn, u32 _BufferInSize, u32 _BufferOut, u32 _BufferOutSize);
	u32 ExecuteCommandV(SIOCtlVBuffer& CommandBuffer);
};

// **********************************************************************************
// Interface for reading and changing network configuration (probably some other stuff as well)
class CWII_IPC_HLE_Device_net_ncd_manage : public IWII_IPC_HLE_Device
{
public:
	CWII_IPC_HLE_Device_net_ncd_manage(u32 _DeviceID, const std::string& _rDeviceName);

	virtual ~CWII_IPC_HLE_Device_net_ncd_manage();

	virtual bool Open(u32 _CommandAddress, u32 _Mode) override;
	virtual bool Close(u32 _CommandAddress, bool _bForce) override;
	virtual bool IOCtlV(u32 _CommandAddress) override;

private:
	enum
	{
		IOCTLV_NCD_LOCKWIRELESSDRIVER    = 0x1,  // NCDLockWirelessDriver
		IOCTLV_NCD_UNLOCKWIRELESSDRIVER  = 0x2,  // NCDUnlockWirelessDriver
		IOCTLV_NCD_GETCONFIG             = 0x3,  // NCDiGetConfig
		IOCTLV_NCD_SETCONFIG             = 0x4,  // NCDiSetConfig
		IOCTLV_NCD_READCONFIG            = 0x5,
		IOCTLV_NCD_WRITECONFIG           = 0x6,
		IOCTLV_NCD_GETLINKSTATUS         = 0x7,  // NCDGetLinkStatus
		IOCTLV_NCD_GETWIRELESSMACADDRESS = 0x8,  // NCDGetWirelessMacAddress
	};

	WiiNetConfig config;
};

//////////////////////////////////////////////////////////////////////////
class CWII_IPC_HLE_Device_net_wd_command : public IWII_IPC_HLE_Device
{
public:
	CWII_IPC_HLE_Device_net_wd_command(u32 DeviceID, const std::string& DeviceName);

	virtual ~CWII_IPC_HLE_Device_net_wd_command();

	virtual bool Open(u32 CommandAddress, u32 Mode) override;
	virtual bool Close(u32 CommandAddress, bool Force) override;
	virtual bool IOCtlV(u32 CommandAddress) override;

private:
	enum
	{
		IOCTLV_WD_GET_MODE          = 0x1001, // WD_GetMode
		IOCTLV_WD_SET_LINKSTATE     = 0x1002, // WD_SetLinkState
		IOCTLV_WD_GET_LINKSTATE     = 0x1003, // WD_GetLinkState
		IOCTLV_WD_SET_CONFIG        = 0x1004, // WD_SetConfig
		IOCTLV_WD_GET_CONFIG        = 0x1005, // WD_GetConfig
		IOCTLV_WD_CHANGE_BEACON     = 0x1006, // WD_ChangeBeacon
		IOCTLV_WD_DISASSOC          = 0x1007, // WD_DisAssoc
		IOCTLV_WD_MP_SEND_FRAME     = 0x1008, // WD_MpSendFrame
		IOCTLV_WD_SEND_FRAME        = 0x1009, // WD_SendFrame
		IOCTLV_WD_SCAN              = 0x100a, // WD_Scan
		IOCTLV_WD_CALL_WL           = 0x100c, // WD_CallWL
		IOCTLV_WD_MEASURE_CHANNEL   = 0x100b, // WD_MeasureChannel
		IOCTLV_WD_GET_LASTERROR     = 0x100d, // WD_GetLastError
		IOCTLV_WD_GET_INFO          = 0x100e, // WD_GetInfo
		IOCTLV_WD_CHANGE_GAMEINFO   = 0x100f, // WD_ChangeGameInfo
		IOCTLV_WD_CHANGE_VTSF       = 0x1010, // WD_ChangeVTSF
		IOCTLV_WD_RECV_FRAME        = 0x8000, // WD_ReceiveFrame
		IOCTLV_WD_RECV_NOTIFICATION = 0x8001  // WD_ReceiveNotification
	};

	enum
	{
		BSSID_SIZE = 6,
		SSID_SIZE = 32
	};

	enum
	{
		SCAN_ACTIVE,
		SCAN_PASSIVE
	};

#pragma pack(push, 1)
	struct ScanInfo
	{
		u16 channel_bitmap;
		u16 max_channel_time;
		u8 bssid[BSSID_SIZE];
		u16 scan_type;
		u16 ssid_length;
		u8 ssid[SSID_SIZE];
		u8 ssid_match_mask[SSID_SIZE];
	};

	struct BSSInfo
	{
		u16 length;
		u16 rssi;
		u8 bssid[BSSID_SIZE];
		u16 ssid_length;
		u8 ssid[SSID_SIZE];
		u16 capabilities;
		struct rate
		{
			u16 basic;
			u16 support;
		};
		u16 beacon_period;
		u16 DTIM_period;
		u16 channel;
		u16 CF_period;
		u16 CF_max_duration;
		u16 element_info_length;
		u16 element_info[1];
	};

	struct Info
	{
		u8 mac[6];
		u16 ntr_allowed_channels;
		u16 unk8;
		char country[2];
		u32 unkc;
		char wlversion[0x50];
		u8 unk[0x30];
	};
#pragma pack(pop)
};
