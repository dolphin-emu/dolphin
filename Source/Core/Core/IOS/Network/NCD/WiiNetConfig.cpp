// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/Network/NCD/WiiNetConfig.h"

#include <cstring>

#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Core/HW/Memmap.h"

namespace IOS
{
namespace HLE
{
namespace Net
{
WiiNetConfig::WiiNetConfig()
{
  m_path = File::GetUserPath(D_SESSION_WIIROOT_IDX) + "/" WII_SYSCONF_DIR "/net/02/config.dat";
  ReadConfig();
}

void WiiNetConfig::ReadConfig()
{
  if (!File::IOFile(m_path, "rb").ReadBytes(&m_data, sizeof(m_data)))
    ResetConfig();
}

void WiiNetConfig::WriteConfig() const
{
  if (!File::Exists(m_path))
  {
    if (!File::CreateFullPath(File::GetUserPath(D_SESSION_WIIROOT_IDX) + "/" WII_SYSCONF_DIR
                                                                         "/net/02/"))
    {
      ERROR_LOG(IOS_NET, "Failed to create directory for network config file");
    }
  }

  File::IOFile(m_path, "wb").WriteBytes(&m_data, sizeof(m_data));
}

void WiiNetConfig::ResetConfig()
{
  if (File::Exists(m_path))
    File::Delete(m_path);

  memset(&m_data, 0, sizeof(m_data));
  m_data.connType = ConfigData::IF_WIRED;
  m_data.connection[0].flags =
      ConnectionSettings::WIRED_IF | ConnectionSettings::DNS_DHCP | ConnectionSettings::IP_DHCP |
      ConnectionSettings::CONNECTION_TEST_OK | ConnectionSettings::CONNECTION_SELECTED;

  WriteConfig();
}

void WiiNetConfig::WriteToMem(const u32 address) const
{
  Memory::CopyToEmu(address, &m_data, sizeof(m_data));
}

void WiiNetConfig::ReadFromMem(const u32 address)
{
  Memory::CopyFromEmu(&m_data, address, sizeof(m_data));
}
}  // namespace Net
}  // namespace HLE
}  // namespace IOS
