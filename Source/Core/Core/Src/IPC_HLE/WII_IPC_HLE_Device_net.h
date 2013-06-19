// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _WII_IPC_HLE_DEVICE_NET_H_
#define _WII_IPC_HLE_DEVICE_NET_H_

#include "WII_IPC_HLE_Device.h"

// data layout of the network configuration file (/shared2/sys/net/02/config.dat)
// needed for /dev/net/ncd/manage
#pragma pack(1)
typedef struct
{
	u8 use_proxy;             // 0x00 -> no proxy;  0x01 -> proxy
	u8 use_proxy_userandpass; // 0x00 -> don't use username and password;  0x01 -> use username and password
	u8 padding_1[2];
	u8 proxy_name[255];
	u8 padding_2;
	u16 proxy_port;           // 0-34463
	u8 proxy_username[32];
	u8 padding_3;
	u8 proxy_password[32];
} netcfg_proxy_t;

typedef struct
{
	// settings common to both wired and wireless connections
	u8 flags; //  Connection selected
	          //  | ?
	          //  | | Internet test passed
	          //  | | | Use Proxy (1 -> on; 0 -> off)
	          //  | | | |
	          //  1 0 1 0  0 1 1 0
	          //           | | | |
	          //           | | | Interface (0 -> Internal wireless; 1 -> Wired LAN adapter)
	          //           | | DNS source (0 -> Manual; 1 -> DHCP)
	          //           | IP source (0 -> Manual; 1 -> DHCP)
	          //           ?

	u8 padding_1[3];

	u8 ip[4];
	u8 netmask[4];
	u8 gateway[4];
	u8 dns1[4];
	u8 dns2[4];
	u8 padding_2[2];

	u16 mtu;         // 0 or 576-1500
	u8 padding_3[8];

	netcfg_proxy_t proxy_settings;
	u8 padding_4;

	netcfg_proxy_t proxy_settings_copy; // seems to be a duplicate of proxy_settings
	u8 padding_5[1297];

	// wireless specific settings
	u8 ssid[32];        // access Point name.

	u8 padding_6;
	u8 ssid_length;     // length of ssid in bytes.
	u8 padding_7[2];

	u8 padding_8;
	u8 encryption;      // (probably) encryption.  OPN: 0x00, WEP64: 0x01, WEP128: 0x02 WPA-PSK (TKIP): 0x04, WPA2-PSK (AES): 0x05, WPA-PSK (AES): 0x06
	u8 padding_9[2];

	u8 padding_10;
	u8 key_length;      // length of key in bytes.  0x00 for WEP64 and WEP128.
	u8 unknown;         // 0x00 or 0x01 toogled with a WPA-PSK (TKIP) and with a WEP entered with hex instead of ascii.
	u8 padding_11;

	u8 key[64];         // encryption key; for WEP, key is stored 4 times (20 bytes for WEP64 and 52 bytes for WEP128) then padded with 0x00
	u8 padding_12[236];
} netcfg_connection_t;

typedef struct
{
	u8 header0;    // 0x00
	u8 header1;    // 0x00
	u8 header2;    // 0x00
	u8 header3;    // 0x00
	u8 header4;    // 0x01 if there's at least one valid connection to the Internet.
	u8 header5;    // 0x00
	u8 header6;    // always 0x07?
	u8 header7;    // 0x00

	netcfg_connection_t connection[3];
} network_config_t;
#pragma pack()


// **********************************************************************************
// KD is the IOS module responsible for implementing WiiConnect24 functionality. It
// can perform HTTPS downloads, send and receive mail via SMTP, and execute a
// JavaScript-like language while the Wii is in standby mode.
class CWII_IPC_HLE_Device_net_kd_request : public IWII_IPC_HLE_Device
{
public:
    CWII_IPC_HLE_Device_net_kd_request(u32 _DeviceID, const std::string& _rDeviceName);

	virtual ~CWII_IPC_HLE_Device_net_kd_request();

	virtual bool Open(u32 _CommandAddress, u32 _Mode);
	virtual bool Close(u32 _CommandAddress, bool _bForce);
	virtual bool IOCtl(u32 _CommandAddress);

private:
	enum
	{
		IOCTL_NWC24_SUSPEND_SCHEDULAR               = 0x01,
		IOCTL_NWC24_EXEC_TRY_SUSPEND_SCHEDULAR      = 0x02,
		IOCTL_NWC24_UNK_3                           = 0x03,
		IOCTL_NWC24_UNK_4                           = 0x04,
		IOCTL_NWC24_UNK_5                           = 0x05,
		IOCTL_NWC24_STARTUP_SOCKET                  = 0x06,
		IOCTL_NWC24_CLEANUP_SOCKET                  = 0x07,
		IOCTL_NWC24_LOCK_SOCKET                     = 0x08,
		IOCTL_NWC24_UNLOCK_SOCKET                   = 0x09,
		IOCTL_NWC24_UNK_A                           = 0x0A,
		IOCTL_NWC24_UNK_B                           = 0x0B,
		IOCTL_NWC24_UNK_C                           = 0x0C,
		IOCTL_NWC24_SAVE_MAIL_NOW                   = 0x0D,
		IOCTL_NWC24_DOWNLOAD_NOW_EX                 = 0x0E,
		IOCTL_NWC24_REQUEST_GENERATED_USER_ID       = 0x0F,
		IOCTL_NWC24_REQUEST_REGISTER_USER_ID        = 0x10,
		IOCTL_NWC24_GET_SCHEDULAR_STAT              = 0x1E,
		IOCTL_NWC24_UNK_1F                          = 0x1F,
		IOCTL_NWC24_UNK_20                          = 0x20,
		IOCTL_NWC24_UNK_21                          = 0x21,
		IOCTL_NWC24_SET_SCRIPT_MODE                 = 0x22,
		IOCTL_NWC24_REQUEST_SHUTDOWN                = 0x28,
	};

	// Max size 32 Bytes
	std::string m_UserID;
};

// **********************************************************************************
class CWII_IPC_HLE_Device_net_kd_time : public IWII_IPC_HLE_Device
{
public:
	CWII_IPC_HLE_Device_net_kd_time(u32 _DeviceID, const std::string& _rDeviceName) :
	  IWII_IPC_HLE_Device(_DeviceID, _rDeviceName)
	  {}

	virtual ~CWII_IPC_HLE_Device_net_kd_time()
	{}

	virtual bool Open(u32 _CommandAddress, u32 _Mode)
	{
		INFO_LOG(WII_IPC_NET, "NET_KD_TIME: Open");
		Memory::Write_U32(GetDeviceID(), _CommandAddress+4);
		return true;
	}

	virtual bool Close(u32 _CommandAddress, bool _bForce)
	{
		INFO_LOG(WII_IPC_NET, "NET_KD_TIME: Close");
		if (!_bForce)
			Memory::Write_U32(0, _CommandAddress + 4);
		return true;
	}

	virtual bool IOCtl(u32 _CommandAddress) 
	{
		u32 Parameter		= Memory::Read_U32(_CommandAddress + 0x0C);
		u32 BufferIn		= Memory::Read_U32(_CommandAddress + 0x10);
		u32 BufferInSize	= Memory::Read_U32(_CommandAddress + 0x14);
		u32 BufferOut		= Memory::Read_U32(_CommandAddress + 0x18);
		u32 BufferOutSize	= Memory::Read_U32(_CommandAddress + 0x1C);		

		switch (Parameter)
		{
		case IOCTL_NW24_SET_RTC_COUNTER: // NWC24iSetRtcCounter (but prolly just the first 4 bytes are intresting...)
			_dbg_assert_msg_(WII_IPC_NET, BufferInSize==0x20, "NET_KD_TIME: Set RTC Counter BufferIn to small");
			_dbg_assert_msg_(WII_IPC_NET, BufferOutSize==0x20, "NET_KD_TIME: Set RTC Counter BufferOut to small");

			for (int i=0; i<0x20; i++)
			{
				m_RtcCounter[i] = Memory::Read_U8(BufferIn+i);
			}

			// send back for sync?? at least there is a out buffer...
			for (int i=0; i<0x20; i++)
			{
				Memory::Write_U8(m_RtcCounter[i], BufferOut+i);
			}

			INFO_LOG(WII_IPC_NET, "NET_KD_TIME: Set RTC Counter");

			Memory::Write_U32(0, _CommandAddress + 0x4);
			return true;

		case IOCTL_NW24_GET_TIME_DIFF: // Input: none, Output: 32
		default:
			ERROR_LOG(WII_IPC_NET, "%s - IOCtl:\n"
				"    Parameter: 0x%x   (0x17 NWC24iSetRtcCounter) \n"
				"    BufferIn: 0x%08x\n"
				"    BufferInSize: 0x%08x\n"
				"    BufferOut: 0x%08x\n"
				"    BufferOutSize: 0x%08x\n",
				GetDeviceName().c_str(), Parameter, BufferIn, BufferInSize, BufferOut, BufferOutSize);
			break;
		}

		// write return value
		Memory::Write_U32(0, _CommandAddress + 0x4);
		return true;
	}

private:
	enum
	{
		IOCTL_NW24_SET_RTC_COUNTER      = 0x17,
		IOCTL_NW24_GET_TIME_DIFF        = 0x18,
	};

	u8 m_RtcCounter[0x20];
};

// **********************************************************************************
class CWII_IPC_HLE_Device_net_ip_top : public IWII_IPC_HLE_Device
{
public:
	CWII_IPC_HLE_Device_net_ip_top(u32 _DeviceID, const std::string& _rDeviceName);

	virtual ~CWII_IPC_HLE_Device_net_ip_top();

	virtual bool Open(u32 _CommandAddress, u32 _Mode);
	virtual bool Close(u32 _CommandAddress, bool _bForce);
	virtual bool IOCtl(u32 _CommandAddress);
	virtual bool IOCtlV(u32 _CommandAddress);
	
private:
	enum
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

	u32 ExecuteCommand(u32 _Parameter, u32 _BufferIn, u32 _BufferInSize, u32 _BufferOut, u32 _BufferOutSize);
};

// **********************************************************************************
// Interface for reading and changing network configuration (probably some other stuff as well)
class CWII_IPC_HLE_Device_net_ncd_manage : public IWII_IPC_HLE_Device
{
public:
	CWII_IPC_HLE_Device_net_ncd_manage(u32 _DeviceID, const std::string& _rDeviceName);

	virtual ~CWII_IPC_HLE_Device_net_ncd_manage();

	virtual bool Open(u32 _CommandAddress, u32 _Mode);
	virtual bool Close(u32 _CommandAddress, bool _bForce);
	virtual bool IOCtlV(u32 _CommandAddress);

private:
	enum {
		IOCTLV_NCD_UNK1                  = 0x1,  // NCDLockWirelessDriver
		IOCTLV_NCD_UNK2                  = 0x2,  // NCDUnlockWirelessDriver
		IOCTLV_NCD_READCONFIG            = 0x3,  // NCDReadConfig?
		IOCTLV_NCD_UNK4                  = 0x4,
		IOCTLV_NCD_GETLINKSTATUS         = 0x7,  // NCDGetLinkStatus
		IOCTLV_NCD_GETWIRELESSMACADDRESS = 0x8,  // NCDGetWirelessMacAddress
	};

	network_config_t m_Ifconfig;
};

#endif
