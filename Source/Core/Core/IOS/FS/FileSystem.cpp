// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/FS/FileSystem.h"

#include "Common/Assert.h"
#include "Common/FileUtil.h"
#include "Core/IOS/FS/HostBackend/FS.h"

namespace IOS::HLE::FS
{
std::unique_ptr<FileSystem> MakeFileSystem(Location location)
{
  const std::string nand_root =
      File::GetUserPath(location == Location::Session ? D_SESSION_WIIROOT_IDX : D_WIIROOT_IDX);
  return std::make_unique<HostFileSystem>(nand_root);
}

FileHandle::FileHandle(FileSystem* fs, Fd fd) : m_fs{fs}, m_fd{fd}
{
}

FileHandle::FileHandle(FileHandle&& other) : m_fs{other.m_fs}, m_fd{other.m_fd}
{
  other.m_fd.reset();
}

FileHandle& FileHandle::operator=(FileHandle&& other)
{
  if (*this != other)
    *this = std::move(other);
  return *this;
}

FileHandle::~FileHandle()
{
  if (m_fd && m_fs)
    ASSERT(m_fs->Close(*m_fd) == FS::ResultCode::Success);
}

Fd FileHandle::Release()
{
  const Fd fd = m_fd.value();
  m_fd.reset();
  return fd;
}

void FileSystem::Init()
{
  if (Delete(0, 0, "/tmp") == ResultCode::Success)
    CreateDirectory(0, 0, "/tmp", 0, Mode::ReadWrite, Mode::ReadWrite, Mode::ReadWrite);
}
}  // namespace IOS::HLE::FS
