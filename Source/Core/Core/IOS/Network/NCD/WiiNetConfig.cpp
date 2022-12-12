// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/Network/NCD/WiiNetConfig.h"

#include <cstring>

#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/FS/FileSystem.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/Uids.h"
#include "Core/System.h"

namespace IOS::HLE::Net
{
static const std::string CONFIG_PATH = "/shared2/sys/net/02/config.dat";

WiiNetConfig::WiiNetConfig() = default;

void WiiNetConfig::ReadConfig(FS::FileSystem* fs)
{
  {
    const auto file = fs->OpenFile(PID_NCD, PID_NCD, CONFIG_PATH, FS::Mode::Read);
    if (file && file->Read(&m_data, 1))
      return;
  }
  ResetConfig(fs);
}

void WiiNetConfig::WriteConfig(FS::FileSystem* fs) const
{
  constexpr FS::Modes public_modes{FS::Mode::ReadWrite, FS::Mode::ReadWrite, FS::Mode::ReadWrite};
  fs->CreateFullPath(PID_NCD, PID_NCD, CONFIG_PATH, 0, public_modes);
  const auto file = fs->CreateAndOpenFile(PID_NCD, PID_NCD, CONFIG_PATH, public_modes);
  if (!file || !file->Write(&m_data, 1))
    ERROR_LOG_FMT(IOS_NET, "Failed to write config");
}

void WiiNetConfig::ResetConfig(FS::FileSystem* fs)
{
  fs->Delete(PID_NCD, PID_NCD, CONFIG_PATH);

  memset(&m_data, 0, sizeof(m_data));
  m_data.connType = ConfigData::IF_WIRED;
  m_data.connection[0].flags =
      ConnectionSettings::WIRED_IF | ConnectionSettings::DNS_DHCP | ConnectionSettings::IP_DHCP |
      ConnectionSettings::CONNECTION_TEST_OK | ConnectionSettings::CONNECTION_SELECTED;

  WriteConfig(fs);
}

void WiiNetConfig::WriteToMem(const u32 address) const
{
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();
  memory.CopyToEmu(address, &m_data, sizeof(m_data));
}

void WiiNetConfig::ReadFromMem(const u32 address)
{
  auto& system = Core::System::GetInstance();
  auto& memory = system.GetMemory();
  memory.CopyFromEmu(&m_data, address, sizeof(m_data));
}
}  // namespace IOS::HLE::Net
