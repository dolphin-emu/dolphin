// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/IOS/WFS/WFSSRV.h"

#include <algorithm>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"
#include "Common/NandPaths.h"
#include "Core/HW/Memmap.h"

namespace IOS::HLE
{
namespace WFS
{
std::string NativePath(const std::string& wfs_path)
{
  return File::GetUserPath(D_WFSROOT_IDX) + Common::EscapePath(wfs_path);
}
}  // namespace WFS

WFSSRVDevice::WFSSRVDevice(Kernel& ios, const std::string& device_name) : Device(ios, device_name)
{
  m_device_name = "msc01";
}

std::optional<IPCReply> WFSSRVDevice::IOCtl(const IOCtlRequest& request)
{
  int return_error_code = IPC_SUCCESS;

  switch (request.request)
  {
  case IOCTL_WFS_INIT:
    // TODO(wfs): Implement.
    INFO_LOG_FMT(IOS_WFS, "IOCTL_WFS_INIT");
    break;

  case IOCTL_WFS_UNKNOWN_8:
    // TODO(wfs): Figure out what this actually does.
    INFO_LOG_FMT(IOS_WFS, "IOCTL_WFS_UNKNOWN_8");
    Memory::Write_U8(7, request.buffer_out);
    Memory::CopyToEmu(request.buffer_out + 1, "msc01\x00\x00\x00", 8);
    break;

  case IOCTL_WFS_SHUTDOWN:
    INFO_LOG_FMT(IOS_WFS, "IOCTL_WFS_SHUTDOWN");

    // Close all hanging attach/detach ioctls with an appropriate error code.
    for (auto address : m_hanging)
    {
      IOCtlRequest hanging_request{address};
      Memory::Write_U32(0x80000000, hanging_request.buffer_out);
      Memory::Write_U32(0, hanging_request.buffer_out + 4);
      Memory::Write_U32(0, hanging_request.buffer_out + 8);
      m_ios.EnqueueIPCReply(hanging_request, 0);
    }
    break;

  case IOCTL_WFS_DEVICE_INFO:
    INFO_LOG_FMT(IOS_WFS, "IOCTL_WFS_DEVICE_INFO");
    Memory::Write_U64(16ull << 30, request.buffer_out);  // 16GB storage.
    Memory::Write_U8(4, request.buffer_out + 8);
    break;

  case IOCTL_WFS_GET_DEVICE_NAME:
  {
    INFO_LOG_FMT(IOS_WFS, "IOCTL_WFS_GET_DEVICE_NAME");
    Memory::Write_U8(static_cast<u8>(m_device_name.size()), request.buffer_out);
    Memory::CopyToEmu(request.buffer_out + 1, m_device_name.data(), m_device_name.size());
    break;
  }

  case IOCTL_WFS_ATTACH_DETACH_2:
    // TODO(wfs): Implement.
    INFO_LOG_FMT(IOS_WFS, "IOCTL_WFS_ATTACH_DETACH_2({})", request.request);
    Memory::Write_U32(1, request.buffer_out);
    Memory::Write_U32(0, request.buffer_out + 4);  // device id?
    Memory::Write_U32(0, request.buffer_out + 8);
    break;

  case IOCTL_WFS_ATTACH_DETACH:
    INFO_LOG_FMT(IOS_WFS, "IOCTL_WFS_ATTACH_DETACH({})", request.request);

    // Leave hanging, but we need to acknowledge the request at shutdown time.
    m_hanging.push_back(request.address);
    return std::nullopt;

  case IOCTL_WFS_FLUSH:
    // Nothing to do.
    INFO_LOG_FMT(IOS_WFS, "IOCTL_WFS_FLUSH: doing nothing");
    break;

  case IOCTL_WFS_MKDIR:
  {
    const std::string path = NormalizePath(
        Memory::GetString(request.buffer_in + 34, Memory::Read_U16(request.buffer_in + 32)));
    const std::string native_path = WFS::NativePath(path);

    if (File::Exists(native_path))
    {
      INFO_LOG_FMT(IOS_WFS, "IOCTL_WFS_MKDIR({}): already exists", path);
      return_error_code = WFS_EEXIST;
    }
    else if (!File::CreateDir(native_path))
    {
      INFO_LOG_FMT(IOS_WFS, "IOCTL_WFS_MKDIR({}): no such file or directory", path);
      return_error_code = WFS_ENOENT;
    }
    else
    {
      INFO_LOG_FMT(IOS_WFS, "IOCTL_WFS_MKDIR({}): directory created", path);
    }
    break;
  }

  // TODO(wfs): Handle more patterns (e.g. with '?')
  case IOCTL_WFS_GLOB_START:
  {
    const std::string pattern = NormalizePath(
        Memory::GetString(request.buffer_in + 0x2, Memory::Read_U16(request.buffer_in)));
    const size_t size = pattern.size();
    if (size < 2 || pattern[size - 1] != '*' || pattern[size - 2] != '/')
    {
      ERROR_LOG_FMT(IOS_WFS, "IOCTL_WFS_GLOB_START({}): unknown pattern", pattern);
      return_error_code = WFS_EINVAL;
      break;
    }
    const std::string path = pattern.substr(0, size - 2);
    const std::string native_path = WFS::NativePath(path);

    File::FSTEntry entry;
    if (path == "/vol" || path == "/dev")
    {
      entry.isDirectory = true;
      entry.size = 1;
      entry.physicalName = native_path;
      entry.virtualName = path;
      entry.children.push_back(File::ScanDirectoryTree(native_path + "/" + m_device_name, false));
    }
    else if (File::IsDirectory(native_path))
    {
      entry = File::ScanDirectoryTree(native_path, false);
    }
    else
    {
      ERROR_LOG_FMT(IOS_WFS, "IOCTL_WFS_GLOB_START({}): no such directory", pattern);
      return_error_code = WFS_ENOENT;
      break;
    }

    const u16 dd = GetNewDescriptor<DirectoryDescriptor>();
    DirectoryDescriptor* dd_obj = &m_dds[dd];
    dd_obj->in_use = true;
    dd_obj->entry = entry;
    dd_obj->position = 0;

    INFO_LOG_FMT(IOS_WFS, "IOCTL_WFS_GLOB_START({}) -> {}", path, dd);
    return_error_code = GlobNext(request, dd_obj);
    Memory::Write_U16(dd, request.buffer_out + 0x114);
    break;
  }

  case IOCTL_WFS_GLOB_NEXT:
  {
    const u16 dd = Memory::Read_U16(request.buffer_in);

    DirectoryDescriptor* dd_obj = FindDescriptor<DirectoryDescriptor>(dd);
    if (dd_obj == nullptr)
    {
      ERROR_LOG_FMT(IOS_WFS, "IOCTL_WFS_GLOB_NEXT: invalid file descriptor {}", dd);
      return_error_code = WFS_EBADFD;
      break;
    }

    INFO_LOG_FMT(IOS_WFS, "IOCTL_WFS_GLOB_NEXT({})", dd);
    return_error_code = GlobNext(request, dd_obj);
    break;
  }

  case IOCTL_WFS_GLOB_END:
  {
    const u16 dd = Memory::Read_U16(request.buffer_in);
    INFO_LOG_FMT(IOS_WFS, "IOCTL_WFS_GLOB_END({})", dd);
    ReleaseDescriptor<DirectoryDescriptor>(dd);
    break;
  }

  case IOCTL_WFS_SET_HOMEDIR:
    m_home_directory =
        Memory::GetString(request.buffer_in + 2, Memory::Read_U16(request.buffer_in));
    INFO_LOG_FMT(IOS_WFS, "IOCTL_WFS_SET_HOMEDIR: {}", m_home_directory);
    break;

  case IOCTL_WFS_CHDIR:
    m_current_directory =
        Memory::GetString(request.buffer_in + 2, Memory::Read_U16(request.buffer_in));
    INFO_LOG_FMT(IOS_WFS, "IOCTL_WFS_CHDIR: {}", m_current_directory);
    break;

  case IOCTL_WFS_GET_HOMEDIR:
    INFO_LOG_FMT(IOS_WFS, "IOCTL_WFS_GET_HOMEDIR: {}", m_home_directory);
    Memory::Write_U16(static_cast<u16>(m_home_directory.size()), request.buffer_out);
    Memory::CopyToEmu(request.buffer_out + 2, m_home_directory.data(), m_home_directory.size());
    break;

  case IOCTL_WFS_GET_ATTRIBUTES:
  {
    const std::string path = NormalizePath(
        Memory::GetString(request.buffer_in + 2, Memory::Read_U16(request.buffer_in)));
    const std::string native_path = WFS::NativePath(path);

    Memory::Memset(0, request.buffer_out, request.buffer_out_size);
    if (!File::Exists(native_path))
    {
      INFO_LOG_FMT(IOS_WFS, "IOCTL_WFS_GET_ATTRIBUTES({}): no such file or directory", path);
      return_error_code = WFS_ENOENT;
    }
    else if (File::IsDirectory(native_path))
    {
      INFO_LOG_FMT(IOS_WFS, "IOCTL_WFS_GET_ATTRIBUTES({}): directory", path);
      Memory::Write_U32(0x80000000, request.buffer_out + 4);
    }
    else
    {
      const auto size = static_cast<u32>(File::GetSize(native_path));
      INFO_LOG_FMT(IOS_WFS, "IOCTL_WFS_GET_ATTRIBUTES({}): file with size {}", path, size);
      Memory::Write_U32(size, request.buffer_out);
    }
    break;
  }

  case IOCTL_WFS_DELETE:
  {
    const std::string path = NormalizePath(
        Memory::GetString(request.buffer_in + 0x2, Memory::Read_U16(request.buffer_in)));
    const std::string native_path = WFS::NativePath(path);

    const bool opened = std::any_of(m_fds.begin(), m_fds.end(),
                                    [&](const auto& fd) { return fd.in_use && fd.path == path; });
    if (opened)
    {
      ERROR_LOG_FMT(IOS_WFS, "IOCTL_WFS_DELETE({}): is opened", path);
      return_error_code = WFS_FILE_IS_OPENED;
      break;
    }

    if (!File::Exists(native_path))
    {
      ERROR_LOG_FMT(IOS_WFS, "IOCTL_WFS_DELETE({}): no such file or directory", path);
      return_error_code = WFS_ENOENT;
      break;
    }

    if (File::IsDirectory(native_path))
    {
      if (!File::DeleteDir(native_path, File::IfAbsentBehavior::NoConsoleWarning))
      {
        ERROR_LOG_FMT(IOS_WFS, "IOCTL_WFS_DELETE({}): directory deletion failed", path);
        return_error_code = WFS_EIO;
        break;
      }
    }
    else
    {
      if (!File::Delete(native_path, File::IfAbsentBehavior::NoConsoleWarning))
      {
        ERROR_LOG_FMT(IOS_WFS, "IOCTL_WFS_DELETE({}): file deletion failed", path);
        return_error_code = WFS_EIO;
        break;
      }
    }

    INFO_LOG_FMT(IOS_WFS, "IOCTL_WFS_DELETE({}): deletion succeeded", path);
    break;
  }

  case IOCTL_WFS_RENAME:
  case IOCTL_WFS_RENAME_2:
  {
    const std::string source_path =
        Memory::GetString(request.buffer_in + 2, Memory::Read_U16(request.buffer_in));
    const std::string dest_path =
        Memory::GetString(request.buffer_in + 512 + 2, Memory::Read_U16(request.buffer_in + 512));
    return_error_code = Rename(source_path, dest_path);
    break;
  }

  case IOCTL_WFS_CREATE_OPEN:
  case IOCTL_WFS_OPEN:
  {
    const char* ioctl_name =
        request.request == IOCTL_WFS_OPEN ? "IOCTL_WFS_OPEN" : "IOCTL_WFS_CREATE_OPEN";
    const std::string path = NormalizePath(
        Memory::GetString(request.buffer_in + 0x22, Memory::Read_U16(request.buffer_in + 0x20)));
    const std::string native_path = WFS::NativePath(path);

    if (request.request == IOCTL_WFS_OPEN)
    {
      if (!File::Exists(native_path))
      {
        ERROR_LOG_FMT(IOS_WFS, "{}({}): no such file or directory", ioctl_name, path);
        return_error_code = WFS_ENOENT;
        break;
      }
      if (!File::IsFile(native_path))
      {
        ERROR_LOG_FMT(IOS_WFS, "{}({}): is a directory", ioctl_name, path);
        return_error_code = WFS_EISDIR;
        break;
      }
    }
    else
    {
      if (File::Exists(native_path))
      {
        ERROR_LOG_FMT(IOS_WFS, "{}({}): already exists", ioctl_name, path);
        return_error_code = WFS_EEXIST;
        break;
      }
    }

    bool allow_reads = true, allow_writes = true;
    if (request.request == IOCTL_WFS_OPEN)
    {
      const u32 mode = Memory::Read_U32(request.buffer_in);
      if (mode == 0 || mode > 3)
      {
        ERROR_LOG_FMT(IOS_WFS, "{}({}): invalid mode {}", ioctl_name, path, mode);
        return_error_code = WFS_EINVAL;
        break;
      }
      allow_reads = mode & 1;
      allow_writes = mode & 2;
    }

    const u16 fd = GetNewDescriptor<FileDescriptor>();
    FileDescriptor* fd_obj = &m_fds[fd];
    fd_obj->in_use = true;
    fd_obj->path = path;
    fd_obj->allow_reads = allow_reads;
    fd_obj->allow_writes = allow_writes;
    fd_obj->must_create = request.request == IOCTL_WFS_CREATE_OPEN;
    fd_obj->position = 0;

    if (!fd_obj->Open())
    {
      ERROR_LOG_FMT(IOS_WFS, "{}({}): error opening file", ioctl_name, path);
      ReleaseDescriptor<FileDescriptor>(fd);
      return_error_code = WFS_EIO;
      break;
    }

    INFO_LOG_FMT(IOS_WFS, "{}({}) -> {}", ioctl_name, path, fd);
    if (request.request == IOCTL_WFS_OPEN)
    {
      Memory::Write_U16(fd, request.buffer_out + 0x14);
    }
    else
    {
      Memory::Write_U16(fd, request.buffer_out);
    }
    break;
  }

  case IOCTL_WFS_GET_SIZE:
  {
    const u16 fd = Memory::Read_U16(request.buffer_in);
    FileDescriptor* fd_obj = FindDescriptor<FileDescriptor>(fd);
    if (fd_obj == nullptr)
    {
      ERROR_LOG_FMT(IOS_WFS, "IOCTL_WFS_GET_SIZE: invalid file descriptor {}", fd);
      return_error_code = WFS_EBADFD;
      break;
    }

    const u64 size = fd_obj->file.GetSize();
    const u32 truncated_size = static_cast<u32>(size);
    INFO_LOG_FMT(IOS_WFS, "IOCTL_WFS_GET_SIZE({}) -> {}", fd, truncated_size);
    if (size != truncated_size)
    {
      ERROR_LOG_FMT(IOS_WFS, "IOCTL_WFS_GET_SIZE: file {} too large ({})", fd, size);
    }
    Memory::Write_U32(truncated_size, request.buffer_out);
    break;
  }

  case IOCTL_WFS_CLOSE:
  {
    const u16 fd = Memory::Read_U16(request.buffer_in + 0x4);
    INFO_LOG_FMT(IOS_WFS, "IOCTL_WFS_CLOSE({})", fd);
    ReleaseDescriptor<FileDescriptor>(fd);
    break;
  }

  case IOCTL_WFS_CLOSE_2:
  {
    // TODO(wfs): Figure out the exact semantics difference from the other
    // close.
    const u16 fd = Memory::Read_U16(request.buffer_in + 0x4);
    INFO_LOG_FMT(IOS_WFS, "IOCTL_WFS_CLOSE_2({})", fd);
    ReleaseDescriptor<FileDescriptor>(fd);
    break;
  }

  case IOCTL_WFS_READ:
  case IOCTL_WFS_READ_ABSOLUTE:
  {
    const u32 addr = Memory::Read_U32(request.buffer_in);
    const u32 position = Memory::Read_U32(request.buffer_in + 4);  // Only for absolute.
    const u16 fd = Memory::Read_U16(request.buffer_in + 0xC);
    const u32 size = Memory::Read_U32(request.buffer_in + 8);

    const bool absolute = request.request == IOCTL_WFS_READ_ABSOLUTE;

    FileDescriptor* fd_obj = FindDescriptor<FileDescriptor>(fd);
    if (fd_obj == nullptr)
    {
      ERROR_LOG_FMT(IOS_WFS, "IOCTL_WFS_READ: invalid file descriptor {}", fd);
      return_error_code = WFS_EBADFD;
      break;
    }

    if (!fd_obj->allow_reads)
    {
      ERROR_LOG_FMT(IOS_WFS, "IOCTL_WFS_READ: reads are not allowed {}", fd);
      return_error_code = WFS_EACCES;
      break;
    }

    const u64 previous_position = fd_obj->file.Tell();
    if (absolute)
    {
      fd_obj->file.Seek(position, File::SeekOrigin::Begin);
    }
    size_t read_bytes;
    fd_obj->file.ReadArray(Memory::GetPointer(addr), size, &read_bytes);
    // TODO(wfs): Handle read errors.
    if (absolute)
    {
      fd_obj->file.Seek(previous_position, File::SeekOrigin::Begin);
    }
    else
    {
      fd_obj->position += read_bytes;
    }

    INFO_LOG_FMT(IOS_WFS, "IOCTL_WFS_READ: read {} bytes from FD {} ({})", read_bytes, fd,
                 fd_obj->path);
    return_error_code = static_cast<int>(read_bytes);
    break;
  }

  case IOCTL_WFS_WRITE:
  case IOCTL_WFS_WRITE_ABSOLUTE:
  {
    const u32 addr = Memory::Read_U32(request.buffer_in);
    const u32 position = Memory::Read_U32(request.buffer_in + 4);  // Only for absolute.
    const u16 fd = Memory::Read_U16(request.buffer_in + 0xC);
    const u32 size = Memory::Read_U32(request.buffer_in + 8);

    const bool absolute = request.request == IOCTL_WFS_WRITE_ABSOLUTE;

    FileDescriptor* fd_obj = FindDescriptor<FileDescriptor>(fd);
    if (fd_obj == nullptr)
    {
      ERROR_LOG_FMT(IOS_WFS, "IOCTL_WFS_WRITE: invalid file descriptor {}", fd);
      return_error_code = WFS_EBADFD;
      break;
    }

    if (!fd_obj->allow_writes)
    {
      ERROR_LOG_FMT(IOS_WFS, "IOCTL_WFS_WRITE: writes are not allowed {}", fd);
      return_error_code = WFS_EACCES;
      break;
    }

    const u64 previous_position = fd_obj->file.Tell();
    if (absolute)
    {
      fd_obj->file.Seek(position, File::SeekOrigin::Begin);
    }
    fd_obj->file.WriteArray(Memory::GetPointer(addr), size);
    // TODO(wfs): Handle write errors.
    if (absolute)
    {
      fd_obj->file.Seek(previous_position, File::SeekOrigin::Begin);
    }
    else
    {
      fd_obj->position += size;
    }

    INFO_LOG_FMT(IOS_WFS, "IOCTL_WFS_WRITE: written {} bytes from FD {} ({})", size, fd,
                 fd_obj->path);
    break;
  }

  default:
    // TODO(wfs): Should be returning -3. However until we have everything
    // properly stubbed it's easier to simulate the methods succeeding.
    request.DumpUnknown(GetDeviceName(), Common::Log::LogType::IOS_WFS,
                        Common::Log::LogLevel::LWARNING);
    Memory::Memset(request.buffer_out, 0, request.buffer_out_size);
    break;
  }

  return IPCReply(return_error_code);
}

s32 WFSSRVDevice::GlobNext(const IOCtlRequest& request, DirectoryDescriptor* dd_obj) const
{
  const std::vector<File::FSTEntry>& children = dd_obj->entry.children;
  size_t& position = dd_obj->position;
  while (position < children.size() && children[position].virtualName.size() > 255)
  {
    position++;
  }

  if (position >= children.size())
  {
    return WFS_ENOENT;
  }

  Memory::Memset(request.buffer_out, 0, 0x120);
  if (File::IsDirectory(children[position].physicalName))
  {
    Memory::Write_U32(0x80000000, request.buffer_out + 0x4);
  }
  const std::string name = children[position].virtualName;
  Memory::CopyToEmu(request.buffer_out + 0x14, name.data(), name.size());

  position++;
  return IPC_SUCCESS;
}

s32 WFSSRVDevice::Rename(std::string source, std::string dest) const
{
  source = NormalizePath(source);
  dest = NormalizePath(dest);

  INFO_LOG_FMT(IOS_WFS, "IOCTL_WFS_RENAME: {} to {}", source, dest);

  const bool opened = std::any_of(m_fds.begin(), m_fds.end(),
                                  [&](const auto& fd) { return fd.in_use && fd.path == source; });

  if (opened)
    return WFS_FILE_IS_OPENED;

  // TODO(wfs): Handle other rename failures
  if (!File::Rename(WFS::NativePath(source), WFS::NativePath(dest)))
    return WFS_ENOENT;

  return IPC_SUCCESS;
}

void WFSSRVDevice::SetHomeDir(const std::string& home_directory)
{
  m_home_directory = home_directory;
}

std::string WFSSRVDevice::NormalizePath(const std::string& path) const
{
  std::string expanded;
  if (!path.empty() && path[0] == '~')
  {
    expanded = m_home_directory + "/" + path.substr(1);
  }
  else if (path.empty() || path[0] != '/')
  {
    expanded = m_current_directory + "/" + path;
  }
  else
  {
    expanded = path;
  }

  std::vector<std::string> components = SplitString(expanded, '/');
  std::vector<std::string> normalized_components;
  for (const auto& component : components)
  {
    if (component.empty() || component == ".")
    {
      continue;
    }
    else if (component == ".." && !normalized_components.empty())
    {
      normalized_components.pop_back();
    }
    else
    {
      normalized_components.push_back(component);
    }
  }
  return "/" + JoinStrings(normalized_components, "/");
}

template <>
std::vector<WFSSRVDevice::FileDescriptor>& WFSSRVDevice::GetDescriptors(void)
{
  return m_fds;
}

template <>
std::vector<WFSSRVDevice::DirectoryDescriptor>& WFSSRVDevice::GetDescriptors(void)
{
  return m_dds;
}

template <typename T>
T* WFSSRVDevice::FindDescriptor(u16 fd)
{
  std::vector<T>& descriptors = GetDescriptors<T>();
  if (fd >= descriptors.size() || !descriptors[fd].in_use)
  {
    return nullptr;
  }
  return &descriptors[fd];
}

template <typename T>
u16 WFSSRVDevice::GetNewDescriptor()
{
  std::vector<T>& descriptors = GetDescriptors<T>();
  for (u32 i = 0; i < descriptors.size(); ++i)
  {
    if (!descriptors[i].in_use)
    {
      return i;
    }
  }
  descriptors.resize(descriptors.size() + 1);
  return static_cast<u16>(descriptors.size() - 1);
}

template <typename T>
void WFSSRVDevice::ReleaseDescriptor(u16 fd)
{
  T* descriptor = FindDescriptor<T>(fd);
  if (!descriptor)
  {
    return;
  }
  descriptor->in_use = false;

  // Garbage collect and shrink the array if possible.
  std::vector<T>& descriptors = GetDescriptors<T>();
  while (!descriptors.empty() && !descriptors[descriptors.size() - 1].in_use)
  {
    descriptors.resize(descriptors.size() - 1);
  }
}

bool WFSSRVDevice::FileDescriptor::Open()
{
  return file.Open(WFS::NativePath(path), must_create ? "wb+x" : "rb+");
}
}  // namespace IOS::HLE
