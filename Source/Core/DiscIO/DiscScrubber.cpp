// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cinttypes>
#include <cstddef>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "DiscIO/DiscScrubber.h"
#include "DiscIO/Filesystem.h"
#include "DiscIO/Volume.h"
#include "DiscIO/VolumeCreator.h"

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
bool DiscScrubber::ReadFromVolume(u64 offset, u32& buffer, bool decrypt)
{
  return m_disc->ReadSwapped(offset, &buffer, decrypt);
}

bool DiscScrubber::ReadFromVolume(u64 offset, u64& buffer, bool decrypt)
{
  u32 temp_buffer;
  if (!m_disc->ReadSwapped(offset, &temp_buffer, decrypt))
    return false;
  buffer = static_cast<u64>(temp_buffer) << 2;
  return true;
}

bool DiscScrubber::ParseDisc()
{
  // Mark the header as used - it's mostly 0s anyways
  MarkAsUsed(0, 0x50000);

  for (u32 x = 0; x < 4; x++)
  {
    if (!ReadFromVolume(0x40000 + (x * 8) + 0, m_partition_group[x].num_partitions, false) ||
        !ReadFromVolume(0x40000 + (x * 8) + 4, m_partition_group[x].partitions_offset, false))
    {
      return false;
    }

    // Read all partitions
    for (u32 i = 0; i < m_partition_group[x].num_partitions; i++)
    {
      Partition partition;

      partition.group_number = x;
      partition.number = i;

      if (!ReadFromVolume(m_partition_group[x].partitions_offset + (i * 8) + 0, partition.offset,
                          false) ||
          !ReadFromVolume(m_partition_group[x].partitions_offset + (i * 8) + 4, partition.type,
                          false) ||
          !ReadFromVolume(partition.offset + 0x2a4, partition.header.tmd_size, false) ||
          !ReadFromVolume(partition.offset + 0x2a8, partition.header.tmd_offset, false) ||
          !ReadFromVolume(partition.offset + 0x2ac, partition.header.cert_chain_size, false) ||
          !ReadFromVolume(partition.offset + 0x2b0, partition.header.cert_chain_offset, false) ||
          !ReadFromVolume(partition.offset + 0x2b4, partition.header.h3_offset, false) ||
          !ReadFromVolume(partition.offset + 0x2b8, partition.header.data_offset, false) ||
          !ReadFromVolume(partition.offset + 0x2bc, partition.header.data_size, false))
      {
        return false;
      }

      m_partition_group[x].partitions.push_back(partition);
    }

    for (auto& partition : m_partition_group[x].partitions)
    {
      const PartitionHeader& header = partition.header;

      MarkAsUsed(partition.offset, 0x2c0);

      MarkAsUsed(partition.offset + header.tmd_offset, header.tmd_size);
      MarkAsUsed(partition.offset + header.cert_chain_offset, header.cert_chain_size);
      MarkAsUsed(partition.offset + header.h3_offset, 0x18000);
      // This would mark the whole (encrypted) data area
      // we need to parse FST and other crap to find what's free within it!
      // MarkAsUsed(partition.offset + header.data_offset, header.data_size);

      // Parse Data! This is where the big gain is
      if (!ParsePartitionData(partition))
        return false;
    }
  }

  return true;
}

// Operations dealing with encrypted space are done here - the volume is swapped to allow this
bool DiscScrubber::ParsePartitionData(Partition& partition)
{
  bool parsed_ok = true;

  // Switch out the main volume temporarily
  std::unique_ptr<IVolume> old_volume;
  m_disc.swap(old_volume);

  // Ready some stuff
  m_disc = CreateVolumeFromFilename(m_filename, partition.group_number, partition.number);
  if (m_disc == nullptr)
  {
    ERROR_LOG(DISCIO, "Failed to create volume from file %s", m_filename.c_str());
    m_disc.swap(old_volume);
    return false;
  }

  std::unique_ptr<IFileSystem> filesystem(CreateFileSystem(m_disc.get()));
  if (!filesystem)
  {
    ERROR_LOG(DISCIO, "Failed to create filesystem for group %u partition %u",
              partition.group_number, partition.number);
    parsed_ok = false;
  }
  else
  {
    // Mark things as used which are not in the filesystem
    // Header, Header Information, Apploader
    parsed_ok = parsed_ok && ReadFromVolume(0x2440 + 0x14, partition.header.apploader_size, true);
    parsed_ok =
        parsed_ok && ReadFromVolume(0x2440 + 0x18, partition.header.apploader_trailer_size, true);
    MarkAsUsedE(partition.offset + partition.header.data_offset, 0,
                0x2440 + partition.header.apploader_size + partition.header.apploader_trailer_size);

    // DOL
    partition.header.dol_offset = filesystem->GetBootDOLOffset();
    partition.header.dol_size = filesystem->GetBootDOLSize(partition.header.dol_offset);
    parsed_ok = parsed_ok && partition.header.dol_offset && partition.header.dol_size;
    MarkAsUsedE(partition.offset + partition.header.data_offset, partition.header.dol_offset,
                partition.header.dol_size);

    // FST
    parsed_ok = parsed_ok && ReadFromVolume(0x424, partition.header.fst_offset, true);
    parsed_ok = parsed_ok && ReadFromVolume(0x428, partition.header.fst_size, true);
    MarkAsUsedE(partition.offset + partition.header.data_offset, partition.header.fst_offset,
                partition.header.fst_size);

    // Go through the filesystem and mark entries as used
    for (const SFileInfo& file : filesystem->GetFileList())
    {
      DEBUG_LOG(DISCIO, "%s", file.m_FullPath.empty() ? "/" : file.m_FullPath.c_str());
      if ((file.m_NameOffset & 0x1000000) == 0)
      {
        MarkAsUsedE(partition.offset + partition.header.data_offset, file.m_Offset,
                    file.m_FileSize);
      }
    }
  }

  // Swap back
  m_disc.swap(old_volume);

  return parsed_ok;
}

}  // namespace DiscIO
