// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/USB/Bluetooth/BTBase.h"

#include <memory>
#include <string>
#include <vector>

#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"
#include "Core/SysConf.h"

namespace IOS::HLE
{
void BackUpBTInfoSection(const SysConf* sysconf)
{
  const std::string filename = File::GetUserPath(D_CONFIG_IDX) + DIR_SEP WII_BTDINF_BACKUP;
  if (File::Exists(filename))
    return;
  File::IOFile backup(filename, "wb");

  const SysConf::Entry* btdinf = sysconf->GetEntry("BT.DINF");
  if (!btdinf)
    return;

  const std::vector<u8>& section = btdinf->bytes;
  if (!backup.WriteBytes(section.data(), section.size()))
    ERROR_LOG_FMT(IOS_WIIMOTE, "Failed to back up BT.DINF section");
}

void RestoreBTInfoSection(SysConf* sysconf)
{
  const std::string filename = File::GetUserPath(D_CONFIG_IDX) + DIR_SEP WII_BTDINF_BACKUP;
  {
    File::IOFile backup(filename, "rb");
    if (!backup)
      return;
    auto& section = sysconf->GetOrAddEntry("BT.DINF", SysConf::Entry::Type::BigArray)->bytes;
    if (!backup.ReadBytes(section.data(), section.size()))
    {
      ERROR_LOG_FMT(IOS_WIIMOTE, "Failed to read backed up BT.DINF section");
      return;
    }
  }

  File::Delete(filename);
}
}  // namespace IOS::HLE
