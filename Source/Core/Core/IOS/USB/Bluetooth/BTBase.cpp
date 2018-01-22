// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/USB/Bluetooth/BTBase.h"

#include <memory>
#include <string>
#include <vector>

#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/SysConf.h"

namespace IOS
{
namespace HLE
{
constexpr u16 BT_INFO_SECTION_LENGTH = 0x460;

void BackUpBTInfoSection(const SysConf* sysconf)
{
  const std::string filename = File::GetUserPath(D_SESSION_WIIROOT_IDX) + DIR_SEP WII_BTDINF_BACKUP;
  if (File::Exists(filename))
    return;
  File::IOFile backup(filename, "wb");

  const SysConf::Entry* btdinf = sysconf->GetEntry("BT.DINF");
  if (!btdinf)
    return;

  const std::vector<u8>& section = btdinf->bytes;
  if (!backup.WriteBytes(section.data(), section.size()))
    ERROR_LOG(IOS_WIIMOTE, "Failed to back up BT.DINF section");
}

void RestoreBTInfoSection(SysConf* sysconf)
{
  const std::string filename = File::GetUserPath(D_SESSION_WIIROOT_IDX) + DIR_SEP WII_BTDINF_BACKUP;
  File::IOFile backup(filename, "rb");
  if (!backup)
    return;
  std::vector<u8> section(BT_INFO_SECTION_LENGTH);
  if (!backup.ReadBytes(section.data(), section.size()))
  {
    ERROR_LOG(IOS_WIIMOTE, "Failed to read backed up BT.DINF section");
    return;
  }

  sysconf->GetOrAddEntry("BT.DINF", SysConf::Entry::Type::BigArray)->bytes = std::move(section);
  File::Delete(filename);
}
}  // namespace HLE
}  // namespace IOS
