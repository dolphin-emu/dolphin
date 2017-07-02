// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/IOS.h"

class PointerWrap;

namespace File
{
class IOFile;
}

namespace IOS
{
namespace HLE
{
std::string BuildFilename(const std::string& wii_path);
void CreateVirtualFATFilesystem();

namespace Device
{
class FileIO : public Device
{
public:
  FileIO(Kernel& ios, const std::string& device_name);

  ReturnCode Close(u32 fd) override;
  ReturnCode Open(const OpenRequest& request) override;
  IPCCommandResult Seek(const SeekRequest& request) override;
  IPCCommandResult Read(const ReadWriteRequest& request) override;
  IPCCommandResult Write(const ReadWriteRequest& request) override;
  IPCCommandResult IOCtl(const IOCtlRequest& request) override;
  void PrepareForState(PointerWrap::Mode mode) override;
  void DoState(PointerWrap& p) override;

  void OpenFile();

private:
  enum
  {
    ISFS_FUNCNULL = 0,
    ISFS_FUNCGETSTAT = 1,
    ISFS_FUNCREADDIR = 2,
    ISFS_FUNCGETATTR = 3,
    ISFS_FUNCGETUSAGE = 4
  };

  enum
  {
    ISFS_IOCTL_FORMAT = 1,
    ISFS_IOCTL_GETSTATS = 2,
    ISFS_IOCTL_CREATEDIR = 3,
    ISFS_IOCTL_READDIR = 4,
    ISFS_IOCTL_SETATTR = 5,
    ISFS_IOCTL_GETATTR = 6,
    ISFS_IOCTL_DELETE = 7,
    ISFS_IOCTL_RENAME = 8,
    ISFS_IOCTL_CREATEFILE = 9,
    ISFS_IOCTL_SETFILEVERCTRL = 10,
    ISFS_IOCTL_GETFILESTATS = 11,
    ISFS_IOCTL_GETUSAGE = 12,
    ISFS_IOCTL_SHUTDOWN = 13
  };

  IPCCommandResult GetFileStats(const IOCtlRequest& request);

  u32 m_Mode = 0;
  u32 m_SeekPos = 0;

  std::string m_filepath;
  std::shared_ptr<File::IOFile> m_file;
};
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
