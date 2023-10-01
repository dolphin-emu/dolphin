// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <string_view>

#include "Common/CommonTypes.h"
#include "DiscIO/Blob.h"

namespace DiscIO
{
class FileInfo;
struct Partition;
class Volume;

class VolumeFileBlobReader final : public BlobReader
{
public:
  static std::unique_ptr<VolumeFileBlobReader>
  Create(const Volume& volume, const Partition& partition, std::string_view file_path);

  BlobType GetBlobType() const override { return BlobType::PLAIN; }
  std::unique_ptr<BlobReader> CopyReader() const override;

  u64 GetRawSize() const override;
  u64 GetDataSize() const override;
  DataSizeType GetDataSizeType() const override { return DataSizeType::Accurate; }

  u64 GetBlockSize() const override;
  bool HasFastRandomAccessInBlock() const override;
  std::string GetCompressionMethod() const override;
  std::optional<int> GetCompressionLevel() const override;

  bool Read(u64 offset, u64 length, u8* out_ptr) override;

private:
  VolumeFileBlobReader(const Volume& volume, const Partition& partition,
                       std::unique_ptr<FileInfo> file_info);

  const Volume& m_volume;
  const Partition& m_partition;
  std::unique_ptr<FileInfo> m_file_info;
};
}  // namespace DiscIO
