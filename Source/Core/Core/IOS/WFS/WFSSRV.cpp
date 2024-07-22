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
#include "Core/System.h"

namespace IOS::HLE
{
namespace WFS
{
std::string NativePath(const std::string& wfs_path)
{
  return File::GetUserPath(D_WFSROOT_IDX) + Common::EscapePath(wfs_path);
}
}  // namespace WFS

WFSSRVDevice::WFSSRVDevice(EmulationKernel& ios, const std::string& device_name)
    : EmulationDevice(ios, device_name)
{
  m_device_name = "msc01";
}

std::optional<IPCReply> WFSSRVDevice::IOCtl(const IOCtlRequest& request)
{
  int return_error_code = IPC_SUCCESS;

  auto& system = GetSystem();
  auto& memory = system.GetMemory();

  switch (request.request)
  {
  case IOCTL_WFS_INIT:
    // TODO(wfs): Implement.
    INFO_LOG_FMT(IOS_WFS, "IOCTL_WFS_INIT");
    break;

  case IOCTL_WFS_UNKNOWN_8:
    // TODO(wfs): Figure out what this actually does.
    INFO_LOG_FMT(IOS_WFS, "IOCTL_WFS_UNKNOWN_8");
    memory.Write_U8(7, request.buffer_out);
    memory.CopyToEmu(request.buffer_out + 1, "msc01\x00\x00\x00", 8);
    break;

  case IOCTL_WFS_SHUTDOWN:
    INFO_LOG_FMT(IOS_WFS, "IOCTL_WFS_SHUTDOWN");

    // Close all hanging attach/detach ioctls with an appropriate error code.
    for (auto address : m_hanging)
    {
      IOCtlRequest hanging_request{system, address};
      memory.Write_U32(0x80000000, hanging_request.buffer_out);
      memory.Write_U32(0, hanging_request.buffer_out + 4);
      memory.Write_U32(0, hanging_request.buffer_out + 8);
      GetEmulationKernel().EnqueueIPCReply(hanging_request, 0);
    }
    break;

  case IOCTL_WFS_DEVICE_INFO:
    INFO_LOG_FMT(IOS_WFS, "IOCTL_WFS_DEVICE_INFO");
    memory.Write_U64(16ull << 30, request.buffer_out);  // 16GB storage.
    memory.Write_U8(4, request.buffer_out + 8);
    break;

  case IOCTL_WFS_GET_DEVICE_NAME:
  {
    INFO_LOG_FMT(IOS_WFS, "IOCTL_WFS_GET_DEVICE_NAME");
    memory.Write_U8(static_cast<u8>(m_device_name.size()), request.buffer_out);
    memory.CopyToEmu(request.buffer_out + 1, m_device_name.data(), m_device_name.size());
    break;
  }

  case IOCTL_WFS_ATTACH_DETACH_2:
    // TODO(wfs): Implement.
    INFO_LOG_FMT(IOS_WFS, "IOCTL_WFS_ATTACH_DETACH_2({})", request.request);
    memory.Write_U32(1, request.buffer_out);
    memory.Write_U32(0, request.buffer_out + 4);  // device id?
    memory.Write_U32(0, request.buffer_out + 8);
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
        memory.GetString(request.buffer_in + 34, memory.Read_U16(request.buffer_in + 32)));
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

  // TODO(wfs): Globbing is not really implemented, we just fake the one case
  // (listing /vol/*) which is required to get the installer to work.
  case IOCTL_WFS_GLOB_START:
    INFO_LOG_FMT(IOS_WFS, "IOCTL_WFS_GLOB_START({})", request.request);
    memory.Memset(request.buffer_out, 0, request.buffer_out_size);
    memory.CopyToEmu(request.buffer_out + 0x14, m_device_name.data(), m_device_name.size());
    break;

  case IOCTL_WFS_GLOB_NEXT:
    INFO_LOG_FMT(IOS_WFS, "IOCTL_WFS_GLOB_NEXT({})", request.request);
    return_error_code = WFS_ENOENT;
    break;

  case IOCTL_WFS_GLOB_END:
    INFO_LOG_FMT(IOS_WFS, "IOCTL_WFS_GLOB_END({})", request.request);
    memory.Memset(request.buffer_out, 0, request.buffer_out_size);
    break;

  case IOCTL_WFS_SET_HOMEDIR:
    m_home_directory = memory.GetString(request.buffer_in + 2, memory.Read_U16(request.buffer_in));
    INFO_LOG_FMT(IOS_WFS, "IOCTL_WFS_SET_HOMEDIR: {}", m_home_directory);
    break;

  case IOCTL_WFS_CHDIR:
    m_current_directory =
        memory.GetString(request.buffer_in + 2, memory.Read_U16(request.buffer_in));
    INFO_LOG_FMT(IOS_WFS, "IOCTL_WFS_CHDIR: {}", m_current_directory);
    break;

  case IOCTL_WFS_GET_HOMEDIR:
    INFO_LOG_FMT(IOS_WFS, "IOCTL_WFS_GET_HOMEDIR: {}", m_home_directory);
    memory.Write_U16(static_cast<u16>(m_home_directory.size()), request.buffer_out);
    memory.CopyToEmu(request.buffer_out + 2, m_home_directory.data(), m_home_directory.size());
    break;

  case IOCTL_WFS_GET_ATTRIBUTES:
  {
    const std::string path =
        NormalizePath(memory.GetString(request.buffer_in + 2, memory.Read_U16(request.buffer_in)));
    const std::string native_path = WFS::NativePath(path);

    memory.Memset(0, request.buffer_out, request.buffer_out_size);
    if (!File::Exists(native_path))
    {
      INFO_LOG_FMT(IOS_WFS, "IOCTL_WFS_GET_ATTRIBUTES({}): no such file or directory", path);
      return_error_code = WFS_ENOENT;
    }
    else if (File::IsDirectory(native_path))
    {
      INFO_LOG_FMT(IOS_WFS, "IOCTL_WFS_GET_ATTRIBUTES({}): directory", path);
      memory.Write_U32(0x80000000, request.buffer_out + 4);
    }
    else
    {
      const auto size = static_cast<u32>(File::GetSize(native_path));
      INFO_LOG_FMT(IOS_WFS, "IOCTL_WFS_GET_ATTRIBUTES({}): file with size {}", path, size);
      memory.Write_U32(size, request.buffer_out);
    }
    break;
  }

  case IOCTL_WFS_RENAME:
  case IOCTL_WFS_RENAME_2:
  {
    const std::string source_path =
        memory.GetString(request.buffer_in + 2, memory.Read_U16(request.buffer_in));
    const std::string dest_path =
        memory.GetString(request.buffer_in + 512 + 2, memory.Read_U16(request.buffer_in + 512));
    return_error_code = Rename(source_path, dest_path);
    break;
  }

  case IOCTL_WFS_CREATE_OPEN:
  case IOCTL_WFS_OPEN:
  {
    const char* ioctl_name =
        request.request == IOCTL_WFS_OPEN ? "IOCTL_WFS_OPEN" : "IOCTL_WFS_CREATE_OPEN";
    const u32 mode = request.request == IOCTL_WFS_OPEN ? memory.Read_U32(request.buffer_in) : 2;
    const u16 path_len = memory.Read_U16(request.buffer_in + 0x20);
    std::string path = memory.GetString(request.buffer_in + 0x22, path_len);

    path = NormalizePath(path);

    const u16 fd = GetNewFileDescriptor();
    FileDescriptor* fd_obj = &m_fds[fd];
    fd_obj->in_use = true;
    fd_obj->path = path;
    fd_obj->mode = mode;
    fd_obj->position = 0;

    if (!fd_obj->Open())
    {
      ERROR_LOG_FMT(IOS_WFS, "{}({}, {}): error opening file", ioctl_name, path, mode);
      ReleaseFileDescriptor(fd);
      return_error_code = WFS_ENOENT;
      break;
    }

    INFO_LOG_FMT(IOS_WFS, "{}({}, {}) -> {}", ioctl_name, path, mode, fd);
    if (request.request == IOCTL_WFS_OPEN)
    {
      memory.Write_U16(fd, request.buffer_out + 0x14);
    }
    else
    {
      memory.Write_U16(fd, request.buffer_out);
    }
    break;
  }

  case IOCTL_WFS_GET_SIZE:
  {
    const u16 fd = memory.Read_U16(request.buffer_in);
    FileDescriptor* fd_obj = FindFileDescriptor(fd);
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
    memory.Write_U32(truncated_size, request.buffer_out);
    break;
  }

  case IOCTL_WFS_CLOSE:
  {
    const u16 fd = memory.Read_U16(request.buffer_in + 0x4);
    INFO_LOG_FMT(IOS_WFS, "IOCTL_WFS_CLOSE({})", fd);
    ReleaseFileDescriptor(fd);
    break;
  }

  case IOCTL_WFS_CLOSE_2:
  {
    // TODO(wfs): Figure out the exact semantics difference from the other
    // close.
    const u16 fd = memory.Read_U16(request.buffer_in + 0x4);
    INFO_LOG_FMT(IOS_WFS, "IOCTL_WFS_CLOSE_2({})", fd);
    ReleaseFileDescriptor(fd);
    break;
  }

  case IOCTL_WFS_READ:
  case IOCTL_WFS_READ_ABSOLUTE:
  {
    const u32 addr = memory.Read_U32(request.buffer_in);
    const u32 position = memory.Read_U32(request.buffer_in + 4);  // Only for absolute.
    const u16 fd = memory.Read_U16(request.buffer_in + 0xC);
    const u32 size = memory.Read_U32(request.buffer_in + 8);

    const bool absolute = request.request == IOCTL_WFS_READ_ABSOLUTE;

    FileDescriptor* fd_obj = FindFileDescriptor(fd);
    if (fd_obj == nullptr)
    {
      ERROR_LOG_FMT(IOS_WFS, "IOCTL_WFS_READ: invalid file descriptor {}", fd);
      return_error_code = WFS_EBADFD;
      break;
    }

    const u64 previous_position = fd_obj->file.Tell();
    if (absolute)
    {
      fd_obj->file.Seek(position, File::SeekOrigin::Begin);
    }
    size_t read_bytes;
    fd_obj->file.ReadArray(memory.GetPointerForRange(addr, size), size, &read_bytes);
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
    const u32 addr = memory.Read_U32(request.buffer_in);
    const u32 position = memory.Read_U32(request.buffer_in + 4);  // Only for absolute.
    const u16 fd = memory.Read_U16(request.buffer_in + 0xC);
    const u32 size = memory.Read_U32(request.buffer_in + 8);

    const bool absolute = request.request == IOCTL_WFS_WRITE_ABSOLUTE;

    FileDescriptor* fd_obj = FindFileDescriptor(fd);
    if (fd_obj == nullptr)
    {
      ERROR_LOG_FMT(IOS_WFS, "IOCTL_WFS_WRITE: invalid file descriptor {}", fd);
      return_error_code = WFS_EBADFD;
      break;
    }

    const u64 previous_position = fd_obj->file.Tell();
    if (absolute)
    {
      fd_obj->file.Seek(position, File::SeekOrigin::Begin);
    }
    fd_obj->file.WriteArray(memory.GetPointerForRange(addr, size), size);
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
    request.DumpUnknown(system, GetDeviceName(), Common::Log::LogType::IOS_WFS,
                        Common::Log::LogLevel::LWARNING);
    memory.Memset(request.buffer_out, 0, request.buffer_out_size);
    break;
  }

  return IPCReply(return_error_code);
}

s32 WFSSRVDevice::Rename(std::string source, std::string dest) const
{
  source = NormalizePath(source);
  dest = NormalizePath(dest);

  INFO_LOG_FMT(IOS_WFS, "IOCTL_WFS_RENAME: {} to {}", source, dest);

  const bool opened =
      std::ranges::any_of(m_fds, [&](const auto& fd) { return fd.in_use && fd.path == source; });

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

WFSSRVDevice::FileDescriptor* WFSSRVDevice::FindFileDescriptor(u16 fd)
{
  if (fd >= m_fds.size() || !m_fds[fd].in_use)
  {
    return nullptr;
  }
  return &m_fds[fd];
}

u16 WFSSRVDevice::GetNewFileDescriptor()
{
  for (u32 i = 0; i < m_fds.size(); ++i)
  {
    if (!m_fds[i].in_use)
    {
      return i;
    }
  }
  m_fds.resize(m_fds.size() + 1);
  return static_cast<u16>(m_fds.size() - 1);
}

void WFSSRVDevice::ReleaseFileDescriptor(u16 fd)
{
  FileDescriptor* fd_obj = FindFileDescriptor(fd);
  if (!fd_obj)
  {
    return;
  }
  fd_obj->in_use = false;

  // Garbage collect and shrink the array if possible.
  while (!m_fds.empty() && !m_fds[m_fds.size() - 1].in_use)
  {
    m_fds.resize(m_fds.size() - 1);
  }
}

bool WFSSRVDevice::FileDescriptor::Open()
{
  const char* mode_string;

  if (mode == 1)
  {
    mode_string = "rb";
  }
  else if (mode == 2)
  {
    mode_string = "wb";
  }
  else if (mode == 3)
  {
    mode_string = "rwb";
  }
  else
  {
    ERROR_LOG_FMT(IOS_WFS, "WFSOpen: invalid mode {}", mode);
    return false;
  }

  return file.Open(WFS::NativePath(path), mode_string);
}
}  // namespace IOS::HLE
