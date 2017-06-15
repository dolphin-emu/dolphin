// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/IOS.h"
#include "DiscIO/Volume.h"

namespace IOS
{
namespace HLE
{
namespace WFS
{
std::string NativePath(const std::string& wfs_path);
}

namespace Device
{
class WFSSRV : public Device
{
public:
  WFSSRV(Kernel& ios, const std::string& device_name);

  IPCCommandResult IOCtl(const IOCtlRequest& request) override;

private:
  // WFS device name, e.g. msc01/msc02.
  std::string m_device_name;

  enum
  {
    IOCTL_WFS_INIT = 0x02,
    IOCTL_WFS_DEVICE_INFO = 0x04,
    IOCTL_WFS_GET_DEVICE_NAME = 0x05,
    IOCTL_WFS_FLUSH = 0x0a,
    IOCTL_WFS_GLOB_START = 0x0d,
    IOCTL_WFS_GLOB_NEXT = 0x0e,
    IOCTL_WFS_GLOB_END = 0x0f,
    IOCTL_WFS_SET_HOMEDIR = 0x10,
    IOCTL_WFS_CHDIR = 0x11,
    IOCTL_WFS_GET_HOMEDIR = 0x12,
    IOCTL_WFS_GETCWD = 0x13,
    IOCTL_WFS_DELETE = 0x15,
    IOCTL_WFS_GET_ATTRIBUTES = 0x17,
    IOCTL_WFS_OPEN = 0x1A,
    IOCTL_WFS_CLOSE = 0x1E,
    IOCTL_WFS_READ = 0x20,
    IOCTL_WFS_WRITE = 0x22,
    IOCTL_WFS_ATTACH_DETACH = 0x2d,
    IOCTL_WFS_ATTACH_DETACH_2 = 0x2e,
  };

  enum
  {
    WFS_EEMPTY = -10028,  // Directory is empty of iteration completed.
  };

  struct FileDescriptor
  {
    bool in_use;
    std::string path;
    int mode;
    size_t position;
    File::IOFile file;

    bool Open();
  };
  std::vector<FileDescriptor> m_fds;

  FileDescriptor* FindFileDescriptor(u16 fd);
  u16 GetNewFileDescriptor();
  void ReleaseFileDescriptor(u16 fd);
};
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
