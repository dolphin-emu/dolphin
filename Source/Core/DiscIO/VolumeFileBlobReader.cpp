// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DiscIO/VolumeFileBlobReader.h"

#include <memory>
#include <string_view>

#include "DiscIO/Filesystem.h"
#include "DiscIO/Volume.h"

namespace DiscIO
{
std::unique_ptr<VolumeFileBlobReader> VolumeFileBlobReader::Create(const Volume& volume,
                                                                   const Partition& partition,
                                                                   std::string_view file_path)
{
  const FileSystem* file_system = volume.GetFileSystem(partition);
  if (!file_system)
    return nullptr;

  std::unique_ptr<FileInfo> file_info = file_system->FindFileInfo(file_path);
  if (!file_info || file_info->IsDirectory())
    return nullptr;

  return std::unique_ptr<VolumeFileBlobReader>{
      new VolumeFileBlobReader(volume, partition, std::move(file_info))};
}

VolumeFileBlobReader::VolumeFileBlobReader(const Volume& volume, const Partition& partition,
                                           std::unique_ptr<FileInfo> file_info)
    : m_volume(volume), m_partition(partition), m_file_info(std::move(file_info))
{
}

std::unique_ptr<BlobReader> VolumeFileBlobReader::CopyReader() const
{
  ASSERT_MSG(DISCIO, false, "Unimplemented");
  return nullptr;
}

u64 VolumeFileBlobReader::GetDataSize() const
{
  return m_file_info->GetSize();
}

u64 VolumeFileBlobReader::GetRawSize() const
{
  return GetDataSize();
}

u64 VolumeFileBlobReader::GetBlockSize() const
{
  return m_volume.GetBlobReader().GetBlockSize();
}

bool VolumeFileBlobReader::HasFastRandomAccessInBlock() const
{
  return m_volume.GetBlobReader().HasFastRandomAccessInBlock();
}

std::string VolumeFileBlobReader::GetCompressionMethod() const
{
  return m_volume.GetBlobReader().GetCompressionMethod();
}

std::optional<int> VolumeFileBlobReader::GetCompressionLevel() const
{
  return m_volume.GetBlobReader().GetCompressionLevel();
}

bool VolumeFileBlobReader::Read(u64 offset, u64 length, u8* out_ptr)
{
  if (offset + length > m_file_info->GetSize())
    return false;

  return m_volume.Read(m_file_info->GetOffset() + offset, length, out_ptr, m_partition);
}
}  // namespace DiscIO
