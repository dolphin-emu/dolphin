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

namespace IOS::HLE
{
namespace WFS
{
std::string NativePath(const std::string& wfs_path);
}

enum
{
  WFS_EINVAL = -10003,          // Invalid argument.
  WFS_EBADFD = -10026,          // Invalid file descriptor.
  WFS_EEXIST = -10027,          // File already exists.
  WFS_ENOENT = -10028,          // No such file or directory.
  WFS_FILE_IS_OPENED = -10032,  // Cannot perform operation on an opened file.
};

namespace Device
{
class WFSSRV : public Device
{
public:
  WFSSRV(Kernel& ios, const std::string& device_name);

  IPCCommandResult IOCtl(const IOCtlRequest& request) override;

  s32 Rename(std::string source, std::string dest) const;
  void SetHomeDir(const std::string& home_dir);

private:
  // WFS device name, e.g. msc01/msc02.
  std::string m_device_name;

  // Home / current directories.
  std::string m_home_directory;
  std::string m_current_directory;

  std::string NormalizePath(const std::string& path) const;

  enum
  {
    IOCTL_WFS_INIT = 0x02,
    IOCTL_WFS_SHUTDOWN = 0x03,
    IOCTL_WFS_DEVICE_INFO = 0x04,
    IOCTL_WFS_GET_DEVICE_NAME = 0x05,
    IOCTL_WFS_UNMOUNT_VOLUME = 0x06,
    IOCTL_WFS_UNKNOWN_8 = 0x08,
    IOCTL_WFS_FLUSH = 0x0a,
    IOCTL_WFS_MKDIR = 0x0c,
    IOCTL_WFS_GLOB_START = 0x0d,
    IOCTL_WFS_GLOB_NEXT = 0x0e,
    IOCTL_WFS_GLOB_END = 0x0f,
    IOCTL_WFS_SET_HOMEDIR = 0x10,
    IOCTL_WFS_CHDIR = 0x11,
    IOCTL_WFS_GET_HOMEDIR = 0x12,
    IOCTL_WFS_GETCWD = 0x13,
    IOCTL_WFS_DELETE = 0x15,
    IOCTL_WFS_RENAME = 0x16,
    IOCTL_WFS_GET_ATTRIBUTES = 0x17,
    IOCTL_WFS_CREATE_OPEN = 0x19,
    IOCTL_WFS_OPEN = 0x1A,
    IOCTL_WFS_GET_SIZE = 0x1B,
    IOCTL_WFS_CLOSE = 0x1E,
    IOCTL_WFS_READ = 0x20,
    IOCTL_WFS_WRITE = 0x22,
    IOCTL_WFS_ATTACH_DETACH = 0x2d,
    IOCTL_WFS_ATTACH_DETACH_2 = 0x2e,
    IOCTL_WFS_RENAME_2 = 0x41,
    IOCTL_WFS_CLOSE_2 = 0x47,
    IOCTL_WFS_READ_ABSOLUTE = 0x48,
    IOCTL_WFS_WRITE_ABSOLUTE = 0x49,
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

  // List of addresses of IPC requests left hanging that need closing at
  // shutdown time.
  std::vector<u32> m_hanging;
};
}  // namespace Device
}  // namespace IOS::HLE
