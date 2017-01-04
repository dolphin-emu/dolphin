// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// DiscScrubber removes the garbage data from discs (currently Wii only) which
// is on the disc due to encryption

// It could be adapted to GameCube discs, but the gain is most likely negligible,
// and having 1:1 backups of discs is always nice when they are reasonably sized

// Note: the technique is inspired by Wiiscrubber, but much simpler - intentionally :)

#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>
#include "Common/CommonTypes.h"

namespace File
{
class IOFile;
}

namespace DiscIO
{
class IVolume;

class DiscScrubber final
{
public:
  DiscScrubber();
  ~DiscScrubber();

  bool SetupScrub(const std::string& filename, int block_size);
  size_t GetNextBlock(File::IOFile& in, u8* buffer);

private:
  struct SPartitionHeader final
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

  struct SPartition final
  {
    u32 GroupNumber;
    u32 Number;
    u64 Offset;
    u32 Type;
    SPartitionHeader Header;
  };

  struct SPartitionGroup final
  {
    u32 numPartitions;
    u64 PartitionsOffset;
    std::vector<SPartition> PartitionsVec;
  };

  void MarkAsUsed(u64 offset, u64 size);
  void MarkAsUsedE(u64 partition_data_offset, u64 offset, u64 size);
  bool ReadFromVolume(u64 offset, u32& buffer, bool decrypt);
  bool ReadFromVolume(u64 offset, u64& buffer, bool decrypt);
  bool ParseDisc();
  bool ParsePartitionData(SPartition& partition);

  std::string m_Filename;
  std::unique_ptr<IVolume> m_disc;

  std::array<SPartitionGroup, 4> PartitionGroup{};

  std::vector<u8> m_FreeTable;
  u64 m_FileSize = 0;
  u64 m_BlockCount = 0;
  u32 m_BlockSize = 0;
  bool m_isScrubbing = false;
};

}  // namespace DiscIO
