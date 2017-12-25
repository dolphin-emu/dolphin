// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "Common/CommonTypes.h"

namespace IOS::HLE
{
namespace FS
{
class FileSystem;
}

namespace Net
{
#pragma pack(push, 1)
struct ConnectionSettings final
{
  struct ProxySettings final
  {
    u8 use_proxy;
    u8 use_proxy_userandpass;
    u8 padding_1[2];
    u8 proxy_name[255];
    u8 padding_2;
    u16 proxy_port;  // 0-34463
    u8 proxy_username[32];
    u8 padding_3;
    u8 proxy_password[32];
  };

  enum
  {
    WIRED_IF = 1,  // 0: WiFi 1: Wired
    DNS_DHCP = 2,  // 0: Manual 1: DHCP
    IP_DHCP = 4,   // 0: Manual 1: DHCP
    USE_PROXY = 16,
    CONNECTION_TEST_OK = 32,
    CONNECTION_SELECTED = 128
  };

  enum
  {
    OPEN = 0,
    WEP64 = 1,
    WEP128 = 2,
    WPA_TKIP = 4,
    WPA2_AES = 5,
    WPA_AES = 6
  };

  enum Status
  {
    LINK_BUSY = 1,
    LINK_NONE = 2,
    LINK_WIRED = 3,
    LINK_WIFI_DOWN = 4,
    LINK_WIFI_UP = 5
  };

  enum
  {
    PERM_NONE = 0,
    PERM_SEND_MAIL = 1,
    PERM_RECV_MAIL = 2,
    PERM_DOWNLOAD = 4,
    PERM_ALL = PERM_SEND_MAIL | PERM_RECV_MAIL | PERM_DOWNLOAD
  };

  // Settings common to both wired and wireless connections
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
  ProxySettings proxy_settings;
  u8 padding_4;
  ProxySettings proxy_settings_copy;
  u8 padding_5[1297];

  // Wireless specific settings
  u8 ssid[32];
  u8 padding_6;
  u8 ssid_length;
  u8 padding_7[3];  // Length of SSID in bytes.
  u8 encryption;
  u8 padding_8[3];
  u8 key_length;  // Length of key in bytes. 0x00 for WEP64 and WEP128.
  u8 unknown;     // 0x00 or 0x01 toggled with a WPA-PSK (TKIP) and with a WEP entered with hex
                  // instead of ASCII.
  u8 padding_9;
  u8 key[64];  // Encryption key; for WEP, key is stored 4 times (20 bytes for WEP64 and 52 bytes
               // for WEP128) then padded with 0x00
  u8 padding_10[236];
};
#pragma pack(pop)

class WiiNetConfig final
{
public:
  WiiNetConfig();

  void ReadConfig(FS::FileSystem* fs);
  void WriteConfig(FS::FileSystem* fs) const;
  void ResetConfig(FS::FileSystem* fs);

  void WriteToMem(u32 address) const;
  void ReadFromMem(u32 address);

private:
// Data layout of the network configuration file (/shared2/sys/net/02/config.dat)
// needed for /dev/net/ncd/manage
#pragma pack(push, 1)
  struct ConfigData final
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

    ConnectionSettings connection[3];
  };
#pragma pack(pop)

  ConfigData m_data;
};
}  // namespace Net
}  // namespace IOS::HLE
