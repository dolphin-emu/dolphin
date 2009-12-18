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

#ifndef _WII_IPC_HLE_DEVICE_NET_H_
#define _WII_IPC_HLE_DEVICE_NET_H_

#include "WII_IPC_HLE_Device.h"

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
    enum {
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
// Seems like just wireless stuff?
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
		IOCTL_NCD_UNK1				= 1, // NCDLockWirelessDriver
		IOCTL_NCD_UNK2				= 2, // NCDUnlockWirelessDriver
		IOCTL_NCD_SETIFCONFIG3		= 3,
		IOCTL_NCD_SETIFCONFIG4		= 4, // NCDGetWirelessMacAddress
		IOCTL_NCD_GETLINKSTATUS		= 7 // NCDGetLinkStatus
	};
};

#endif
