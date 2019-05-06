// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/File.h"
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
  bool IsDataSizeAccurate() const override { return true; }

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
  u64 m_size = 0;
};

}  // namespace DiscIO
