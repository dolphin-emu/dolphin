// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DiscIO/VolumeDisc.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Crypto/SHA1.h"
#include "DiscIO/DiscUtils.h"
#include "DiscIO/Enums.h"
#include "DiscIO/Filesystem.h"

namespace DiscIO
{
std::string VolumeDisc::GetGameID(const Partition& partition) const
{
  char id[6];

  if (!Read(0, sizeof(id), reinterpret_cast<u8*>(id), partition))
    return std::string();

  return DecodeString(id);
}

Country VolumeDisc::GetCountry(const Partition& partition) const
{
  // The 0 that we use as a default value is mapped to Country::Unknown and Region::Unknown
  const u8 country_byte = ReadSwapped<u8>(3, partition).value_or(0);
  const Region region = GetRegion();
  const std::optional<u16> revision = GetRevision();

  if (CountryCodeToRegion(country_byte, GetVolumeType(), region, revision) != region)
    return TypicalCountryForRegion(region);

  return CountryCodeToCountry(country_byte, GetVolumeType(), region, revision);
}

Region VolumeDisc::RegionCodeToRegion(std::optional<u32> region_code) const
{
  if (!region_code)
    return Region::Unknown;

  const Region region = static_cast<Region>(*region_code);
  return region <= Region::NTSC_K ? region : Region::Unknown;
}

std::string VolumeDisc::GetMakerID(const Partition& partition) const
{
  char maker_id[2];

  if (!Read(0x4, sizeof(maker_id), reinterpret_cast<u8*>(&maker_id), partition))
    return std::string();

  return DecodeString(maker_id);
}

std::optional<u16> VolumeDisc::GetRevision(const Partition& partition) const
{
  std::optional<u8> revision = ReadSwapped<u8>(7, partition);
  return revision ? *revision : std::optional<u16>();
}

std::string VolumeDisc::GetInternalName(const Partition& partition) const
{
  char name[0x60];
  if (!Read(0x20, sizeof(name), reinterpret_cast<u8*>(&name), partition))
    return std::string();

  return DecodeString(name);
}

std::string VolumeDisc::GetApploaderDate(const Partition& partition) const
{
  char date[16];

  if (!Read(0x2440, sizeof(date), reinterpret_cast<u8*>(&date), partition))
    return std::string();

  return DecodeString(date);
}

std::optional<u8> VolumeDisc::GetDiscNumber(const Partition& partition) const
{
  return ReadSwapped<u8>(6, partition);
}

bool VolumeDisc::IsNKit() const
{
  constexpr u32 NKIT_MAGIC = 0x4E4B4954;  // "NKIT"
  return ReadSwapped<u32>(0x200, PARTITION_NONE) == NKIT_MAGIC;
}

void VolumeDisc::AddGamePartitionToSyncHash(Common::SHA1::Context* context) const
{
  const Partition partition = GetGamePartition();

  // All headers at the beginning of the partition, plus the apploader
  ReadAndAddToSyncHash(context, 0, 0x2440 + GetApploaderSize(*this, partition).value_or(0),
                       partition);

  // Boot DOL (may be missing if this is a Datel disc)
  if (const std::optional<u64> dol_offset = GetBootDOLOffset(*this, partition))
  {
    ReadAndAddToSyncHash(context, *dol_offset,
                         GetBootDOLSize(*this, partition, *dol_offset).value_or(0), partition);
  }

  // File system
  if (const std::optional<u64> fst_offset = GetFSTOffset(*this, partition))
    ReadAndAddToSyncHash(context, *fst_offset, GetFSTSize(*this, partition).value_or(0), partition);

  // opening.bnr (name and banner)
  if (const FileSystem* file_system = GetFileSystem(partition))
  {
    std::unique_ptr<FileInfo> file_info = file_system->FindFileInfo("opening.bnr");
    if (file_info && !file_info->IsDirectory())
      ReadAndAddToSyncHash(context, file_info->GetOffset(), file_info->GetSize(), partition);
  }
}

}  // namespace DiscIO
