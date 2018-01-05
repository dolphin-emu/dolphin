// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DiscIO/Filesystem.h"
#include <memory>
#include "DiscIO/FileSystemGCWii.h"
#include "DiscIO/Volume.h"

namespace DiscIO
{
IFileSystem::IFileSystem(const IVolume* _rVolume, const Partition& partition)
    : m_rVolume(_rVolume), m_partition(partition)
{
}

IFileSystem::~IFileSystem()
{
}

std::unique_ptr<IFileSystem> CreateFileSystem(const IVolume* volume, const Partition& partition)
{
  if (!volume)
    return nullptr;

  std::unique_ptr<IFileSystem> filesystem = std::make_unique<CFileSystemGCWii>(volume, partition);

  if (!filesystem)
    return nullptr;

  if (!filesystem->IsValid())
    filesystem.reset();

  return filesystem;
}

}  // namespace
