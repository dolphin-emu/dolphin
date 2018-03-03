// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Result.h"

class PointerWrap;

namespace IOS::HLE::FS
{
enum class ResultCode
{
  Success,
  Invalid,
  AccessDenied,
  SuperblockWriteFailed,
  SuperblockInitFailed,
  AlreadyExists,
  NotFound,
  FstFull,
  NoFreeSpace,
  NoFreeHandle,
  TooManyPathComponents,
  InUse,
  BadBlock,
  EccError,
  CriticalEccError,
  FileNotEmpty,
  CheckFailed,
  UnknownError,
  ShortRead,
};

template <typename T>
using Result = Common::Result<ResultCode, T>;

using Uid = u32;
using Gid = u16;
using Fd = u32;

enum class Mode : u8
{
  None = 0,
  Read = 1,
  Write = 2,
  ReadWrite = 3,
};

enum class SeekMode : u32
{
  Set = 0,
  Current = 1,
  End = 2,
};

using FileAttribute = u8;

struct Metadata
{
  Uid uid;
  Gid gid;
  FileAttribute attribute;
  Mode owner_mode, group_mode, other_mode;
  bool is_file;
  u32 size;
  u16 fst_index;
};

struct NandStats
{
  u32 cluster_size;
  u32 free_clusters;
  u32 used_clusters;
  u32 bad_clusters;
  u32 reserved_clusters;
  u32 free_inodes;
  u32 used_inodes;
};

struct DirectoryStats
{
  u32 used_clusters;
  u32 used_inodes;
};

struct FileStatus
{
  u32 offset;
  u32 size;
};

class FileSystem
{
public:
  virtual ~FileSystem() = default;

  virtual void DoState(PointerWrap& p) = 0;

  /// Format the file system.
  virtual ResultCode Format(Uid uid) = 0;

  /// Get a file descriptor for accessing a file.
  virtual Result<Fd> OpenFile(Uid uid, Gid gid, const std::string& path, Mode mode) = 0;
  /// Close a file descriptor.
  virtual ResultCode Close(Fd fd) = 0;
  /// Read `size` bytes from the file descriptor. Returns the number of bytes read.
  virtual Result<u32> ReadBytesFromFile(Fd fd, u8* ptr, u32 size) = 0;
  /// Write `size` bytes to the file descriptor. Returns the number of bytes written.
  virtual Result<u32> WriteBytesToFile(Fd fd, const u8* ptr, u32 size) = 0;
  /// Reposition the file offset for a file descriptor.
  virtual Result<u32> SeekFile(Fd fd, u32 offset, SeekMode mode) = 0;
  /// Get status for a file descriptor.
  virtual Result<FileStatus> GetFileStatus(Fd fd) = 0;

  template <typename T>
  Result<u32> ReadFile(Fd fd, T* ptr, u32 count)
  {
    const Result<u32> bytes = ReadBytesFromFile(fd, reinterpret_cast<u8*>(ptr), sizeof(T) * count);
    if (!bytes)
      return bytes.Error();
    if (*bytes != sizeof(T) * count)
      return ResultCode::ShortRead;
    return count;
  }

  template <typename T>
  Result<u32> WriteFile(Fd fd, const T* ptr, u32 count)
  {
    const auto result = WriteBytesToFile(fd, reinterpret_cast<const u8*>(ptr), sizeof(T) * count);
    if (!result)
      return result.Error();
    return count;
  }

  /// Create a file with the specified path and metadata.
  virtual ResultCode CreateFile(Uid caller_uid, Gid caller_gid, const std::string& path,
                                FileAttribute attribute, Mode owner_mode, Mode group_mode,
                                Mode other_mode) = 0;
  /// Create a directory with the specified path and metadata.
  virtual ResultCode CreateDirectory(Uid caller_uid, Gid caller_gid, const std::string& path,
                                     FileAttribute attribute, Mode owner_mode, Mode group_mode,
                                     Mode other_mode) = 0;

  /// Delete a file or directory with the specified path.
  virtual ResultCode Delete(Uid caller_uid, Gid caller_gid, const std::string& path) = 0;
  /// Rename a file or directory with the specified path.
  virtual ResultCode Rename(Uid caller_uid, Gid caller_gid, const std::string& old_path,
                            const std::string& new_path) = 0;

  /// List the children of a directory (non-recursively).
  virtual Result<std::vector<std::string>> ReadDirectory(Uid caller_uid, Gid caller_gid,
                                                         const std::string& path) = 0;

  /// Get metadata about a file.
  virtual Result<Metadata> GetMetadata(Uid caller_uid, Gid caller_gid, const std::string& path) = 0;
  /// Set metadata for a file.
  virtual ResultCode SetMetadata(Uid caller_uid, const std::string& path, Uid uid, Gid gid,
                                 FileAttribute attribute, Mode owner_mode, Mode group_mode,
                                 Mode other_mode) = 0;

  /// Get usage information about the NAND (block size, cluster and inode counts).
  virtual Result<NandStats> GetNandStats() = 0;
  /// Get usage information about a directory (used cluster and inode counts).
  virtual Result<DirectoryStats> GetDirectoryStats(const std::string& path) = 0;
};

std::unique_ptr<FileSystem> MakeFileSystem();

}  // namespace IOS::HLE::FS
