// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IPC_HLE/WII_IPC_HLE_Device_usb_wfssrv.h"

#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/NandPaths.h"
#include "Core/HW/DVDInterface.h"
#include "Core/HW/Memmap.h"

namespace WFS
{
std::string NativePath(const std::string& wfs_path)
{
  return File::GetUserPath(D_WFSROOT_IDX) + Common::EscapePath(wfs_path);
}
}

CWII_IPC_HLE_Device_usb_wfssrv::CWII_IPC_HLE_Device_usb_wfssrv(u32 device_id,
                                                               const std::string& device_name)
    : IWII_IPC_HLE_Device(device_id, device_name)
{
  m_device_name = "msc01";
}

IPCCommandResult CWII_IPC_HLE_Device_usb_wfssrv::IOCtl(u32 command_address)
{
  u32 command = Memory::Read_U32(command_address + 0xC);
  u32 buffer_in = Memory::Read_U32(command_address + 0x10);
  u32 buffer_in_size = Memory::Read_U32(command_address + 0x14);
  u32 buffer_out = Memory::Read_U32(command_address + 0x18);
  u32 buffer_out_size = Memory::Read_U32(command_address + 0x1C);

  int return_error_code = IPC_SUCCESS;

  switch (command)
  {
  case IOCTL_WFS_INIT:
    // TODO(wfs): Implement.
    INFO_LOG(WII_IPC_HLE, "IOCTL_WFS_INIT");
    break;

  case IOCTL_WFS_DEVICE_INFO:
    INFO_LOG(WII_IPC_HLE, "IOCTL_WFS_DEVICE_INFO");
    Memory::Write_U64(16ull << 30, buffer_out);  // 16GB storage.
    Memory::Write_U8(4, buffer_out + 8);
    break;

  case IOCTL_WFS_GET_DEVICE_NAME:
  {
    INFO_LOG(WII_IPC_HLE, "IOCTL_WFS_GET_DEVICE_NAME");
    Memory::Write_U8(static_cast<u8>(m_device_name.size()), buffer_out);
    Memory::CopyToEmu(buffer_out + 1, m_device_name.data(), m_device_name.size());
    break;
  }

  case IOCTL_WFS_ATTACH_DETACH_2:
    // TODO(wfs): Implement.
    INFO_LOG(WII_IPC_HLE, "IOCTL_WFS_ATTACH_DETACH_2(%d)", command);
    Memory::Write_U32(1, buffer_out);
    Memory::Write_U32(0, buffer_out + 4);  // device id?
    Memory::Write_U32(0, buffer_out + 8);
    break;

  case IOCTL_WFS_ATTACH_DETACH:
    INFO_LOG(WII_IPC_HLE, "IOCTL_WFS_ATTACH_DETACH(%d)", command);
    Memory::Write_U32(1, buffer_out);
    Memory::Write_U32(0, buffer_out + 4);
    Memory::Write_U32(0, buffer_out + 8);
    return GetNoReply();

  // TODO(wfs): Globbing is not really implemented, we just fake the one case
  // (listing /vol/*) which is required to get the installer to work.
  case IOCTL_WFS_GLOB_START:
    INFO_LOG(WII_IPC_HLE, "IOCTL_WFS_GLOB_START(%d)", command);
    Memory::Memset(buffer_out, 0, buffer_out_size);
    memcpy(Memory::GetPointer(buffer_out + 0x14), m_device_name.data(), m_device_name.size());
    break;

  case IOCTL_WFS_GLOB_NEXT:
    INFO_LOG(WII_IPC_HLE, "IOCTL_WFS_GLOB_NEXT(%d)", command);
    return_error_code = WFS_EEMPTY;
    break;

  case IOCTL_WFS_GLOB_END:
    INFO_LOG(WII_IPC_HLE, "IOCTL_WFS_GLOB_END(%d)", command);
    Memory::Memset(buffer_out, 0, buffer_out_size);
    break;

  case IOCTL_WFS_OPEN:
  {
    u32 mode = Memory::Read_U32(buffer_in);
    u16 path_len = Memory::Read_U16(buffer_in + 0x20);
    std::string path = Memory::GetString(buffer_in + 0x22, path_len);

    u16 fd = GetNewFileDescriptor();
    FileDescriptor* fd_obj = &m_fds[fd];
    fd_obj->in_use = true;
    fd_obj->path = path;
    fd_obj->mode = mode;
    fd_obj->position = 0;

    if (!fd_obj->Open())
    {
      ERROR_LOG(WII_IPC_HLE, "IOCTL_WFS_OPEN(%s, %d): error opening file", path.c_str(), mode);
      ReleaseFileDescriptor(fd);
      return_error_code = -1;  // TODO(wfs): proper error code.
      break;
    }

    INFO_LOG(WII_IPC_HLE, "IOCTL_WFS_OPEN(%s, %d) -> %d", path.c_str(), mode, fd);
    Memory::Write_U16(fd, buffer_out + 0x14);
    break;
  }

  case IOCTL_WFS_CLOSE:
  {
    u16 fd = Memory::Read_U16(buffer_in + 0x4);
    INFO_LOG(WII_IPC_HLE, "IOCTL_WFS_CLOSE(%d)", fd);
    ReleaseFileDescriptor(fd);
    break;
  }

  case IOCTL_WFS_READ:
  {
    u32 addr = Memory::Read_U32(buffer_in);
    u16 fd = Memory::Read_U16(buffer_in + 0xC);
    u32 size = Memory::Read_U32(buffer_in + 8);

    FileDescriptor* fd_obj = FindFileDescriptor(fd);
    if (fd_obj == nullptr)
    {
      ERROR_LOG(WII_IPC_HLE, "IOCTL_WFS_READ: invalid file descriptor %d", fd);
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

    INFO_LOG(WII_IPC_HLE, "IOCTL_WFS_READ: read %zd bytes from FD %d (%s)", read_bytes, fd,
             fd_obj->path.c_str());
    return_error_code = static_cast<int>(read_bytes);
    break;
  }

  default:
    // TODO(wfs): Should be returning -3. However until we have everything
    // properly stubbed it's easier to simulate the methods succeeding.
    WARN_LOG(WII_IPC_HLE, "%s unimplemented IOCtl(0x%08x, size_in=%08x, size_out=%08x)\n%s\n%s",
             m_name.c_str(), command, buffer_in_size, buffer_out_size,
             HexDump(Memory::GetPointer(buffer_in), buffer_in_size).c_str(),
             HexDump(Memory::GetPointer(buffer_out), buffer_out_size).c_str());
    Memory::Memset(buffer_out, 0, buffer_out_size);
    break;
  }

  Memory::Write_U32(return_error_code, command_address + 4);
  return GetDefaultReply();
}

IPCCommandResult CWII_IPC_HLE_Device_usb_wfssrv::IOCtlV(u32 command_address)
{
  SIOCtlVBuffer command_buffer(command_address);
  ERROR_LOG(WII_IPC_HLE, "IOCtlV on /dev/usb/wfssrv -- unsupported");
  Memory::Write_U32(IPC_EINVAL, command_address + 4);
  return GetDefaultReply();
}

CWII_IPC_HLE_Device_usb_wfssrv::FileDescriptor*
CWII_IPC_HLE_Device_usb_wfssrv::FindFileDescriptor(u16 fd)
{
  if (fd >= m_fds.size() || !m_fds[fd].in_use)
  {
    return nullptr;
  }
  return &m_fds[fd];
}

u16 CWII_IPC_HLE_Device_usb_wfssrv::GetNewFileDescriptor()
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

void CWII_IPC_HLE_Device_usb_wfssrv::ReleaseFileDescriptor(u16 fd)
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

bool CWII_IPC_HLE_Device_usb_wfssrv::FileDescriptor::Open()
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
    ERROR_LOG(WII_IPC_HLE, "WFSOpen: invalid mode %d", mode);
    return false;
  }

  return file.Open(WFS::NativePath(path), mode_string);
}
