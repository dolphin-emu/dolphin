// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// DiscScrubber removes the pseudorandom padding data from discs

// Note: the technique is inspired by Wiiscrubber, but much simpler - intentionally :)

#pragma once

#include <array>
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

  bool SetupScrub(const Volume* disc);

  // Returns true if the specified 32 KiB block only contains unused data
  bool CanBlockBeScrubbed(u64 offset) const;

  static constexpr size_t CLUSTER_SIZE = 0x8000;

private:
  void MarkAsUsed(u64 offset, u64 size);
  void MarkAsUsedE(u64 partition_data_offset, u64 offset, u64 size);
  u64 ToClusterOffset(u64 offset) const;
  bool ReadFromVolume(u64 offset, u32& buffer, const Partition& partition);
  bool ReadFromVolume(u64 offset, u64& buffer, const Partition& partition);
  bool ParseDisc();
  bool ParsePartitionData(const Partition& partition);
  void ParseFileSystemData(u64 partition_data_offset, const FileInfo& directory);

  const Volume* m_disc = nullptr;

  std::vector<u8> m_free_table;
  u64 m_file_size = 0;
  bool m_is_scrubbing = false;
};

}  // namespace DiscIO
