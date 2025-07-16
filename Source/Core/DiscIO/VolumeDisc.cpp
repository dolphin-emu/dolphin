// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DiscIO/VolumeDisc.h"

#include <memory>
#include <optional>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/Crypto/SHA1.h"
#include "DiscIO/DiscUtils.h"
#include "DiscIO/Enums.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/VolumeGC.h"

namespace DiscIO
{
std::string VolumeDisc::GetGameID(const Partition& partition) const
{
  char id[6];

  if (GetVolumeType() == Platform::Triforce)
  {
    // Triforce games have their Game ID stored in the boot.id file
    const BootID* boot_id = static_cast<const VolumeGC*>(this)->GetTriforceBootID();

    // Construct game ID from the BTID
    id[0] = 'G';

    memcpy(id + 1, boot_id->gameId + 2, 2);

    switch (boot_id->regionFlags)
    {
    default:
    case 0x02:  // JAPAN
      id[3] = 'J';
      break;
    case 0x08:  // ASIA
      id[3] = 'W';
      break;
    case 0x0E:  // USA
      id[3] = 'E';
      break;
    case 0x0C:  // EXPORT
      id[3] = 'P';
      break;
    }

    // There only seem to be three different makers here,
    // so we can just check the first char to difference between them.
    //
    // NAMCO CORPORATION, SEGA CORPORATION and Hitmaker co,ltd.

    switch (boot_id->manufacturer[0])
    {
    case 'N':  // NAMCO CORPORATION
      id[4] = '8';
      id[5] = '2';
      break;
    default:
    case 'H':  // Hitmaker co,ltd.
    case 'S':  // SEGA CORPORATION
      id[4] = '6';
      id[5] = 'E';
      break;
    }

    return DecodeString(id);
  }

  if (!Read(0, sizeof(id), reinterpret_cast<u8*>(id), partition))
    return std::string();

  return DecodeString(id);
}

Country VolumeDisc::GetCountry(const Partition& partition) const
{
  u8 country_byte = 0;

  if (GetVolumeType() == Platform::Triforce)
  {
    const BootID* boot_id = static_cast<const VolumeGC*>(this)->GetTriforceBootID();

    switch (boot_id->regionFlags)
    {
    default:
    case 0x02:  // JAPAN
      country_byte = 'J';
      break;
    case 0x08:  // ASIA
      country_byte = 'W';
      break;
    case 0x0E:  // USA
      country_byte = 'E';
      break;
    case 0x0C:  // EXPORT
      country_byte = 'P';
      break;
    }
  }
  else
  {
    // The 0 that we use as a default value is mapped to Country::Unknown and Region::Unknown
    country_byte = ReadSwapped<u8>(3, partition).value_or(0);
  }

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

  // Triforce games have their maker stored in the boot.id file as an actual string
  if (GetVolumeType() == Platform::Triforce)
  {
    const BootID* boot_id = static_cast<const VolumeGC*>(this)->GetTriforceBootID();

    switch (boot_id->manufacturer[0])
    {
    case 'S':  // SEGA CORPORATION
    case 'H':  // Hitmaker co,ltd
      return DecodeString("6E");
    case 'N':  // NAMCO CORPORATION
      return DecodeString("82");
    default:
      break;
    }
    // Fall back to normal maker from header
  }

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

  // Triforce games have their Title stored in the boot.id file
  if (GetVolumeType() == Platform::Triforce)
  {
    const BootID* boot_id = static_cast<const VolumeGC*>(this)->GetTriforceBootID();
    return DecodeString(boot_id->gameName);
  }

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
  const std::optional<u64> dol_offset = GetBootDOLOffset(*this, partition);
  if (dol_offset)
  {
    ReadAndAddToSyncHash(context, *dol_offset,
                         GetBootDOLSize(*this, partition, *dol_offset).value_or(0), partition);
  }

  // File system
  const std::optional<u64> fst_offset = GetFSTOffset(*this, partition);
  if (fst_offset)
    ReadAndAddToSyncHash(context, *fst_offset, GetFSTSize(*this, partition).value_or(0), partition);

  // opening.bnr (name and banner)
  const FileSystem* file_system = GetFileSystem(partition);
  if (file_system)
  {
    std::unique_ptr<FileInfo> file_info = file_system->FindFileInfo("opening.bnr");
    if (file_info && !file_info->IsDirectory())
      ReadAndAddToSyncHash(context, file_info->GetOffset(), file_info->GetSize(), partition);
  }
}

}  // namespace DiscIO
