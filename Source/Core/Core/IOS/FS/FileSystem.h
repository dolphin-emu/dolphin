// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
// TODO: Horrible hack, remove ASAP!
#include <Windows.h>
#endif

#include "Common/CommonTypes.h"
#include "Common/Result.h"

class PointerWrap;

namespace IOS::HLE
{
enum ReturnCode : s32;

namespace FS
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

struct NandRedirect
{
  // A Wii FS path, eg. "/title/00010000/534d4e45/data".
  std::string source_path;

  // An absolute host filesystem path the above should be redirected to.
  std::string target_path;
};

struct Modes
{
  Mode owner, group, other;
};
inline bool operator==(const Modes& lhs, const Modes& rhs)
{
  const auto fields = [](const Modes& obj) { return std::tie(obj.owner, obj.group, obj.other); };
  return fields(lhs) == fields(rhs);
}

inline bool operator!=(const Modes& lhs, const Modes& rhs)
{
  return !(lhs == rhs);
}

struct Metadata
{
  Uid uid;
  Gid gid;
  FileAttribute attribute;
  Modes modes;
  bool is_file;
  u32 size;
  u16 fst_index;
};

// size of a single cluster in the NAND in bytes
constexpr u16 CLUSTER_SIZE = 16384;

// total number of clusters available in the NAND
constexpr u16 TOTAL_CLUSTERS = 0x7ec0;

// number of clusters reserved for bad blocks and similar, not accessible to normal writes
constexpr u16 RESERVED_CLUSTERS = 0x0300;

// number of clusters actually usable by the file system
constexpr u16 USABLE_CLUSTERS = TOTAL_CLUSTERS - RESERVED_CLUSTERS;

// size of a single 'block' as defined by the Wii System Menu in clusters
constexpr u16 CLUSTERS_PER_BLOCK = 8;

// total number of user-accessible blocks in the NAND
constexpr u16 USER_BLOCKS = 2176;

// total number of user-accessible clusters in the NAND
constexpr u16 USER_CLUSTERS = USER_BLOCKS * CLUSTERS_PER_BLOCK;

// the inverse of that, the amount of usable clusters reserved for system files
constexpr u16 SYSTEM_CLUSTERS = USABLE_CLUSTERS - USER_CLUSTERS;

// total number of inodes available in the NAND
constexpr u16 TOTAL_INODES = 0x17ff;

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

// Not a real Wii data struct, but useful for calculating how full the user's NAND is even if it's
// way larger than it should be.
struct ExtendedDirectoryStats
{
  u64 used_clusters;
  u64 used_inodes;
};

struct FileStatus
{
  u32 offset;
  u32 size;
};

/// The maximum number of components a path can have.
constexpr size_t MaxPathDepth = 8;
/// The maximum number of characters a path can have.
constexpr size_t MaxPathLength = 64;
/// The maximum number of characters a filename can have.
constexpr size_t MaxFilenameLength = 12;

/// Returns whether a Wii path is valid.
bool IsValidPath(std::string_view path);
bool IsValidNonRootPath(std::string_view path);
bool IsValidFilename(std::string_view filename);

struct SplitPathResult
{
  std::string parent;
  std::string file_name;
};
inline bool operator==(const SplitPathResult& lhs, const SplitPathResult& rhs)
{
  const auto fields = [](const SplitPathResult& obj) {
    return std::tie(obj.parent, obj.file_name);
  };
  return fields(lhs) == fields(rhs);
}

inline bool operator!=(const SplitPathResult& lhs, const SplitPathResult& rhs)
{
  return !(lhs == rhs);
}

/// Split a path into a parent path and the file name. Takes a *valid non-root* path.
///
/// Example: /shared2/sys/SYSCONF => {/shared2/sys, SYSCONF}
SplitPathResult SplitPathAndBasename(std::string_view path);

class FileSystem;
class FileHandle final
{
public:
  FileHandle(FileSystem* fs, Fd fd);
  FileHandle(FileHandle&&);
  ~FileHandle();
  FileHandle(const FileHandle&) = delete;
  FileHandle& operator=(const FileHandle&) = delete;
  FileHandle& operator=(FileHandle&&);

  /// Release the FD so that it is not automatically closed.
  Fd Release();

  template <typename T>
  Result<size_t> Read(T* ptr, size_t count) const;

  template <typename T>
  Result<size_t> Write(const T* ptr, size_t count) const;

  Result<u32> Seek(u32 offset, SeekMode mode) const;
  Result<FileStatus> GetStatus() const;

private:
  FileSystem* m_fs;
  std::optional<Fd> m_fd;
};

class FileSystem
{
public:
  virtual ~FileSystem() = default;

  virtual void DoState(PointerWrap& p) = 0;

  /// Format the file system.
  virtual ResultCode Format(Uid uid) = 0;

  /// Get a file descriptor for accessing a file. The FD will be automatically closed after use.
  virtual Result<FileHandle> OpenFile(Uid uid, Gid gid, const std::string& path, Mode mode) = 0;
  /// Create a file if it doesn't exist and open it in read/write mode.
  Result<FileHandle> CreateAndOpenFile(Uid uid, Gid gid, const std::string& path, Modes modes);
  /// Close a file descriptor.
  virtual ResultCode Close(Fd fd) = 0;
  /// Read `size` bytes from the file descriptor. Returns the number of bytes read.
  virtual Result<u32> ReadBytesFromFile(Fd fd, u8* ptr, u32 size) = 0;
  /// Write `size` bytes to the file descriptor. Returns the number of bytes written.
  virtual Result<u32> WriteBytesToFile(Fd fd, const u8* ptr, u32 size) = 0;
  /// Reposition the file offset for a file descriptor.
  virtual Result<u32> SeekFile(Fd fd, u32 offset, SeekMode mode) = 0;
  /// Get status for a file descriptor.
  /// Guaranteed to succeed for a valid file descriptor.
  virtual Result<FileStatus> GetFileStatus(Fd fd) = 0;

  /// Create a file with the specified path and metadata.
  virtual ResultCode CreateFile(Uid caller_uid, Gid caller_gid, const std::string& path,
                                FileAttribute attribute, Modes modes) = 0;
  /// Create a directory with the specified path and metadata.
  virtual ResultCode CreateDirectory(Uid caller_uid, Gid caller_gid, const std::string& path,
                                     FileAttribute attribute, Modes modes) = 0;

  /// Create any parent directories for a path with the specified metadata.
  /// Example: "/a/b" to create directory /a; "/a/b/" to create directories /a and /a/b
  ResultCode CreateFullPath(Uid caller_uid, Gid caller_gid, const std::string& path,
                            FileAttribute attribute, Modes modes);

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
                                 FileAttribute attribute, Modes modes) = 0;

  /// Get usage information about the NAND (block size, cluster and inode counts).
  virtual Result<NandStats> GetNandStats() = 0;
  /// Get usage information about a directory (used cluster and inode counts).
  virtual Result<DirectoryStats> GetDirectoryStats(const std::string& path) = 0;

  /// Like GetDirectoryStats() but not limited to the actual 512 MB NAND limit.
  virtual Result<ExtendedDirectoryStats> GetExtendedDirectoryStats(const std::string& path) = 0;

  virtual void SetNandRedirects(std::vector<NandRedirect> nand_redirects) = 0;
};

template <typename T>
Result<size_t> FileHandle::Read(T* ptr, size_t count) const
{
  const Result<u32> bytes = m_fs->ReadBytesFromFile(*m_fd, reinterpret_cast<u8*>(ptr),
                                                    static_cast<u32>(sizeof(T) * count));
  if (!bytes)
    return bytes.Error();
  if (*bytes != sizeof(T) * count)
    return ResultCode::ShortRead;
  return count;
}

template <typename T>
Result<size_t> FileHandle::Write(const T* ptr, size_t count) const
{
  const auto result = m_fs->WriteBytesToFile(*m_fd, reinterpret_cast<const u8*>(ptr),
                                             static_cast<u32>(sizeof(T) * count));
  if (!result)
    return result.Error();
  return count;
}

enum class Location
{
  Configured,
  Session,
};

std::unique_ptr<FileSystem> MakeFileSystem(Location location = Location::Session,
                                           std::vector<NandRedirect> nand_redirects = {});

/// Convert a FS result code to an IOS error code.
IOS::HLE::ReturnCode ConvertResult(ResultCode code);

}  // namespace FS
}  // namespace IOS::HLE
