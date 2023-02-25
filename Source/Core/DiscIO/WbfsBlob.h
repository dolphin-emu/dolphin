// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/IOFile.h"
#include "DiscIO/Blob.h"

namespace DiscIO
{
static constexpr u32 WBFS_MAGIC = 0x53464257;  // "WBFS" (byteswapped to little endian)

class WbfsFileReader : public BlobReader
{
public:
  ~WbfsFileReader();

  static std::unique_ptr<WbfsFileReader> Create(File::IOFile file, const std::string& path);

  BlobType GetBlobType() const override { return BlobType::WBFS; }

  u64 GetRawSize() const override { return m_size; }
  u64 GetDataSize() const override;
  DataSizeType GetDataSizeType() const override { return DataSizeType::UpperBound; }

  u64 GetBlockSize() const override { return m_wbfs_sector_size; }
  bool HasFastRandomAccessInBlock() const override { return true; }
  std::string GetCompressionMethod() const override { return {}; }
  std::optional<int> GetCompressionLevel() const override { return std::nullopt; }

  bool Read(u64 offset, u64 nbytes, u8* out_ptr) override;

private:
  WbfsFileReader(File::IOFile file, const std::string& path);

  void OpenAdditionalFiles(const std::string& path);
  bool AddFileToList(File::IOFile file);
  bool ReadHeader();

  File::IOFile& SeekToCluster(u64 offset, u64* available);
  bool IsGood() { return m_good; }
  struct FileEntry
  {
    FileEntry(File::IOFile file_, u64 base_address_, u64 size_)
        : file(std::move(file_)), base_address(base_address_), size(size_)
    {
    }

    File::IOFile file;
    u64 base_address;
    u64 size;
  };

  std::vector<FileEntry> m_files;

  u64 m_size;

  u64 m_hd_sector_size;
  u64 m_wbfs_sector_size;
  u64 m_wbfs_sector_count;
  u64 m_disc_info_size;

#pragma pack(1)
  struct WbfsHeader
  {
    u32 magic;
    u32 hd_sector_count;
    u8 hd_sector_shift;
    u8 wbfs_sector_shift;
    u8 padding[2];
    u8 disc_table[500];
  } m_header;
#pragma pack()

  std::vector<u16> m_wlba_table;
  u64 m_blocks_per_disc;

  bool m_good;
};

}  // namespace DiscIO
