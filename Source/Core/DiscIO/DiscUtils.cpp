// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DiscIO/DiscUtils.h"

#include <algorithm>
#include <locale>
#include <optional>
#include <string>
#include <vector>

#include <fmt/format.h>

#include "Common/CommonTypes.h"
#include "Common/MathUtil.h"
#include "DiscIO/Blob.h"
#include "DiscIO/Filesystem.h"
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

    return fmt::format("{}{}", include_prefix ? "P" : "", partition_type);
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

u64 GetBiggestReferencedOffset(const Volume& volume)
{
  std::vector<Partition> partitions = volume.GetPartitions();

  // If a partition doesn't seem to contain any valid data, skip it.
  // This can happen when certain programs that create WBFS files scrub the entirety of
  // the Masterpiece partitions in Super Smash Bros. Brawl without removing them from
  // the partition table. https://bugs.dolphin-emu.org/issues/8733
  const auto it =
      std::remove_if(partitions.begin(), partitions.end(), [&](const Partition& partition) {
        return volume.ReadSwapped<u32>(0x18, partition) != WII_DISC_MAGIC;
      });
  partitions.erase(it, partitions.end());

  if (partitions.empty())
    partitions.push_back(PARTITION_NONE);

  return GetBiggestReferencedOffset(volume, partitions);
}

static u64 GetBiggestReferencedOffset(const Volume& volume, const FileInfo& file_info)
{
  if (file_info.IsDirectory())
  {
    u64 biggest_offset = 0;
    for (const FileInfo& f : file_info)
      biggest_offset = std::max(biggest_offset, GetBiggestReferencedOffset(volume, f));
    return biggest_offset;
  }
  else
  {
    return file_info.GetOffset() + file_info.GetSize();
  }
}

u64 GetBiggestReferencedOffset(const Volume& volume, const std::vector<Partition>& partitions)
{
  const u64 disc_header_size = volume.GetVolumeType() == Platform::GameCubeDisc ? 0x460 : 0x50000;
  u64 biggest_offset = disc_header_size;
  for (const Partition& partition : partitions)
  {
    if (partition != PARTITION_NONE)
    {
      const u64 offset = volume.PartitionOffsetToRawOffset(0x440, partition);
      biggest_offset = std::max(biggest_offset, offset);
    }

    const std::optional<u64> dol_offset = GetBootDOLOffset(volume, partition);
    if (dol_offset)
    {
      const std::optional<u64> dol_size = GetBootDOLSize(volume, partition, *dol_offset);
      if (dol_size)
      {
        const u64 offset = volume.PartitionOffsetToRawOffset(*dol_offset + *dol_size, partition);
        biggest_offset = std::max(biggest_offset, offset);
      }
    }

    const std::optional<u64> fst_offset = GetFSTOffset(volume, partition);
    const std::optional<u64> fst_size = GetFSTSize(volume, partition);
    if (fst_offset && fst_size)
    {
      const u64 offset = volume.PartitionOffsetToRawOffset(*fst_offset + *fst_size, partition);
      biggest_offset = std::max(biggest_offset, offset);
    }

    const FileSystem* fs = volume.GetFileSystem(partition);
    if (fs)
    {
      const u64 offset_in_partition = GetBiggestReferencedOffset(volume, fs->GetRoot());
      const u64 offset = volume.PartitionOffsetToRawOffset(offset_in_partition, partition);
      biggest_offset = std::max(biggest_offset, offset);
    }
  }
  return biggest_offset;
}

bool IsGCZBlockSizeLegacyCompatible(int block_size, u64 file_size)
{
  // In order for versions of Dolphin prior to 5.0-11893 to be able to convert a GCZ file
  // to ISO without messing up the final part of the file in some way, the file size
  // must be an integer multiple of the block size (fixed in 3aa463c) and must not be
  // an integer multiple of the block size multiplied by 32 (fixed in 26b21e3).
  return file_size % block_size == 0 && file_size % (block_size * 32) != 0;
}

bool IsDiscImageBlockSizeValid(int block_size, DiscIO::BlobType format)
{
  switch (format)
  {
  case DiscIO::BlobType::GCZ:
    // Block size "must" be a power of 2
    if (!MathUtil::IsPow2(block_size))
      return false;

    break;
  case DiscIO::BlobType::WIA:
    // Block size must not be less than the minimum, and must be a multiple of it
    if (block_size < WIA_MIN_BLOCK_SIZE || block_size % WIA_MIN_BLOCK_SIZE != 0)
      return false;

    break;
  case DiscIO::BlobType::RVZ:
    // Block size must not be smaller than the minimum
    // Block sizes smaller than the large block size threshold must be a power of 2
    // Block sizes larger than that threshold must be a multiple of the threshold
    if (block_size < RVZ_MIN_BLOCK_SIZE ||
        (block_size < RVZ_BIG_BLOCK_SIZE_LCM && !MathUtil::IsPow2(block_size)) ||
        (block_size > RVZ_BIG_BLOCK_SIZE_LCM && block_size % RVZ_BIG_BLOCK_SIZE_LCM != 0))
    {
      return false;
    }

    break;
  default:
    ASSERT(false);
    break;
  }

  return true;
}

}  // namespace DiscIO
