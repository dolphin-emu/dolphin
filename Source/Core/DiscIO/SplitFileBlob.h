// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/IOFile.h"
#include "DiscIO/Blob.h"

namespace DiscIO
{
class SplitPlainFileReader final : public BlobReader
{
public:
  static std::unique_ptr<SplitPlainFileReader> Create(std::string_view first_file_path);

  BlobType GetBlobType() const override { return BlobType::SPLIT_PLAIN; }
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
  struct SingleFile
  {
    File::IOFile file;
    u64 offset;
    u64 size;
  };

  SplitPlainFileReader(std::vector<SingleFile> m_files);

  std::vector<SingleFile> m_files;
  u64 m_size;
};

}  // namespace DiscIO
