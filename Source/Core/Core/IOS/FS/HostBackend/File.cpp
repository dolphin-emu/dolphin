// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/FS/HostBackend/FS.h"

#include <algorithm>
#include <memory>

#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"

namespace IOS::HLE::FS
{
// This isn't theadsafe, but it's only called from the CPU thread.
std::shared_ptr<File::IOFile> HostFileSystem::OpenHostFile(const std::string& host_path)
{
  // On the wii, all file operations are strongly ordered.
  // If a game opens the same file twice (or 8 times, looking at you PokePark Wii)
  // and writes to one file handle, it will be able to immediately read the written
  // data from the other handle.
  // On 'real' operating systems, there are various buffers and caches meaning
  // applications doing such naughty things will not get expected results.

  // So we fix this by catching any attempts to open the same file twice and
  // only opening one file. Accesses to a single file handle are ordered.
  //
  // Hall of Shame:
  //    - PokePark Wii (gets stuck on the loading screen of Pikachu falling)
  //    - PokePark 2 (Also gets stuck while loading)
  //    - Wii System Menu (Can't access the system settings, gets stuck on blank screen)
  //    - The Beatles: Rock Band (saving doesn't work)

  // Check if the file has already been opened.
  auto search = m_open_files.find(host_path);
  if (search != m_open_files.end())
  {
    // Lock a shared pointer to use.
    return search->second.lock();
  }

  // All files are opened read/write. Actual access rights will be controlled per handle by the
  // read/write functions below
  File::IOFile file;
  while (!file.Open(host_path, "r+b"))
  {
    const bool try_again =
        PanicYesNoFmt("File \"{}\" could not be opened!\n"
                      "This may happen with improper permissions or use by another process.\n"
                      "Press \"Yes\" to make another attempt.",
                      host_path);

    if (!try_again)
    {
      // We've failed to open the file:
      ERROR_LOG_FMT(IOS_FS, "OpenHostFile {}", host_path);
      return nullptr;
    }
  }

  // This code will be called when all references to the shared pointer below have been removed.
  auto deleter = [this, host_path](File::IOFile* ptr) {
    delete ptr;                     // IOFile's deconstructor closes the file.
    m_open_files.erase(host_path);  // erase the weak pointer from the list of open files.
  };

  // Use the custom deleter from above.
  std::shared_ptr<File::IOFile> file_ptr(new File::IOFile(std::move(file)), deleter);

  // Store a weak pointer to our newly opened file in the cache.
  m_open_files[host_path] = std::weak_ptr<File::IOFile>(file_ptr);

  return file_ptr;
}

Result<FileHandle> HostFileSystem::OpenFile(Uid, Gid, const std::string& path, Mode mode)
{
  Handle* handle = AssignFreeHandle();
  if (!handle)
    return ResultCode::NoFreeHandle;

  const std::string host_path = BuildFilename(path).host_path;
  if (!File::IsFile(host_path))
  {
    *handle = Handle{};
    return ResultCode::NotFound;
  }

  handle->host_file = OpenHostFile(host_path);
  if (!handle->host_file)
  {
    *handle = Handle{};
    return ResultCode::AccessDenied;
  }

  handle->wii_path = path;
  handle->mode = mode;
  handle->file_offset = 0;
  return FileHandle{this, ConvertHandleToFd(handle)};
}

ResultCode HostFileSystem::Close(Fd fd)
{
  Handle* handle = GetHandleFromFd(fd);
  if (!handle)
    return ResultCode::Invalid;

  // Let go of our pointer to the file, it will automatically close if we are the last handle
  // accessing it.
  *handle = Handle{};
  return ResultCode::Success;
}

Result<u32> HostFileSystem::ReadBytesFromFile(Fd fd, u8* ptr, u32 count)
{
  Handle* handle = GetHandleFromFd(fd);
  if (!handle || !handle->host_file->IsOpen())
    return ResultCode::Invalid;

  if ((u8(handle->mode) & u8(Mode::Read)) == 0)
    return ResultCode::AccessDenied;

  const u32 file_size = static_cast<u32>(handle->host_file->GetSize());
  // IOS has this check in the read request handler.
  if (count + handle->file_offset > file_size)
    count = file_size - handle->file_offset;

  // File might be opened twice, need to seek before we read
  handle->host_file->Seek(handle->file_offset, File::SeekOrigin::Begin);
  const u32 actually_read = static_cast<u32>(fread(ptr, 1, count, handle->host_file->GetHandle()));

  if (actually_read != count && ferror(handle->host_file->GetHandle()))
    return ResultCode::AccessDenied;

  // IOS returns the number of bytes read and adds that value to the seek position,
  // instead of adding the *requested* read length.
  handle->file_offset += actually_read;
  return actually_read;
}

Result<u32> HostFileSystem::WriteBytesToFile(Fd fd, const u8* ptr, u32 count)
{
  Handle* handle = GetHandleFromFd(fd);
  if (!handle || !handle->host_file->IsOpen())
    return ResultCode::Invalid;

  if ((u8(handle->mode) & u8(Mode::Write)) == 0)
    return ResultCode::AccessDenied;

  // File might be opened twice, need to seek before we read
  handle->host_file->Seek(handle->file_offset, File::SeekOrigin::Begin);
  if (!handle->host_file->WriteBytes(ptr, count))
    return ResultCode::AccessDenied;

  handle->file_offset += count;
  return count;
}

Result<u32> HostFileSystem::SeekFile(Fd fd, std::uint32_t offset, SeekMode mode)
{
  Handle* handle = GetHandleFromFd(fd);
  if (!handle || !handle->host_file->IsOpen())
    return ResultCode::Invalid;

  u32 new_position = 0;
  switch (mode)
  {
  case SeekMode::Set:
    new_position = offset;
    break;
  case SeekMode::Current:
    new_position = handle->file_offset + offset;
    break;
  case SeekMode::End:
    new_position = handle->host_file->GetSize() + offset;
    break;
  default:
    return ResultCode::Invalid;
  }

  // This differs from POSIX behaviour which allows seeking past the end of the file.
  if (handle->host_file->GetSize() < new_position)
    return ResultCode::Invalid;

  handle->file_offset = new_position;
  return handle->file_offset;
}

Result<FileStatus> HostFileSystem::GetFileStatus(Fd fd)
{
  const Handle* handle = GetHandleFromFd(fd);
  if (!handle || !handle->host_file->IsOpen())
    return ResultCode::Invalid;

  FileStatus status;
  status.size = handle->host_file->GetSize();
  status.offset = handle->file_offset;
  return status;
}

HostFileSystem::Handle* HostFileSystem::AssignFreeHandle()
{
  const auto it = std::find_if(m_handles.begin(), m_handles.end(),
                               [](const Handle& handle) { return !handle.opened; });
  if (it == m_handles.end())
    return nullptr;

  *it = Handle{};
  it->opened = true;
  return &*it;
}

HostFileSystem::Handle* HostFileSystem::GetHandleFromFd(Fd fd)
{
  if (fd >= m_handles.size() || !m_handles[fd].opened)
    return nullptr;
  return &m_handles[fd];
}

Fd HostFileSystem::ConvertHandleToFd(const Handle* handle) const
{
  return handle - m_handles.data();
}

}  // namespace IOS::HLE::FS
