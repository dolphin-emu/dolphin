// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <map>
#include <string>

#include "Common/CommonTypes.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/FS/FileSystem.h"
#include "Core/IOS/IOS.h"

class PointerWrap;

namespace IOS::HLE
{
constexpr FS::Fd INVALID_FD = 0xffffffff;

class FSDevice : public Device
{
public:
  FSDevice(Kernel& ios, const std::string& device_name);

  void DoState(PointerWrap& p) override;

  std::optional<IPCReply> Open(const OpenRequest& request) override;
  std::optional<IPCReply> Close(u32 fd) override;
  std::optional<IPCReply> Read(const ReadWriteRequest& request) override;
  std::optional<IPCReply> Write(const ReadWriteRequest& request) override;
  std::optional<IPCReply> Seek(const SeekRequest& request) override;
  std::optional<IPCReply> IOCtl(const IOCtlRequest& request) override;
  std::optional<IPCReply> IOCtlV(const IOCtlVRequest& request) override;

private:
  struct Handle
  {
    u16 gid = 0;
    u32 uid = 0;
    FS::Fd fs_fd = INVALID_FD;
    // We use a std::array to keep this savestate friendly.
    std::array<char, 64> name{};
    bool superblock_flush_needed = false;
  };

  enum
  {
    ISFS_IOCTL_FORMAT = 1,
    ISFS_IOCTL_GETSTATS = 2,
    ISFS_IOCTL_CREATEDIR = 3,
    ISFS_IOCTLV_READDIR = 4,
    ISFS_IOCTL_SETATTR = 5,
    ISFS_IOCTL_GETATTR = 6,
    ISFS_IOCTL_DELETE = 7,
    ISFS_IOCTL_RENAME = 8,
    ISFS_IOCTL_CREATEFILE = 9,
    ISFS_IOCTL_SETFILEVERCTRL = 10,
    ISFS_IOCTL_GETFILESTATS = 11,
    ISFS_IOCTLV_GETUSAGE = 12,
    ISFS_IOCTL_SHUTDOWN = 13,
  };

  IPCReply Format(const Handle& handle, const IOCtlRequest& request);
  IPCReply GetStats(const Handle& handle, const IOCtlRequest& request);
  IPCReply CreateDirectory(const Handle& handle, const IOCtlRequest& request);
  IPCReply ReadDirectory(const Handle& handle, const IOCtlVRequest& request);
  IPCReply SetAttribute(const Handle& handle, const IOCtlRequest& request);
  IPCReply GetAttribute(const Handle& handle, const IOCtlRequest& request);
  IPCReply DeleteFile(const Handle& handle, const IOCtlRequest& request);
  IPCReply RenameFile(const Handle& handle, const IOCtlRequest& request);
  IPCReply CreateFile(const Handle& handle, const IOCtlRequest& request);
  IPCReply SetFileVersionControl(const Handle& handle, const IOCtlRequest& request);
  IPCReply GetFileStats(const Handle& handle, const IOCtlRequest& request);
  IPCReply GetUsage(const Handle& handle, const IOCtlVRequest& request);
  IPCReply Shutdown(const Handle& handle, const IOCtlRequest& request);

  u64 EstimateTicksForReadWrite(const Handle& handle, const ReadWriteRequest& request);
  u64 SimulatePopulateFileCache(u32 fd, u32 offset, u32 file_size);
  u64 SimulateFlushFileCache();
  bool HasCacheForFile(u32 fd, u32 offset) const;

  std::map<u32, Handle> m_fd_map;
  u32 m_cache_fd = INVALID_FD;
  u16 m_cache_chain_index = 0;
  bool m_dirty_cache = false;
};
}  // namespace IOS::HLE
