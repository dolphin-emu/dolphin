// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>

#include "Common/CommonTypes.h"
#include "DiscIO/Blob.h"

namespace DiscIO
{
class FileInfo;
class FileSystem;
class Volume;

class VolumeFileBlobReader final : public BlobReader
{
public:
  static std::unique_ptr<VolumeFileBlobReader>
  Create(const Volume& volume, const FileSystem& file_system, const std::string& file_path);

  BlobType GetBlobType() const override { return BlobType::PLAIN; }
  u64 GetDataSize() const override;
  u64 GetRawSize() const override;
  bool Read(u64 offset, u64 length, u8* out_ptr) override;

private:
  VolumeFileBlobReader(const Volume& volume, const FileSystem& file_system,
                       std::unique_ptr<FileInfo> file_info);

  const Volume& m_volume;
  const FileSystem& m_file_system;
  std::unique_ptr<FileInfo> m_file_info;
};
}  // namespace
