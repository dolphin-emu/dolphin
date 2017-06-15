// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DiscIO/Filesystem.h"
#include <memory>
#include "DiscIO/FileSystemGCWii.h"
#include "DiscIO/Volume.h"

namespace DiscIO
{
FileInfo::~FileInfo() = default;

FileSystem::FileSystem(const Volume* volume, const Partition& partition)
    : m_volume(volume), m_partition(partition)
{
}

FileSystem::~FileSystem() = default;

std::unique_ptr<FileSystem> CreateFileSystem(const Volume* volume, const Partition& partition)
{
  if (!volume)
    return nullptr;

  std::unique_ptr<FileSystem> filesystem = std::make_unique<FileSystemGCWii>(volume, partition);

  if (!filesystem)
    return nullptr;

  if (!filesystem->IsValid())
    filesystem.reset();

  return filesystem;
}

}  // namespace
