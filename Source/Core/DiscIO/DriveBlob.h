// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/IOFile.h"
#include "DiscIO/Blob.h"

#ifdef _WIN32
#include <windows.h>
#include <winioctl.h>
#endif

namespace DiscIO
{
class DriveReader : public SectorReader
{
public:
  static std::unique_ptr<DriveReader> Create(const std::string& drive);
  ~DriveReader();

  BlobType GetBlobType() const override { return BlobType::DRIVE; }

  u64 GetRawSize() const override { return m_size; }
  u64 GetDataSize() const override { return m_size; }
  DataSizeType GetDataSizeType() const override { return DataSizeType::Accurate; }

  u64 GetBlockSize() const override { return ECC_BLOCK_SIZE; }
  bool HasFastRandomAccessInBlock() const override { return false; }
  std::string GetCompressionMethod() const override { return {}; }
  std::optional<int> GetCompressionLevel() const override { return std::nullopt; }

private:
  DriveReader(const std::string& drive);
  bool GetBlock(u64 block_num, u8* out_ptr) override;
  bool ReadMultipleAlignedBlocks(u64 block_num, u64 num_blocks, u8* out_ptr) override;

#ifdef _WIN32
  HANDLE m_disc_handle = INVALID_HANDLE_VALUE;
  PREVENT_MEDIA_REMOVAL m_lock_cdrom;
  bool IsOK() const { return m_disc_handle != INVALID_HANDLE_VALUE; }
#else
  File::IOFile m_file;
  bool IsOK() const { return m_file.IsOpen() && m_file.IsGood(); }
#endif
  static constexpr u64 ECC_BLOCK_SIZE = 0x8000;
  u64 m_size = 0;
};

}  // namespace DiscIO
