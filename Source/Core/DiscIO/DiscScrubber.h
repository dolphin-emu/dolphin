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
class FileInfo;
class Volume;
struct Partition;

class DiscScrubber final
{
public:
  DiscScrubber();
  ~DiscScrubber();

  bool SetupScrub(const std::string& filename, int block_size);
  size_t GetNextBlock(File::IOFile& in, u8* buffer);

private:
  struct PartitionHeader final
  {
    u8* ticket[0x2a4];
    u32 tmd_size;
    u64 tmd_offset;
    u32 cert_chain_size;
    u64 cert_chain_offset;
    // H3Size is always 0x18000
    u64 h3_offset;
    u64 data_offset;
    u64 data_size;
    // TMD would be here
    u64 dol_offset;
    u64 dol_size;
    u64 fst_offset;
    u64 fst_size;
    u32 apploader_size;
    u32 apploader_trailer_size;
  };

  void MarkAsUsed(u64 offset, u64 size);
  void MarkAsUsedE(u64 partition_data_offset, u64 offset, u64 size);
  bool ReadFromVolume(u64 offset, u32& buffer, const Partition& partition);
  bool ReadFromVolume(u64 offset, u64& buffer, const Partition& partition);
  bool ParseDisc();
  bool ParsePartitionData(const Partition& partition, PartitionHeader* header);
  void ParseFileSystemData(u64 partition_data_offset, const FileInfo& directory);

  std::string m_filename;
  std::unique_ptr<Volume> m_disc;

  std::vector<u8> m_free_table;
  u64 m_file_size = 0;
  u64 m_block_count = 0;
  u32 m_block_size = 0;
  bool m_is_scrubbing = false;
};

}  // namespace DiscIO
