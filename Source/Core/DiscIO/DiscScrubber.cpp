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
namespace DiscScrubber
{
#define CLUSTER_SIZE 0x8000

static u8* m_FreeTable = nullptr;
static u64 m_FileSize;
static u64 m_BlockCount;
static u32 m_BlockSize;
static int m_BlocksPerCluster;
static bool m_isScrubbing = false;

static std::string m_Filename;
static std::unique_ptr<IVolume> s_disc;

struct SPartitionHeader
{
  u8* Ticket[0x2a4];
  u32 TMDSize;
  u64 TMDOffset;
  u32 CertChainSize;
  u64 CertChainOffset;
  // H3Size is always 0x18000
  u64 H3Offset;
  u64 DataOffset;
  u64 DataSize;
  // TMD would be here
  u64 DOLOffset;
  u64 DOLSize;
  u64 FSTOffset;
  u64 FSTSize;
  u32 ApploaderSize;
  u32 ApploaderTrailerSize;
};

void MarkAsUsed(u64 _Offset, u64 _Size);
void MarkAsUsedE(u64 partition_data_offset, u64 offset, u64 size);
bool ReadFromVolume(u64 _Offset, u32& _Buffer, const Partition& partition);
bool ReadFromVolume(u64 _Offset, u64& _Buffer, const Partition& partition);
bool ParseDisc();
bool ParsePartitionData(const Partition& partition, SPartitionHeader header);

bool SetupScrub(const std::string& filename, int block_size)
{
  bool success = true;
  m_Filename = filename;
  m_BlockSize = block_size;

  if (CLUSTER_SIZE % m_BlockSize != 0)
  {
    ERROR_LOG(DISCIO, "Block size %i is not a factor of 0x8000, scrubbing not possible",
              m_BlockSize);
    return false;
  }

  m_BlocksPerCluster = CLUSTER_SIZE / m_BlockSize;

  s_disc = CreateVolumeFromFilename(filename);
  if (!s_disc)
    return false;

  m_FileSize = s_disc->GetSize();

  u32 numClusters = (u32)(m_FileSize / CLUSTER_SIZE);

  // Warn if not DVD5 or DVD9 size
  if (numClusters != 0x23048 && numClusters != 0x46090)
    WARN_LOG(DISCIO, "%s is not a standard sized Wii disc! (%x blocks)", filename.c_str(),
             numClusters);

  // Table of free blocks
  m_FreeTable = new u8[numClusters];
  std::fill(m_FreeTable, m_FreeTable + numClusters, 1);

  // Fill out table of free blocks
  success = ParseDisc();

  // Done with it; need it closed for the next part
  s_disc.reset();
  m_BlockCount = 0;

  // Let's not touch the file if we've failed up to here :p
  if (!success)
    Cleanup();

  m_isScrubbing = success;
  return success;
}

size_t GetNextBlock(File::IOFile& in, u8* buffer)
{
  u64 CurrentOffset = m_BlockCount * m_BlockSize;
  u64 i = CurrentOffset / CLUSTER_SIZE;

  size_t ReadBytes = 0;
  if (m_isScrubbing && m_FreeTable[i])
  {
    DEBUG_LOG(DISCIO, "Freeing 0x%016" PRIx64, CurrentOffset);
    std::fill(buffer, buffer + m_BlockSize, 0x00);
    in.Seek(m_BlockSize, SEEK_CUR);
    ReadBytes = m_BlockSize;
  }
  else
  {
    DEBUG_LOG(DISCIO, "Used    0x%016" PRIx64, CurrentOffset);
    in.ReadArray(buffer, m_BlockSize, &ReadBytes);
  }

  m_BlockCount++;
  return ReadBytes;
}

void Cleanup()
{
  if (m_FreeTable)
    delete[] m_FreeTable;
  m_FreeTable = nullptr;
  m_FileSize = 0;
  m_BlockCount = 0;
  m_BlockSize = 0;
  m_BlocksPerCluster = 0;
  m_isScrubbing = false;
}

void MarkAsUsed(u64 _Offset, u64 _Size)
{
  u64 CurrentOffset = _Offset;
  u64 EndOffset = CurrentOffset + _Size;

  DEBUG_LOG(DISCIO, "Marking 0x%016" PRIx64 " - 0x%016" PRIx64 " as used", _Offset, EndOffset);

  while ((CurrentOffset < EndOffset) && (CurrentOffset < m_FileSize))
  {
    m_FreeTable[CurrentOffset / CLUSTER_SIZE] = 0;
    CurrentOffset += CLUSTER_SIZE;
  }
}

// Compensate for 0x400 (SHA-1) per 0x8000 (cluster), and round to whole clusters
void MarkAsUsedE(u64 partition_data_offset, u64 offset, u64 size)
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
bool ReadFromVolume(u64 _Offset, u32& _Buffer, const Partition& partition)
{
  return s_disc->ReadSwapped(_Offset, &_Buffer, partition);
}

bool ReadFromVolume(u64 _Offset, u64& _Buffer, const Partition& partition)
{
  u32 temp_buffer;
  if (!s_disc->ReadSwapped(_Offset, &temp_buffer, partition))
    return false;
  _Buffer = static_cast<u64>(temp_buffer) << 2;
  return true;
}

bool ParseDisc()
{
  // Mark the header as used - it's mostly 0s anyways
  MarkAsUsed(0, 0x50000);

  for (DiscIO::Partition& partition : s_disc->GetPartitions())
  {
    SPartitionHeader header;

    if (!ReadFromVolume(partition.offset + 0x2a4, header.TMDSize, PARTITION_NONE) ||
        !ReadFromVolume(partition.offset + 0x2a8, header.TMDOffset, PARTITION_NONE) ||
        !ReadFromVolume(partition.offset + 0x2ac, header.CertChainSize, PARTITION_NONE) ||
        !ReadFromVolume(partition.offset + 0x2b0, header.CertChainOffset, PARTITION_NONE) ||
        !ReadFromVolume(partition.offset + 0x2b4, header.H3Offset, PARTITION_NONE) ||
        !ReadFromVolume(partition.offset + 0x2b8, header.DataOffset, PARTITION_NONE) ||
        !ReadFromVolume(partition.offset + 0x2bc, header.DataSize, PARTITION_NONE))
      return false;

    MarkAsUsed(partition.offset, 0x2c0);

    MarkAsUsed(partition.offset + header.TMDOffset, header.TMDSize);
    MarkAsUsed(partition.offset + header.CertChainOffset, header.CertChainSize);
    MarkAsUsed(partition.offset + header.H3Offset, 0x18000);
    // This would mark the whole (encrypted) data area
    // we need to parse FST and other crap to find what's free within it!
    // MarkAsUsed(partition + header.DataOffset, header.DataSize);

    // Parse Data! This is where the big gain is
    if (!ParsePartitionData(partition, header))
      return false;
  }

  return true;
}

// Operations dealing with encrypted space are done here
bool ParsePartitionData(const Partition& partition, SPartitionHeader header)
{
  std::unique_ptr<IFileSystem> filesystem(CreateFileSystem(s_disc.get(), partition));
  if (!filesystem)
  {
    ERROR_LOG(DISCIO, "Failed to read file system for the partition at 0x%" PRIx64,
              partition.offset);
    return false;
  }
  else
  {
    const u64 partition_data_offset = partition.offset + header.DataOffset;

    // Mark things as used which are not in the filesystem
    // Header, Header Information, Apploader
    if (!ReadFromVolume(0x2440 + 0x14, header.ApploaderSize, partition) ||
        !ReadFromVolume(0x2440 + 0x18, header.ApploaderTrailerSize, partition))
      return false;
    MarkAsUsedE(partition_data_offset, 0,
                0x2440 + header.ApploaderSize + header.ApploaderTrailerSize);

    // DOL
    header.DOLOffset = filesystem->GetBootDOLOffset();
    header.DOLSize = filesystem->GetBootDOLSize(header.DOLOffset);
    if (header.DOLOffset == 0 || header.DOLSize == 0)
      return false;
    MarkAsUsedE(partition_data_offset, header.DOLOffset, header.DOLSize);

    // FST
    if (!ReadFromVolume(0x424, header.FSTOffset, partition) ||
        !ReadFromVolume(0x428, header.FSTSize, partition))
      return false;
    MarkAsUsedE(partition_data_offset, header.FSTOffset, header.FSTSize);

    // Go through the filesystem and mark entries as used
    for (SFileInfo file : filesystem->GetFileList())
    {
      DEBUG_LOG(DISCIO, "%s", file.m_FullPath.empty() ? "/" : file.m_FullPath.c_str());
      if ((file.m_NameOffset & 0x1000000) == 0)
        MarkAsUsedE(partition_data_offset, file.m_Offset, file.m_FileSize);
    }

    return true;
  }
}

}  // namespace DiscScrubber

}  // namespace DiscIO
