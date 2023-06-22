// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/IOFile.h"
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
  HostFileSystem(const std::string& root_path, std::vector<NandRedirect> nand_redirects = {});
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

  void SetNandRedirects(std::vector<NandRedirect> nand_redirects) override;

private:
  void DoStateWriteOrMeasure(PointerWrap& p, std::string start_directory_path);
  void DoStateRead(PointerWrap& p, std::string start_directory_path);

  struct FstEntry
  {
    bool CheckPermission(Uid uid, Gid gid, Mode requested_mode) const;
    std::string name;
    Metadata data{};
    /// Children of this FST entry. Only valid for directories.
    ///
    /// We use a vector rather than a list here because iterating over children
    /// happens a lot more often than removals.
    /// Newly created entries are added at the end.
    std::vector<FstEntry> children;
  };

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

  struct HostFilename
  {
    std::string host_path;
    bool is_redirect;
  };
  HostFilename BuildFilename(const std::string& wii_path) const;
  std::shared_ptr<File::IOFile> OpenHostFile(const std::string& host_path);

  ResultCode CreateFileOrDirectory(Uid uid, Gid gid, const std::string& path,
                                   FileAttribute attribute, Modes modes, bool is_file);
  bool IsFileOpened(const std::string& path) const;
  bool IsDirectoryInUse(const std::string& path) const;

  std::string GetFstFilePath() const;
  void ResetFst();
  void LoadFst();
  void SaveFst();
  /// Get the FST entry for a file (or directory).
  /// Automatically creates fallback entries for parents if they do not exist.
  /// Returns nullptr if the path is invalid or the file does not exist.
  FstEntry* GetFstEntryForPath(const std::string& path);

  /// FST entry for the filesystem root.
  ///
  /// Note that unlike a real Wii's FST, ours is the single source of truth only for
  /// filesystem metadata and ordering. File existence must be checked by querying
  /// the host filesystem.
  /// The reasons for this design are twofold: existing users do not have a FST
  /// and we do not want FS to break if the user adds or removes files in their
  /// filesystem root manually.
  FstEntry m_root_entry{};
  std::string m_root_path;
  std::map<std::string, std::weak_ptr<File::IOFile>> m_open_files;
  std::array<Handle, 16> m_handles{};

  FstEntry m_redirect_fst{};
  std::vector<NandRedirect> m_nand_redirects;
};

}  // namespace IOS::HLE::FS
