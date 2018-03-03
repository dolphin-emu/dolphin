// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/FS/FileSystem.h"

#include "Core/IOS/FS/HostBackend/FS.h"

namespace IOS::HLE::FS
{
std::unique_ptr<FileSystem> MakeFileSystem()
{
  return std::make_unique<HostFileSystem>();
}

void FileSystem::Init()
{
  if (Delete(0, 0, "/tmp") == ResultCode::Success)
    CreateDirectory(0, 0, "/tmp", 0, Mode::ReadWrite, Mode::ReadWrite, Mode::ReadWrite);
}
}  // namespace IOS::HLE::FS
