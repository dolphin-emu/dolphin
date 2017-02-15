// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/CommonTypes.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/IPC.h"
#include "Core/IOS/Network/Config.h"

#ifdef _WIN32
#include <ws2tcpip.h>
#endif

namespace IOS
{
namespace HLE
{
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

// TODO: split this up.
namespace Device
{
class NetIPTop : public Device
{
public:
  NetIPTop(u32 device_id, const std::string& device_name);

  virtual ~NetIPTop();

  IPCCommandResult IOCtl(const IOCtlRequest& request) override;
  IPCCommandResult IOCtlV(const IOCtlVRequest& request) override;

  void Update() override;

private:
#ifdef _WIN32
  WSADATA InitData;
#endif
};

// **********************************************************************************
// Interface for reading and changing network configuration (probably some other stuff as well)
class NetNCDManage : public Device
{
public:
  NetNCDManage(u32 device_id, const std::string& device_name);

  IPCCommandResult IOCtlV(const IOCtlVRequest& request) override;

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
class NetWDCommand : public Device
{
public:
  NetWDCommand(u32 device_id, const std::string& device_name);

  IPCCommandResult IOCtlV(const IOCtlVRequest& request) override;

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
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
