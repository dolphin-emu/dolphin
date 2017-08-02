// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DiscIO/Filesystem.h"
#include <memory>
#include "DiscIO/Volume.h"

namespace DiscIO
{
FileInfo::~FileInfo() = default;

FileSystem::FileSystem(const Volume* volume, const Partition& partition)
    : m_volume(volume), m_partition(partition)
{
}

FileSystem::~FileSystem() = default;

}  // namespace
