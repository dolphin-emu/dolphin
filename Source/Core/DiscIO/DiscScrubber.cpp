// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DiscIO/DiscScrubber.h"

#include <algorithm>
#include <cinttypes>
#include <cstddef>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Common/Logging/Log.h"

#include "DiscIO/DiscExtractor.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/Volume.h"

namespace DiscIO
{
constexpr size_t CLUSTER_SIZE = 0x8000;

DiscScrubber::DiscScrubber() = default;
DiscScrubber::~DiscScrubber() = default;

bool DiscScrubber::SetupScrub(const std::string& filename, int block_size)
{
  m_filename = filename;
  m_block_size = block_size;

  if (CLUSTER_SIZE % m_block_size != 0)
  {
    ERROR_LOG(DISCIO, "Block size %u is not a factor of 0x8000, scrubbing not possible",
              m_block_size);
    return false;
  }

  m_disc = CreateVolumeFromFilename(filename);
  if (!m_disc)
    return false;

  m_file_size = m_disc->GetSize();

  const size_t num_clusters = static_cast<size_t>(m_file_size / CLUSTER_SIZE);

  // Warn if not DVD5 or DVD9 size
  if (num_clusters != 0x23048 && num_clusters != 0x46090)
  {
    WARN_LOG(DISCIO, "%s is not a standard sized Wii disc! (%zx blocks)", filename.c_str(),
             num_clusters);
  }

  // Table of free blocks
  m_free_table.resize(num_clusters, 1);

  // Fill out table of free blocks
  const bool success = ParseDisc();

  // Done with it; need it closed for the next part
  m_disc.reset();
  m_block_count = 0;

  m_is_scrubbing = success;
  return success;
}

size_t DiscScrubber::GetNextBlock(File::IOFile& in, u8* buffer)
{
  const u64 current_offset = m_block_count * m_block_size;
  const u64 i = current_offset / CLUSTER_SIZE;

  size_t read_bytes = 0;
  if (m_is_scrubbing && m_free_table[i])
  {
    DEBUG_LOG(DISCIO, "Freeing 0x%016" PRIx64, current_offset);
    std::fill(buffer, buffer + m_block_size, 0x00);
    in.Seek(m_block_size, SEEK_CUR);
    read_bytes = m_block_size;
  }
  else
  {
    DEBUG_LOG(DISCIO, "Used    0x%016" PRIx64, current_offset);
    in.ReadArray(buffer, m_block_size, &read_bytes);
  }

  m_block_count++;
  return read_bytes;
}

void DiscScrubber::MarkAsUsed(u64 offset, u64 size)
{
  u64 current_offset = offset;
  const u64 end_offset = current_offset + size;

  DEBUG_LOG(DISCIO, "Marking 0x%016" PRIx64 " - 0x%016" PRIx64 " as used", offset, end_offset);

  while (current_offset < end_offset && current_offset < m_file_size)
  {
    m_free_table[current_offset / CLUSTER_SIZE] = 0;
    current_offset += CLUSTER_SIZE;
  }
}

// Compensate for 0x400 (SHA-1) per 0x8000 (cluster), and round to whole clusters
void DiscScrubber::MarkAsUsedE(u64 partition_data_offset, u64 offset, u64 size)
{
  u64 first_cluster_start = offset / 0x7c00 * CLUSTER_SIZE + partition_data_offset;

  u64 last_cluster_end;
  if (size == 0)
  {
    // Without this special case, a size of 0 can be rounded to 1 cluster instead of 0
    last_cluster_end = first_cluster_start;
  }
  else
  {
    last_cluster_end = ((offset + size - 1) / 0x7c00 + 1) * CLUSTER_SIZE + partition_data_offset;
  }

  MarkAsUsed(first_cluster_start, last_cluster_end - first_cluster_start);
}

// Helper functions for reading the BE volume
bool DiscScrubber::ReadFromVolume(u64 offset, u32& buffer, const Partition& partition)
{
  std::optional<u32> value = m_disc->ReadSwapped<u32>(offset, partition);
  if (value)
    buffer = *value;
  return value.has_value();
}

bool DiscScrubber::ReadFromVolume(u64 offset, u64& buffer, const Partition& partition)
{
  std::optional<u64> value = m_disc->ReadSwappedAndShifted(offset, partition);
  if (value)
    buffer = *value;
  return value.has_value();
}

bool DiscScrubber::ParseDisc()
{
  // Mark the header as used - it's mostly 0s anyways
  MarkAsUsed(0, 0x50000);

  for (const DiscIO::Partition& partition : m_disc->GetPartitions())
  {
    PartitionHeader header;

    if (!ReadFromVolume(partition.offset + 0x2a4, header.tmd_size, PARTITION_NONE) ||
        !ReadFromVolume(partition.offset + 0x2a8, header.tmd_offset, PARTITION_NONE) ||
        !ReadFromVolume(partition.offset + 0x2ac, header.cert_chain_size, PARTITION_NONE) ||
        !ReadFromVolume(partition.offset + 0x2b0, header.cert_chain_offset, PARTITION_NONE) ||
        !ReadFromVolume(partition.offset + 0x2b4, header.h3_offset, PARTITION_NONE) ||
        !ReadFromVolume(partition.offset + 0x2b8, header.data_offset, PARTITION_NONE) ||
        !ReadFromVolume(partition.offset + 0x2bc, header.data_size, PARTITION_NONE))
    {
      return false;
    }

    MarkAsUsed(partition.offset, 0x2c0);

    MarkAsUsed(partition.offset + header.tmd_offset, header.tmd_size);
    MarkAsUsed(partition.offset + header.cert_chain_offset, header.cert_chain_size);
    MarkAsUsed(partition.offset + header.h3_offset, 0x18000);
    // This would mark the whole (encrypted) data area
    // we need to parse FST and other crap to find what's free within it!
    // MarkAsUsed(partition.offset + header.data_offset, header.data_size);

    // Parse Data! This is where the big gain is
    if (!ParsePartitionData(partition, &header))
      return false;
  }

  return true;
}

// Operations dealing with encrypted space are done here
bool DiscScrubber::ParsePartitionData(const Partition& partition, PartitionHeader* header)
{
  std::unique_ptr<FileSystem> filesystem(CreateFileSystem(m_disc.get(), partition));
  if (!filesystem)
  {
    ERROR_LOG(DISCIO, "Failed to read file system for the partition at 0x%" PRIx64,
              partition.offset);
    return false;
  }

  const u64 partition_data_offset = partition.offset + header->data_offset;

  // Mark things as used which are not in the filesystem
  // Header, Header Information, Apploader
  if (!ReadFromVolume(0x2440 + 0x14, header->apploader_size, partition) ||
      !ReadFromVolume(0x2440 + 0x18, header->apploader_size, partition))
  {
    return false;
  }
  MarkAsUsedE(partition_data_offset, 0,
              0x2440 + header->apploader_size + header->apploader_trailer_size);

  // DOL
  const std::optional<u64> dol_offset = GetBootDOLOffset(*m_disc, partition);
  if (!dol_offset)
    return false;
  const std::optional<u64> dol_size = GetBootDOLSize(*m_disc, partition, *dol_offset);
  if (!dol_size)
    return false;
  header->dol_offset = *dol_offset;
  header->dol_size = *dol_size;
  MarkAsUsedE(partition_data_offset, header->dol_offset, header->dol_size);

  // FST
  if (!ReadFromVolume(0x424, header->fst_offset, partition) ||
      !ReadFromVolume(0x428, header->fst_size, partition))
  {
    return false;
  }
  MarkAsUsedE(partition_data_offset, header->fst_offset, header->fst_size);

  // Go through the filesystem and mark entries as used
  ParseFileSystemData(partition_data_offset, filesystem->GetRoot());

  return true;
}

void DiscScrubber::ParseFileSystemData(u64 partition_data_offset, const FileInfo& directory)
{
  for (const DiscIO::FileInfo& file_info : directory)
  {
    DEBUG_LOG(DISCIO, "Scrubbing %s", file_info.GetPath().c_str());
    if (file_info.IsDirectory())
      ParseFileSystemData(partition_data_offset, file_info);
    else
      MarkAsUsedE(partition_data_offset, file_info.GetOffset(), file_info.GetSize());
  }
}

}  // namespace DiscIO
