// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstdio>
#include <memory>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/IOFile.h"
#include "DiscIO/Blob.h"

namespace DiscIO
{
class PlainFileReader : public BlobReader
{
public:
  static std::unique_ptr<PlainFileReader> Create(File::IOFile file);

  BlobType GetBlobType() const override { return BlobType::PLAIN; }
  std::unique_ptr<BlobReader> CopyReader() const override;

  u64 GetRawSize() const override { return m_size; }
  u64 GetDataSize() const override { return m_size; }
  DataSizeType GetDataSizeType() const override { return DataSizeType::Accurate; }

  u64 GetBlockSize() const override { return 0; }
  bool HasFastRandomAccessInBlock() const override { return true; }
  std::string GetCompressionMethod() const override { return {}; }
  std::optional<int> GetCompressionLevel() const override { return std::nullopt; }

  bool Read(u64 offset, u64 nbytes, u8* out_ptr) override;

private:
  PlainFileReader(File::IOFile file);

  File::IOFile m_file;
  u64 m_size;
};

}  // namespace DiscIO
