// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/File.h"
#include "Core/IOS/FS/FileSystem.h"

namespace IOS::HLE::FS
{
/// Backend that uses the host file system as backend.
///
/// Ignores metadata like permissions, attributes and various checks and also
/// sometimes returns wrong information because metadata is not available.
class HostFileSystem final : public FileSystem
{
public:
  HostFileSystem(const std::string& root_path);
  ~HostFileSystem();

  void DoState(PointerWrap& p) override;

  ResultCode Format(Uid uid) override;

  Result<FileHandle> OpenFile(Uid uid, Gid gid, const std::string& path, Mode mode) override;
  ResultCode Close(Fd fd) override;
  Result<u32> ReadBytesFromFile(Fd fd, u8* ptr, u32 size) override;
  Result<u32> WriteBytesToFile(Fd fd, const u8* ptr, u32 size) override;
  Result<u32> SeekFile(Fd fd, u32 offset, SeekMode mode) override;
  Result<FileStatus> GetFileStatus(Fd fd) override;

  ResultCode CreateFile(Uid caller_uid, Gid caller_gid, const std::string& path,
                        FileAttribute attribute, Modes modes) override;

  ResultCode CreateDirectory(Uid caller_uid, Gid caller_gid, const std::string& path,
                             FileAttribute attribute, Modes modes) override;

  ResultCode Delete(Uid caller_uid, Gid caller_gid, const std::string& path) override;
  ResultCode Rename(Uid caller_uid, Gid caller_gid, const std::string& old_path,
                    const std::string& new_path) override;

  Result<std::vector<std::string>> ReadDirectory(Uid caller_uid, Gid caller_gid,
                                                 const std::string& path) override;

  Result<Metadata> GetMetadata(Uid caller_uid, Gid caller_gid, const std::string& path) override;
  ResultCode SetMetadata(Uid caller_uid, const std::string& path, Uid uid, Gid gid,
                         FileAttribute attribute, Modes modes) override;

  Result<NandStats> GetNandStats() override;
  Result<DirectoryStats> GetDirectoryStats(const std::string& path) override;

private:
  struct Handle
  {
    bool opened = false;
    Mode mode = Mode::None;
    std::string wii_path;
    std::shared_ptr<File::IOFile> host_file;
    u32 file_offset = 0;
  };
  Handle* AssignFreeHandle();
  Handle* GetHandleFromFd(Fd fd);
  Fd ConvertHandleToFd(const Handle* handle) const;

  std::string BuildFilename(const std::string& wii_path) const;
  std::shared_ptr<File::IOFile> OpenHostFile(const std::string& host_path);

  std::string m_root_path;
  std::map<std::string, std::weak_ptr<File::IOFile>> m_open_files;
  std::array<Handle, 16> m_handles{};
};

}  // namespace IOS::HLE::FS
