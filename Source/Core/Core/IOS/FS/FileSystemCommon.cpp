// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/FS/FileSystem.h"

#include <algorithm>
#include <array>
#include <expected>

#include "Common/Assert.h"
#include "Common/FileUtil.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/FS/HostBackend/FS.h"

namespace IOS::HLE::FS
{
constexpr u32 BUFFER_CHUNK_SIZE = 65536;

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
         !std::ranges::any_of(filename, [](char c) { return c == '/'; });
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
  if (Result<FileHandle> file = OpenFile(uid, gid, path, Mode::ReadWrite))
    return file;

  const ResultCode result = CreateFile(uid, gid, path, 0, modes);
  if (result != ResultCode::Success)
    return std::unexpected{result};

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
    if (!metadata && metadata.error() != ResultCode::NotFound)
      return metadata.error();
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

void FileSystem::DoStateRead(PointerWrap& p, const std::string& directory_path)
{
  const ResultCode delete_result = Delete(0, 0, directory_path);
  if (delete_result != ResultCode::Success && delete_result != ResultCode::NotFound)
  {
    ERROR_LOG_FMT(IOS_FS, "DoStateRead failed to call Delete: {}", delete_result);
    p.SetVerifyMode();
    return;
  }

  Metadata metadata{};
  p.Do(metadata.uid);
  p.Do(metadata.gid);
  p.Do(metadata.attribute);
  p.Do(metadata.modes);
  p.Do(metadata.is_file);
  p.Do(metadata.size);
  p.Do(metadata.fst_index);

  const ResultCode create_directory_result = CreateDirectory(
      metadata.uid, metadata.gid, directory_path, metadata.attribute, metadata.modes);
  if (create_directory_result != ResultCode::Success)
  {
    ERROR_LOG_FMT(IOS_FS, "DoStateRead failed to call CreateDirectory: {}",
                  create_directory_result);
    p.SetVerifyMode();
    return;
  }

  // Now restore from the stream
  std::vector<std::string> children;
  p.DoEachElement(children, [this, &directory_path](PointerWrap& p_, std::string& child_name) {
    Metadata child_metadata;
    p_.Do(child_metadata);
    p_.Do(child_name);

    std::string child_path;
    child_path.reserve(directory_path.size() + child_name.size() + 1);
    child_path.append(directory_path);
    if (directory_path.back() != '/')
      child_path.push_back('/');
    child_path.append(child_name);

    if (child_metadata.is_file)
    {
      const ResultCode create_file_result =
          CreateFile(child_metadata.uid, child_metadata.gid, child_path, child_metadata.attribute,
                     child_metadata.modes);
      if (create_file_result != ResultCode::Success)
      {
        ERROR_LOG_FMT(IOS_FS, "DoStateRead failed to call CreateFile for {}: {}", child_name,
                      create_file_result);
        p_.SetVerifyMode();
        return;
      }

      std::array<u8, BUFFER_CHUNK_SIZE> buffer;
      Result<FileHandle> handle = OpenFile(0, 0, child_path, Mode::Write);
      if (!handle)
      {
        ERROR_LOG_FMT(IOS_FS, "DoStateRead failed to call OpenFile for {}: {}", child_name,
                      handle.error());
        p_.SetVerifyMode();
        return;
      }

      u32 i = 0;
      while (i < child_metadata.size)
      {
        const u32 bytes_to_write =
            std::min(child_metadata.size - i, static_cast<u32>(buffer.size()));
        p_.DoArray(buffer.data(), bytes_to_write);

        Result<size_t> write_result = handle->Write(buffer.data(), bytes_to_write);
        if (!write_result)
        {
          ERROR_LOG_FMT(IOS_FS, "DoStateRead failed to call Write for {}: {}", child_name,
                        write_result.error());
          p_.SetVerifyMode();
          return;
        }
        if (*write_result != bytes_to_write)
        {
          ERROR_LOG_FMT(IOS_FS, "DoStateRead tried to write {} bytes to {} but wrote {} bytes",
                        child_name, bytes_to_write, *write_result);
          p_.SetVerifyMode();
          return;
        }

        i += bytes_to_write;
      }
    }
    else
    {
      DoStateRead(p_, child_path);
    }
  });
}

void FileSystem::DoStateWriteOrMeasure(PointerWrap& p, const std::string& directory_path)
{
  const Result<Metadata> metadata = GetMetadata(0, 0, directory_path);
  if (!metadata)
  {
    ERROR_LOG_FMT(IOS_FS, "DoStateWriteOrMeasure failed to call GetMetadata: {}", metadata.error());
    p.SetVerifyMode();
    return;
  }
  p.Do(metadata->uid);
  p.Do(metadata->gid);
  p.Do(metadata->attribute);
  p.Do(metadata->modes);
  p.Do(metadata->is_file);
  p.Do(metadata->size);
  p.Do(metadata->fst_index);

  auto children = ReadDirectory(0, 0, directory_path);
  if (!children)
  {
    ERROR_LOG_FMT(IOS_FS, "DoStateWriteOrMeasure failed to call ReadDirectory: {}",
                  children.error());
    p.SetVerifyMode();
    return;
  }

  p.DoEachElement(*children, [this, &directory_path](PointerWrap& p_, std::string& child_name) {
    std::string child_path;
    child_path.reserve(directory_path.size() + child_name.size() + 1);
    child_path.append(directory_path);
    if (directory_path.back() != '/')
      child_path.push_back('/');
    child_path.append(child_name);

    Result<Metadata> child_metadata = GetMetadata(0, 0, child_path);
    if (!child_metadata)
    {
      ERROR_LOG_FMT(IOS_FS, "DoStateWriteOrMeasure failed to call GetMetadata for {}: {}",
                    child_name, child_metadata.error());
      p_.SetVerifyMode();
      return;
    }

    p_.Do(*child_metadata);
    p_.Do(child_name);

    if (child_metadata->is_file)
    {
      std::array<u8, BUFFER_CHUNK_SIZE> buffer;
      Result<FileHandle> handle = OpenFile(0, 0, child_path, Mode::Read);
      if (!handle)
      {
        ERROR_LOG_FMT(IOS_FS, "DoStateWriteOrMeasure failed to call OpenFile for {}: {}",
                      child_name, handle.error());
        p_.SetVerifyMode();
        return;
      }

      u32 i = 0;
      while (i < child_metadata->size)
      {
        const u32 bytes_to_read =
            std::min(child_metadata->size - i, static_cast<u32>(buffer.size()));
        Result<size_t> read_result = handle->Read(buffer.data(), bytes_to_read);
        if (!read_result)
        {
          ERROR_LOG_FMT(IOS_FS, "DoStateWriteOrMeasure failed to call Read for {}: {}", child_name,
                        read_result.error());
          p_.SetVerifyMode();
          return;
        }
        if (*read_result != bytes_to_read)
        {
          ERROR_LOG_FMT(IOS_FS,
                        "DoStateWriteOrMeasure tried to read {} bytes from {} but got {} bytes",
                        child_name, bytes_to_read, *read_result);
          p_.SetVerifyMode();
          return;
        }

        p_.DoArray(buffer.data(), bytes_to_read);
        i += bytes_to_read;
      }
    }
    else
    {
      DoStateWriteOrMeasure(p_, child_path);
    }
  });
}
}  // namespace IOS::HLE::FS
