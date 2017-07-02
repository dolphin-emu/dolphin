// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/WFS/WFSSRV.h"

#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/NandPaths.h"
#include "Core/HW/Memmap.h"

namespace IOS
{
namespace HLE
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
    INFO_LOG(IOS, "IOCTL_WFS_INIT");
    break;

  case IOCTL_WFS_DEVICE_INFO:
    INFO_LOG(IOS, "IOCTL_WFS_DEVICE_INFO");
    Memory::Write_U64(16ull << 30, request.buffer_out);  // 16GB storage.
    Memory::Write_U8(4, request.buffer_out + 8);
    break;

  case IOCTL_WFS_GET_DEVICE_NAME:
  {
    INFO_LOG(IOS, "IOCTL_WFS_GET_DEVICE_NAME");
    Memory::Write_U8(static_cast<u8>(m_device_name.size()), request.buffer_out);
    Memory::CopyToEmu(request.buffer_out + 1, m_device_name.data(), m_device_name.size());
    break;
  }

  case IOCTL_WFS_ATTACH_DETACH_2:
    // TODO(wfs): Implement.
    INFO_LOG(IOS, "IOCTL_WFS_ATTACH_DETACH_2(%u)", request.request);
    Memory::Write_U32(1, request.buffer_out);
    Memory::Write_U32(0, request.buffer_out + 4);  // device id?
    Memory::Write_U32(0, request.buffer_out + 8);
    break;

  case IOCTL_WFS_ATTACH_DETACH:
    INFO_LOG(IOS, "IOCTL_WFS_ATTACH_DETACH(%u)", request.request);
    Memory::Write_U32(1, request.buffer_out);
    Memory::Write_U32(0, request.buffer_out + 4);
    Memory::Write_U32(0, request.buffer_out + 8);
    return GetNoReply();

  // TODO(wfs): Globbing is not really implemented, we just fake the one case
  // (listing /vol/*) which is required to get the installer to work.
  case IOCTL_WFS_GLOB_START:
    INFO_LOG(IOS, "IOCTL_WFS_GLOB_START(%u)", request.request);
    Memory::Memset(request.buffer_out, 0, request.buffer_out_size);
    Memory::CopyToEmu(request.buffer_out + 0x14, m_device_name.data(), m_device_name.size());
    break;

  case IOCTL_WFS_GLOB_NEXT:
    INFO_LOG(IOS, "IOCTL_WFS_GLOB_NEXT(%u)", request.request);
    return_error_code = WFS_EEMPTY;
    break;

  case IOCTL_WFS_GLOB_END:
    INFO_LOG(IOS, "IOCTL_WFS_GLOB_END(%u)", request.request);
    Memory::Memset(request.buffer_out, 0, request.buffer_out_size);
    break;

  case IOCTL_WFS_OPEN:
  {
    u32 mode = Memory::Read_U32(request.buffer_in);
    u16 path_len = Memory::Read_U16(request.buffer_in + 0x20);
    std::string path = Memory::GetString(request.buffer_in + 0x22, path_len);

    u16 fd = GetNewFileDescriptor();
    FileDescriptor* fd_obj = &m_fds[fd];
    fd_obj->in_use = true;
    fd_obj->path = path;
    fd_obj->mode = mode;
    fd_obj->position = 0;

    if (!fd_obj->Open())
    {
      ERROR_LOG(IOS, "IOCTL_WFS_OPEN(%s, %d): error opening file", path.c_str(), mode);
      ReleaseFileDescriptor(fd);
      return_error_code = -1;  // TODO(wfs): proper error code.
      break;
    }

    INFO_LOG(IOS, "IOCTL_WFS_OPEN(%s, %d) -> %d", path.c_str(), mode, fd);
    Memory::Write_U16(fd, request.buffer_out + 0x14);
    break;
  }

  case IOCTL_WFS_CLOSE:
  {
    u16 fd = Memory::Read_U16(request.buffer_in + 0x4);
    INFO_LOG(IOS, "IOCTL_WFS_CLOSE(%d)", fd);
    ReleaseFileDescriptor(fd);
    break;
  }

  case IOCTL_WFS_READ:
  {
    u32 addr = Memory::Read_U32(request.buffer_in);
    u16 fd = Memory::Read_U16(request.buffer_in + 0xC);
    u32 size = Memory::Read_U32(request.buffer_in + 8);

    FileDescriptor* fd_obj = FindFileDescriptor(fd);
    if (fd_obj == nullptr)
    {
      ERROR_LOG(IOS, "IOCTL_WFS_READ: invalid file descriptor %d", fd);
      return_error_code = -1;  // TODO(wfs): proper error code.
      break;
    }

    size_t read_bytes;
    if (!fd_obj->file.ReadArray(Memory::GetPointer(addr), size, &read_bytes))
    {
      return_error_code = -1;  // TODO(wfs): proper error code.
      break;
    }
    fd_obj->position += read_bytes;

    INFO_LOG(IOS, "IOCTL_WFS_READ: read %zd bytes from FD %d (%s)", read_bytes, fd,
             fd_obj->path.c_str());
    return_error_code = static_cast<int>(read_bytes);
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
  while (m_fds.size() > 0 && !m_fds[m_fds.size() - 1].in_use)
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
    ERROR_LOG(IOS, "WFSOpen: invalid mode %d", mode);
    return false;
  }

  return file.Open(WFS::NativePath(path), mode_string);
}
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
