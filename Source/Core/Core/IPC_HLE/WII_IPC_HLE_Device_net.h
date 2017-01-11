// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Core/HW/EXI_DeviceIPL.h"
#include "Core/HW/Memmap.h"
#include "Core/IPC_HLE/NWC24Config.h"
#include "Core/IPC_HLE/WII_IPC_HLE.h"
#include "Core/IPC_HLE/WII_IPC_HLE_Device.h"
#include "Core/IPC_HLE/WiiNetConfig.h"

#ifdef _WIN32
#include <ws2tcpip.h>
#endif

//////////////////////////////////////////////////////////////////////////
// KD is the IOS module responsible for implementing WiiConnect24 functionality.
// It can perform HTTPS downloads, send and receive mail via SMTP, and execute a
// JavaScript-like language while the Wii is in standby mode.
class CWII_IPC_HLE_Device_net_kd_request : public IWII_IPC_HLE_Device
{
public:
  CWII_IPC_HLE_Device_net_kd_request(u32 _DeviceID, const std::string& _rDeviceName);

  virtual ~CWII_IPC_HLE_Device_net_kd_request();

  IPCCommandResult IOCtl(u32 _CommandAddress) override;

private:
  enum
  {
    IOCTL_NWC24_SUSPEND_SCHEDULAR = 0x01,
    IOCTL_NWC24_EXEC_TRY_SUSPEND_SCHEDULAR = 0x02,
    IOCTL_NWC24_EXEC_RESUME_SCHEDULAR = 0x03,
    IOCTL_NWC24_KD_GET_TIME_TRIGGERS = 0x04,
    IOCTL_NWC24_SET_SCHEDULE_SPAN = 0x05,
    IOCTL_NWC24_STARTUP_SOCKET = 0x06,
    IOCTL_NWC24_CLEANUP_SOCKET = 0x07,
    IOCTL_NWC24_LOCK_SOCKET = 0x08,
    IOCTL_NWC24_UNLOCK_SOCKET = 0x09,
    IOCTL_NWC24_CHECK_MAIL_NOW = 0x0A,
    IOCTL_NWC24_SEND_MAIL_NOW = 0x0B,
    IOCTL_NWC24_RECEIVE_MAIL_NOW = 0x0C,
    IOCTL_NWC24_SAVE_MAIL_NOW = 0x0D,
    IOCTL_NWC24_DOWNLOAD_NOW_EX = 0x0E,
    IOCTL_NWC24_REQUEST_GENERATED_USER_ID = 0x0F,
    IOCTL_NWC24_REQUEST_REGISTER_USER_ID = 0x10,
    IOCTL_NWC24_GET_SCHEDULAR_STAT = 0x1E,
    IOCTL_NWC24_SET_FILTER_MODE = 0x1F,
    IOCTL_NWC24_SET_DEBUG_MODE = 0x20,
    IOCTL_NWC24_KD_SET_NEXT_WAKEUP = 0x21,
    IOCTL_NWC24_SET_SCRIPT_MODE = 0x22,
    IOCTL_NWC24_REQUEST_SHUTDOWN = 0x28,
  };

  enum
  {
    MODEL_RVT = 0,
    MODEL_RVV = 0,
    MODEL_RVL = 1,
    MODEL_RVD = 2,
    MODEL_ELSE = 7
  };

  u8 GetAreaCode(const std::string& area) const;
  u8 GetHardwareModel(const std::string& model) const;

  s32 NWC24MakeUserID(u64* nwc24_id, u32 hollywood_id, u16 id_ctr, u8 hardware_model, u8 area_code);

  NWC24::NWC24Config config;
};

//////////////////////////////////////////////////////////////////////////
class CWII_IPC_HLE_Device_net_kd_time : public IWII_IPC_HLE_Device
{
public:
  CWII_IPC_HLE_Device_net_kd_time(u32 _DeviceID, const std::string& _rDeviceName)
      : IWII_IPC_HLE_Device(_DeviceID, _rDeviceName), rtc(), utcdiff()
  {
  }

  virtual ~CWII_IPC_HLE_Device_net_kd_time() {}
  IPCCommandResult IOCtl(u32 _CommandAddress) override
  {
    u32 Parameter = Memory::Read_U32(_CommandAddress + 0x0C);
    u32 BufferIn = Memory::Read_U32(_CommandAddress + 0x10);
    u32 BufferOut = Memory::Read_U32(_CommandAddress + 0x18);

    u32 result = 0;
    u32 common_result = 0;
    // TODO Writes stuff to /shared2/nwc24/misc.bin
    // u32 update_misc = 0;

    switch (Parameter)
    {
    case IOCTL_NW24_GET_UNIVERSAL_TIME:
      Memory::Write_U64(GetAdjustedUTC(), BufferOut + 4);
      break;

    case IOCTL_NW24_SET_UNIVERSAL_TIME:
      SetAdjustedUTC(Memory::Read_U64(BufferIn));
      // update_misc = Memory::Read_U32(BufferIn + 8);
      break;

    case IOCTL_NW24_SET_RTC_COUNTER:
      rtc = Memory::Read_U32(BufferIn);
      // update_misc = Memory::Read_U32(BufferIn + 4);
      break;

    case IOCTL_NW24_GET_TIME_DIFF:
      Memory::Write_U64(GetAdjustedUTC() - rtc, BufferOut + 4);
      break;

    case IOCTL_NW24_UNIMPLEMENTED:
      result = -9;
      break;

    default:
      ERROR_LOG(WII_IPC_NET, "%s - unknown IOCtl: %x", GetDeviceName().c_str(), Parameter);
      break;
    }

    // write return values
    Memory::Write_U32(common_result, BufferOut);
    Memory::Write_U32(result, _CommandAddress + 4);
    return GetDefaultReply();
  }

private:
  enum
  {
    IOCTL_NW24_GET_UNIVERSAL_TIME = 0x14,
    IOCTL_NW24_SET_UNIVERSAL_TIME = 0x15,
    IOCTL_NW24_UNIMPLEMENTED = 0x16,
    IOCTL_NW24_SET_RTC_COUNTER = 0x17,
    IOCTL_NW24_GET_TIME_DIFF = 0x18,
  };

  u64 rtc;
  s64 utcdiff;

  // TODO: depending on CEXIIPL is a hack which I don't feel like
  // removing because the function itself is pretty hackish;
  // wait until I re-port my netplay rewrite

  // Returns seconds since Wii epoch
  // +/- any bias set from IOCTL_NW24_SET_UNIVERSAL_TIME
  u64 GetAdjustedUTC() const { return CEXIIPL::GetEmulatedTime(CEXIIPL::WII_EPOCH) + utcdiff; }
  // Store the difference between what the Wii thinks is UTC and
  // what the host OS thinks
  void SetAdjustedUTC(u64 wii_utc)
  {
    utcdiff = CEXIIPL::GetEmulatedTime(CEXIIPL::WII_EPOCH) - wii_utc;
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

  IPCCommandResult IOCtl(u32 _CommandAddress) override;
  IPCCommandResult IOCtlV(u32 _CommandAddress) override;

  void Update() override;

private:
#ifdef _WIN32
  WSADATA InitData;
#endif
};

// **********************************************************************************
// Interface for reading and changing network configuration (probably some other stuff as well)
class CWII_IPC_HLE_Device_net_ncd_manage : public IWII_IPC_HLE_Device
{
public:
  CWII_IPC_HLE_Device_net_ncd_manage(u32 _DeviceID, const std::string& _rDeviceName);

  virtual ~CWII_IPC_HLE_Device_net_ncd_manage();

  IPCCommandResult IOCtlV(u32 _CommandAddress) override;

private:
  enum
  {
    IOCTLV_NCD_LOCKWIRELESSDRIVER = 0x1,    // NCDLockWirelessDriver
    IOCTLV_NCD_UNLOCKWIRELESSDRIVER = 0x2,  // NCDUnlockWirelessDriver
    IOCTLV_NCD_GETCONFIG = 0x3,             // NCDiGetConfig
    IOCTLV_NCD_SETCONFIG = 0x4,             // NCDiSetConfig
    IOCTLV_NCD_READCONFIG = 0x5,
    IOCTLV_NCD_WRITECONFIG = 0x6,
    IOCTLV_NCD_GETLINKSTATUS = 0x7,          // NCDGetLinkStatus
    IOCTLV_NCD_GETWIRELESSMACADDRESS = 0x8,  // NCDGetWirelessMacAddress
  };

  Net::WiiNetConfig config;
};

//////////////////////////////////////////////////////////////////////////
class CWII_IPC_HLE_Device_net_wd_command : public IWII_IPC_HLE_Device
{
public:
  CWII_IPC_HLE_Device_net_wd_command(u32 DeviceID, const std::string& DeviceName);

  virtual ~CWII_IPC_HLE_Device_net_wd_command();

  IPCCommandResult IOCtlV(u32 CommandAddress) override;

private:
  enum
  {
    IOCTLV_WD_GET_MODE = 0x1001,          // WD_GetMode
    IOCTLV_WD_SET_LINKSTATE = 0x1002,     // WD_SetLinkState
    IOCTLV_WD_GET_LINKSTATE = 0x1003,     // WD_GetLinkState
    IOCTLV_WD_SET_CONFIG = 0x1004,        // WD_SetConfig
    IOCTLV_WD_GET_CONFIG = 0x1005,        // WD_GetConfig
    IOCTLV_WD_CHANGE_BEACON = 0x1006,     // WD_ChangeBeacon
    IOCTLV_WD_DISASSOC = 0x1007,          // WD_DisAssoc
    IOCTLV_WD_MP_SEND_FRAME = 0x1008,     // WD_MpSendFrame
    IOCTLV_WD_SEND_FRAME = 0x1009,        // WD_SendFrame
    IOCTLV_WD_SCAN = 0x100a,              // WD_Scan
    IOCTLV_WD_CALL_WL = 0x100c,           // WD_CallWL
    IOCTLV_WD_MEASURE_CHANNEL = 0x100b,   // WD_MeasureChannel
    IOCTLV_WD_GET_LASTERROR = 0x100d,     // WD_GetLastError
    IOCTLV_WD_GET_INFO = 0x100e,          // WD_GetInfo
    IOCTLV_WD_CHANGE_GAMEINFO = 0x100f,   // WD_ChangeGameInfo
    IOCTLV_WD_CHANGE_VTSF = 0x1010,       // WD_ChangeVTSF
    IOCTLV_WD_RECV_FRAME = 0x8000,        // WD_ReceiveFrame
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
