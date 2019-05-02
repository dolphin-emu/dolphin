// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/WFS/WFSSRV.h"

#include <algorithm>
#include <cinttypes>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Common/FileUtil.h"
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

namespace Device
{
WFSSRV::WFSSRV(Kernel& ios, const std::string& device_name) : Device(ios, device_name)
{
  m_device_name = "msc01";
}

IPCCommandResult WFSSRV::IOCtl(const IOCtlRequest& request)
{
  int return_error_code = IPC_SUCCESS;

  switch (request.request)
  {
  case IOCTL_WFS_INIT:
    // TODO(wfs): Implement.
    INFO_LOG(IOS_WFS, "IOCTL_WFS_INIT");
    break;

  case IOCTL_WFS_UNKNOWN_8:
    // TODO(wfs): Figure out what this actually does.
    INFO_LOG(IOS_WFS, "IOCTL_WFS_UNKNOWN_8");
    Memory::Write_U8(7, request.buffer_out);
    Memory::CopyToEmu(request.buffer_out + 1, "msc01\x00\x00\x00", 8);
    break;

  case IOCTL_WFS_SHUTDOWN:
    INFO_LOG(IOS_WFS, "IOCTL_WFS_SHUTDOWN");

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
    INFO_LOG(IOS_WFS, "IOCTL_WFS_DEVICE_INFO");
    Memory::Write_U64(16ull << 30, request.buffer_out);  // 16GB storage.
    Memory::Write_U8(4, request.buffer_out + 8);
    break;

  case IOCTL_WFS_GET_DEVICE_NAME:
  {
    INFO_LOG(IOS_WFS, "IOCTL_WFS_GET_DEVICE_NAME");
    Memory::Write_U8(static_cast<u8>(m_device_name.size()), request.buffer_out);
    Memory::CopyToEmu(request.buffer_out + 1, m_device_name.data(), m_device_name.size());
    break;
  }

  case IOCTL_WFS_ATTACH_DETACH_2:
    // TODO(wfs): Implement.
    INFO_LOG(IOS_WFS, "IOCTL_WFS_ATTACH_DETACH_2(%u)", request.request);
    Memory::Write_U32(1, request.buffer_out);
    Memory::Write_U32(0, request.buffer_out + 4);  // device id?
    Memory::Write_U32(0, request.buffer_out + 8);
    break;

  case IOCTL_WFS_ATTACH_DETACH:
    INFO_LOG(IOS_WFS, "IOCTL_WFS_ATTACH_DETACH(%u)", request.request);

    // Leave hanging, but we need to acknowledge the request at shutdown time.
    m_hanging.push_back(request.address);
    return GetNoReply();

  case IOCTL_WFS_FLUSH:
    // Nothing to do.
    INFO_LOG(IOS_WFS, "IOCTL_WFS_FLUSH: doing nothing");
    break;

  case IOCTL_WFS_MKDIR:
  {
    std::string path = NormalizePath(
        Memory::GetString(request.buffer_in + 34, Memory::Read_U16(request.buffer_in + 32)));
    std::string native_path = WFS::NativePath(path);

    if (File::Exists(native_path))
    {
      INFO_LOG(IOS_WFS, "IOCTL_WFS_MKDIR(%s): already exists", path.c_str());
      return_error_code = WFS_EEXIST;
    }
    else if (!File::CreateDir(native_path))
    {
      INFO_LOG(IOS_WFS, "IOCTL_WFS_MKDIR(%s): no such file or directory", path.c_str());
      return_error_code = WFS_ENOENT;
    }
    else
    {
      INFO_LOG(IOS_WFS, "IOCTL_WFS_MKDIR(%s): directory created", path.c_str());
    }
    break;
  }

  // TODO(wfs): Globbing is not really implemented, we just fake the one case
  // (listing /vol/*) which is required to get the installer to work.
  case IOCTL_WFS_GLOB_START:
    INFO_LOG(IOS_WFS, "IOCTL_WFS_GLOB_START(%u)", request.request);
    Memory::Memset(request.buffer_out, 0, request.buffer_out_size);
    Memory::CopyToEmu(request.buffer_out + 0x14, m_device_name.data(), m_device_name.size());
    break;

  case IOCTL_WFS_GLOB_NEXT:
    INFO_LOG(IOS_WFS, "IOCTL_WFS_GLOB_NEXT(%u)", request.request);
    return_error_code = WFS_ENOENT;
    break;

  case IOCTL_WFS_GLOB_END:
    INFO_LOG(IOS_WFS, "IOCTL_WFS_GLOB_END(%u)", request.request);
    Memory::Memset(request.buffer_out, 0, request.buffer_out_size);
    break;

  case IOCTL_WFS_SET_HOMEDIR:
    m_home_directory =
        Memory::GetString(request.buffer_in + 2, Memory::Read_U16(request.buffer_in));
    INFO_LOG(IOS_WFS, "IOCTL_WFS_SET_HOMEDIR: %s", m_home_directory.c_str());
    break;

  case IOCTL_WFS_CHDIR:
    m_current_directory =
        Memory::GetString(request.buffer_in + 2, Memory::Read_U16(request.buffer_in));
    INFO_LOG(IOS_WFS, "IOCTL_WFS_CHDIR: %s", m_current_directory.c_str());
    break;

  case IOCTL_WFS_GET_HOMEDIR:
    INFO_LOG(IOS_WFS, "IOCTL_WFS_GET_HOMEDIR: %s", m_home_directory.c_str());
    Memory::Write_U16(static_cast<u16>(m_home_directory.size()), request.buffer_out);
    Memory::CopyToEmu(request.buffer_out + 2, m_home_directory.data(), m_home_directory.size());
    break;

  case IOCTL_WFS_GET_ATTRIBUTES:
  {
    std::string path = NormalizePath(
        Memory::GetString(request.buffer_in + 2, Memory::Read_U16(request.buffer_in)));
    std::string native_path = WFS::NativePath(path);
    Memory::Memset(0, request.buffer_out, request.buffer_out_size);
    if (!File::Exists(native_path))
    {
      INFO_LOG(IOS_WFS, "IOCTL_WFS_GET_ATTRIBUTES(%s): no such file or directory", path.c_str());
      return_error_code = WFS_ENOENT;
    }
    else if (File::IsDirectory(native_path))
    {
      INFO_LOG(IOS_WFS, "IOCTL_WFS_GET_ATTRIBUTES(%s): directory", path.c_str());
      Memory::Write_U32(0x80000000, request.buffer_out + 4);
    }
    else
    {
      u32 size = static_cast<u32>(File::GetSize(native_path));
      INFO_LOG(IOS_WFS, "IOCTL_WFS_GET_ATTRIBUTES(%s): file with size %d", path.c_str(), size);
      Memory::Write_U32(size, request.buffer_out);
    }
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
    u32 mode = request.request == IOCTL_WFS_OPEN ? Memory::Read_U32(request.buffer_in) : 2;
    u16 path_len = Memory::Read_U16(request.buffer_in + 0x20);
    std::string path = Memory::GetString(request.buffer_in + 0x22, path_len);

    path = NormalizePath(path);

    u16 fd = GetNewFileDescriptor();
    FileDescriptor* fd_obj = &m_fds[fd];
    fd_obj->in_use = true;
    fd_obj->path = path;
    fd_obj->mode = mode;
    fd_obj->position = 0;

    if (!fd_obj->Open())
    {
      ERROR_LOG(IOS_WFS, "%s(%s, %d): error opening file", ioctl_name, path.c_str(), mode);
      ReleaseFileDescriptor(fd);
      return_error_code = WFS_ENOENT;
      break;
    }

    INFO_LOG(IOS_WFS, "%s(%s, %d) -> %d", ioctl_name, path.c_str(), mode, fd);
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
    u16 fd = Memory::Read_U16(request.buffer_in);
    FileDescriptor* fd_obj = FindFileDescriptor(fd);
    if (fd_obj == nullptr)
    {
      ERROR_LOG(IOS_WFS, "IOCTL_WFS_GET_SIZE: invalid file descriptor %d", fd);
      return_error_code = WFS_EBADFD;
      break;
    }

    u64 size = fd_obj->file.GetSize();
    u32 truncated_size = static_cast<u32>(size);
    INFO_LOG(IOS_WFS, "IOCTL_WFS_GET_SIZE(%d) -> %d", fd, truncated_size);
    if (size != truncated_size)
    {
      ERROR_LOG(IOS_WFS, "IOCTL_WFS_GET_SIZE: file %d too large (%" PRIu64 ")", fd, size);
    }
    Memory::Write_U32(truncated_size, request.buffer_out);
    break;
  }

  case IOCTL_WFS_CLOSE:
  {
    u16 fd = Memory::Read_U16(request.buffer_in + 0x4);
    INFO_LOG(IOS_WFS, "IOCTL_WFS_CLOSE(%d)", fd);
    ReleaseFileDescriptor(fd);
    break;
  }

  case IOCTL_WFS_CLOSE_2:
  {
    // TODO(wfs): Figure out the exact semantics difference from the other
    // close.
    u16 fd = Memory::Read_U16(request.buffer_in + 0x4);
    INFO_LOG(IOS_WFS, "IOCTL_WFS_CLOSE_2(%d)", fd);
    ReleaseFileDescriptor(fd);
    break;
  }

  case IOCTL_WFS_READ:
  case IOCTL_WFS_READ_ABSOLUTE:
  {
    u32 addr = Memory::Read_U32(request.buffer_in);
    u32 position = Memory::Read_U32(request.buffer_in + 4);  // Only for absolute.
    u16 fd = Memory::Read_U16(request.buffer_in + 0xC);
    u32 size = Memory::Read_U32(request.buffer_in + 8);

    bool absolute = request.request == IOCTL_WFS_READ_ABSOLUTE;

    FileDescriptor* fd_obj = FindFileDescriptor(fd);
    if (fd_obj == nullptr)
    {
      ERROR_LOG(IOS_WFS, "IOCTL_WFS_READ: invalid file descriptor %d", fd);
      return_error_code = WFS_EBADFD;
      break;
    }

    u64 previous_position = fd_obj->file.Tell();
    if (absolute)
    {
      fd_obj->file.Seek(position, SEEK_SET);
    }
    size_t read_bytes;
    fd_obj->file.ReadArray(Memory::GetPointer(addr), size, &read_bytes);
    // TODO(wfs): Handle read errors.
    if (absolute)
    {
      fd_obj->file.Seek(previous_position, SEEK_SET);
    }
    else
    {
      fd_obj->position += read_bytes;
    }

    INFO_LOG(IOS_WFS, "IOCTL_WFS_READ: read %zd bytes from FD %d (%s)", read_bytes, fd,
             fd_obj->path.c_str());
    return_error_code = static_cast<int>(read_bytes);
    break;
  }

  case IOCTL_WFS_WRITE:
  case IOCTL_WFS_WRITE_ABSOLUTE:
  {
    u32 addr = Memory::Read_U32(request.buffer_in);
    u32 position = Memory::Read_U32(request.buffer_in + 4);  // Only for absolute.
    u16 fd = Memory::Read_U16(request.buffer_in + 0xC);
    u32 size = Memory::Read_U32(request.buffer_in + 8);

    bool absolute = request.request == IOCTL_WFS_WRITE_ABSOLUTE;

    FileDescriptor* fd_obj = FindFileDescriptor(fd);
    if (fd_obj == nullptr)
    {
      ERROR_LOG(IOS_WFS, "IOCTL_WFS_WRITE: invalid file descriptor %d", fd);
      return_error_code = WFS_EBADFD;
      break;
    }

    u64 previous_position = fd_obj->file.Tell();
    if (absolute)
    {
      fd_obj->file.Seek(position, SEEK_SET);
    }
    fd_obj->file.WriteArray(Memory::GetPointer(addr), size);
    // TODO(wfs): Handle write errors.
    if (absolute)
    {
      fd_obj->file.Seek(previous_position, SEEK_SET);
    }
    else
    {
      fd_obj->position += size;
    }

    INFO_LOG(IOS_WFS, "IOCTL_WFS_WRITE: written %d bytes from FD %d (%s)", size, fd,
             fd_obj->path.c_str());
    break;
  }

  default:
    // TODO(wfs): Should be returning -3. However until we have everything
    // properly stubbed it's easier to simulate the methods succeeding.
    request.DumpUnknown(GetDeviceName(), LogTypes::IOS, LogTypes::LWARNING);
    Memory::Memset(request.buffer_out, 0, request.buffer_out_size);
    break;
  }

  return GetDefaultReply(return_error_code);
}

s32 WFSSRV::Rename(std::string source, std::string dest) const
{
  source = NormalizePath(source);
  dest = NormalizePath(dest);

  INFO_LOG(IOS_WFS, "IOCTL_WFS_RENAME: %s to %s", source.c_str(), dest.c_str());

  const bool opened = std::any_of(m_fds.begin(), m_fds.end(),
                                  [&](const auto& fd) { return fd.in_use && fd.path == source; });

  if (opened)
    return WFS_FILE_IS_OPENED;

  // TODO(wfs): Handle other rename failures
  if (!File::Rename(WFS::NativePath(source), WFS::NativePath(dest)))
    return WFS_ENOENT;

  return IPC_SUCCESS;
}

void WFSSRV::SetHomeDir(const std::string& home_directory)
{
  m_home_directory = home_directory;
}

std::string WFSSRV::NormalizePath(const std::string& path) const
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

WFSSRV::FileDescriptor* WFSSRV::FindFileDescriptor(u16 fd)
{
  if (fd >= m_fds.size() || !m_fds[fd].in_use)
  {
    return nullptr;
  }
  return &m_fds[fd];
}

u16 WFSSRV::GetNewFileDescriptor()
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

void WFSSRV::ReleaseFileDescriptor(u16 fd)
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

bool WFSSRV::FileDescriptor::Open()
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
    ERROR_LOG(IOS_WFS, "WFSOpen: invalid mode %d", mode);
    return false;
  }

  return file.Open(WFS::NativePath(path), mode_string);
}
}  // namespace Device
}  // namespace IOS::HLE
