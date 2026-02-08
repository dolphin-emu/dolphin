// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DiscIO/DiscScrubber.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Common/Align.h"
#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"

#include "DiscIO/DiscUtils.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/Volume.h"

namespace DiscIO
{
DiscScrubber::DiscScrubber() = default;

bool DiscScrubber::SetupScrub(const Volume& disc)
{
  m_file_size = disc.GetDataSize();
  m_has_wii_hashes = disc.HasWiiHashes();

  // Round up when diving by CLUSTER_SIZE, otherwise MarkAsUsed might write out of bounds
  const size_t num_clusters = static_cast<size_t>((m_file_size + CLUSTER_SIZE - 1) / CLUSTER_SIZE);

  // Table of free blocks
  m_free_table.resize(num_clusters, 1);

  // Fill out table of free blocks
  const bool success = ParseDisc(disc);

  m_is_scrubbing = success;
  return success;
}

bool DiscScrubber::CanBlockBeScrubbed(u64 offset) const
{
  if (!m_is_scrubbing)
    return false;

  const u64 cluster_index = offset / CLUSTER_SIZE;
  return cluster_index >= m_free_table.size() || m_free_table[cluster_index];
}

void DiscScrubber::MarkAsUsed(u64 offset, u64 size)
{
  u64 current_offset = Common::AlignDown(offset, CLUSTER_SIZE);
  const u64 end_offset = offset + size;

  DEBUG_LOG_FMT(DISCIO, "Marking {:#018x} - {:#018x} as used", offset, end_offset);

  while (current_offset < end_offset && current_offset < m_file_size)
  {
    m_free_table[current_offset / CLUSTER_SIZE] = 0;
    current_offset += CLUSTER_SIZE;
  }
}

void DiscScrubber::MarkAsUsedE(u64 partition_data_offset, u64 offset, u64 size)
{
  if (partition_data_offset == 0)
  {
    MarkAsUsed(offset, size);
  }
  else
  {
    u64 first_cluster_start = ToClusterOffset(offset) + partition_data_offset;

    u64 last_cluster_end;
    if (size == 0)
    {
      // Without this special case, a size of 0 can be rounded to 1 cluster instead of 0
      last_cluster_end = first_cluster_start;
    }
    else
    {
      last_cluster_end = ToClusterOffset(offset + size - 1) + CLUSTER_SIZE + partition_data_offset;
    }

    MarkAsUsed(first_cluster_start, last_cluster_end - first_cluster_start);
  }
}

// Compensate for 0x400 (SHA-1) per 0x8000 (cluster), and round to whole clusters
u64 DiscScrubber::ToClusterOffset(u64 offset) const
{
  if (m_has_wii_hashes)
    return offset / 0x7c00 * CLUSTER_SIZE;
  else
    return Common::AlignDown(offset, CLUSTER_SIZE);
}

// Helper functions for reading the BE volume
bool DiscScrubber::ReadFromVolume(const Volume& disc, u64 offset, u32& buffer,
                                  const Partition& partition)
{
  std::optional<u32> value = disc.ReadSwapped<u32>(offset, partition);
  if (value)
    buffer = *value;
  return value.has_value();
}

bool DiscScrubber::ReadFromVolume(const Volume& disc, u64 offset, u64& buffer,
                                  const Partition& partition)
{
  std::optional<u64> value = disc.ReadSwappedAndShifted(offset, partition);
  if (value)
    buffer = *value;
  return value.has_value();
}

bool DiscScrubber::ParseDisc(const Volume& disc)
{
  if (disc.GetPartitions().empty())
    return ParsePartitionData(disc, PARTITION_NONE);

  // Mark the header as used - it's mostly 0s anyways
  MarkAsUsed(0, 0x50000);

  for (const DiscIO::Partition& partition : disc.GetPartitions())
  {
    u32 tmd_size;
    u64 tmd_offset;
    u32 cert_chain_size;
    u64 cert_chain_offset;
    u64 h3_offset;
    // The H3 size is always 0x18000

    if (!ReadFromVolume(disc, partition.offset + WII_PARTITION_TMD_SIZE_ADDRESS, tmd_size,
                        PARTITION_NONE) ||
        !ReadFromVolume(disc, partition.offset + WII_PARTITION_TMD_OFFSET_ADDRESS, tmd_offset,
                        PARTITION_NONE) ||
        !ReadFromVolume(disc, partition.offset + WII_PARTITION_CERT_CHAIN_SIZE_ADDRESS,
                        cert_chain_size, PARTITION_NONE) ||
        !ReadFromVolume(disc, partition.offset + WII_PARTITION_CERT_CHAIN_OFFSET_ADDRESS,
                        cert_chain_offset, PARTITION_NONE) ||
        !ReadFromVolume(disc, partition.offset + WII_PARTITION_H3_OFFSET_ADDRESS, h3_offset,
                        PARTITION_NONE))
    {
      return false;
    }

    MarkAsUsed(partition.offset, 0x2c0);

    MarkAsUsed(partition.offset + tmd_offset, tmd_size);
    MarkAsUsed(partition.offset + cert_chain_offset, cert_chain_size);
    MarkAsUsed(partition.offset + h3_offset, WII_PARTITION_H3_SIZE);

    // Parse Data! This is where the big gain is
    if (!ParsePartitionData(disc, partition))
      return false;
  }

  return true;
}

// Operations dealing with encrypted space are done here
bool DiscScrubber::ParsePartitionData(const Volume& disc, const Partition& partition)
{
  const FileSystem* filesystem = disc.GetFileSystem(partition);
  if (!filesystem)
  {
    ERROR_LOG_FMT(DISCIO, "Failed to read file system for the partition at {:#x}",
                  partition.offset);
    return false;
  }

  u64 partition_data_offset;
  if (partition == PARTITION_NONE)
  {
    partition_data_offset = 0;
  }
  else
  {
    u64 data_offset;
    if (!ReadFromVolume(disc, partition.offset + 0x2b8, data_offset, PARTITION_NONE))
      return false;

    partition_data_offset = partition.offset + data_offset;
  }

  // Mark things as used which are not in the filesystem
  // Header, Header Information, Apploader
  u32 apploader_size;
  u32 apploader_trailer_size;
  if (!ReadFromVolume(disc, 0x2440 + 0x14, apploader_size, partition) ||
      !ReadFromVolume(disc, 0x2440 + 0x18, apploader_trailer_size, partition))
  {
    return false;
  }
  MarkAsUsedE(partition_data_offset, 0, 0x2440 + apploader_size + apploader_trailer_size);

  // DOL
  const std::optional<u64> dol_offset = GetBootDOLOffset(disc, partition);
  if (!dol_offset)
    return false;
  const std::optional<u64> dol_size = GetBootDOLSize(disc, partition, *dol_offset);
  if (!dol_size)
    return false;
  MarkAsUsedE(partition_data_offset, *dol_offset, *dol_size);

  // FST
  const std::optional<u64> fst_offset = GetFSTOffset(disc, partition);
  const std::optional<u64> fst_size = GetFSTSize(disc, partition);
  if (!fst_offset || !fst_size)
    return false;
  MarkAsUsedE(partition_data_offset, *fst_offset, *fst_size);

  // Go through the filesystem and mark entries as used
  ParseFileSystemData(partition_data_offset, filesystem->GetRoot());

  return true;
}

void DiscScrubber::ParseFileSystemData(u64 partition_data_offset, const FileInfo& directory)
{
  for (const DiscIO::FileInfo& file_info : directory)
  {
    DEBUG_LOG_FMT(DISCIO, "Scrubbing {}", file_info.GetPath());
    if (file_info.IsDirectory())
      ParseFileSystemData(partition_data_offset, file_info);
    else
      MarkAsUsedE(partition_data_offset, file_info.GetOffset(), file_info.GetSize());
  }
}

}  // namespace DiscIO
