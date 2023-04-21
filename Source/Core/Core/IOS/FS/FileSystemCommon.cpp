// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/FS/FileSystem.h"

#include <algorithm>

#include "Common/Assert.h"
#include "Common/FileUtil.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/FS/HostBackend/FS.h"

namespace IOS::HLE::FS
{
bool IsValidPath(std::string_view path)
{
  return path == "/" || IsValidNonRootPath(path);
}

bool IsValidNonRootPath(std::string_view path)
{
  return path.length() > 1 && path.length() <= MaxPathLength && path[0] == '/' &&
         path.back() != '/';
}

bool IsValidFilename(std::string_view filename)
{
  return filename.length() <= MaxFilenameLength &&
         !std::any_of(filename.begin(), filename.end(), [](char c) { return c == '/'; });
}

SplitPathResult SplitPathAndBasename(std::string_view path)
{
  const auto last_separator = path.rfind('/');
  return {std::string(path.substr(0, std::max<size_t>(1, last_separator))),
          std::string(path.substr(last_separator + 1))};
}

std::unique_ptr<FileSystem> MakeFileSystem(Location location,
                                           std::vector<NandRedirect> nand_redirects)
{
  const std::string nand_root =
      File::GetUserPath(location == Location::Session ? D_SESSION_WIIROOT_IDX : D_WIIROOT_IDX);
  return std::make_unique<HostFileSystem>(nand_root, std::move(nand_redirects));
}

IOS::HLE::ReturnCode ConvertResult(ResultCode code)
{
  if (code == ResultCode::Success)
    return IPC_SUCCESS;
  // FS error codes start at -100. Since result codes in the enum are listed in the same way
  // as the IOS codes, we just need to return -100-code.
  return static_cast<ReturnCode>(-(static_cast<s32>(code) + 100));
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
  if (std::tie(m_fs, m_fd) != std::tie(other.m_fs, other.m_fd))
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

Result<u32> FileHandle::Seek(u32 offset, SeekMode mode) const
{
  return m_fs->SeekFile(*m_fd, offset, mode);
}

Result<FileStatus> FileHandle::GetStatus() const
{
  return m_fs->GetFileStatus(*m_fd);
}

Result<FileHandle> FileSystem::CreateAndOpenFile(Uid uid, Gid gid, const std::string& path,
                                                 Modes modes)
{
  Result<FileHandle> file = OpenFile(uid, gid, path, Mode::ReadWrite);
  if (file.Succeeded())
    return file;

  const ResultCode result = CreateFile(uid, gid, path, 0, modes);
  if (result != ResultCode::Success)
    return result;

  return OpenFile(uid, gid, path, Mode::ReadWrite);
}

ResultCode FileSystem::CreateFullPath(Uid uid, Gid gid, const std::string& path,
                                      FileAttribute attribute, Modes modes)
{
  std::string::size_type position = 1;
  while (true)
  {
    position = path.find('/', position);
    if (position == std::string::npos)
      return ResultCode::Success;

    const std::string subpath = path.substr(0, position);
    const Result<Metadata> metadata = GetMetadata(uid, gid, subpath);
    if (!metadata && metadata.Error() != ResultCode::NotFound)
      return metadata.Error();
    if (metadata && metadata->is_file)
      return ResultCode::Invalid;

    if (!metadata)
    {
      const ResultCode result = CreateDirectory(uid, gid, subpath, attribute, modes);
      if (result != ResultCode::Success)
        return result;
    }

    ++position;
  }
}
}  // namespace IOS::HLE::FS
