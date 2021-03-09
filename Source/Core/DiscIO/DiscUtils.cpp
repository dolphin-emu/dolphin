// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DiscIO/DiscUtils.h"

#include <algorithm>
#include <locale>
#include <optional>
#include <string>

#include <fmt/format.h>

#include "Common/CommonTypes.h"
#include "DiscIO/Volume.h"

namespace DiscIO
{
std::string NameForPartitionType(u32 partition_type, bool include_prefix)
{
  switch (partition_type)
  {
  case PARTITION_DATA:
    return "DATA";
  case PARTITION_UPDATE:
    return "UPDATE";
  case PARTITION_CHANNEL:
    return "CHANNEL";
  case PARTITION_INSTALL:
    // wit doesn't recognize the name "INSTALL", so we can't use it when naming partition folders
    if (!include_prefix)
      return "INSTALL";
    [[fallthrough]];
  default:
    const std::string type_as_game_id{static_cast<char>((partition_type >> 24) & 0xFF),
                                      static_cast<char>((partition_type >> 16) & 0xFF),
                                      static_cast<char>((partition_type >> 8) & 0xFF),
                                      static_cast<char>(partition_type & 0xFF)};
    if (std::all_of(type_as_game_id.cbegin(), type_as_game_id.cend(),
                    [](char c) { return std::isalnum(c, std::locale::classic()); }))
    {
      return include_prefix ? "P-" + type_as_game_id : type_as_game_id;
    }

    return fmt::format(include_prefix ? "P{}" : "{}", partition_type);
  }
}

std::optional<u64> GetApploaderSize(const Volume& volume, const Partition& partition)
{
  constexpr u64 header_size = 0x20;
  const std::optional<u32> apploader_size = volume.ReadSwapped<u32>(0x2440 + 0x14, partition);
  const std::optional<u32> trailer_size = volume.ReadSwapped<u32>(0x2440 + 0x18, partition);
  if (!apploader_size || !trailer_size)
    return std::nullopt;

  return header_size + *apploader_size + *trailer_size;
}

std::optional<u64> GetBootDOLOffset(const Volume& volume, const Partition& partition)
{
  const Platform volume_type = volume.GetVolumeType();
  if (!IsDisc(volume_type))
    return std::nullopt;

  std::optional<u64> dol_offset = volume.ReadSwappedAndShifted(0x420, partition);

  // Datel AR disc has 0x00000000 as the offset (invalid) and doesn't use it in the AppLoader.
  if (dol_offset && *dol_offset == 0)
    dol_offset.reset();

  return dol_offset;
}

std::optional<u32> GetBootDOLSize(const Volume& volume, const Partition& partition, u64 dol_offset)
{
  if (!IsDisc(volume.GetVolumeType()))
    return std::nullopt;

  u32 dol_size = 0;

  // Iterate through the 7 code segments
  for (size_t i = 0; i < 7; i++)
  {
    const std::optional<u32> offset = volume.ReadSwapped<u32>(dol_offset + 0x00 + i * 4, partition);
    const std::optional<u32> size = volume.ReadSwapped<u32>(dol_offset + 0x90 + i * 4, partition);
    if (!offset || !size)
      return {};
    dol_size = std::max(*offset + *size, dol_size);
  }

  // Iterate through the 11 data segments
  for (size_t i = 0; i < 11; i++)
  {
    const std::optional<u32> offset = volume.ReadSwapped<u32>(dol_offset + 0x1c + i * 4, partition);
    const std::optional<u32> size = volume.ReadSwapped<u32>(dol_offset + 0xac + i * 4, partition);
    if (!offset || !size)
      return {};
    dol_size = std::max(*offset + *size, dol_size);
  }

  return dol_size;
}

std::optional<u64> GetFSTOffset(const Volume& volume, const Partition& partition)
{
  const Platform volume_type = volume.GetVolumeType();
  if (!IsDisc(volume_type))
    return std::nullopt;

  return volume.ReadSwappedAndShifted(0x424, partition);
}

std::optional<u64> GetFSTSize(const Volume& volume, const Partition& partition)
{
  const Platform volume_type = volume.GetVolumeType();
  if (!IsDisc(volume_type))
    return std::nullopt;

  return volume.ReadSwappedAndShifted(0x428, partition);
}

}  // namespace DiscIO
